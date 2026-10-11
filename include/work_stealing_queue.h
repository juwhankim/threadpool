#pragma once

#include "aligned_circular_array.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace nnn::threadpool {

enum class return_state { valid, empty, abort };

template<class T>
struct return_type {
    T value{};
    return_state state = return_state::empty;
    explicit operator bool() const noexcept { return state == return_state::valid; }
};

// Chase-Lev deque: exactly ONE owner may push/pop; any number of thieves may
// steal. Slots do not own pointed-to tasks. Only a successful pop/steal owns one.
template<class T, std::size_t Alignment = cache_line_size>
class work_stealing_queue {
    using array_type = aligned_circular_array<T, Alignment>;
    alignas(cache_line_size) std::atomic<std::int64_t> top_{0};
    alignas(cache_line_size) std::atomic<std::int64_t> bottom_{0};
    alignas(cache_line_size) std::atomic<array_type*> array_;
    // A thief can still be reading an old buffer after a new one is published.
    // Retain all buffers until destruction, after the workers have joined.
    std::vector<std::unique_ptr<array_type>> buffers_;

    array_type* grow(array_type* old, std::int64_t top, std::int64_t bottom) {
        auto next = std::make_unique<array_type>(old->size() * 2);
        for (auto i = top; i < bottom; ++i) next->put(i, old->get(i));
        auto* published = next.get();
        buffers_.push_back(std::move(next));
        array_.store(published, std::memory_order_release);
        return published;
    }

public:
    explicit work_stealing_queue(std::size_t capacity = 16) : array_(nullptr) {
        buffers_.push_back(std::make_unique<array_type>(capacity));
        array_.store(buffers_.back().get(), std::memory_order_relaxed);
    }
    work_stealing_queue(const work_stealing_queue&) = delete;
    work_stealing_queue& operator=(const work_stealing_queue&) = delete;

    std::size_t capacity() const noexcept { return array_.load(std::memory_order_acquire)->size(); }
    bool indices_are_lock_free() const noexcept {
        return top_.is_lock_free() && bottom_.is_lock_free() && array_.is_lock_free();
    }

    void push(T value) {
        const auto bottom = bottom_.load(std::memory_order_relaxed);
        const auto top = top_.load(std::memory_order_acquire);
        if (bottom == std::numeric_limits<std::int64_t>::max())
            throw std::overflow_error("Work-stealing queue index exhausted");
        auto* array = array_.load(std::memory_order_relaxed);
        if (static_cast<std::uint64_t>(bottom - top) >= array->size() - 1)
            array = grow(array, top, bottom);
        array->put(bottom, value);
        // Publish the slot before advertising another element.
        bottom_.store(bottom + 1, std::memory_order_release);
    }

    return_type<T> pop() noexcept {
        const auto bottom = bottom_.load(std::memory_order_relaxed) - 1;
        auto* array = array_.load(std::memory_order_relaxed);
        bottom_.store(bottom, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        auto top = top_.load(std::memory_order_relaxed);
        if (top > bottom) {
            bottom_.store(bottom + 1, std::memory_order_relaxed);
            return {};
        }
        return_type<T> result{array->get(bottom), return_state::valid};
        if (top == bottom) {
            // Owner and thieves compete for the final element through top.
            if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                             std::memory_order_relaxed)) result = {};
            bottom_.store(bottom + 1, std::memory_order_relaxed);
        }
        return result;
    }

    return_type<T> steal() noexcept {
        auto top = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        const auto bottom = bottom_.load(std::memory_order_acquire);
        if (top >= bottom) return {};
        auto* array = array_.load(std::memory_order_acquire);
        const auto value = array->get(top);
        if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                         std::memory_order_relaxed))
            return {T{}, return_state::abort};
        return {value, return_state::valid};
    }
};

} // namespace nnn::threadpool
