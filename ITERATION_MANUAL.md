# Development Iteration Manual
## The Change → Start → Test Cycle for Arrow ADBC Cube Driver

**Version**: 1.0
**Last Updated**: December 2025
**Audience**: Developers maintaining or extending the driver

---

## Table of Contents

1. [Introduction](#introduction)
2. [The Core Cycle](#the-core-cycle)
3. [Phase 1: Change](#phase-1-change)
4. [Phase 2: Build](#phase-2-build)
5. [Phase 3: Start Services](#phase-3-start-services)
6. [Phase 4: Test](#phase-4-test)
7. [Phase 5: Debug](#phase-5-debug)
8. [Phase 6: Document](#phase-6-document)
9. [Common Scenarios](#common-scenarios)
10. [Troubleshooting](#troubleshooting)
11. [Best Practices](#best-practices)

---

## Introduction

Software development is iterative. This manual documents the proven cycle for developing and debugging the Arrow ADBC Cube Driver. Each iteration teaches something; accumulated iterations produce working software.

### Philosophy

1. **Fail Fast**: Quick feedback beats perfect planning
2. **Fail Informatively**: Errors should tell you what went wrong
3. **Iterate Rapidly**: Small changes, frequent tests
4. **Document Everything**: Future-you will thank present-you

### Typical Iteration Time

- **Quick iteration** (C code change + test): 2-5 minutes
- **Medium iteration** (with debugging): 10-30 minutes
- **Deep iteration** (architectural change): 1-4 hours

---

## The Core Cycle

```
    ┌──────────────────────────────────────────────────┐
    │                                                  │
    │   1. CHANGE                                      │
    │      ↓                                           │
    │   2. BUILD                                       │
    │      ↓                                           │
    │   3. START SERVICES                              │
    │      ↓                                           │
    │   4. TEST                                        │
    │      ↓                                           │
    │   5. DEBUG (if failed)                           │
    │      ↓                                           │
    │   6. DOCUMENT                                    │
    │      ↓                                           │
    └────→ REPEAT ─────────────────────────────────────┘
```

**Exit Conditions**:
- Tests pass → Document and commit
- Stuck → Ask for help or take break
- Segfault → Add debugging, reduce scope

---

## Phase 1: Change

### 1.1 Identify What to Change

**Sources of Changes**:
- New feature requirement
- Bug report or test failure
- Performance improvement
- Code cleanup

**Key Question**: What is the smallest change that moves toward the goal?

### 1.2 Locate Relevant Files

**C++ Driver** (`3rd_party/apache-arrow-adbc/c/driver/cube/`):
```
arrow_reader.cc          # Arrow IPC parsing, FlatBuffer handling
arrow_reader.h           # Arrow reader interface
native_client.cc         # Protocol communication, socket I/O
native_client.h          # Native client interface
cube_driver.cc           # ADBC API implementation
CMakeLists.txt           # Build configuration
```

**Elixir Tests** (`test/`):
```
adbc_cube_basic_test.exs    # Basic functionality tests (6 tests)
adbc_cube_test.exs          # Comprehensive tests (22 tests)
test_helper.exs             # Test configuration
```

**Documentation**:
```
*.md files                  # Various documentation
```

### 1.3 Make the Change

**C++ Changes**:

**Example 1: Add debugging output**
```cpp
// In arrow_reader.cc
AdbcStatusCode CubeArrowReader::GetNext(struct ArrowArray* out) {
  fprintf(stderr, "[CubeArrowReader::GetNext] Called\n");
  fprintf(stderr, "[CubeArrowReader::GetNext] offset_=%zu, buffer size=%zu\n",
          offset_, buffer_.size());

  // ... existing code ...
}
```

**Example 2: Add new type support**
```cpp
// In arrow_reader.cc, MapFlatBufferTypeToArrow()
case org::apache::arrow::flatbuf::Type::Int32:
  return NANOARROW_TYPE_INT32;  // Add INT32 support

// In BuildArrayForField()
case NANOARROW_TYPE_INT32: {
  // Allocate validity and data buffers
  NANOARROW_RETURN_NOT_OK(ArrowBitmapReserve(&validity, num_rows));
  NANOARROW_RETURN_NOT_OK(ArrowBufferReserve(&data_buffer, num_rows * 4));

  // Extract buffers and populate...
  break;
}
```

**Elixir Test Changes**:

**Example: Add new test**
```elixir
# In test/adbc_cube_basic_test.exs
test "handles INT32 type", %{conn: conn} do
  assert {:ok, results} = Connection.query(conn, "SELECT CAST(42 AS INTEGER) as num")

  materialized = Result.materialize(results)

  assert %Result{
           data: [
             %Column{
               name: "num",
               type: :s32,  # Note: INT32 maps to :s32
               data: [42]
             }
           ]
         } = materialized
end
```

### 1.4 Pre-Build Checklist

- [ ] Syntax looks correct (no obvious typos)
- [ ] Includes are in place
- [ ] Debug logging added (fprintf statements)
- [ ] Comments explain non-obvious logic
- [ ] Ready to compile

---

## Phase 2: Build

### 2.1 Navigate to Build Directory

```bash
cd /home/io/projects/learn_erl/adbc
```

### 2.2 Build the Driver

**Full Build** (first time or after CMake changes):
```bash
make clean
make
```

**Incremental Build** (after C++ source changes):
```bash
make
```

**Fast Build** (parallel compilation):
```bash
make -j4
```

### 2.3 Monitor Build Output

**Success Indicators**:
```
[ 40%] Built target nanoarrow
[100%] Built target adbc_driver_cube_shared
[100%] Built target adbc_nif
```

**Failure Indicators**:
```
error: 'foo' was not declared in this scope
undefined reference to 'bar'
CMake Error: ...
```

### 2.4 Filter Build Errors

**Quick error scan**:
```bash
make 2>&1 | grep -E "(error|warning)" | head -20
```

**Focus on first error**:
```bash
make 2>&1 | grep "error" | head -5
```

### 2.5 Build Checklist

- [ ] Build completed without errors
- [ ] Shared library exists: `priv/lib/libadbc_driver_cube.so`
- [ ] File size is reasonable (several MB)
- [ ] Timestamp is recent: `ls -lh priv/lib/libadbc_driver_cube.so`

**Verify Library**:
```bash
ls -lh priv/lib/libadbc_driver_cube.so
file priv/lib/libadbc_driver_cube.so
# Should show: ELF 64-bit LSO shared object
```

---

## Phase 3: Start Services

The driver requires two services running:

### 3.1 Terminal Layout

Open three terminals:

```
┌─────────────────┬─────────────────┬─────────────────┐
│   Terminal 1    │   Terminal 2    │   Terminal 3    │
│                 │                 │                 │
│  Cube.js API    │   cubesqld      │   Tests         │
│  (Node.js)      │   (Rust)        │   (Elixir/Python)│
└─────────────────┴─────────────────┴─────────────────┘
```

### 3.2 Start Cube.js API (Terminal 1)

```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cube-api.sh
```

**Wait for**:
```
🚀 Cube API server is listening on 4008
```

**Verify**:
```bash
# In another terminal
lsof -i :4008
# Should show node process
```

**If port 4008 is busy**:
```bash
# Find and kill the process
lsof -ti :4008 | xargs kill
# Or use a different port in .env
```

### 3.3 Start cubesqld (Terminal 2)

```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
```

**Wait for**:
```
🔗 Cube SQL (pg) is listening on 0.0.0.0:4444
🔗 Cube SQL (arrow) is listening on 0.0.0.0:4445
```

**Verify**:
```bash
# In another terminal
lsof -i :4445
# Should show cubesqld process
```

**If cubesqld isn't found**:
```bash
# Check if built
ls ~/projects/learn_erl/cube/rust/cubesql/target/release/cubesqld

# If not, build it
cd ~/projects/learn_erl/cube
cargo build --release --bin cubesqld
```

### 3.4 Service Checklist

- [ ] Port 4008: Cube.js API running
- [ ] Port 4445: cubesqld Arrow Native running
- [ ] No error messages in logs
- [ ] Both services show "listening" messages

### 3.5 Quick Service Test

```bash
# Test Cube.js API
curl http://localhost:4008/readyz
# Should return: {"health":"HEALTH_SERVING"}

# Test cubesqld (requires psql)
psql -h 127.0.0.1 -p 4444 -U root -c "SELECT 1;"
# Should return: 1
```

---

## Phase 4: Test

### 4.1 Choose Test Level

**Level 1: Quick C/Python Test** (fastest, ~5 seconds)
```bash
cd ~/projects/learn_erl/adbc
python3 quick_test.py
```

**Level 2: Basic Elixir Tests** (reliable, ~10 seconds)
```bash
cd ~/projects/learn_erl/adbc
./test/run_cube_basic_tests.sh
```

**Level 3: Specific Elixir Test** (targeted, ~5 seconds)
```bash
cd ~/projects/learn_erl/adbc
mix test test/adbc_cube_basic_test.exs:66 --include cube
```

**Level 4: Full Elixir Test Suite** (comprehensive, ~30 seconds, may crash)
```bash
cd ~/projects/learn_erl/adbc
mix test test/adbc_cube_test.exs --include cube
```

### 4.2 Reading Test Output

**Success Pattern**:
```
......

Finished in 1.5 seconds (1.5s async, 0.00s sync)
6 tests, 0 failures
```

**Failure Pattern**:
```
  1) test handles STRING type (Adbc.CubeBasicTest)
     test/adbc_cube_basic_test.exs:101
     match (=) failed
     code:  assert %Result{data: [%Column{type: :string}]} = materialized
     left:  %Adbc.Result{...}
     right: %Adbc.Result{data: [%Adbc.Column{type: :utf8, ...}]}
     stacktrace:
       test/adbc_cube_basic_test.exs:107: (test)
```

**Segfault Pattern**:
```
[CubeArrowReader::GetNext] Called
[CubeArrowReader::GetNext] Successfully parsed RecordBatch
Segmentation fault (core dumped)
```

### 4.3 Test Checklist

- [ ] All expected tests ran
- [ ] No unexpected errors
- [ ] Debug output appears (fprintf messages)
- [ ] No segfaults
- [ ] Results match expectations

### 4.4 Interpreting Results

**Test Passed**: Move to documentation phase

**Test Failed with Clear Error**: Move to debug phase

**Segfault**: Add more debug logging, reduce test scope

**Timeout**: Check if services are running, check for infinite loops

---

## Phase 5: Debug

### 5.1 Debug Levels

**Level 1: Read the Error Message**
- 70% of problems have clear error messages
- Read carefully, don't assume

**Level 2: Add fprintf Debugging**
- Print variable values
- Print function entry/exit
- Print conditional branches taken

**Level 3: Examine Debug Output**
- stderr shows all fprintf output
- Look for unexpected values
- Check execution flow

**Level 4: Hexdump Analysis**
- Check `/tmp/cube_arrow_ipc_data.bin`
- Verify FlatBuffer structure
- Check buffer offsets

**Level 5: GDB Debugging**
- Attach debugger
- Set breakpoints
- Inspect memory

### 5.2 Common Debug Patterns

**Pattern 1: Type Mismatch**
```elixir
# Test expects
type: :string

# But got
type: :utf8
```

**Solution**: Update test expectations or fix type mapping

**Pattern 2: Null Pointer**
```
[CubeArrowReader::BuildArrayForField] Building array for field 'name'
Segmentation fault
```

**Solution**: Add null checks
```cpp
if (validity_buffer == nullptr) {
  fprintf(stderr, "ERROR: validity_buffer is NULL\n");
  return ADBC_STATUS_INTERNAL;
}
```

**Pattern 3: Buffer Overflow**
```
[ParseRecordBatchFlatBuffer] Buffer 0 offset: 1000
[ParseRecordBatchFlatBuffer] Buffer size exceeds data!
```

**Solution**: Check offset calculations
```cpp
if (buffer_offset + buffer_length > buffer_.size()) {
  fprintf(stderr, "ERROR: Buffer exceeds data bounds\n");
  return ADBC_STATUS_INTERNAL;
}
```

### 5.3 Debug Tools

**fprintf Debugging** (most useful):
```cpp
fprintf(stderr, "[FUNCTION] Message with value=%d\n", value);

// Print hex dump
for (size_t i = 0; i < 16 && i < size; i++) {
  fprintf(stderr, " %02x", (unsigned char)buffer[i]);
}
fprintf(stderr, "\n");
```

**Hexdump Analysis**:
```bash
hexdump -C /tmp/cube_arrow_ipc_data.bin | head -20
```

**GDB Debugging**:
```bash
# Start with GDB
gdb --args python3 quick_test.py

# Set breakpoint
(gdb) break CubeArrowReader::GetNext

# Run
(gdb) run

# Examine variables
(gdb) print offset_
(gdb) print buffer_.size()

# Examine memory
(gdb) x/16xb buffer_.data()
```

**Valgrind (memory errors)**:
```bash
valgrind --leak-check=full python3 quick_test.py 2>&1 | less
```

### 5.4 Systematic Debugging Process

1. **Reproduce**: Can you make it fail consistently?
2. **Isolate**: What's the smallest test case that fails?
3. **Hypothesize**: What could cause this failure?
4. **Instrument**: Add debug logging around hypothesis
5. **Test**: Run again, check debug output
6. **Iterate**: Refine hypothesis, repeat

### 5.5 Debug Checklist

- [ ] Error message is understood
- [ ] Debug output shows expected flow
- [ ] Variable values are in expected ranges
- [ ] Null checks are in place
- [ ] Buffer bounds are respected
- [ ] Type assumptions are validated

---

## Phase 6: Document

### 6.1 What to Document

**Always Document**:
- What was changed (files, functions)
- Why it was changed (problem solved)
- How it was tested
- Any new limitations or assumptions

**Sometimes Document**:
- Performance implications
- Alternative approaches considered
- Future work needed

**Never Document**:
- "Fixed bug" (not helpful)
- Obvious code comments (code is self-documenting)

### 6.2 Where to Document

**Code Comments** (for non-obvious logic):
```cpp
// Cube sends two separate Arrow IPC streams:
// 1. Schema-only stream (which we skip)
// 2. Batch stream (which contains both schema and data)
// We must ignore the schema-only stream to avoid corruption.
if (msg_type == MessageType::QueryResponseSchema) {
  fprintf(stderr, "[NativeClient::ExecuteQuery] Skipping schema-only message\n");
  continue;
}
```

**Commit Messages**:
```
Fix: Skip schema-only Arrow IPC stream from Cube

Cube sends two separate streams which caused PyArrow to stop
reading after the first EOS marker. Now we skip the schema-only
stream and only use the batch stream.

Tested:
- quick_test.py: PASS
- test_different_values.py: PASS
```

**Markdown Documentation** (for architectural changes):
```markdown
# Arrow IPC Parsing Fix

## Problem
Cube sends schema and data as separate Arrow IPC streams...

## Solution
Skip the schema-only stream...

## Testing
All tests pass...
```

### 6.3 Documentation Checklist

- [ ] Code comments added for non-obvious logic
- [ ] Commit message describes what and why
- [ ] Markdown documentation updated (if applicable)
- [ ] Test expectations documented
- [ ] Known limitations noted

---

## Common Scenarios

### Scenario 1: Adding Support for New Data Type

**Example**: Add TIMESTAMP support

**Iteration 1: Understand the Type**
1. **Change**: Add fprintf to see what FlatBuffer type ID comes from Cube
```cpp
fprintf(stderr, "[ParseSchemaFlatBuffer] Field %zu: type=%d\n", i, field_type);
```

2. **Build**: `make`

3. **Start Services**: `./start-cube-api.sh`, `./start-cubesqld.sh`

4. **Test**:
```python
cursor.execute("SELECT NOW() as ts")
```

5. **Debug**: Check stderr for type ID
```
[ParseSchemaFlatBuffer] Field 0: type=16
```

6. **Document**: TIMESTAMP is FlatBuffer type 16

**Iteration 2: Add Type Mapping**
1. **Change**: Update `MapFlatBufferTypeToArrow()`
```cpp
case org::apache::arrow::flatbuf::Type::Timestamp:
  return NANOARROW_TYPE_TIMESTAMP;
```

2. **Build**: `make`

3. **Test**: Run query again

4. **Debug**: Check for new error (probably "unsupported type in BuildArrayForField")

**Iteration 3: Implement Array Builder**
1. **Change**: Add case in `BuildArrayForField()`
```cpp
case NANOARROW_TYPE_TIMESTAMP: {
  // Similar to INT64: validity bitmap + 8-byte values
  // Extract buffer 0 (validity)
  // Extract buffer 1 (int64 timestamp micros)
  break;
}
```

2. **Build**: `make`

3. **Test**: Run query

4. **Debug**: Check if values are correct

**Iteration 4: Add Elixir Test**
1. **Change**: Add test
```elixir
test "handles TIMESTAMP type", %{conn: conn} do
  {:ok, results} = Connection.query(conn, "SELECT NOW() as ts")
  materialized = Result.materialize(results)
  assert %Result{data: [%Column{type: :timestamp}]} = materialized
end
```

2. **Build**: Not needed (Elixir)

3. **Test**: `mix test test/adbc_cube_basic_test.exs:150 --include cube`

4. **Debug**: Fix type atom mapping if needed

5. **Document**: Update documentation with new type support

**Time**: 1-2 hours total

---

### Scenario 2: Fixing a Segfault

**Example**: Segfault during string extraction

**Iteration 1: Reproduce Minimally**
1. **Change**: Create minimal test
```elixir
test "minimal string test", %{conn: conn} do
  {:ok, _} = Connection.query(conn, "SELECT 'hello' as str")
end
```

2. **Test**: `mix test test/adbc_cube_basic_test.exs:200 --include cube`

3. **Result**: Segfault confirmed

**Iteration 2: Add Debug Logging**
1. **Change**: Add fprintf throughout string handling
```cpp
case NANOARROW_TYPE_STRING: {
  fprintf(stderr, "[BuildArrayForField] Building STRING field\n");
  fprintf(stderr, "[BuildArrayForField] num_rows=%ld\n", num_rows);

  // Extract validity buffer
  fprintf(stderr, "[BuildArrayForField] Extracting validity buffer\n");
  ExtractBuffer(...);
  fprintf(stderr, "[BuildArrayForField] Validity buffer at %p\n", validity_buffer);

  // ... more logging ...
}
```

2. **Build**: `make`

3. **Test**: Run minimal test

4. **Debug**: Check where segfault occurs
```
[BuildArrayForField] Building STRING field
[BuildArrayForField] num_rows=1
[BuildArrayForField] Extracting validity buffer
Segmentation fault
```

**Conclusion**: Segfault in ExtractBuffer()

**Iteration 3: Check Buffer Bounds**
1. **Change**: Add bounds checking
```cpp
AdbcStatusCode CubeArrowReader::ExtractBuffer(...) {
  fprintf(stderr, "[ExtractBuffer] buffer_idx=%d, buffer_offset=%zu, buffer_size=%zu\n",
          buffer_idx, buffer_offset, buffer_length);

  if (buffer_offset + buffer_length > buffer_.size()) {
    fprintf(stderr, "[ExtractBuffer] ERROR: Buffer exceeds bounds!\n");
    return ADBC_STATUS_INTERNAL;
  }

  // ... rest of function ...
}
```

2. **Build**: `make`

3. **Test**: Run minimal test

4. **Debug**: Check output
```
[ExtractBuffer] buffer_idx=2, buffer_offset=10000, buffer_size=5
[ExtractBuffer] ERROR: Buffer exceeds bounds!
```

**Conclusion**: Buffer offset is wrong

**Iteration 4: Fix Offset Calculation**
1. **Change**: Fix FlatBuffer offset extraction
```cpp
// Was: buffer_offset = buffer_info->offset()
// Should be: buffer_offset = base_offset + buffer_info->offset()
buffer_offset = body_offset + buffer_info->offset();
```

2. **Build**: `make`

3. **Test**: Run minimal test

4. **Result**: Test passes!

5. **Document**: Add comment explaining offset calculation

**Time**: 1-3 hours

---

### Scenario 3: Performance Optimization

**Example**: Speed up large result sets

**Iteration 1: Measure Baseline**
1. **Change**: Add timing
```cpp
auto start = std::chrono::high_resolution_clock::now();
// ... parsing code ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
fprintf(stderr, "[GetNext] Parse time: %ld ms\n", duration.count());
```

2. **Build**: `make`

3. **Test**: Run large query
```elixir
{:ok, _} = Connection.query(conn, "SELECT * FROM large_table LIMIT 1000")
```

4. **Debug**: Check timing
```
[GetNext] Parse time: 150 ms
```

**Iteration 2: Profile Hotspots**
1. **Change**: Add more granular timing
```cpp
fprintf(stderr, "[GetNext] Schema parse time: %ld ms\n", schema_time);
fprintf(stderr, "[GetNext] Batch parse time: %ld ms\n", batch_time);
fprintf(stderr, "[GetNext] Array build time: %ld ms\n", array_time);
```

2. **Build**: `make`

3. **Test**: Run query

4. **Debug**: Identify bottleneck
```
[GetNext] Schema parse time: 5 ms
[GetNext] Batch parse time: 10 ms
[GetNext] Array build time: 135 ms  ← bottleneck
```

**Iteration 3: Optimize Array Building**
1. **Change**: Reduce allocations, use memcpy
```cpp
// Before: Loop assigning each element
for (size_t i = 0; i < num_rows; i++) {
  values[i] = data[i];
}

// After: Single memcpy
memcpy(values, data, num_rows * sizeof(int64_t));
```

2. **Build**: `make`

3. **Test**: Run query

4. **Debug**: Check new timing
```
[GetNext] Array build time: 45 ms  ← 3x faster!
```

5. **Document**: Note optimization in comments

**Time**: 2-4 hours

---

## Troubleshooting

### Build Failures

**Error**: `fatal error: flatbuffers/flatbuffers.h: No such file or directory`

**Solution**:
```bash
sudo apt-get install libflatbuffers-dev
```

---

**Error**: `undefined reference to 'FlatBufferFunction'`

**Solution**: Check CMakeLists.txt has FlatBuffers linked:
```cmake
target_link_libraries(adbc_driver_cube_shared PRIVATE flatbuffers)
```

---

**Error**: `make: *** [Makefile:2: all] Error 2`

**Solution**: Read actual error (usually further up in output):
```bash
make 2>&1 | less
# Scroll to first "error:"
```

---

### Runtime Failures

**Error**: Connection refused on port 4445

**Solution**: Start cubesqld
```bash
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cubesqld.sh
```

---

**Error**: Unknown option: token

**Solution**: Use `adbc.cube.` prefix:
```elixir
"adbc.cube.token": "test"  # Correct
# Not: token: "test"
```

---

**Error**: Segmentation fault

**Solution**:
1. Add debug logging
2. Reduce test scope (single test)
3. Check for null pointers
4. Verify buffer bounds
5. Run under gdb or valgrind

---

### Test Failures

**Error**: Test timeout

**Causes**:
- Services not running
- Infinite loop in C code
- Deadlock

**Solution**: Check services, add timeouts, inspect loop conditions

---

**Error**: Type mismatch in test

**Solution**: Check actual vs expected types:
```elixir
# Print actual type
IO.inspect(materialized, label: "Actual result")

# Update test expectation
```

---

## Best Practices

### 1. Version Control

**Commit Often**:
```bash
git add -u
git commit -m "WIP: Add debug logging for string parsing"
```

**Commit Messages**:
```
<type>: <summary>

<optional body>

<optional footer>

Types: Fix, Add, Update, Remove, Refactor
```

### 2. Code Hygiene

**Before Committing**:
- [ ] Remove unnecessary debug logging (keep useful ones)
- [ ] Format code consistently
- [ ] Remove commented-out code
- [ ] Update documentation

**Don't Commit**:
- Binary files (*.so, *.o)
- Temporary files (/tmp/*, *.swp)
- Build artifacts (cmake_adbc/, _build/)

### 3. Testing Strategy

**Test Pyramid**:
```
        ┌───────────┐
        │  Manual   │  ← Rare, exploratory
        └───────────┘
      ┌───────────────┐
      │  Integration  │  ← Some, Elixir tests
      └───────────────┘
    ┌───────────────────┐
    │    Unit Tests     │  ← Many, C++ tests (future)
    └───────────────────┘
```

**When to Test**:
- After every code change
- Before committing
- After fixing a bug (add regression test)
- Before deploying

### 4. Debug Logging

**Good Logging**:
```cpp
fprintf(stderr, "[Function] Operation starting: param=%d\n", param);
fprintf(stderr, "[Function] Result: value=%d, status=%s\n", value, status);
```

**Bad Logging**:
```cpp
printf("here\n");  // Unhelpful
fprintf(stderr, "foo=%d\n", foo);  // No context
```

**Logging Levels** (future enhancement):
```cpp
#define DEBUG 1
#if DEBUG
  fprintf(stderr, "[DEBUG] ...\n");
#endif
```

### 5. Performance

**Measure, Don't Guess**:
- Add timing to suspected slow paths
- Profile before optimizing
- Verify optimization actually helps

**Premature Optimization**:
- Don't optimize without evidence
- Readable code > fast code (usually)
- Simple solutions often fast enough

### 6. Documentation

**Good Docs**:
- Explain why, not what
- Include examples
- Update with code changes
- Cover edge cases

**Bad Docs**:
- Out of date
- Obvious ("This function returns a value")
- Missing examples

---

## Appendix: Quick Reference

### Build Commands

```bash
# Full rebuild
make clean && make

# Incremental
make

# Parallel
make -j4

# Check library
ls -lh priv/lib/libadbc_driver_cube.so
```

### Service Commands

```bash
# Start Cube.js API
cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
./start-cube-api.sh

# Start cubesqld
./start-cubesqld.sh

# Check services
lsof -i :4008  # Cube.js
lsof -i :4445  # Arrow Native

# Stop services
pkill -f "node.*cube"
pkill cubesqld
```

### Test Commands

```bash
# Python quick test
cd ~/projects/learn_erl/adbc
python3 quick_test.py

# Elixir basic tests
./test/run_cube_basic_tests.sh

# Elixir specific test
mix test test/adbc_cube_basic_test.exs:66 --include cube

# Elixir all tests
mix test test/adbc_cube_test.exs --include cube

# Verbose Elixir test
mix test test/adbc_cube_basic_test.exs --trace --include cube
```

### Debug Commands

```bash
# View stderr output
<command> 2>&1 | less

# Hexdump Arrow IPC data
hexdump -C /tmp/cube_arrow_ipc_data.bin | head -50

# GDB debugging
gdb --args python3 quick_test.py
# (gdb) break CubeArrowReader::GetNext
# (gdb) run

# Valgrind
valgrind --leak-check=full python3 quick_test.py
```

---

## Conclusion

The iteration cycle is simple in principle, nuanced in practice. Each cycle teaches something. Accumulated cycles produce expertise.

**Key Takeaways**:
1. Change small, test often
2. Debug methodically, not frantically
3. Document for future-you
4. Iterate until it works, then iterate once more

**The cycle never ends**. There's always another feature, another bug, another optimization. Embrace the iteration.

Now go forth and iterate.
