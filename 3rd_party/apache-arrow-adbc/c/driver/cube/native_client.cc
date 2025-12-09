#include "native_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "adbc.h"

namespace adbc::cube {

// Helper to set error messages
void SetNativeClientError(AdbcError* error, const std::string& message) {
    if (error) {
        error->message = new char[message.length() + 1];
        std::strcpy(error->message, message.c_str());
    }
}

NativeClient::NativeClient()
    : socket_fd_(-1), authenticated_(false) {}

NativeClient::~NativeClient() {
    Close();
}

AdbcStatusCode NativeClient::Connect(const std::string& host, int port, AdbcError* error) {
    if (IsConnected()) {
        SetError(error, "Already connected");
        return AdbcStatusCode::ADBC_STATUS_INVALID_STATE;
    }

    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        SetError(error, "Failed to create socket: " + std::string(strerror(errno)));
        return AdbcStatusCode::ADBC_STATUS_IO;
    }

    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        close(socket_fd_);
        socket_fd_ = -1;
        SetError(error, "Failed to resolve hostname: " + host);
        return AdbcStatusCode::ADBC_STATUS_IO;
    }

    // Setup server address
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    // Connect to server
    if (connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr),
                sizeof(server_addr)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        SetError(error, "Failed to connect to " + host + ":" + std::to_string(port) +
                            ": " + std::string(strerror(errno)));
        return AdbcStatusCode::ADBC_STATUS_IO;
    }

    // Perform handshake
    auto status = PerformHandshake(error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        Close();
        return status;
    }

    return AdbcStatusCode::ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::PerformHandshake(AdbcError* error) {
    // Send handshake request
    HandshakeRequest request;
    request.version = PROTOCOL_VERSION;

    auto data = request.Encode();
    auto status = WriteMessage(data, error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        return status;
    }

    // Receive handshake response
    auto response_data = ReadMessage(error);
    if (response_data.empty()) {
        SetError(error, "Empty handshake response");
        return AdbcStatusCode::ADBC_STATUS_IO;
    }

    // Skip length prefix (first 4 bytes) and decode
    try {
        auto response = HandshakeResponse::Decode(response_data.data() + 4,
                                                  response_data.size() - 4);

        if (response->version != PROTOCOL_VERSION) {
            SetError(error, "Protocol version mismatch. Client: " +
                                std::to_string(PROTOCOL_VERSION) +
                                ", Server: " + std::to_string(response->version));
            return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
        }

        server_version_ = response->server_version;
    } catch (const std::exception& e) {
        SetError(error, "Failed to decode handshake response: " + std::string(e.what()));
        return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
    }

    return AdbcStatusCode::ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::Authenticate(const std::string& token,
                                         const std::string& database,
                                         AdbcError* error) {
    if (!IsConnected()) {
        SetError(error, "Not connected");
        return AdbcStatusCode::ADBC_STATUS_INVALID_STATE;
    }

    if (authenticated_) {
        SetError(error, "Already authenticated");
        return AdbcStatusCode::ADBC_STATUS_INVALID_STATE;
    }

    // Send authentication request
    AuthRequest request;
    request.token = token;
    request.database = database;

    auto data = request.Encode();
    auto status = WriteMessage(data, error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        return status;
    }

    // Receive authentication response
    auto response_data = ReadMessage(error);
    if (response_data.empty()) {
        SetError(error, "Empty authentication response");
        return AdbcStatusCode::ADBC_STATUS_IO;
    }

    // Skip length prefix and decode
    try {
        auto response = AuthResponse::Decode(response_data.data() + 4,
                                            response_data.size() - 4);

        if (!response->success) {
            SetError(error, "Authentication failed");
            return AdbcStatusCode::ADBC_STATUS_UNAUTHENTICATED;
        }

        session_id_ = response->session_id;
        authenticated_ = true;
    } catch (const std::exception& e) {
        SetError(error, "Failed to decode authentication response: " + std::string(e.what()));
        return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
    }

    return AdbcStatusCode::ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::ExecuteQuery(const std::string& sql,
                                         struct ArrowArrayStream* out,
                                         AdbcError* error) {
    if (!IsConnected()) {
        SetError(error, "Not connected");
        return AdbcStatusCode::ADBC_STATUS_INVALID_STATE;
    }

    if (!authenticated_) {
        SetError(error, "Not authenticated");
        return AdbcStatusCode::ADBC_STATUS_UNAUTHENTICATED;
    }

    // Send query request
    QueryRequest request;
    request.sql = sql;

    auto data = request.Encode();
    auto status = WriteMessage(data, error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        return status;
    }

    // Collect all Arrow IPC data
    std::vector<uint8_t> arrow_ipc_data;
    bool query_complete = false;
    int64_t rows_affected = 0;

    while (!query_complete) {
        auto response_data = ReadMessage(error);
        if (response_data.empty()) {
            SetError(error, "Empty query response");
            return AdbcStatusCode::ADBC_STATUS_IO;
        }

        // Check message type (byte at offset 4, after length prefix)
        MessageType msg_type = static_cast<MessageType>(response_data[4]);

        try {
            switch (msg_type) {
                case MessageType::QueryResponseSchema: {
                    auto response = QueryResponseSchema::Decode(
                        response_data.data() + 4, response_data.size() - 4);
                    // Append schema to Arrow IPC data
                    arrow_ipc_data.insert(arrow_ipc_data.end(),
                                        response->arrow_ipc_schema.begin(),
                                        response->arrow_ipc_schema.end());
                    break;
                }

                case MessageType::QueryResponseBatch: {
                    auto response = QueryResponseBatch::Decode(
                        response_data.data() + 4, response_data.size() - 4);
                    // Append batch to Arrow IPC data
                    arrow_ipc_data.insert(arrow_ipc_data.end(),
                                        response->arrow_ipc_batch.begin(),
                                        response->arrow_ipc_batch.end());
                    break;
                }

                case MessageType::QueryComplete: {
                    auto response = QueryComplete::Decode(
                        response_data.data() + 4, response_data.size() - 4);
                    rows_affected = response->rows_affected;
                    query_complete = true;
                    break;
                }

                case MessageType::Error: {
                    auto response = ErrorMessage::Decode(
                        response_data.data() + 4, response_data.size() - 4);
                    SetError(error, "Query error [" + response->code + "]: " + response->message);
                    return AdbcStatusCode::ADBC_STATUS_UNKNOWN;
                }

                default: {
                    SetError(error, "Unexpected message type: " +
                                        std::to_string(static_cast<uint8_t>(msg_type)));
                    return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
                }
            }
        } catch (const std::exception& e) {
            SetError(error, "Failed to decode response: " + std::string(e.what()));
            return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
        }
    }

    // Parse Arrow IPC data using CubeArrowReader
    if (arrow_ipc_data.empty()) {
        SetError(error, "No Arrow IPC data received");
        return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
    }

    try {
        auto reader = std::make_unique<CubeArrowReader>(std::move(arrow_ipc_data));
        auto init_status = reader->Init();
        if (init_status != AdbcStatusCode::ADBC_STATUS_OK) {
            SetError(error, "Failed to initialize Arrow reader");
            return init_status;
        }

        // Export to ArrowArrayStream
        auto export_status = reader->ExportTo(out);
        if (export_status != AdbcStatusCode::ADBC_STATUS_OK) {
            SetError(error, "Failed to export ArrowArrayStream");
            return export_status;
        }

        // Reader ownership transferred to ArrowArrayStream
        reader.release();

    } catch (const std::exception& e) {
        SetError(error, "Failed to parse Arrow IPC data: " + std::string(e.what()));
        return AdbcStatusCode::ADBC_STATUS_INVALID_DATA;
    }

    return AdbcStatusCode::ADBC_STATUS_OK;
}

void NativeClient::Close() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    authenticated_ = false;
    session_id_.clear();
    server_version_.clear();
}

std::vector<uint8_t> NativeClient::ReadMessage(AdbcError* error) {
    // Read 4-byte length prefix
    uint8_t length_buf[4];
    auto status = ReadExact(length_buf, 4, error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        return {};
    }

    // Decode length (big-endian)
    uint32_t length = (static_cast<uint32_t>(length_buf[0]) << 24) |
                     (static_cast<uint32_t>(length_buf[1]) << 16) |
                     (static_cast<uint32_t>(length_buf[2]) << 8) |
                     (static_cast<uint32_t>(length_buf[3]));

    if (length == 0 || length > 100 * 1024 * 1024) {  // 100MB max
        SetError(error, "Invalid message length: " + std::to_string(length));
        return {};
    }

    // Read payload
    std::vector<uint8_t> payload(length);
    status = ReadExact(payload.data(), length, error);
    if (status != AdbcStatusCode::ADBC_STATUS_OK) {
        return {};
    }

    // Return length prefix + payload (for easier parsing)
    std::vector<uint8_t> result;
    result.insert(result.end(), length_buf, length_buf + 4);
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

AdbcStatusCode NativeClient::WriteMessage(const std::vector<uint8_t>& data, AdbcError* error) {
    return WriteExact(data.data(), data.size(), error);
}

AdbcStatusCode NativeClient::ReadExact(uint8_t* buffer, size_t length, AdbcError* error) {
    size_t total_read = 0;
    while (total_read < length) {
        ssize_t n = read(socket_fd_, buffer + total_read, length - total_read);
        if (n < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            SetError(error, "Socket read error: " + std::string(strerror(errno)));
            return AdbcStatusCode::ADBC_STATUS_IO;
        }
        if (n == 0) {
            SetError(error, "Connection closed by server");
            return AdbcStatusCode::ADBC_STATUS_IO;
        }
        total_read += n;
    }
    return AdbcStatusCode::ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::WriteExact(const uint8_t* buffer, size_t length, AdbcError* error) {
    size_t total_written = 0;
    while (total_written < length) {
        ssize_t n = write(socket_fd_, buffer + total_written, length - total_written);
        if (n < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            SetError(error, "Socket write error: " + std::string(strerror(errno)));
            return AdbcStatusCode::ADBC_STATUS_IO;
        }
        total_written += n;
    }
    return AdbcStatusCode::ADBC_STATUS_OK;
}

void NativeClient::SetError(AdbcError* error, const std::string& message) {
    SetNativeClientError(error, message);
}

}  // namespace adbc::cube
