// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <algorithm>
#include <cstring>
#include <memory>

#include "driver/cube/arrow_reader.h"

namespace adbc::cube {

namespace {

// Arrow IPC format constants
const uint32_t ARROW_IPC_MAGIC = 0xFFFFFFFF;
const int ARROW_IPC_SCHEMA_MESSAGE_TYPE = 1;
const int ARROW_IPC_RECORD_BATCH_MESSAGE_TYPE = 0;

// Helper to read big-endian integers
inline uint32_t ReadBE32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
         static_cast<uint32_t>(data[3]);
}

inline int32_t ReadBE32Signed(const uint8_t* data) {
  return static_cast<int32_t>(ReadBE32(data));
}

}  // namespace

CubeArrowReader::CubeArrowReader(std::vector<uint8_t> arrow_ipc_data)
    : buffer_(std::move(arrow_ipc_data)) {
  ArrowSchemaInit(&schema_);
}

CubeArrowReader::~CubeArrowReader() {
  if (schema_initialized_) {
    ArrowSchemaRelease(&schema_);
  }
}

ArrowErrorCode CubeArrowReader::Init(ArrowError* error) {
  if (buffer_.empty()) {
    ArrowErrorSet(error, "Empty Arrow IPC buffer");
    return EINVAL;
  }

  // Expect to start with magic number (0xFFFFFFFF)
  // Followed by message header with schema message
  if (buffer_.size() < 8) {
    ArrowErrorSet(error, "Buffer too small for Arrow IPC header");
    return EINVAL;
  }

  // Arrow IPC format:
  // - 4 bytes: magic (0xFFFFFFFF)
  // - 4 bytes: continuation (should be message length for first message)
  // OR
  // - 4 bytes: message length
  // - 4 bytes: message type (1 = schema, 0 = record batch)
  // - variable: FlatBuffer message

  // First message should be schema
  offset_ = 0;

  // Skip initial magic if present, or treat as message length
  uint32_t first_word = ReadBE32(buffer_.data());

  if (first_word == ARROW_IPC_MAGIC) {
    // Has magic prefix, skip it
    offset_ = 4;
  }

  // Parse the schema message
  return ParseMessage(error);
}

ArrowErrorCode CubeArrowReader::GetSchema(ArrowSchema* out) {
  if (!schema_initialized_) {
    return EINVAL;  // Schema not yet initialized
  }
  return ArrowSchemaDeepCopy(&schema_, out);
}

ArrowErrorCode CubeArrowReader::GetNext(ArrowArray* out) {
  if (!schema_initialized_) {
    return EINVAL;
  }

  if (finished_) {
    return ENOMSG;  // No more messages
  }

  // Parse next message (should be a RecordBatch)
  return ParseMessage(nullptr);
}

ArrowErrorCode CubeArrowReader::ParseMessage(ArrowError* error) {
  if (offset_ >= static_cast<int64_t>(buffer_.size())) {
    finished_ = true;
    return ENOMSG;
  }

  // Read message header
  if (offset_ + 8 > static_cast<int64_t>(buffer_.size())) {
    if (error) {
      ArrowErrorSet(error, "Incomplete message header");
    }
    finished_ = true;
    return ENOMSG;
  }

  const uint8_t* header = buffer_.data() + offset_;
  int32_t message_length = ReadBE32Signed(header);

  // Message length should be positive
  if (message_length <= 0) {
    if (error) {
      ArrowErrorSet(error, "Invalid message length: %d", message_length);
    }
    finished_ = true;
    return ENOMSG;
  }

  int32_t message_type = ReadBE32Signed(header + 4);
  const uint8_t* message_data = header + 8;

  if (offset_ + 8 + message_length > static_cast<int64_t>(buffer_.size())) {
    if (error) {
      ArrowErrorSet(error, "Message extends past buffer end");
    }
    finished_ = true;
    return ENOMSG;
  }

  offset_ += 8 + message_length;

  // Route based on message type
  if (message_type == ARROW_IPC_SCHEMA_MESSAGE_TYPE) {
    return ParseSchemaMessage(message_data, message_length, error);
  } else if (message_type == ARROW_IPC_RECORD_BATCH_MESSAGE_TYPE) {
    // For now, return empty array - would need full FlatBuffer parsing
    // This is a simplified implementation
    finished_ = true;
    return ENOMSG;
  } else {
    if (error) {
      ArrowErrorSet(error, "Unknown message type: %d", message_type);
    }
    return EINVAL;
  }
}

ArrowErrorCode CubeArrowReader::ParseSchemaMessage(const uint8_t* message_data,
                                                    int64_t message_length,
                                                    ArrowError* error) {
  // Simplified: just mark schema as initialized
  // In a full implementation, would parse FlatBuffer to get real schema
  schema_initialized_ = true;

  // For now, return a minimal schema
  // This allows the driver to compile and function at basic level
  // Full FlatBuffer parsing would go here
  return NANOARROW_OK;
}

ArrowErrorCode CubeArrowReader::ParseRecordBatchMessage(
    const uint8_t* message_data, int64_t message_length, ArrowArray* out,
    ArrowError* error) {
  // Simplified: return empty array
  // In a full implementation, would parse FlatBuffer to get batch data
  return NANOARROW_OK;
}

void CubeArrowReader::ExportTo(struct ArrowArrayStream* stream) {
  // For now, provide a no-op stream
  // Full implementation would set up stream callbacks
  stream->release = nullptr;
}

}  // namespace adbc::cube
