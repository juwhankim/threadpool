#include "threadpool.h"
#include "greedy_threadpool.h"
#include "unicode_normalization.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace tp = nnn::threadpool;
namespace normalization = nnn::basic_normalization;
using clock_type = std::chrono::steady_clock;

namespace {
struct processed_document {
    std::string text;
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    bool operator==(const processed_document& other) const {
        return text == other.text && spans == other.spans;
    }
};

// A tokenizer preparation workload: normalize UTF-8, then identify word-like
// byte spans. This is deliberately a simple scanner, not a Unicode word-break
// implementation or the eventual tokenizer algorithm. Non-ASCII bytes remain
// together; ASCII whitespace/punctuation separates spans without splitting UTF-8.
bool separator(unsigned char byte) {
    return byte <= 0x20 || (byte >= 0x21 && byte <= 0x2f) ||
           (byte >= 0x3a && byte <= 0x40) || (byte >= 0x5b && byte <= 0x60) ||
           (byte >= 0x7b && byte <= 0x7e);
}

processed_document process(std::string_view input) {
    processed_document output;
    output.text = normalization::nfkc(input);
    std::size_t start = 0;
    while (start < output.text.size()) {
        while (start < output.text.size() && separator(static_cast<unsigned char>(output.text[start]))) ++start;
        if (start == output.text.size()) break;
        auto end = start + 1;
        while (end < output.text.size() && !separator(static_cast<unsigned char>(output.text[end]))) ++end;
        output.spans.emplace_back(start, end - start);
        start = end;
    }
    return output;
}

struct corpus {
    std::string name;
    std::vector<std::string> documents;
    std::size_t bytes = 0;
};

corpus make_corpus(const char* name, std::size_t count, std::size_t minimum, std::size_t maximum) {
    // Deterministic generated text models varied messages/articles. It is not
    // sampled production data. Escapes keep source encoding portable on MSVC.
    const std::vector<std::string> paragraphs = {
        "The tokenizer reads documents, normalizes text, and prepares word spans. Order #1042 costs $19.95.\n",
        "Search query: portable C++17 work stealing; email user@example.org; URL https://example.org/docs.\n",
        normalization::encode_utf8(U"Cafe\u0301, nai\u0308ve, coo\u0308perate; \uFB03 ligature, \u2460 item, \uFF21\uFF22\uFF23 \uFF11\uFF12\uFF13.\n"),
        normalization::encode_utf8(U"\uD55C\uAE00 \uBB38\uC11C \uCC98\uB9AC: \u1112\u1161\u11AB\u1100\u1173\u11AF \uC815\uADDC\uD654 \uD14C\uC2A4\uD2B8.\n"),
        normalization::encode_utf8(U"\u65E5\u672C\u8A9E\u306E\u6587\u7AE0 \u4E2D\u6587\u6587\u672C \uFF76\uFF80\uFF76\uFF85 \u5168\u89D2\u3000\u7A7A\u767D.\n"),
        normalization::encode_utf8(U"\u0627\u0644\u0646\u0635 \u0627\u0644\u0639\u0631\u0628\u064A \u043F\u0440\u0438\u0432\u0435\u0442 \u03B3\u03B5\u03B9\u03AC \U0001F600 \U0001F680.\n")
    };
    corpus result{name, {}, 0};
    result.documents.reserve(count);
    std::uint64_t random = 0x4e4e4e;
    for (std::size_t i = 0; i < count; ++i) {
        random = random * 6364136223846793005ULL + 1442695040888963407ULL;
        const auto target = minimum + static_cast<std::size_t>(random % (maximum - minimum + 1));
        std::string document = "Document " + std::to_string(i) + ": ";
        // Append whole paragraphs so varying byte lengths never truncate UTF-8.
        std::size_t paragraph = i % paragraphs.size();
        while (document.size() < target) {
            document += paragraphs[paragraph];
            paragraph = (paragraph + 1) % paragraphs.size();
        }
        result.bytes += document.size();
        result.documents.push_back(std::move(document));
    }
    return result;
}

using outputs = std::vector<processed_document>;

outputs serial(const corpus& input) {
    outputs result(input.documents.size());
    for (std::size_t i = 0; i < input.documents.size(); ++i) result[i] = process(input.documents[i]);
    return result;
}

template<class Pool>
outputs parallel(Pool& pool, const corpus& input, std::size_t batch) {
    outputs result(input.documents.size());
    std::vector<tp::receipt<void>> receipts;
    receipts.reserve((input.documents.size() + batch - 1) / batch);
    std::exception_ptr error;
    for (std::size_t first = 0; first < input.documents.size(); first += batch) {
        const auto last = std::min(first + batch, input.documents.size());
        try { receipts.push_back(pool.submit([&, first, last] {
            // Each task writes separate vector elements; no result mutex or
            // atomic checksum bottleneck. Preserve the original document order.
            for (auto i = first; i < last; ++i) result[i] = process(input.documents[i]);
        })); } catch (...) { error = std::current_exception(); break; }
    }
    // Drain all receipts before propagating an error: tasks capture result/input
    // by reference, so their buffers must remain alive until every task finishes.
    for (auto& receipt : receipts) {
        try { receipt.get(); } catch (...) { if (!error) error = std::current_exception(); }
    }
    pool.wait_idle();
    if (error) std::rethrow_exception(error);
    return result;
}

void fixtures() {
    const auto input = normalization::encode_utf8(U"\uFF21\uFF22\uFF23 \uFB03 Cafe\u0301, \u1112\u1161\u11AB!");
    const auto output = process(input);
    const auto expected = normalization::encode_utf8(U"ABC ffi Caf\u00E9, \uD55C!");
    if (output.text != expected || output.spans != std::vector<std::pair<std::size_t, std::size_t>>{
            {0, 3}, {4, 3}, {8, 5}, {15, 3}})
        throw std::runtime_error("Normalization/span fixture failed");
    if (!process("").spans.empty() || !process(" \t\n,! ").spans.empty())
        throw std::runtime_error("Empty/separator fixture failed");
    if (process(std::string("one\0two", 7)).spans.size() != 2)
        throw std::runtime_error("Embedded NUL fixture failed");
    bool rejected = false;
    try { (void)process(std::string("\xff", 1)); } catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Malformed UTF-8 was accepted");
}

template<class Function>
double measure(Function&& function, const outputs& expected, unsigned repetitions) {
    if (function() != expected) throw std::runtime_error("Warmup output mismatch");
    std::vector<double> samples;
    for (unsigned i = 0; i < repetitions; ++i) {
        const auto begin = clock_type::now();
        auto result = function();
        const auto seconds = std::chrono::duration<double>(clock_type::now() - begin).count();
        // Compare all normalized bytes and all spans, outside the timed interval.
        if (result != expected) throw std::runtime_error("Parallel/serial output mismatch");
        samples.push_back(seconds);
    }
    std::sort(samples.begin(), samples.end());
    const auto middle = samples.size() / 2;
    return samples.size() % 2 ? samples[middle] : (samples[middle - 1] + samples[middle]) / 2;
}

void report(const char* mode, std::size_t workers, std::size_t batch, const corpus& input,
            double elapsed, double baseline) {
    std::cout << std::left << std::setw(8) << mode << std::right << std::setw(8) << workers
              << std::setw(8) << batch << std::fixed << std::setprecision(3)
              << std::setw(12) << elapsed * 1000 << std::setprecision(1)
              << std::setw(12) << static_cast<double>(input.bytes) / (1024 * 1024) / elapsed
              << std::setw(14) << static_cast<double>(input.documents.size()) / elapsed
              << std::setprecision(2) << std::setw(10) << baseline / elapsed << "x\n";
}

template<class Pool>
void run_mode(const char* name, std::size_t workers, const corpus& input,
              const outputs& expected, double baseline, unsigned repetitions, bool test_only) {
    Pool pool(workers);
    if constexpr (std::is_same_v<Pool, tp::greedy_threadpool>) pool.go();
    for (const auto batch : {std::size_t{1}, std::size_t{8}, std::size_t{32}}) {
        const auto elapsed = measure([&] { return parallel(pool, input, batch); }, expected, repetitions);
        if (!test_only) report(name, workers, batch, input, elapsed, baseline);
    }
    // Also check that errors in one document don't invalidate captured buffers
    // while the remaining queued documents are still being processed.
    if (test_only) {
        auto invalid = input;
        invalid.documents[0] = std::string("\xff", 1);
        bool rejected = false;
        try { (void)parallel(pool, invalid, 1); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || parallel(pool, input, 8) != expected)
            throw std::runtime_error("Parallel exception/recovery test failed");
    }
    if constexpr (std::is_same_v<Pool, tp::greedy_threadpool>) pool.stop();
}
} // namespace

