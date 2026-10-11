#include "threadpool.h"
#include "greedy_threadpool.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace tp = nnn::threadpool;
using namespace std::chrono_literals;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Fail instead of hanging forever when a synchronization regression occurs.
class watchdog {
    std::mutex mutex_;
    std::condition_variable condition_;
    bool finished_ = false;
    std::thread thread_;
public:
    watchdog() : thread_([this] {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condition_.wait_for(lock, 45s, [this] { return finished_; })) {
            std::cerr << "FAIL: concurrency test timed out\n";
            std::_Exit(2);
        }
    }) {}
    ~watchdog() {
        { std::lock_guard<std::mutex> lock(mutex_); finished_ = true; }
        condition_.notify_one();
        thread_.join();
    }
};

template<class Pool>
void start(Pool& pool) {
    if constexpr (std::is_same_v<Pool, tp::greedy_threadpool>) pool.go();
    else (void)pool;
}

void queue_tests() {
    tp::aligned_circular_array<unsigned> array(3);
    require(array.size() == 4, "capacity rounding");
    array.put(5, 23);
    require(array.get(1) == 23, "ring indexing");
    bool rejected = false;
    try { tp::aligned_circular_array<unsigned> huge(std::numeric_limits<std::size_t>::max()); }
    catch (const std::length_error&) { rejected = true; }
    require(rejected, "capacity overflow must throw");

    tp::work_stealing_queue<unsigned> order(2);
    require(!order.pop() && !order.steal(), "empty deque");
    order.push(1); order.push(2); order.push(3);
    require(order.steal().value == 1 && order.pop().value == 3 && order.pop().value == 2,
            "owner LIFO / thief FIFO");
    require(!order.pop(), "last-item consumption");
    std::cout << "Queue index atomics lock-free: " << std::boolalpha << order.indices_are_lock_free() << '\n';

    constexpr unsigned count = 100000;
    tp::work_stealing_queue<unsigned> queue(2);
    std::vector<std::atomic<unsigned>> seen(count);
    for (auto& value : seen) value.store(0);
    std::atomic<unsigned> consumed{0};
    std::atomic<bool> bad{false};
    const auto record = [&](unsigned value) {
        if (value >= count) bad.store(true);
        else if (seen[value].fetch_add(1) != 0) bad.store(true);
        consumed.fetch_add(1);
    };
    // Force multiple growth steps, then overlap owner pushes/pops with thieves.
    for (unsigned i = 0; i < 4096; ++i) queue.push(i);
    std::vector<std::thread> thieves;
    for (unsigned i = 0; i < 4; ++i) thieves.emplace_back([&] {
        while (consumed.load() < count) {
            if (auto result = queue.steal()) {
                record(result.value);
                // Slow occasional consumers so growth also overlaps stealing.
                if (result.value % 32 == 0) std::this_thread::sleep_for(1us);
            }
            else std::this_thread::yield();
        }
    });
    for (unsigned i = 4096; i < count; ++i) {
        queue.push(i);
        if (i % 3 == 0) if (auto result = queue.pop()) record(result.value);
    }
    while (consumed.load() < count) {
        if (auto result = queue.pop()) record(result.value);
        else std::this_thread::yield();
    }
    for (auto& thief : thieves) thief.join();
    require(!bad.load() && consumed.load() == count, "deque race duplicated/invalid task");
    for (const auto& value : seen) require(value.load() == 1, "deque race lost a task");
}

struct object {
    int value = 12;
    int add(int increment) { return value + increment; }
};

template<class Pool>
unsigned tree(Pool& pool, unsigned depth) {
    if (depth == 0) return 1;
    auto left = pool.submit([&pool, depth] { return tree(pool, depth - 1); });
    auto right = pool.submit([&pool, depth] { return tree(pool, depth - 1); });
    return left.get() + right.get();
}

