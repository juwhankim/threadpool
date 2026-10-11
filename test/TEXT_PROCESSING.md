# Text preparation integration benchmark

This workload exercises the threadpool together with `nnn::basic_normalization`:
normalize each UTF-8 document to NFKC, identify word-like byte spans, and retain
normalized text and spans in the original document order. Tasks perform real
normalization, scanning, dynamic allocation, and result writes. The span scanner
uses ASCII whitespace/punctuation boundaries and keeps non-ASCII byte sequences
together; it is not Unicode word segmentation or the eventual nnntoken algorithm.

The deterministic generated corpus contains English, decomposed accents, Hangul
and jamo, Japanese, Chinese, Arabic, Cyrillic, Greek, emoji, ligatures, fullwidth
characters, URLs, and numbers. Documents have variable lengths. Whole paragraphs
are appended so UTF-8 is never truncated. This is an in-memory model of messages
and articles, not production data, a disk/network benchmark, or tokenization
throughput against an actual vocabulary.

## Build and run

From the workspace root:

```sh
g++ -std=c++17 -O3 -pthread -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ithreadpool/include -Ibasic_normalization/include threadpool/test/text_processing.cpp basic_normalization/src/unicode_normalization.cpp -o threadpool/build/threadpool_text_processing
./threadpool/build/threadpool_text_processing --test
./threadpool/build/threadpool_text_processing
```

Append `.exe` on Windows. With CMake, the package adds the optional
`threadpool_text_processing` executable and its `--test` CTest entry when the
sibling normalization sources are available. The threadpool library itself has
no normalization dependency.

`--test` runs a smaller corpus plus independent fixtures for compatibility
normalization, combining accents, Hangul composition, expected span offsets,
empty input, separators, embedded NUL, and malformed UTF-8. Both pool modes are
also checked for malformed-document exception propagation and successful reuse.
Pending tasks are drained before propagating a processing or submission exception
so their referenced input/output buffers remain alive.

## Measurement

The full run compares serial processing with normal and greedy pools at 1, 4,
and up to 16 workers, with batches of 1, 8, or 32 documents per scheduled task.
Each case has one warmup and five timed samples; the median is reported.
Every normalized byte, span offset/length, and document position is compared
against the serial reference, outside the timed interval. Pool lifetimes do not
overlap, preventing greedy polling from consuming CPU during other pool cases.

Corpus generation, pool construction, verification, and final output destruction
are excluded. Output allocation, submission, normalization, scanning, result
collection, receipt destruction, and the idle wait are included. Workers write
distinct result elements without a shared checksum/result mutex. Throughput uses
original input bytes (MiB/s) and original document counts (documents/s).

## Local results

2026-10-10, Windows x86-64, 16 physical / 32 logical cores, MSYS2 UCRT64 GCC
16.2.0, C++17 `-O3 -pthread`. Strict warning checks passed. All output comparisons
and fixtures passed. Full measurements are in
[TEXT_PROCESSING_RESULTS.txt](TEXT_PROCESSING_RESULTS.txt).

Short messages: 8,192 documents, 2,675,224 input bytes.

| Mode | Workers | Documents/task | Median ms | MiB/s | Serial speedup |
|---|---:|---:|---:|---:|---:|
| Serial | 1 | — | 27.668 | 92.2 | 1.00x |
| Normal | 16 | 1 | 20.849 | 122.4 | 1.33x |
| Normal | 16 | 32 | 3.152 | 809.5 | 8.78x |
| Greedy | 16 | 1 | 20.012 | 127.5 | 1.38x |
| Greedy | 16 | 32 | 2.891 | 882.6 | 9.57x |

Long articles: 512 documents, 10,618,315 input bytes.

| Mode | Workers | Documents/task | Median ms | MiB/s | Serial speedup |
|---|---:|---:|---:|---:|---:|
| Serial | 1 | — | 98.076 | 103.3 | 1.00x |
| Normal | 16 | 1 | 8.492 | 1,192.5 | 11.55x |
| Normal | 16 | 32 | 8.675 | 1,167.3 | 11.31x |
| Greedy | 16 | 1 | 8.588 | 1,179.2 | 11.42x |
| Greedy | 16 | 32 | 8.673 | 1,167.6 | 11.31x |

Short documents benefit strongly from batching: individual work is too small
to amortize scheduling well. Long documents contain enough work for one task
each, and larger batches offer little benefit here. One worker is roughly serial
speed with adequate batches; scheduling individual short messages is slower.
The small normal/greedy differences on articles should not be treated as a
general ranking. Results depend on the corpus, batching, hardware, allocator,
cache state, and concurrent system activity.