int main(int argc, char** argv) {
    try {
        const bool test_only = argc == 2 && std::string_view(argv[1]) == "--test";
        if (argc > 1 && !test_only) throw std::invalid_argument("Usage: text_processing [--test]");
        fixtures();
        const unsigned repetitions = test_only ? 1 : 5;
        const std::vector<corpus> corpora = {
            make_corpus("short messages", test_only ? 128 : 8192, 64, 512),
            make_corpus("long articles", test_only ? 16 : 512, 8192, 32768)
        };
        const auto maximum_workers = std::min(16u, std::max(1u, std::thread::hardware_concurrency()));
        std::vector<std::size_t> workers = {1, std::min(4u, maximum_workers), maximum_workers};
        std::sort(workers.begin(), workers.end());
        workers.erase(std::unique(workers.begin(), workers.end()), workers.end());
        if (!test_only) std::cout << "NFKC UTF-8 normalization + word-like byte spans; generated multilingual corpus.\n"
            << "Five timed repetitions after warmup; median reported; every output checked against serial.\n"
            << "Corpus generation, pool construction, verification and final output destruction excluded.\n"
            << "Output allocation, submission, processing, result collection and idle wait included.\n";
        for (const auto& input : corpora) {
            const auto expected = serial(input);
            const auto baseline = measure([&] { return serial(input); }, expected, repetitions);
            if (!test_only) {
                std::cout << '\n' << input.name << ": " << input.documents.size() << " documents, "
                          << input.bytes << " input bytes\n"
                          << "Mode     Workers   Batch          ms       MiB/s    documents/s   Speedup\n";
                report("serial", 1, 0, input, baseline, baseline);
            }
            for (auto count : workers) {
                run_mode<tp::threadpool>("normal", count, input, expected, baseline, repetitions, test_only);
                run_mode<tp::greedy_threadpool>("greedy", count, input, expected, baseline, repetitions, test_only);
            }
        }
        std::cout << "PASS: fixtures, normalized bytes, spans and document order match.\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
