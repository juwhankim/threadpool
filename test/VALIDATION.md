# Local validation

## Standalone package revalidation (2026-10-12)

On Windows x86-64 with GCC 16.2.0, `python build.py --test` rebuilt the
correctness suite, synthetic benchmark and standalone example with
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`; no diagnostics were
reported. All six headers also compiled independently as the first include.
Normal and greedy correctness tests, fork/join, producers, wakeups, shutdown
and pause/resume passed. The example returned 42 and 7. A short benchmark
(`4 256 1000 3`) verified every checksum; it is a smoke check, not a replacement
for the historical performance measurements below.

The correctness suite also passed a fresh UBSan-trap build. These checks use
only this package, with no nnntoken normalization or logger dependency.
The added CI configuration targets Linux, Windows and macOS; its remote runs
are separate from the local checks recorded here.

Date: 2026-10-10 (Asia/Seoul). Platform: Windows x86-64, 16 physical cores and
32 logical cores. Compiler: MSYS2 UCRT64 GCC 16.2.0. Language: C++17.

## Optimization revalidation

After the scheduling changes described in [OPTIMIZATION.md](OPTIMIZATION.md),
both executables were rebuilt with all the strict warning flags below and no
diagnostics. The expanded correctness suite passed five release runs and five
UBSan-trap runs, all with exit status zero. It now checks 300 greedy pause/resume
cycles per run and verifies that a 1,024-task backlog stays paused after already
claimed, blocked tasks are released, then runs exactly once after resuming.
Queue races, cooperative fork/join, concurrent producers, exceptions, and
draining shutdown remained covered. ASan/ThreadSanitizer availability is unchanged.

Performance was checked using three alternating before/after pairs, each with
nine timed samples per case after warmup. The raw suites and median-of-medians
comparison are linked from [OPTIMIZATION.md](OPTIMIZATION.md). The GCC analyzer
observations below describe the earlier implementation check; strict compiler
lint and dynamic/instrumented tests were rerun for the optimized implementation.

## Compiler checks and tests

The added text preparation integration workload also compiled with the strict
flags below. Its full benchmark verified every normalized byte and span against
serial output across both corpus sizes, both modes, three worker counts, and
three batch sizes. Its `--test` fixtures and malformed-input recovery passed
both the release and UBSan-trap builds. Results and timing boundaries are recorded
in [TEXT_PROCESSING.md](TEXT_PROCESSING.md).

Both test executables compile without diagnostics using:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

An additional GCC `-fanalyzer` pass completed without diagnostics for the
benchmark translation unit. The full correctness translation unit emitted
`result_decl not supported by dump_expr` uninitialized-value diagnostics at
void task lambdas, so that analyzer run is **inconclusive**, not a clean pass.
No analyzer warnings were suppressed in the source. GCC 16.2's
[analyzer documentation](https://gcc.gnu.org/onlinedocs/gcc-16.2.0/gcc/Static-Analyzer-Options.html)
states that the analyzer is suitable only for C in that release. Strict C++
compiler lint and the release/instrumented tests are the completed C++ checks.

Release correctness tests passed five repeated runs after fixing the greedy
stop/claim race, plus a final run after simplifying standard task invocation.
The same sequence passed with undefined-behavior sanitizer traps enabled:

```sh
g++ -std=c++17 -O1 -g -pthread -fsanitize=undefined -fsanitize-undefined-trap-on-error -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ithreadpool/include threadpool/test/correctness.cpp -o threadpool/build/threadpool_tests_ubsan_trap
```

All runs exited with status zero and no sanitizer traps. Output:

```text
Queue index atomics lock-free: true
PASS: normal invocation, fork/join, producers, wakeups, shutdown
PASS: greedy invocation, fork/join, producers, wakeups, shutdown
PASS: greedy pause/resume and all correctness tests
```

The suite includes 100,000 deque items shared among an owner and four thieves,
forced queue growth, 6,000 concurrent producer tasks per mode, recursive trees
with one and multiple workers, move-only and reference values, standard and
nonstandard exceptions, repeated idle wakeups, greedy pause/resume, automatic
and explicit draining, receipts surviving destruction, children submitted during
shutdown, and external submission racing shutdown. A watchdog fails hung tests.

## Benchmark

The release benchmark uses `-O3`, the strict warning flags above, and `-pthread`.
The current capture runs 16 workers, 4,096 CPU work items, 20,000 mixing rounds
per item, and nine timed repetitions after one warmup. Tiny-task cases run 32,768 work items with
zero rounds. Checksums are verified for every repetition. Median times,
throughput, and serial-relative speed are captured in
[BENCHMARK_RESULTS.txt](BENCHMARK_RESULTS.txt).

These are synthetic scheduling/CPU results for this machine. They do not measure
tokenization, prove race freedom, or compare against the original upstream build.

## Normalization namespace change

The normalization implementation and tests were rebuilt with the same strict
warning flags. Its conformance executable passed all **10,690,455** checks after
the move to `nnn::basic_normalization`. The demo also passed a compiler syntax
check. The table generator's emitted namespace was updated together with the
checked-in generated tables, so regeneration preserves the new namespace.

## Environment limits

The normal UBSan runtime link failed because this MinGW installation does not
include `libubsan`; trap instrumentation was used successfully instead.
ASan and ThreadSanitizer were not run. ThreadSanitizer is not available in this
Windows toolchain, so concurrency validation consists of stress tests and code
review rather than a sanitizer proof. Linux, macOS, and MSVC builds remain
unverified here. CMake is not installed locally; the direct GCC commands were
used to build and run the package.
