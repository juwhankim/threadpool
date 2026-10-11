#pragma once

#include "pool_core.h"

namespace nnn::threadpool {

// Starts paused. stop() pauses dispatch without interrupting running tasks.
class greedy_threadpool final : public detail::pool_base {
public:
    explicit greedy_threadpool(std::size_t thread_count = 0) : pool_base(thread_count, true) {}
    void go() { set_active(true); }
    void stop() { set_active(false); }
};

template<class T> using greedy_threadpool_receit = receipt<T>;

} // namespace nnn::threadpool
