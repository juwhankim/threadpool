#include "threadpool.h"
#include "greedy_threadpool.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace tp = nnn::threadpool;
using clock_type = std::chrono::steady_clock;

namespace {
struct configuration {
    std::size_t workers = std::min(16u, std::max(1u, std::thread::hardware_concurrency()));
    std::size_t tasks = 4096;
    unsigned rounds = 20000;
    unsigned repetitions = 5;
};

// Synthetic CPU work, not a tokenizer performance claim. The dependent integer
// operations cannot be parallelized inside one task; task IDs vary the input.
std::uint64_t work(std::size_t id, unsigned rounds) {
    auto value = static_cast<std::uint64_t>(id) + 0x9e3779b97f4a7c15ULL;
    for (unsigned i = 0; i < rounds; ++i) {
        value ^= value >> 12;
        value ^= value << 25;
        value ^= value >> 27;
        value *= 0x2545f4914f6cdd1dULL;
    }
    return value;
}

std::uint64_t serial(std::size_t tasks, unsigned rounds) {
    std::uint64_t checksum = 0;
    for (std::size_t i = 0; i < tasks; ++i) checksum += work(i, rounds);
    return checksum;
}

template<class Pool>
std::uint64_t external(Pool& pool, std::size_t tasks, unsigned rounds) {
    std::vector<tp::receipt<std::uint64_t>> results;
    results.reserve(tasks);
    for (std::size_t i = 0; i < tasks; ++i) results.push_back(pool.submit(work, i, rounds));
    std::uint64_t checksum = 0;
    for (auto& result : results) checksum += result.get();
    pool.wait_idle();
    return checksum;
}

template<class Pool>
std::uint64_t recursive(Pool& pool, std::size_t first, std::size_t last, unsigned rounds) {
    // Group work to amortize dispatch costs. Children land on local owner queues,
    // exercising work stealing rather than only the external injection queue.
    if (last - first <= 16) {
        std::uint64_t checksum = 0;
        for (auto i = first; i < last; ++i) checksum += work(i, rounds);
        return checksum;
    }
    const auto middle = first + (last - first) / 2;
    auto left = pool.submit([&pool, first, middle, rounds] { return recursive(pool, first, middle, rounds); });
    const auto right = recursive(pool, middle, last, rounds);
    return left.get() + right;
}

template<class Function>
double measure(Function&& function, unsigned repetitions, std::uint64_t expected) {
    if (function() != expected) throw std::runtime_error("Warmup checksum mismatch");
    std::vector<double> samples;
    samples.reserve(repetitions);
    for (unsigned i = 0; i < repetitions; ++i) {
        const auto begin = clock_type::now();
        const auto checksum = function();
        const auto elapsed = std::chrono::duration<double>(clock_type::now() - begin).count();
        if (checksum != expected) throw std::runtime_error("Benchmark checksum mismatch");
        samples.push_back(elapsed);
    }
    std::sort(samples.begin(), samples.end());
    const auto middle = samples.size() / 2;
    return samples.size() % 2 ? samples[middle] : (samples[middle - 1] + samples[middle]) / 2;
}

void print(const std::string& name, std::size_t tasks, double seconds, double baseline) {
    std::cout << std::left << std::setw(24) << name << std::right << std::fixed
              << std::setprecision(3) << std::setw(12) << seconds * 1000
              << std::setprecision(0) << std::setw(16) << static_cast<double>(tasks) / seconds
              << std::setprecision(4) << std::setw(12) << baseline / seconds << "x\n";
}

template<class Pool>
void run_pool(const configuration& config, const char* name, std::uint64_t expected,
              double baseline, double tiny_baseline, std::uint64_t tiny_expected) {
    Pool pool(config.workers);
    if constexpr (std::is_same_v<Pool, tp::greedy_threadpool>) pool.go();
    const auto cpu = measure([&] { return external(pool, config.tasks, config.rounds); }, config.repetitions, expected);
    print(std::string(name) + " external CPU", config.tasks, cpu, baseline);
    const auto fork_join = measure([&] {
        auto root = pool.submit([&] { return recursive(pool, 0, config.tasks, config.rounds); });
        const auto result = root.get();
        pool.wait_idle();
        return result;
    }, config.repetitions, expected);
    print(std::string(name) + " fork/join CPU", config.tasks, fork_join, baseline);
    const auto tiny_tasks = config.tasks * 8;
    const auto tiny = measure([&] { return external(pool, tiny_tasks, 0); }, config.repetitions, tiny_expected);
    print(std::string(name) + " tiny tasks", tiny_tasks, tiny, tiny_baseline);
    if constexpr (std::is_same_v<Pool, tp::greedy_threadpool>) pool.stop();
}

std::uint64_t positive_argument(const char* argument, std::uint64_t limit) {
    const std::string text(argument);
    if (text.empty() || text.front() == '-') throw std::invalid_argument("Arguments must be positive integers");
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed);
    if (consumed != text.size() || value == 0 || value > limit)
        throw std::invalid_argument("Argument outside supported range");
    return value;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 5) throw std::invalid_argument("Usage: benchmark [workers [tasks [rounds [repetitions]]]]");
        configuration config;
        if (argc > 1) config.workers = static_cast<std::size_t>(positive_argument(argv[1], 256));
        if (argc > 2) config.tasks = static_cast<std::size_t>(positive_argument(argv[2], 1000000));
        if (argc > 3) config.rounds = static_cast<unsigned>(positive_argument(argv[3], 1000000));
        if (argc > 4) config.repetitions = static_cast<unsigned>(positive_argument(argv[4], 100));
        const auto expected = serial(config.tasks, config.rounds);
        const auto baseline = measure([&] { return serial(config.tasks, config.rounds); }, config.repetitions, expected);
        const auto tiny_tasks = config.tasks * 8;
        const auto tiny_expected = serial(tiny_tasks, 0);
        const auto tiny_baseline = measure([&] { return serial(tiny_tasks, 0); }, config.repetitions, tiny_expected);
        std::cout << "Workers=" << config.workers << ", CPU tasks=" << config.tasks
                  << ", rounds/task=" << config.rounds << ", repetitions=" << config.repetitions
                  << " (median, one warmup, checksums verified)\n"
                  << "Pool construction excluded; submission, execution and waiting included.\n"
                  << "Tiny-task speedup uses the serial tiny-task baseline.\n"
                  << std::left << std::setw(24) << "Case" << std::right << std::setw(12) << "ms"
                  << std::setw(16) << "work items/s" << std::setw(13) << "speedup" << '\n';
        print("serial CPU", config.tasks, baseline, baseline);
        print("serial tiny tasks", tiny_tasks, tiny_baseline, tiny_baseline);
        // Pools have disjoint lifetimes so greedy idle polling cannot consume
        // CPU during the serial or normal-mode measurements.
        run_pool<tp::threadpool>(config, "normal", expected, baseline, tiny_baseline, tiny_expected);
        run_pool<tp::greedy_threadpool>(config, "greedy", expected, baseline, tiny_baseline, tiny_expected);
        std::cout << "CPU checksum=" << expected << ", tiny checksum=" << tiny_expected << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }
}
