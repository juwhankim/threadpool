#pragma once

#include "pool_core.h"

namespace nnn::threadpool {

// Normal mode: sleep when idle and wake when tasks are published.
class threadpool final : public detail::pool_base {
public:
    explicit threadpool(std::size_t thread_count = 0) : pool_base(thread_count, false) {}
};

// Keep the upstream spelling; new callers can use receipt<T>.
template<class T> using threadpool_receit = receipt<T>;

} // namespace nnn::threadpool
