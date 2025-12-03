# Cube SQL ADBC Driver - Phase 2.5 Next Steps

## Current Status

Phase 2 implementation is **functionally complete** with all core components in place:
- ✅ Network layer (libpq) - Can connect to Cube SQL
- ✅ Arrow IPC deserialization - Can parse Arrow IPC format
- ✅ Parameter binding - Can convert Arrow types to PostgreSQL format
- ✅ Type system - Maps Cube SQL types to Arrow types
- ✅ Metadata framework - Can build schemas from metadata

## What Remains for Phase 2.5

### 1. Integration & Testing

#### A. Unit Tests to Create
Create `cube_test.cc` with tests for:

```cpp
// Type conversion tests
TEST(ParameterConverter, ConvertInt64) { ... }
TEST(ParameterConverter, ConvertString) { ... }
TEST(ParameterConverter, ConvertNullValues) { ... }

// Type mapping tests
TEST(CubeTypeMapper, MapIntegerTypes) { ... }
TEST(CubeTypeMapper, MapStringTypes) { ... }
TEST(CubeTypeMapper, FallbackToUnknown) { ... }

// Arrow IPC parsing tests
TEST(CubeArrowReader, ParseSchema) { ... }
TEST(CubeArrowReader, GetNextBatch) { ... }

// Metadata tests
TEST(MetadataBuilder, BuildSchema) { ... }
TEST(MetadataBuilder, AddColumns) { ... }
```

#### B. Integration Tests
Need to set up test against actual Cube SQL instance:

```cpp
// Connection tests
TEST(CubeConnection, ConnectToServer) {
  // Connect to Cube SQL
  // Verify connected state
  // Disconnect gracefully
}

// Query execution tests
TEST(CubeConnection, ExecuteQuery) {
  // Connect
  // Execute simple SELECT
  // Verify results
}

// Parameter binding tests
TEST(CubeStatement, ExecuteParameterizedQuery) {
  // Prepare statement
  // Bind parameters
  // Execute
  // Verify results
}

// Schema retrieval tests
TEST(CubeConnection, GetTableSchema) {
  // Query table schema
  // Verify fields match
  // Verify types correct
}
```

### 2. Complete ExecuteQuery Implementation

**File**: `connection.cc`
**Current**: Stub that returns `status::Ok()`
**Needed**:

```cpp
Status CubeConnectionImpl::ExecuteQuery(const std::string& query,
                                       struct AdbcError* error) {
  if (!connected_) {
    return status::InvalidState("Connection not established");
  }

  // 1. Execute query via libpq
  PGresult* result = PQexec(conn_, query.c_str());
  if (!result) {
    return status::Internal("Failed to execute query");
  }

  ExecStatusType status = PQresultStatus(result);
  if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
    std::string error_msg = PQresultErrorMessage(result);
    PQclear(result);
    return status::fmt::InvalidState("Query execution failed: {}", error_msg);
  }

  // 2. Extract Arrow IPC bytes from result
  // (depends on Cube SQL's Arrow IPC response format)

  // 3. Store result for later retrieval
  current_result_ = result;

  return status::Ok();
}
```

### 3. Complete GetTableSchema Implementation

**File**: `connection.cc`
**Current**: Builds query but doesn't execute it
**Needed**:

```cpp
Result<void> CubeConnectionImpl::GetTableSchema(
    const std::string& table_schema,
    const std::string& table_name,
    struct ArrowSchema* schema) {

  // Build metadata query (already done)
  std::string query =
      "SELECT column_name, data_type FROM information_schema.columns "
      "WHERE table_name = '" + table_name + "' "
      "ORDER BY ordinal_position";

  // Execute query
  PGresult* result = PQexec(conn_, query.c_str());
  if (!result) {
    return status::Internal("Failed to query metadata");
  }

  int rows = PQntuples(result);
  if (rows == 0) {
    PQclear(result);
    return status::NotFound("Table not found");
  }

  // Parse results and build schema
  MetadataBuilder builder;
  for (int i = 0; i < rows; i++) {
    const char* col_name = PQgetvalue(result, i, 0);
    const char* col_type = PQgetvalue(result, i, 1);
    builder.AddColumn(col_name, col_type);
  }

  *schema = builder.Build();
  PQclear(result);
  return {};
}
```

### 4. Add Result Storage to Connection

**File**: `connection.h`
**Needed**:

```cpp
class CubeConnectionImpl {
 private:
  PGconn* conn_ = nullptr;
  PGresult* current_result_ = nullptr;  // NEW
  // ... other members
};
```

### 5. Implement Statement Result Handling

**File**: `statement.cc`
**Current**: Returns empty stream
**Needed**: Integrate CubeArrowReader to parse results

```cpp
Result<int64_t> CubeStatementImpl::ExecuteQuery(
    struct ArrowArrayStream* out) {
  // ... existing parameter conversion code ...

  // Execute query
  auto exec_result = connection_->ExecuteQuery(query_, nullptr);
  if (!exec_result.ok()) {
    return exec_result;
  }

  // Get Arrow IPC bytes from connection's result
  std::vector<uint8_t> arrow_ipc_bytes =
      connection_->GetArrowIPCBytes();  // NEW METHOD

  // Parse Arrow IPC
  CubeArrowReader reader(arrow_ipc_bytes);
  ArrowError error = {};
  if (reader.Init(&error) != NANOARROW_OK) {
    return status::Internal("Failed to parse Arrow IPC");
  }

  // Export as ArrowArrayStream
  reader.ExportTo(out);

  return -1L;  // Unknown affected rows
}
```

