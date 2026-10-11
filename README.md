# C++17 work-stealing threadpool

A header-only work-stealing threadpool for C++17, with sleeping workers,
an explicitly controlled polling mode, move-only task receipts and draining
shutdown. It depends only on the standard library and uses `nnn::threadpool`.

This is a modernization of [juwhankim/threadpool](https://github.com/juwhankim/threadpool),
based on snapshot `3b819f4`. It retains the MIT license and original copyright
in [LICENSE](LICENSE). Legacy helper APIs and the logger dependency have been
removed; see the compatibility notes below before upgrading.

## Usage

```cpp
#include "threadpool.h"
#include "greedy_threadpool.h"

namespace tp = nnn::threadpool;

int main() {
    tp::threadpool normal(4);
    auto result = normal.submit([](int value) { return value * 2; }, 21);
    const int answer = result.get(); // 42

    tp::greedy_threadpool greedy(4); // Initially paused.
    auto work = greedy.submit([] { return 7; });
    greedy.go();
    const int other = work.get();
    greedy.wait_idle();
    greedy.stop();
    return answer == 42 && other == 7 ? 0 : 1;
}
```

Add `include/` to your include path. The public headers live in
`include/`, and package tests and benchmarks live in `test/`.
`submit()` accepts functions, functors,
member pointers, and move-only callables/arguments. Arguments are decay-copied
and moved into a one-shot invocation; use `std::ref` or `std::cref` for references.
The caller must keep referenced objects alive until the task finishes.

Normal workers sleep when no task is available. Greedy workers poll and yield
while active, consuming CPU even when idle. `stop()` prevents new top-level task
claims after it returns; already claimed tasks finish. An in-flight task waiting
for a child may keep helping execute tasks so that fork/join can complete.
For a fully idle pause, call `wait_idle()` before `stop()` and coordinate producers.
A paused pool with queued tasks needs `go()` before waiting for those tasks.

`receipt<T>` is move-only, and `get()` consumes its future. It supports void,
reference, and move-only results and preserves the original exception type.
A default receipt's `wait()` is a no-op; `get()` on an invalid receipt throws
`std::future_error`. Waiting workers execute other tasks, including with just
one worker. Waiting external threads do not manipulate an owner's deque.
Receipts remain usable after their pool is destroyed.

`destroy()` is idempotent and drains accepted tasks, including children submitted
by running tasks. It resumes paused greedy workers for draining and rejects new
external submissions once shutdown starts. The destructor also drains. Call
`destroy()` and `wait_idle()` from outside the pool's workers; self-join and
self-idle waits throw `std::logic_error`. Destroying the pool object from one of
its own tasks is unsupported. Concurrent external `submit()` and explicit
`destroy()` are supported while the pool object remains alive. `wait_idle()`
waits for zero outstanding work; it does not prevent subsequent submissions.

## Queue and synchronization

Each worker owns a Chase-Lev deque: owner push/pop, concurrent thief steal.
Index arbitration and slot access use atomics, with no deque mutex. Growing the
deque retains old buffers until the pool state is released, avoiding premature
reclamation while thieves read them. Geometric growth retains less than twice
the largest buffer's slot storage. C++17 over-aligned allocation replaces manual
pointer alignment and placement construction. Queue indices have checked
exhaustion rather than wrapping.

The complete pool is **not lock-free**: external submission uses a mutex-protected
injection queue; normal-mode sleep/wake, shutdown, and greedy pause coordination
also use standard synchronization. Queue growth allocates memory, and atomic
lock-freedom depends on the target. The test reports index atomic lock-freedom.
Owner queue operations must never be called concurrently by multiple owners.

Workers check an atomic queue count before attempting injection and use try-lock
so a busy injector does not block stealing. A successful fetch takes a small
batch (at most eight tasks) and puts extra tasks on its empty local deque for
other workers to steal. Normal task publication wakes one waiter; lifecycle
events wake all waiters. Per-worker atomic claim flags synchronize greedy pause
without a shared dispatch lock on every task or poll. `stop()` may briefly wait
for an ongoing claim, and does not wait for the claimed task's execution.

Each queued task directly owns its `std::packaged_task`. External `get()` uses
the future's built-in wait, avoiding an extra pool-state reference acquisition
and a redundant wait. Worker receipts still help execute tasks while waiting.

## Modernization and compatibility

- `std::thread` and native `thread_local` replace pthread wrappers and keys.
- Standard type traits, forwarding, tuples, `std::apply`, and `std::invoke`
  replace custom traits, argument packs, and fixed-arity invocation macros.
- `std::packaged_task` and `std::future` replace custom result/exception storage.
- RAII ownership and C++17 aligned allocation replace raw task/buffer lifetimes.
- Old buffers are retained safely; draining shutdown replaces queued-task deletion.
- Predicate synchronization fixes lost wakeups and the greedy stop/claim race.
- Thread creation failure joins already created workers before propagating errors.

This is a substantial modernization of the implementation, preserving the
owner/thief architecture and the two operating modes rather than binary/API
compatibility with every upstream helper. The old helper headers and automatic
global `instance` pools have been removed; construct explicit pool objects.
`thread_count` now counts dedicated worker threads, excluding the submitting
thread; zero selects hardware concurrency with a fallback of one. Receipts are
one-shot and move-only. The old misspellings `threadpool_receit<T>` and
`greedy_threadpool_receit<T>` remain aliases for `receipt<T>`.

## Build and test

From this package root, with GCC:

```sh
mkdir -p build
g++ -std=c++17 -O2 -pthread -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Iinclude test/correctness.cpp -o build/threadpool_tests
g++ -std=c++17 -O3 -pthread -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Iinclude test/benchmark.cpp -o build/threadpool_benchmark
./build/threadpool_tests
./build/threadpool_benchmark 16 4096 20000 5
```

On Windows, append `.exe` to executable names. MSVC supports `/std:c++17 /EHsc`
and `/Iinclude`; it does not require `-pthread`.

For Windows PowerShell (or any platform with Python and GCC), the build helper
compiles tests, the benchmark and [the usage example](examples/basic.cpp), with
strict warnings treated as errors:

```powershell
python build.py --compiler C:\msys64\ucrt64\bin\g++.exe --test
.\build\threadpool_benchmark.exe 4 256 1000 3
```

Use `--compiler g++` when GCC is on PATH. The helper also works on Linux/macOS.
Alternatively, use CMake:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
```

For embedding, clone this package into `threadpool/`, use `add_subdirectory(threadpool)` and link `nnn::threadpool`.
Set `BUILD_TESTING=OFF` if package tests should not be built. Sanitizer options
`NNN_THREADPOOL_SANITIZE` (ASan/UBSan) and `NNN_THREADPOOL_TSAN` (ThreadSanitizer)
are provided for toolchains/platforms that supply their runtimes.

Correctness tests cover queue growth and competing thieves, exactly-once
execution, recursive fork/join, concurrent producers, result/exception handling,
idle wakeups, pause/resume, draining shutdown, and shutdown/submission races.
See [test/VALIDATION.md](test/VALIDATION.md) for the checks performed locally.

## Benchmark

`benchmark [workers [tasks [rounds [repetitions]]]]` reports median elapsed time,
work items per second, and speed relative to the matching serial workload.
One warmup precedes each case, and every checksum is verified. Pool construction
is excluded; submission, execution, result collection, and waiting are included.

The CPU workload is dependent integer mixing. External cases submit one task per
work item. Fork/join cases recursively split the work into batches of at most
16 items, exercising local queues and stealing while reducing dispatch overhead.
Tiny-task cases use eight times as many items with zero mixing rounds to expose
scheduling costs. Pool lifetimes do not overlap, so greedy polling cannot compete
with the serial and normal-mode cases. Results are synthetic, machine-specific,
and are not tokenizer throughput measurements or an upstream comparison.

Captured output is in [test/BENCHMARK_RESULTS.txt](test/BENCHMARK_RESULTS.txt).
The optimization comparison and its limitations are documented in
[test/OPTIMIZATION.md](test/OPTIMIZATION.md).

An optional text preparation integration test and benchmark combines NFKC normalization
with word-like span scanning on variable-length multilingual documents. It
compares serial processing with both pool modes and several worker/batch counts,
verifying the entire output. See [test/TEXT_PROCESSING.md](test/TEXT_PROCESSING.md)
for build commands, measured throughput, and workload limitations. This test
requires the separate basic_normalization component from nnntoken and is not
part of standalone correctness checks.