template<class Pool>
void callable_tests() {
    Pool pool(4);
    start(pool);
    require(pool.thread_count() == 4, "worker count");
    require(pool.submit([](int a, int b) { return a + b; }, 20, 22).get() == 42, "argument invocation");
    object instance;
    require(pool.submit(&object::add, &instance, 3).get() == 15, "member invocation");
    auto reference = pool.submit(&object::value, std::ref(instance));
    reference.get() = 19;
    require(instance.value == 19, "reference result");
    auto pointer = pool.submit([](std::unique_ptr<int> value) { return value; }, std::make_unique<int>(31));
    require(*pointer.get() == 31, "move-only argument/result");
    auto capture = pool.submit([value = std::make_unique<int>(7)] { return *value; });
    require(capture.get() == 7, "move-only callable");
    require(pool.submit([](auto... values) { return (values + ...); },
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1).get() == 18,
            "removed old fixed-arity limit");
    auto void_task = pool.submit([&instance] { instance.value = 29; });
    void_task.get();
    require(instance.value == 29, "void result");
    auto error = pool.submit([]() -> int { throw std::out_of_range("original"); });
    bool preserved = false;
    try { (void)error.get(); }
    catch (const std::out_of_range& exception) { preserved = std::string(exception.what()) == "original"; }
    require(preserved, "standard exception type/message");
    auto nonstandard = pool.submit([] { throw 17; });
    preserved = false;
    try { nonstandard.get(); } catch (int value) { preserved = value == 17; }
    require(preserved, "nonstandard exception");
    auto self_destroy = pool.submit([&pool] { pool.destroy(); });
    preserved = false;
    try { self_destroy.get(); } catch (const std::logic_error&) { preserved = true; }
    require(preserved, "self-join must be rejected");
    auto self_idle = pool.submit([&pool] { pool.wait_idle(); });
    preserved = false;
    try { self_idle.get(); } catch (const std::logic_error&) { preserved = true; }
    require(preserved, "worker wait_idle must be rejected");
    pool.wait_idle();

    // Independent pools and single-worker nested waits exercise native TLS.
    Pool single(1);
    start(single);
    require(single.submit([&single] { return tree(single, 8); }).get() == 256, "single-worker fork/join");
    require(pool.submit([&pool] { return tree(pool, 9); }).get() == 512, "work stealing fork/join");

    tp::receipt<int> empty;
    empty.wait();
    require(!empty.valid(), "default receipt");
    auto original = pool.submit([] { return 23; });
    auto moved = std::move(original);
    require(!original.valid() && moved.get() == 23, "receipt move");
    require(!moved.valid(), "get consumes the future");
}

template<class Pool>
void producer_tests() {
    Pool pool(4);
    start(pool);
    constexpr unsigned producers = 4;
    constexpr unsigned per_producer = 1500;
    std::vector<std::atomic<unsigned>> seen(producers * per_producer);
    for (auto& value : seen) value.store(0);
    std::atomic<bool> bad{false};
    std::vector<std::thread> threads;
    for (unsigned p = 0; p < producers; ++p) threads.emplace_back([&, p] {
        std::vector<tp::receipt<unsigned>> results;
        for (unsigned i = 0; i < per_producer; ++i) {
            const auto id = p * per_producer + i;
            results.push_back(pool.submit([&, id] { seen[id].fetch_add(1); return id; }));
        }
        for (unsigned i = 0; i < per_producer; ++i)
            if (results[i].get() != p * per_producer + i) bad.store(true);
    });
    for (auto& thread : threads) thread.join();
    pool.wait_idle();
    require(!bad.load(), "concurrent producer results");
    for (const auto& value : seen) require(value.load() == 1, "producer task lost or duplicated");

    // Repeated idle wakeups catch notification-before-wait regressions.
    for (unsigned i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(1ms);
        require(pool.submit([i] { return i; }).get() == i, "idle wakeup");
    }
}

