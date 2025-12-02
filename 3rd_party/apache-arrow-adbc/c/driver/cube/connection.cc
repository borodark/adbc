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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nanoarrow/nanoarrow.hpp>

#include "driver/cube/connection.h"
#include "driver/cube/database.h"

namespace adbc::cube {

CubeConnectionImpl::CubeConnectionImpl(const CubeDatabase& database)
    : host_(database.host()),
      port_(database.port()),
      token_(database.token()),
      database_(database.database()),
      user_(database.user()),
      password_(database.password()) {}

CubeConnectionImpl::~CubeConnectionImpl() {
  if (connected_) {
    AdbcError error = {};
    std::ignore = Disconnect(&error);
    error.release(&error);
  }
}

Status CubeConnectionImpl::Connect(struct AdbcError* error) {
  if (host_.empty() || port_.empty()) {
    return status::fmt::InvalidArgument(
        "Connection requires host and port. Got host='{}', port='{}'", host_,
        port_);
  }

  // Build PostgreSQL connection string
  std::string conn_str = "host=" + host_ + " port=" + port_;

  if (!database_.empty()) {
    conn_str += " dbname=" + database_;
  }

  if (!user_.empty()) {
    conn_str += " user=" + user_;
  }

  if (!password_.empty()) {
    conn_str += " password=" + password_;
  }

  // Add output format parameter to use Arrow IPC
  conn_str += " output_format=arrow_ipc";

  // Connect to Cube SQL via PostgreSQL protocol
  conn_ = PQconnectdb(conn_str.c_str());

  if (!conn_) {
    return status::Internal("Failed to allocate PQconnect connection");
  }

  if (PQstatus(conn_) != CONNECTION_OK) {
    std::string error_msg = PQerrorMessage(conn_);
    PQfinish(conn_);
    conn_ = nullptr;
    return status::fmt::InvalidState(
        "Failed to connect to Cube SQL at {}:{}: {}",
        host_, port_, error_msg);
  }

  connected_ = true;
  return status::Ok();
}

Status CubeConnectionImpl::Disconnect(struct AdbcError* error) {
  if (conn_) {
    PQfinish(conn_);
    conn_ = nullptr;
  }
  connected_ = false;
  return status::Ok();
}

Status CubeConnectionImpl::ExecuteQuery(const std::string& query,
                                       struct AdbcError* error) {
  // TODO: Implement query execution against Cube SQL
  // This would:
  // 1. Send query to Cube SQL API
  // 2. Receive Arrow IPC serialized results
  // 3. Deserialize Arrow records
  // 4. Return results via ArrowArrayStream

  if (!connected_) {
    return status::InvalidState("Connection not established");
  }

  return status::Ok();
}


// CubeConnection implementation

Status CubeConnection::InitImpl(void* raw_connection) {
  // raw_connection is the AdbcDatabase* passed from CConnectionInit
  auto* cube_database = static_cast<CubeDatabase*>(raw_connection);
  impl_ = std::make_unique<CubeConnectionImpl>(*cube_database);

  struct AdbcError error = ADBC_ERROR_INIT;
  auto status = impl_->Connect(&error);
  if (error.message) {
    error.release(&error);
  }
  return status;
}

Status CubeConnection::ReleaseImpl() {
  if (impl_) {
    struct AdbcError error = ADBC_ERROR_INIT;
    auto status = impl_->Disconnect(&error);
    if (error.message) {
      error.release(&error);
    }
    impl_.reset();
    return status;
  }
  return status::Ok();
}

Status CubeConnection::SetOptionImpl(std::string_view key, driver::Option value) {
  // Connection-specific options can be added here
  return status::NotImplemented("Connection options not yet implemented");
}

}  // namespace adbc::cube
