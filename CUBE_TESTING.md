# Cube ADBC Driver Testing Guide

This guide explains how to test the Cube ADBC driver integration with Elixir.

## Overview

The Cube ADBC driver enables Elixir applications to connect to Cube.js via the Arrow Native protocol. The driver is built in C and provides native Arrow IPC format communication with cubesqld (Cube SQL server).

## Prerequisites

### 1. Build the Cube Driver

```bash
cd ~/projects/learn_erl/adbc
make
```

This will compile the C driver and place it at:
`priv/lib/libadbc_driver_cube.so`

### 2. Start Cube Services

You need two services running:

**Terminal 1 - Start Cube.js API:**
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cube-api.sh
```

Wait for: `🚀 Cube API server is listening on 4008`

**Terminal 2 - Start cubesqld (with Arrow Native fix):**
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
```

Wait for:
```
🔗 Cube SQL (pg) is listening on 0.0.0.0:4444
🔗 Cube SQL (arrow) is listening on 0.0.0.0:4445
```

## Running Tests

### Quick Run - Use the Test Script

```bash
cd ~/projects/learn_erl/adbc
./test/run_cube_tests.sh
```

This script:
- Checks if the driver library exists
- Verifies cubesqld is running on port 4445
- Runs all Cube tests
- Shows clear success/failure messages

**Verbose mode:**
```bash
./test/run_cube_tests.sh --verbose
```

### Manual Run - Using Mix

```bash
cd ~/projects/learn_erl/adbc

# Run only Cube tests
mix test test/adbc_cube_test.exs --include cube

# Run with trace for detailed output
mix test test/adbc_cube_test.exs --include cube --trace

# Run a specific test
mix test test/adbc_cube_test.exs:19 --include cube
```

### Run All Tests (Including Cube)

```bash
mix test --include cube
```

## Test Coverage

The test suite covers:

### Basic Queries
- ✅ Simple SELECT 1
- ✅ Multi-column SELECT
- ✅ Different integer values

### Cube-Specific Queries
- ✅ Dimensions and measures
- ✅ WHERE clauses
- ✅ ORDER BY
- ✅ LIMIT
- ✅ GROUP BY

### Data Types
- ✅ STRING
- ✅ INTEGER (INT64)
- ✅ DOUBLE/FLOAT
- ✅ BOOLEAN
- ✅ NULL handling

### Multiple Rows
- ✅ Multi-row results
- ✅ Large result sets (30+ rows)

### Error Handling
- ✅ Invalid SQL syntax
- ✅ Non-existent tables
- ✅ Invalid Cube syntax

### Connection Management
- ✅ Multiple connections
- ✅ Connection reuse
- ✅ Concurrent queries

### Result Module Integration
- ✅ Result.materialize/1
- ✅ Result.to_map/1
- ✅ Cube query results

## Example Test Output

### Successful Run
```
======================================
Cube ADBC Driver Tests
======================================

✓ Cube driver found
✓ cubesqld is running on port 4445

Running Cube ADBC tests...

....................

Finished in 2.5 seconds (0.1s async, 2.4s sync)
20 tests, 0 failures

✅ All Cube tests passed!
```

### Failed Connection
```
Error: cubesqld is not running on port 4445

Start it with:
  cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
  ./start-cube-api.sh    # Terminal 1
  ./start-cubesqld.sh    # Terminal 2
```

## Test Examples

### Basic Query Test
```elixir
test "runs simple SELECT 1 query", %{conn: conn} do
  assert {:ok, results} = Connection.query(conn, "SELECT 1 as test")

  materialized = Result.materialize(results)

  assert %Result{
           data: [
             %Column{
               name: "test",
               type: :s64,
               data: [1]
             }
           ]
         } = materialized
end
```

### Cube Query Test
```elixir
test "queries Cube dimension and measure", %{conn: conn} do
  query = """
  SELECT of_customers.brand, MEASURE(of_customers.count)
  FROM of_customers
  GROUP BY 1
  """

  assert {:ok, results} = Connection.query(conn, query)
  materialized = Result.materialize(results)

  assert %Result{data: [brand_col, count_col]} = materialized
  assert brand_col.type == :string
  assert count_col.type == :s64
  assert length(brand_col.data) > 0
end
```