template<class Pool>
void shutdown_tests() {
    std::atomic<unsigned> completed{0};
    tp::receipt<int> survives;
    {
        Pool pool(2);
        // Greedy pool deliberately stays paused: destruction must still drain.
        for (unsigned i = 0; i < 300; ++i) (void)pool.submit([&completed] { completed.fetch_add(1); });
        survives = pool.submit([] { return 42; });
        pool.destroy();
        pool.destroy();
        bool rejected = false;
        try { (void)pool.submit([] {}); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "closed pool submission");
    }
    require(completed.load() == 300 && survives.get() == 42, "shutdown drains and receipts survive");

    {
        Pool automatic(2);
        for (unsigned i = 0; i < 100; ++i) (void)automatic.submit([&completed] { completed.fetch_add(1); });
    }
    require(completed.load() == 400, "destructor drains automatically");

    Pool draining(2);
    start(draining);
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    auto parent = draining.submit([&] {
        entered.set_value();
        gate.wait();
        return draining.submit([] { return 39; }).get();
    });
    entered.get_future().wait();
    std::thread closer([&] { draining.destroy(); });
    while (!draining.is_closed()) std::this_thread::yield();
    release.set_value();
    closer.join();
    require(parent.get() == 39, "shutdown accepts children of in-flight tasks");

    Pool racing(3);
    start(racing);
    std::atomic<unsigned> accepted{0}, executed{0};
    std::promise<void> producing;
    std::thread producer([&] {
        (void)racing.submit([&] { executed.fetch_add(1); });
        accepted.fetch_add(1);
        producing.set_value();
        for (;;) {
            try { (void)racing.submit([&] { executed.fetch_add(1); }); accepted.fetch_add(1); }
            catch (const std::runtime_error&) { break; }
        }
    });
    producing.get_future().wait();
    racing.destroy();
    producer.join();
    require(accepted.load() == executed.load(), "shutdown versus external submit");
}

void greedy_lifecycle_tests() {
    tp::greedy_threadpool pool(2);
    std::atomic<unsigned> count{0};
    auto paused = pool.submit([&] { count.fetch_add(1); });
    std::this_thread::sleep_for(10ms);
    require(count.load() == 0, "greedy starts paused");
    pool.go(); paused.get(); pool.wait_idle();
    for (unsigned i = 0; i < 300; ++i) {
        pool.stop();
        auto task = pool.submit([&] { count.fetch_add(1); });
        std::this_thread::sleep_for(1ms);
        require(count.load() == i + 1, "greedy pause dispatch");
        pool.go(); task.get(); pool.wait_idle();
    }
    pool.stop();

    // Already claimed tasks can finish after stop(), but queued work must stay
    // paused. Block every worker, stop dispatch, then enqueue a large backlog.
    tp::greedy_threadpool busy(4);
    std::atomic<unsigned> entered{0}, queued_completed{0};
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::vector<tp::receipt<void>> blockers, queued;
    for (unsigned i = 0; i < 4; ++i) blockers.push_back(busy.submit([&] {
        entered.fetch_add(1);
        gate.wait();
    }));
    busy.go();
    while (entered.load() != 4) std::this_thread::yield();
    busy.stop();
    for (unsigned i = 0; i < 1024; ++i)
        queued.push_back(busy.submit([&] { queued_completed.fetch_add(1); }));
    release.set_value();
    for (auto& blocker : blockers) blocker.get();
    require(queued_completed.load() == 0, "queued backlog dispatched while paused");
    busy.go();
    for (auto& task : queued) task.get();
    busy.wait_idle();
    require(queued_completed.load() == 1024, "resume lost batched backlog");
}

template<class Pool>
void run_mode(const char* name) {
    callable_tests<Pool>();
    producer_tests<Pool>();
    shutdown_tests<Pool>();
    std::cout << "PASS: " << name << " invocation, fork/join, producers, wakeups, shutdown\n";
}
} // namespace

int main() {
    watchdog timeout;
    try {
        queue_tests();
        run_mode<tp::threadpool>("normal");
        run_mode<tp::greedy_threadpool>("greedy");
        greedy_lifecycle_tests();
        std::cout << "PASS: greedy pause/resume and all correctness tests\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
