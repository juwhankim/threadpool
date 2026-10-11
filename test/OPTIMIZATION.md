# Scheduling optimization comparison

Measured on 2026-10-10 on Windows x86-64 with 16 physical / 32 logical cores,
MSYS2 UCRT64 GCC 16.2.0, C++17, `-O3 -pthread`, and the strict warning flags from
the package README. The computation, benchmark source, compiler flags, and
16-worker configuration were unchanged between binaries.

The baseline executable was preserved before editing the implementation.
Three before/after pairs were run sequentially, alternating their order:
before/after, after/before, before/after. No correctness tests or compiler jobs
ran concurrently with these measurements. Each suite used nine timed samples
after one warmup, validating all checksums. The table reports the median of the
three per-suite medians, rather than the fastest observed run.

| Case | Before, ms | After, ms | Throughput change |
|---|---:|---:|---:|
| Normal external CPU | 20.319 | 16.926 | +20.0% |
| Greedy external CPU | 17.413 | 16.420 | +6.0% |
| Normal tiny tasks | 44.803 | 38.654 | +15.9% |
| Greedy tiny tasks | 45.543 | 39.822 | +14.4% |
| Normal fork/join CPU | 8.800 | 8.910 | -1.2% |
| Greedy fork/join CPU | 8.726 | 8.891 | -1.9% |

CPU cases perform 4,096 work items with 20,000 mixing rounds each. Tiny cases
perform 32,768 zero-round work items. The greatest gains occur where scheduling
overhead matters. Batched fork/join already spends most of its time computing;
it showed a small regression in these samples. This is not a universal speedup,
and timing varies with the workload, scheduling, and hardware. The combined
changes were measured together; these numbers do not attribute gains to any
single change or constitute hardware-profiler measurements.

Raw suites are in [BENCHMARK_COMPARISON.txt](BENCHMARK_COMPARISON.txt). The initial
baseline capture is in [BENCHMARK_BEFORE.txt](BENCHMARK_BEFORE.txt), and
[BENCHMARK_RESULTS.txt](BENCHMARK_RESULTS.txt) contains the last optimized suite.
The baseline binary remains at `build/threadpool_benchmark_before.exe` in the
ignored build directory for local reruns.

## Implementation changes

- Store the standard packaged task directly in one queued object, replacing a
  shared packaged-task wrapper nested inside `std::function`.
- Avoid a pool-state weak-pointer lock and a duplicate future wait for external
  `get()` calls. Cooperative waiting remains in place for workers.
- Check an atomic injection count before locking an empty queue. Workers use
  try-lock and can steal while another producer/worker holds the injection mutex.
- Transfer a small batch into the worker's empty owner deque, amortizing injection
  lock acquisition while keeping those tasks available to thieves.
- Wake one normal-mode waiter per publication instead of waking the entire pool.
- Replace greedy dispatch's shared mutex with separate, cache-aligned claim flags
  for each worker. Sequentially consistent registration, active checks, and the
  stop scan preserve the pause boundary without a globally contended reader lock.
- Separate hot counters onto different cache lines.

Normal waiters explicitly recheck pending injection in their wake predicate so
a failed try-lock cannot leave queued work stranded. The existing deque CAS and
memory-fence algorithm is unchanged. Experimental empty-deque shortcuts were
discarded because they did not improve the measured workload.

The public pool/receipt interfaces, greedy pause behavior, exception propagation,
and draining shutdown contract are unchanged. Correctness coverage was expanded
to exercise frequent pause/resume and a queued backlog behind blocked workers.
