#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace nnn::threadpool {

inline constexpr std::size_t cache_line_size = 64;

// C++17 over-aligned new[] supplies allocation alignment and destruction.
// Every atomic has a real lifetime, replacing the old raw-byte/union storage.
template<class T, std::size_t Alignment = cache_line_size>
struct alignas(Alignment) aligned_element {
    std::atomic<T> value{T{}};
};

template<class T, std::size_t Alignment = cache_line_size>
class aligned_circular_array {
    static_assert(std::is_trivially_copyable_v<T>, "Queue slots must be atomic-compatible");
    using slot = aligned_element<T, Alignment>;
    const std::size_t capacity_;
    std::unique_ptr<slot[]> slots_;

    static std::size_t rounded_capacity(std::size_t requested) {
        std::size_t capacity = 2;
        while (capacity < requested) {
            if (capacity > std::numeric_limits<std::size_t>::max() / 2)
                throw std::length_error("Work-stealing queue capacity overflow");
            capacity *= 2;
        }
        if (capacity > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 2))
            throw std::length_error("Work-stealing queue capacity exceeds index range");
        return capacity;
    }

public:
    explicit aligned_circular_array(std::size_t requested)
        : capacity_(rounded_capacity(requested)), slots_(std::make_unique<slot[]>(capacity_)) {}

    std::size_t size() const noexcept { return capacity_; }
    T get(std::int64_t index) const noexcept {
        return slots_[static_cast<std::size_t>(index) & (capacity_ - 1)].value.load(std::memory_order_relaxed);
    }
    void put(std::int64_t index, T value) noexcept {
        slots_[static_cast<std::size_t>(index) & (capacity_ - 1)].value.store(value, std::memory_order_relaxed);
    }
};

} // namespace nnn::threadpool
