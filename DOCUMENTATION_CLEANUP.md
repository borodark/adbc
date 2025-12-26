# Documentation Cleanup - Arrow Native Only

**Date:** 2025-12-26
**Focus:** Removed PostgreSQL proxy (port 4444) references, focus exclusively on Arrow Native (port 4445)

## Files Updated

### 1. `test/cube_preagg_benchmark.exs` ✅
**Changed:** Server startup instructions in error message

**Before:**
```elixir
export CUBESQL_CUBESTORE_DIRECT=true
export CUBESQL_CUBE_URL=http://localhost:4008/cubejs-api
export CUBESQL_CUBESTORE_URL=ws://127.0.0.1:3030/ws
export CUBESQL_CUBE_TOKEN=test
export CUBESQL_PG_PORT=4444        # ❌ PostgreSQL port
export CUBEJS_ARROW_PORT=4445
export RUST_LOG=info
```

**After:**
```elixir
./start-cubesqld.sh
# Or manually:
export CUBESQL_CUBE_URL=http://localhost:4008/cubejs-api
export CUBESQL_CUBE_TOKEN=test
export CUBEJS_ARROW_PORT=4445      # ✅ Arrow Native only
export CUBESQL_ARROW_RESULTS_CACHE_ENABLED=true
export CUBESQL_LOG_LEVEL=info
```

### 2. `test/TEST_SUMMARY.md` ✅
**Changed:** Removed specific mention of "port 4444" in irrelevant tests section

**Before:**
```markdown
- `test/adbc_postgres_test.exs` - PostgreSQL wire protocol (port 4444) - NOT RELEVANT
**Note:** We are NOT testing the PostgreSQL proxy (port 4444), only Arrow Native (port 4445).
```

**After:**
```markdown
- `test/adbc_postgres_test.exs` - PostgreSQL driver tests - NOT RELEVANT
**Focus:** We are testing ONLY Arrow Native protocol on port 4445.
```

**Also updated:** Test environment requirements section to be clearer:
- Explicitly labels "Arrow Native server - Port 4445"
- Uses updated environment variables (CUBESQL_ARROW_RESULTS_*)
- Points to correct script paths

## Files Already Clean ✅

### `tests/cpp/README.md`
- Line 219: Contains note "Default port: 4445 (Arrow Native), not 4444 (PostgreSQL wire protocol)"
- This is a helpful clarification, not a usage instruction
- **Decision:** Keep as-is for clarity

### `tests/cpp/QUICK_START.md`
- Only references port 4445 for Arrow Native
- No PostgreSQL mentions
- Already clean ✅

### `tests/cpp/REBASE_VERIFICATION.md`
- Only documents Arrow Native testing
- No PostgreSQL references
- Already clean ✅

### C++ Test Files
- `test_simple.cpp` - Only uses port 4445
- `test_all_types.cpp` - Only uses port 4445
- `test_cube_integration.cpp` - Only uses port 4445
- `test_error_handling.cpp` - Only uses port 4445
- All scripts clean ✅

## Documentation Strategy

### What We Document
✅ **Arrow Native Protocol (Port 4445)**
- Connection configuration
- Environment variables
- Server startup procedures
- Test procedures

### What We Don't Document
❌ **PostgreSQL Wire Protocol (Port 4444)**
- No configuration examples
- No startup procedures
- No test instructions
- Minimal clarifying mentions only

## Summary

All ADBC documentation now focuses exclusively on Arrow Native protocol testing with the two deployed `orders` cubes:
- `orders_with_preagg` (uses pre-aggregations)
- `orders_no_preagg` (direct queries)

**Port usage:**
- ✅ 4445 - Arrow Native server (documented, tested, supported)
- ❌ 4444 - PostgreSQL proxy (not documented, not tested, not supported)

**Environment variables documented:**
- `CUBEJS_ARROW_PORT=4445`
- `CUBESQL_ARROW_RESULTS_CACHE_ENABLED`
- `CUBESQL_ARROW_RESULTS_CACHE_MAX_ENTRIES`
- `CUBESQL_ARROW_RESULTS_CACHE_TTL`
- `CUBESQL_CUBE_URL`
- `CUBESQL_CUBE_TOKEN`
- `CUBESQL_LOG_LEVEL`

**Removed from documentation:**
- ❌ `CUBESQL_PG_PORT` (PostgreSQL port)
- ❌ `CUBESQL_CUBESTORE_DIRECT` (deprecated approach)
- ❌ `CUBESQL_CUBESTORE_URL` (not needed for Arrow Native)
