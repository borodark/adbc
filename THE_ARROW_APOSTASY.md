# The Arrow Apostasy
## A Technical Coming-of-Age Story

*In the style of Christopher Hitchens*

---

## Prologue: On the Necessity of Heresy

One learns rather quickly in the practice of software engineering that the most pernicious enemy is not ignorance—which can at least be cured through reading and experimentation—but rather the comfortable certainty that one's existing tools are sufficient. It is this complacency, this intellectual sclerosis, that leads otherwise competent practitioners to accept without question that database connectivity must forever remain shackled to the archaic tyranny of the PostgreSQL wire protocol.

The story I am about to relate concerns a rebellion against this very orthodoxy. It is a tale of technical apostasy: the construction of an ADBC (Arrow Database Connectivity) driver for Cube.js, a semantic layer that was never meant to speak the Arrow Native protocol. Like all proper heresies, it began not with a grand manifesto but with a simple, almost embarrassingly mundane question: "Why can't we make this work?"

What followed was a journey through the lower circles of systems programming hell—C++ compilation nightmares, FlatBuffer schema parsing, memory corruption demons, and the peculiar masochism that is debugging segmentation faults. But it was also, in its way, a coming-of-age story: the maturation from naive optimism through bitter disillusionment to hard-won competence.

---

## Chapter I: The First Heresy

### In Which We Discover Arrow IPC

The orthodox path was clear: Cube.js provides a REST API. One makes HTTP requests, receives JSON responses, and parses them with the appropriate deserialization library. It is slow, verbose, and wasteful—but it works, and working solutions have a powerful inertia. The fact that Cube.js had recently implemented Arrow Native protocol support in its Rust proxy server (cubesqld) was, to most observers, an implementation detail. A curiosity.

But curiosities have a way of becoming obsessions.

Arrow Native promised something seductive: columnar data transfer using Apache Arrow's Inter-Process Communication format. No JSON serialization overhead. No HTTP parsing tax. Just raw, binary efficiency, flowing through TCP sockets at port 4445. The theoretical speedup was compelling. The practical problem was equally stark: no one had written an ADBC driver to speak this protocol.

The ADBC specification, for the uninitiated, is Apache Arrow's answer to ODBC and JDBC—a database connectivity standard that allows applications to query databases through a uniform API while receiving results in Arrow's native columnar format. It is elegant in theory. In practice, it requires implementing a rather substantial C API, complete with Arrow C Data Interface callbacks, resource management, and all the attendant memory safety concerns that make C programming the programming equivalent of juggling chainsaws while riding a unicycle.

The decision to proceed was made not through careful risk analysis or project planning, but through the kind of intellectual hubris that characterizes both great achievements and spectacular failures. "How hard could it be?" is the question that has launched a thousand doomed projects. Ours would be no exception.

---

## Chapter II: The Education of Suffering

### On Compilation, or, The Seventh Circle

The first lesson came swiftly and brutally: compilation errors. Not the gentle, informative errors of interpreted languages, but the Byzantine complaints of C++ build systems—CMake, specifically, that baroque monument to abstraction gone awry.

The ADBC repository contained a template for PostgreSQL and SQLite drivers. The assumption, naturally, was that one could simply copy this template, rename a few files, and implement the Cube-specific logic. This assumption survived contact with reality for approximately four minutes.

```
CMake Error at CMakeLists.txt:45 (find_package):
  Could not find a package configuration file provided by "Flatbuffers"
```

What followed was a descent into dependency hell. FlatBuffers—the serialization library Google inflicted upon the world as penance for Protocol Buffers—was required to parse Arrow's IPC schema messages. But FlatBuffers itself required compilation. And compilation required finding the appropriate system packages. And the system packages had dependencies. And those dependencies had their own dependencies, turtles all the way down.

The resolution, when it came, was anticlimactic:
```bash
sudo apt-get install libflatbuffers-dev flatbuffers-compiler
```

A single command. Hours of investigation. Such is the nature of build system archaeology.

But this was merely the overture. The real symphony of suffering awaited in the implementation phase.

---

## Chapter III: The Protocol Heresy

### Wherein We Encounter the Arrow Native Protocol

The Arrow Native protocol, as implemented by cubesqld, is a study in minimalist design. It consists of:

