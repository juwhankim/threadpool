#pragma once

#include "thread_task.h"
#include "work_stealing_queue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace nnn::threadpool {

namespace detail {
struct pool_state;
// Native TLS replaces pthread keys; multiple pools cannot alias local queues.
inline thread_local pool_state* current_pool = nullptr;
inline thread_local std::size_t current_worker = 0;

struct pool_state {
    using queue = work_stealing_queue<task*>;
    struct alignas(cache_line_size) claim_flag { std::atomic<bool> value{false}; };
    const bool greedy;
    std::vector<std::unique_ptr<queue>> queues;
    std::atomic<bool> active;
    std::atomic<bool> closing{false};
    alignas(cache_line_size) std::atomic<std::size_t> pending{0}; // Queued plus executing tasks.
    alignas(cache_line_size) std::atomic<std::uint64_t> epoch{0};
    alignas(cache_line_size) std::atomic<std::size_t> injected{0};
    std::mutex injection_mutex;
    std::deque<std::unique_ptr<task>> injection;
    std::unique_ptr<claim_flag[]> claims;
    std::mutex sleep_mutex;
    std::condition_variable wake;
    std::mutex idle_mutex;
    std::condition_variable idle;

    pool_state(std::size_t count, bool greedy_mode)
        : greedy(greedy_mode), active(!greedy_mode), claims(std::make_unique<claim_flag[]>(count)) {
        queues.reserve(count);
        for (std::size_t i = 0; i < count; ++i) queues.push_back(std::make_unique<queue>());
    }

    void signal(bool all = true) {
        // Change the predicate under the waiter's mutex to prevent lost wakes.
        {
            std::lock_guard<std::mutex> lock(sleep_mutex);
            epoch.fetch_add(1, std::memory_order_release);
        }
        if (all) wake.notify_all();
        else wake.notify_one();
    }

    void completed() {
        if (pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            {
                std::lock_guard<std::mutex> lock(idle_mutex);
            }
            idle.notify_all();
            signal();
        }
    }

    void enqueue(std::unique_ptr<task> value) {
        if (current_pool == this) {
            // Children of executing tasks are accepted during draining shutdown.
            pending.fetch_add(1, std::memory_order_relaxed);
            try {
                queues[current_worker]->push(value.get());
                value.release();
            } catch (...) { completed(); throw; }
        } else {
            // External callers are multiple producers; serialize injection
            // rather than violating the deque's single-owner push contract.
            std::lock_guard<std::mutex> lock(injection_mutex);
            if (closing.load(std::memory_order_acquire))
                throw std::runtime_error("Cannot submit to a closed threadpool");
            pending.fetch_add(1, std::memory_order_relaxed);
            try {
                injection.push_back(std::move(value));
                injected.store(injection.size(), std::memory_order_release);
            }
            catch (...) { completed(); throw; }
        }
        // One new task needs one waiter, not a wakeup of the entire pool.
        if (!greedy) signal(false);
    }

    std::unique_ptr<task> fetch(std::size_t worker) {
        if (auto result = queues[worker]->pop()) return std::unique_ptr<task>(result.value);
        // Idle pollers avoid locking an empty injection queue. Publication is
        // ordered before the wake epoch so a normal worker cannot miss new work
        // and sleep on its already observed epoch.
        if (injected.load(std::memory_order_acquire) != 0) {
            // A worker can steal useful work instead of blocking behind another
            // worker or producer. Normal waiters recheck injected before sleep.
            std::unique_lock<std::mutex> lock(injection_mutex, std::try_to_lock);
            if (lock.owns_lock() && !injection.empty()) {
                auto value = std::move(injection.front());
                injection.pop_front();
                // Amortize the producer mutex over a small batch. This deque
                // is empty here; its initial capacity accommodates seven extras
                // without allocation. Extra tasks remain available to thieves.
                const auto extra = std::min<std::size_t>(7, injection.size() / queues.size());
                for (std::size_t i = 0; i < extra; ++i) {
                    queues[worker]->push(injection.front().get());
                    injection.front().release();
                    injection.pop_front();
                }
                injected.store(injection.size(), std::memory_order_release);
                return value;
            }
        }
        for (std::size_t offset = 1; offset < queues.size(); ++offset) {
            if (auto result = queues[(worker + offset) % queues.size()]->steal())
                return std::unique_ptr<task>(result.value);
        }
        return {};
    }

    void set_active(bool value) {
        // A per-worker claim flag replaces shared-lock traffic on every poll.
        // Sequential consistency orders registration, the active check, and
        // stop's scan: a claim either precedes stop (and stop waits for it), or
        // sees inactive and cannot fetch. No global dispatch mutex is needed.
        active.store(value, std::memory_order_seq_cst);
        if (!value) {
            for (std::size_t i = 0; i < queues.size(); ++i)
                while (claims[i].value.load(std::memory_order_seq_cst)) std::this_thread::yield();
        }
    }