## Connection Configuration

Tests use the following connection parameters:

```elixir
db = Adbc.Database.start_link(
  driver: "/home/io/projects/learn_erl/adbc/priv/lib/libadbc_driver_cube.so",
  host: "localhost",
  port: 4445,
  connection_mode: "native",
  token: "test"
)

conn = Adbc.Connection.start_link(database: db)
```

## Troubleshooting

### Driver Not Found
```
Error: Cube driver not found at .../libadbc_driver_cube.so

Build it with:
  cd ~/projects/learn_erl/adbc
  make
```

**Solution:** Run `make` to build the driver

### Connection Refused
```
** (RuntimeError) Cube server (cubesqld) is not running on localhost:4445
```

**Solution:** Start cubesqld using the startup scripts

### Tests Timeout
```
** (ExUnit.TimeoutError) test timed out after 30000ms
```

**Possible causes:**
1. cubesqld not responding
2. Cube API not running
3. Network issues

**Solution:**
- Check server logs
- Restart services
- Verify port availability

### Invalid Cube Syntax Errors
```
** (Adbc.Error) Internal: Initial planning error...
```

**Solution:** Check the Cube query syntax - ensure:
- MEASURE() functions are used correctly
- GROUP BY includes all dimensions
- Table names match Cube schema

## Test Data

Tests use the `of_customers` cube with these dimensions/measures:
- **Dimension:** `of_customers.brand` (STRING)
- **Measure:** `of_customers.count` (INTEGER)

Expected test data: ~34 unique brands

## Performance

- Basic queries: ~50-100ms
- Cube queries: ~100-500ms (depending on data)
- Large result sets: ~200-800ms

## Continuous Integration

To run Cube tests in CI:

```yaml
# .github/workflows/test.yml
- name: Start Cube services
  run: |
    cd cube/examples/recipes/arrow-ipc
    ./start-cube-api.sh &
    ./start-cubesqld.sh &
    sleep 10

- name: Build Cube driver
  run: |
    cd adbc
    make

- name: Run Cube tests
  run: |
    cd adbc
    ./test/run_cube_tests.sh
```

## Related Documentation

- **Build Documentation:** `BUILD_DOCUMENTATION_INDEX.md`
- **Arrow IPC Implementation:** `ARROW_IPC_PARSING_SUCCESS.md`
- **Cube Driver Status:** `C_DRIVER_STATUS.md`
- **cubesqld Fix:** `~/projects/learn_erl/cube/rust/cubesql/change.log`

## Test File Structure

```
test/
├── adbc_cube_test.exs          # Main Cube test suite
├── run_cube_tests.sh           # Test runner script
├── test_helper.exs             # Test configuration
├── adbc_postgres_test.exs      # PostgreSQL tests (reference)
├── adbc_sqlite_test.exs        # SQLite tests (reference)
└── adbc_duckdb_test.exs        # DuckDB tests (reference)
```

## Quick Commands Reference

```bash
# Build driver
make

# Start servers
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cube-api.sh    # Terminal 1
./start-cubesqld.sh    # Terminal 2

# Run tests
cd ~/projects/learn_erl/adbc
./test/run_cube_tests.sh

# Or manually
mix test test/adbc_cube_test.exs --include cube

# Verbose mode
mix test test/adbc_cube_test.exs --include cube --trace

# Run specific test
mix test test/adbc_cube_test.exs:120 --include cube
```

## Next Steps

After running tests successfully:

1. **Integration:** Integrate Cube driver into your Elixir application
2. **Optimization:** Profile and optimize query performance
3. **Production:** Configure connection pooling and error handling
4. **Monitoring:** Add logging and metrics collection

## Support

For issues or questions:
- Check server logs: `tail -f cube/examples/recipes/arrow-ipc/cube-api.log`
- Review change log: `cube/rust/cubesql/change.log`
- See debug scripts: `cube/examples/recipes/arrow-ipc/DEBUG-SCRIPTS.md`