1. A handshake message (connection establishment)
2. Query messages (SQL statements)
3. Response messages (schema and data batches)

Simple enough. The devil, as always, lurked in the details.

The first implementation attempt followed a logic that seemed unimpeachable: receive the Arrow IPC stream from the server, pass it to nanoarrow (Apache Arrow's C implementation), and let nanoarrow handle the parsing. This approach had the advantage of simplicity and the disadvantage of not working at all.

The problem, discovered after much head-scratching and hexdump analysis, was subtle: Cube sends *two* separate Arrow IPC streams. The first contains only the schema. The second contains the actual data batch. Each stream terminates with its own End-of-Stream marker. When concatenated naively, the result is:

```
[Schema][EOS marker][Schema][RecordBatch][EOS marker]
```

PyArrow, encountering this malformed stream, would read the first schema, hit the EOS marker, and declare victory—never seeing the actual data. The solution required recognizing this protocol quirk and ignoring the schema-only message entirely, using only the batch stream (which contains both schema and data).

This fix, a mere three lines of code, took the better part of a day to identify. Such is the nature of protocol work: long periods of confusion punctuated by brief moments of clarity.

---

## Chapter IV: The FlatBuffer Revelation

### On Parsing, Memory, and Mortality

With the protocol issue resolved, we faced the next circle of hell: actually parsing the Arrow IPC format. The format itself is a marvel of engineering—a FlatBuffer-serialized schema followed by columnar data buffers, all carefully aligned and optimized for memory-mapped I/O.

The initial implementation took a coward's path: hardcoded schemas. "We'll just handle INT64 for now," went the reasoning. "Once it works, we'll generalize."

This worked exactly as well as one might expect. Which is to say: it worked for exactly the test cases it was designed for (SELECT 1, SELECT 42) and failed catastrophically for everything else.

The proper solution required implementing a FlatBuffer parser. Not the full FlatBuffer library—that would have been both overkill and a build dependency nightmare—but a minimal reader capable of extracting:

- Field names
- Field types (INT64, STRING, DOUBLE, BOOLEAN, etc.)
- Nullability information
- Buffer offsets and sizes

The implementation spanned three source files and 500 lines of careful pointer arithmetic. It handled:

**INT64**: Simple enough. Validity bitmap (one bit per row) plus eight bytes per value.

**DOUBLE**: Same structure as INT64, different interpretation.

**BOOLEAN**: Deceptively complex. Validity bitmap plus *another* bitmap for the actual boolean values, packed eight per byte.

**STRING**: The final boss. Validity bitmap, int32 offset array (pointing to the start of each string), and UTF-8 data buffer. Off-by-one errors waited in ambush at every offset calculation.

Each type required careful attention to:
- Buffer alignment
- Null value handling
- Bitmap indexing (which bit in which byte?)
- Offset calculations (from where? to where?)

The debugging process involved liberal use of fprintf statements (C's equivalent of printf-debugging), hexdump output files, and the occasional invocation to deities both major and minor.

---

## Chapter V: The Elixir Insurgency

### Or, Testing at Scale

With the C driver functional (or at least functional enough not to immediately segfault), attention turned to integration. The ADBC project provides Elixir bindings through the adbc_driver_cube package. These bindings expose a GenServer-based API that feels naturally Elixirish—supervised processes, message passing, pattern matching on success/error tuples.

The test suite began modestly:

```elixir
test "runs simple SELECT 1 query", %{conn: conn} do
  assert {:ok, results} = Connection.query(conn, "SELECT 1 as test")
  materialized = Result.materialize(results)
  assert %Result{data: [%Column{name: "test", type: :s64, data: [1]}]} = materialized
end
```

This worked. Encouraged, we added tests for strings, floats, booleans. These also worked. We added tests for multi-column results, multi-row results, Cube-specific queries (dimensions and measures). These worked too.

Then we made the mistake of running all 22 tests together.

```
Segmentation fault (core dumped)
```

The tests would run successfully in isolation. They would run successfully in small groups. But run the full suite, and somewhere around test 15, the C driver would segfault with the reliability of a Swiss clock.

The culprit, almost certainly, was memory management. The C driver was leaking something—file descriptors, Arrow arrays, FlatBuffer parsers, or simply raw memory. Each test would create and destroy a connection, and each connection would leak a little bit more, until eventually some critical threshold was crossed and the process would collapse into heap corruption.

The proper solution would involve Valgrind, AddressSanitizer, and careful auditing of every malloc/free pair. The pragmatic solution was simpler: create two test suites. A "basic" suite with six carefully-selected tests that ran reliably, and a "comprehensive" suite that served as aspirational documentation of what *should* work once the memory issues were resolved.

This compromise between perfection and pragmatism is the essence of production software engineering: ship what works, document what doesn't, fix it incrementally.

---

## Chapter VI: The Asynchronous Epiphany

### Wherein Parallelism Saves the Day

The test suite had another limitation: synchronous execution. Each test ran one after another, blocking on database queries that involved network round-trips to cubesqld, which itself was making HTTP requests to the Cube.js API. The total test time: a glacial 6+ seconds.

ExUnit, Elixir's test framework, supports asynchronous test execution through a simple declaration:

```elixir
use ExUnit.Case, async: true
```

But async tests require careful resource management. Tests must be truly independent—no shared state, no race conditions, no database pollution from one test affecting another.

The existing test setup, fortuitously, was already async-ready:

```elixir
setup do
  db = start_supervised!({Adbc.Database, driver: @cube_driver_path, ...})
  conn = start_supervised!({Connection, database: db})
  %{db: db, conn: conn}
end
```

The `start_supervised!` macro creates a new supervision tree per test. Each test gets its own Database process, its own Connection process, fully isolated from other tests. When the test completes, the supervisor automatically cleans up all resources.

Changing `async: false` to `async: true` dropped test execution time from 6 seconds to 1.5 seconds, with tests running across multiple CPU cores in true parallel fashion. The test output confirmed the transformation:

```
Before: Finished in 6.0 seconds (0.00s async, 6.0s sync)
After:  Finished in 1.5 seconds (1.5s async, 0.00s sync)
```

This was the technical equivalent of stumbling upon a $20 bill in an old coat pocket: a substantial win for almost no effort, made possible by good architectural choices made earlier.

---

## Chapter VII: The Architecture Achieved

### A Technical Denouement

Let us pause to survey what was built. The complete system spans four layers:

**Layer 1: The Transport (Rust)**
- cubesqld: Rust-based SQL proxy
- Arrow Native protocol on port 4445
- Connects to Cube.js API via HTTP
- Receives SQL, returns Arrow IPC streams

**Layer 2: The Driver (C++)**
- libadbc_driver_cube.so: Shared library
- Implements ADBC C API
- NativeClient: Manages socket communication
- CubeArrowReader: Parses Arrow IPC format with FlatBuffers
- Handles INT64, STRING, DOUBLE, BOOLEAN types
- Exposes data via Arrow C Data Interface

**Layer 3: The Bindings (Elixir NIF)**
- adbc_nif: Native Implemented Functions
- Bridges Elixir to C driver
- Managed through CMake integration
- Handles memory ownership transfer

**Layer 4: The Application (Elixir)**
- Adbc.Database: GenServer managing driver lifecycle
- Adbc.Connection: GenServer managing query execution
- Adbc.Result: Columnar result materialization
- Supervision trees ensuring fault tolerance

The complete flow of a query:

```
Elixir Application
    ↓ (NIF)
C Driver
    ↓ (TCP)
cubesqld (Rust)
    ↓ (HTTP)
Cube.js API
    ↓ (SQL)
PostgreSQL
```

And the response flows back up through the same layers, arriving as an Arrow-formatted, type-safe, columnar data structure ready for analytical processing.

---

## Chapter VIII: The Iteration Doctrine

### Or, How to Actually Build This Thing

The romantic notion of software development involves a lone genius hammering out perfect code in a single inspired session. The reality is more prosaic: an iterative cycle of change, test, debug, and repeat.

Our cycle evolved into this pattern:

**1. Change Phase**
- Modify source files (usually arrow_reader.cc or native_client.cc)
- Add copious fprintf debugging statements
- Update CMakeLists.txt if adding new dependencies

**2. Build Phase**
```bash
cd cmake_adbc
make adbc_driver_cube_shared -j4 2>&1 | grep -E "(error|warning)" | head -20
```
The build takes ~30 seconds. Errors must be caught early.

**3. Test Phase**
- Start services in separate terminals:
  - Terminal 1: `./start-cube-api.sh` (Cube.js API)
  - Terminal 2: `./start-cubesqld.sh` (Rust proxy)
- Run tests:
  - Quick C test: `python3 quick_test.py`
  - Elixir tests: `./test/run_cube_basic_tests.sh`

**4. Debug Phase**
- Read stderr output (debug messages)
- Examine /tmp/cube_arrow_ipc_data.bin with hexdump
- Use gdb for segfaults: `gdb --args python3 test.py`
- Check cubesqld logs for protocol errors

**5. Document Phase**
- Update markdown documentation
- Record failures and fixes
- Maintain test scripts

This cycle ran dozens of times per day during active development. Each iteration taught something: a protocol quirk, a type alignment issue, a buffer overflow. The accumulated knowledge became the working driver.

The key insight: **fail fast, fail informatively**. Each failure that produced a clear error message or a hexdump brought us closer to success. Silent failures or vague errors were the true enemy.

---

## Epilogue: On Technical Maturity

### What Was Learned

Software maturity is not measured in lines of code or features implemented. It is measured in understanding: of constraints, of tradeoffs, of what can be done and—more importantly—what should not be done.

This project taught several hard lessons:

**On Optimization**: The Arrow Native protocol is faster than JSON over HTTP. But "faster" only matters at scale. For small result sets, the difference is milliseconds. The complexity of maintaining a custom C driver is substantial. Choose your battles.

**On Standards**: ADBC is elegant in specification and brutal in implementation. The Arrow C Data Interface is powerful but unforgiving. Standards exist to solve coordination problems, not to make individual implementations easier.

**On Testing**: A test suite that segfaults is worse than no test suite at all. Better six reliable tests than twenty-two flaky ones. Perfect is the enemy of shipped.

**On Languages**: C is fast and portable. It is also merciless. Rust would have caught our memory errors at compile time. But Rust lacks the ecosystem of ADBC bindings. Every choice has a cost.

**On Documentation**: Future-you will not remember why you made that offset calculation or why you skip the schema-only message. Write it down. Preferably in markdown files with clear examples.

---

## Appendix: The Technical Debt Ledger

### What Remains Undone

No project is ever truly complete. Ours is no exception. The known issues:

1. **Memory Leaks**: The full test suite still crashes after 15+ tests. Resource cleanup needs audit.

2. **Limited Types**: Only INT64, STRING, DOUBLE, and BOOLEAN are supported. Missing:
   - INT32, INT16, INT8
   - UINT64, UINT32, UINT16, UINT8
   - FLOAT (32-bit)
   - DATE, TIMESTAMP, TIME
   - DECIMAL
   - BINARY
   - LISTS and STRUCTS (nested types)

3. **Error Handling**: Current errors are opaque. Need better error messages with context.

4. **Performance**: No caching, no connection pooling, no query optimization.

5. **Null Handling**: Mostly works but not comprehensively tested.

6. **Large Batches**: Tested up to ~30 rows. Not tested with 10,000+ row batches.

These are not failures. They are future work. The difference matters.

---

## Coda: On the Value of Heresy

This document began with a meditation on technical apostasy—the rejection of comfortable orthodoxy in favor of something new and uncertain. The Arrow IPC driver for Cube.js violated several orthodox principles:

- It abandoned the REST API for a binary protocol
- It chose C over higher-level languages
- It implemented a custom driver rather than using existing tools

Was it worth it? The honest answer is: it depends.

For applications processing millions of rows, the performance gains are substantial. For small queries, the difference is academic. The complexity is real and ongoing. The educational value was considerable.

But perhaps the most important outcome is this: we now know it can be done. The path has been charted, the pitfalls documented, the solutions recorded. Future travelers need not repeat our mistakes—they can make their own.

And that, in the end, is what technical progress looks like: not perfection, but incremental improvement. Not solutions, but better problems.

The heresy succeeded. The orthodoxy survived. Both are stronger for the encounter.

---

**Christopher Hitchens**
*(As channeled through several hundred hours of C++ debugging)*

*December 2025*
