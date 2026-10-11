#pragma once

#include <functional>
#include <future>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nnn::threadpool::detail {

// One queued object owns its packaged task directly. A shared packaged-task
// wrapper inside std::function added extra allocations and reference counting.
struct task {
    virtual ~task() = default;
    virtual void operator()() = 0;
};

template<class Result>
struct packaged_task final : task {
    std::packaged_task<Result()> function;
    template<class Callable>
    explicit packaged_task(Callable&& callable) : function(std::forward<Callable>(callable)) {}
    void operator()() override { function(); }
};

// std::apply unpacks the tuple using standard INVOKE semantics, handling
// functions, functors, reference wrappers and member pointers uniformly.
// Arguments are decay-copied then moved into this one-shot invocation. Use
// std::ref/std::cref to access an existing object by reference.
template<class F, class... Args>
auto make_packaged_task(F&& function, Args&&... args) {
    using result_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
    auto call = [function = std::decay_t<F>(std::forward<F>(function)),
                 arguments = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]() mutable -> result_type {
        if constexpr (std::is_void_v<result_type>)
            std::apply(std::move(function), std::move(arguments));
        else return std::apply(std::move(function), std::move(arguments));
    };
    // The future's standard shared state survives independently of this object.
    // The callable/arguments remain move-only-capable, without std::function.
    return std::make_unique<packaged_task<result_type>>(std::move(call));
}

} // namespace nnn::threadpool::detail