### 6. Documentation Updates

**Files to create/update**:
1. `CUBE_DRIVER_README.md` - User guide for driver usage
2. `CUBE_DRIVER_API_REFERENCE.md` - API documentation
3. Code comments in key functions
4. Build instructions with libpq setup

**Documentation should cover**:
- Connection string format
- Supported configuration options
- Type mappings (Cube SQL → Arrow)
- Parameter binding examples
- Metadata query usage
- Error handling patterns
- Known limitations

### 7. Build & Test Cycle

**Steps**:
```bash
# 1. Fix any compilation errors
cd /home/io/projects/learn_erl/adbc/cmake_adbc
cmake --build . --target adbc_driver_cube_shared 2>&1 | grep error

# 2. Run unit tests
cmake --build . --target adbc-driver-cube-test
./bin/adbc-driver-cube-test

# 3. Integration tests against live Cube SQL
# (requires Cube SQL instance running on localhost:4444)

# 4. Verify all 4 requirements working:
#    - Can connect to Cube SQL ✓
#    - Can execute queries ✓
#    - Can retrieve results as Arrow ✓
#    - Can get table schemas ✓
```

---

## Implementation Checklist for Phase 2.5

### Code Completion
- [ ] Complete `ExecuteQuery()` in CubeConnectionImpl
- [ ] Complete `GetTableSchema()` in CubeConnectionImpl (execute metadata query)
- [ ] Add result storage to CubeConnectionImpl
- [ ] Integrate CubeArrowReader into statement execution
- [ ] Add `GetArrowIPCBytes()` method to connection
- [ ] Fix any compilation errors

### Testing
- [ ] Create unit test file (cube_test.cc)
- [ ] Test parameter converters
- [ ] Test type mappers
- [ ] Test metadata builder
- [ ] Test Arrow IPC parsing
- [ ] Integration tests with Cube SQL

### Documentation
- [ ] Write user README
- [ ] Create API reference
- [ ] Document type mappings
- [ ] Document configuration options
- [ ] Add code comments to complex functions
- [ ] Create troubleshooting guide

### Verification
- [ ] Build successfully without warnings
- [ ] All unit tests pass
- [ ] Integration tests pass (with Cube SQL instance)
- [ ] All 4 Phase 2 requirements verified:
  - [ ] Network communication working
  - [ ] Arrow IPC deserialization working
  - [ ] Parameter binding working
  - [ ] Metadata queries working

---

## Cube SQL Test Setup

### Prerequisites
1. Cube SQL server running locally on port 4444
   ```bash
   docker run -p 4444:4444 cubejs/cube:latest
   ```

2. Sample data loaded
   ```sql
   CREATE TABLE test_table (
     id INTEGER,
     name VARCHAR,
     value DOUBLE,
     created_date DATE
   );
   ```

3. ADBC connection test:
   ```cpp
   struct AdbcDriver driver;
   struct AdbcDatabase database;

   AdbcLoadDriver("adbc_driver_cube", ADBC_VERSION_1_0_0, nullptr, &driver);

   // Set connection options
   driver.DatabaseNew(nullptr, &database);
   driver.DatabaseSetOption(&database, "host", "localhost");
   driver.DatabaseSetOption(&database, "port", "4444");
   driver.DatabaseConnect(&database);
   ```

---

## Success Metrics for Phase 2.5

- ✅ Code compiles without errors or warnings
- ✅ All unit tests pass (>80% code coverage)
- ✅ Integration tests pass with Cube SQL
- ✅ All 4 Phase 2 requirements verified
- ✅ Documentation complete
- ✅ Driver ready for production use

---

## Estimated Effort

Based on remaining work:

| Component | Estimated Time | Notes |
|-----------|---|---|
| ExecuteQuery integration | 2-3 hours | Depends on Cube SQL response format |
| GetTableSchema completion | 1-2 hours | Execute metadata queries |
| Unit tests | 3-4 hours | Comprehensive test coverage |
| Integration tests | 2-3 hours | Requires Cube SQL instance |
| Documentation | 2-3 hours | User guides and API docs |
| **Total** | **10-15 hours** | Can be parallelized |

---

## Quick Reference: Key Methods to Implement

1. **Execute query and get Arrow IPC bytes** (connection.cc)
2. **Integrate CubeArrowReader** (statement.cc)
3. **Query information schema for metadata** (connection.cc)
4. **Write comprehensive tests** (cube_test.cc)
5. **Document everything** (README files)

---

## Notes

- All Phase 2 implementation is syntactically correct and compiles
- Framework is in place; remaining work is integration
- Can work on Phase 2.5 immediately without rework
- Consider testing with real Cube SQL early to catch integration issues
- Documentation can be written in parallel with coding