    bool help_one(std::size_t worker, bool honor_pause = false) {
        std::unique_ptr<task> value;
        if (greedy && honor_pause) {
            claims[worker].value.store(true, std::memory_order_seq_cst);
            if (!active.load(std::memory_order_seq_cst)) {
                claims[worker].value.store(false, std::memory_order_seq_cst);
                return false;
            }
            try { value = fetch(worker); }
            catch (...) { claims[worker].value.store(false, std::memory_order_seq_cst); throw; }
            claims[worker].value.store(false, std::memory_order_seq_cst);
        } else value = fetch(worker);
        if (!value) return false;
        (*value)(); // packaged_task stores all user exceptions in the future.
        completed();
        return true;
    }

    void worker(std::size_t index) {
        current_pool = this;
        current_worker = index;
        for (;;) {
            const auto observed = epoch.load(std::memory_order_acquire);
            if (closing.load(std::memory_order_acquire) && pending.load(std::memory_order_acquire) == 0) break;
            if (active.load(std::memory_order_acquire)) {
                if (help_one(index, true)) continue;
                if (greedy) { std::this_thread::yield(); continue; }
            }
            std::unique_lock<std::mutex> lock(sleep_mutex);
            wake.wait(lock, [&] {
                return epoch.load(std::memory_order_acquire) != observed ||
                    (active.load(std::memory_order_acquire) && injected.load(std::memory_order_acquire) != 0) ||
                    (closing.load(std::memory_order_acquire) && pending.load(std::memory_order_acquire) == 0);
            });
        }
        current_pool = nullptr;
        current_worker = 0;
    }
};
} // namespace detail

// Move-only, one-shot receipts use standard futures and preserve exception types.
// A waiting worker runs queued tasks, allowing recursive fork/join with one
// worker. External threads wait without accessing an owner's deque.
template<class T>
class receipt {
    std::future<T> future_;
    std::weak_ptr<detail::pool_state> state_;
public:
    receipt() = default;
    receipt(std::future<T> future, const std::shared_ptr<detail::pool_state>& state)
        : future_(std::move(future)), state_(state) {}
    bool valid() const noexcept { return future_.valid(); }
    bool is_set() const {
        return valid() && future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    void wait() {
        if (!valid()) return; // Preserve default-receipt wait as a no-op.
        if (detail::current_pool == nullptr) { future_.wait(); return; }
        auto state = state_.lock();
        if (state && detail::current_pool == state.get()) {
            while (!is_set()) {
                if (!state->help_one(detail::current_worker)) std::this_thread::yield();
            }
        } else future_.wait();
    }
    T get() {
        // future::get already waits. Only pool workers need cooperative waiting;
        // external callers avoid a weak_ptr lock and a redundant future wait.
        if (detail::current_pool != nullptr) wait();
        if constexpr (std::is_void_v<T>) future_.get();
        else return future_.get();
    }
};

namespace detail {
class pool_base {
    std::shared_ptr<pool_state> state_;
    std::vector<std::thread> workers_;
    std::mutex lifecycle_mutex_;

protected:
    explicit pool_base(std::size_t count, bool greedy)
        : state_(std::make_shared<pool_state>(count == 0 ?
              std::max(1u, std::thread::hardware_concurrency()) : count, greedy)) {
        try {
            workers_.reserve(state_->queues.size());
            for (std::size_t i = 0; i < state_->queues.size(); ++i)
                workers_.emplace_back([state = state_, i] { state->worker(i); });
        } catch (...) {
            state_->closing.store(true, std::memory_order_release);
            state_->set_active(true);
            state_->signal();
            for (auto& worker : workers_) if (worker.joinable()) worker.join();
            throw;
        }
    }

    void set_active(bool value) {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (state_->closing.load(std::memory_order_acquire))
            throw std::runtime_error("Cannot start or stop a closed threadpool");
        state_->set_active(value);
        state_->signal();
    }

public:
    template<class T> using receipt_type = receipt<T>;
    pool_base(const pool_base&) = delete;
    pool_base& operator=(const pool_base&) = delete;
    ~pool_base() { destroy(); }

    std::size_t thread_count() const noexcept { return state_->queues.size(); }
    bool is_closed() const noexcept { return state_->closing.load(std::memory_order_acquire); }

    template<class F, class... Args>
    auto submit(F&& function, Args&&... args) {
        auto packaged = make_packaged_task(std::forward<F>(function), std::forward<Args>(args)...);
        auto future = packaged->function.get_future();
        using result_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        state_->enqueue(std::move(packaged));
        return receipt<result_type>(std::move(future), state_);
    }

    // This waits for zero outstanding tasks, not for future submissions.
    void wait_idle() {
        if (current_pool == state_.get())
            throw std::logic_error("A worker cannot wait for itself to become idle");
        std::unique_lock<std::mutex> lock(state_->idle_mutex);
        state_->idle.wait(lock, [&] { return state_->pending.load(std::memory_order_acquire) == 0; });
    }

    // Idempotent draining shutdown; paused greedy pools are resumed to drain.
    // Call externally: a worker cannot join itself or destroy its own pool.
    void destroy() {
        if (current_pool == state_.get())
            throw std::logic_error("A worker cannot destroy its own threadpool");
        std::lock_guard<std::mutex> lifecycle(lifecycle_mutex_);
        {
            std::lock_guard<std::mutex> gate(state_->injection_mutex);
            state_->closing.store(true, std::memory_order_release);
        }
        state_->set_active(true);
        state_->signal();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
    }
};
} // namespace detail
} // namespace nnn::threadpool
