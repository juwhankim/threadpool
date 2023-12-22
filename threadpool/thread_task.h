#ifndef juwhan_thread_task_h
#define juwhan_thread_task_h

#include <exception>
#include <cstdlib>
#include <atomic>
#include "aligned_circular_array.h"
#include "juwhan_std.h"
#include "fast_function.h"

#define PARAM0
#define PARAM1 this->arg1
#define PARAM2 PARAM1, this->arg2
#define PARAM3 PARAM2, this->arg3
#define PARAM4 PARAM3, this->arg4
#define PARAM5 PARAM4, this->arg5
#define PARAM6 PARAM5, this->arg6
#define PARAM7 PARAM6, this->arg7
#define PARAM8 PARAM7, this->arg8
#define PARAM9 PARAM8, this->arg9
#define PARAM10 PARAM9, this->arg10
#define FUNC_MACRO(N) func(PARAM##N)

// This header file defines a variable length thread_task class.

namespace juwhan {


// Function return type.
    struct function_return_type_base_implementation {
        ::std::atomic<size_t> _shared_count;
        char pad0[JUWHAN_CACHELINE_SIZE];
        ::std::atomic<bool> _is_set;
        char pad1[JUWHAN_CACHELINE_SIZE];
        ::std::atomic<bool> _is_exceptional;
        char pad2[JUWHAN_CACHELINE_SIZE];
        ::std::exception *e;

        function_return_type_base_implementation() : _shared_count{}, _is_set{} {
            _shared_count.store(0, ::std::memory_order_relaxed);
            _is_set.store(false, ::std::memory_order_relaxed);
        };

        function_return_type_base_implementation(function_return_type_base_implementation &other) = delete;

        function_return_type_base_implementation(function_return_type_base_implementation &&other) = default;

        function_return_type_base_implementation &operator=(function_return_type_base_implementation &other) = delete;

        function_return_type_base_implementation &operator=(function_return_type_base_implementation &&other) {
            auto _shared_count_ = other._shared_count.load(::std::memory_order_relaxed);
            auto _is_set_ = other._is_set.load(::std::memory_order_relaxed);
            _shared_count.store(_shared_count_, ::std::memory_order_relaxed);
            _is_set.store(_is_set_, ::std::memory_order_release);
            return *this;
        };

        void share() { _shared_count.fetch_add(1, ::std::memory_order_relaxed); };

        void unshare() { _shared_count.fetch_sub(1, ::std::memory_order_relaxed); };

        void set() { _is_set.store(true, ::std::memory_order_relaxed); };

        void unset() { _is_set.store(false, ::std::memory_order_relaxed); };

        bool is_shared() { return (_shared_count.load(::std::memory_order_relaxed) == 0) ? false : true; };

        bool is_set() { return _is_set.load(::std::memory_order_relaxed); };

        size_t shared_count() { return _shared_count.load(::std::memory_order_relaxed); };

        // Exception handling.
        void set_exception(::std::exception &e_) {
            e = &e_;
            _is_exceptional.store(true, ::std::memory_order_relaxed);
            set();
        };

        bool is_exceptional() { return _is_exceptional.load(::std::memory_order_relaxed); };

        ::std::exception &get_exception() { return *e; };
    };


    template<typename T>
    struct function_return_type {
        T *value_ptr;
        function_return_type_base_implementation *base;

        function_return_type() : base{new function_return_type_base_implementation}, value_ptr{new T} {};

        function_return_type(function_return_type &other) : value_ptr(other.value_ptr), base{other.base} {
            base->share();
        };

        function_return_type(function_return_type &&other) : value_ptr(other.value_ptr), base{other.base} {
            other.value_ptr = nullptr;
            other.base = nullptr;
        }

        function_return_type(T &value) : function_return_type{} {
            *value_ptr = value;
            base->set();
        };

        function_return_type(T &&value) : function_return_type{} {
            *value_ptr = move(value);
            base->set();
        };

        function_return_type &operator=(function_return_type &other) {
            destroy();
            value_ptr = other.value_ptr;
            base = other.base;
            base->share();
            return *this;
        }

        function_return_type &operator=(function_return_type &&other) {
            destroy();
            value_ptr = other.value_ptr;
            base = other.base;
            other.value_ptr = nullptr;
            other.base = nullptr;
            return *this;
        }

        function_return_type operator=(T &value) {
            *value_ptr = value;
            base->set();
            return *this;
        };

        function_return_type operator=(T &&value) {
            *value_ptr = move(value);
            base->set();
            return *this;
        };

        void set(T &value) {
            *value_ptr = value;
            base->set();
        };

        void set(T &&value) {
            *value_ptr = move(value);
            base->set();
        };

        operator T() const { *value_ptr; };

        operator bool() const { return base->is_set(); };

        T get() const { return *value_ptr; };

        void destroy() {
            if (base) {
                if (base->is_shared()) {
                    base->unshare();
                } else {
                    delete value_ptr;
                    delete base;
                }
            }
        };

        ~function_return_type() {
            destroy();
        };

        bool is_set() { return base->is_set(); };

        bool is_shared() { return base->is_shared(); };

        size_t shared_count() { return base->shared_count(); };

        // exception handling.
        void set_exception(::std::exception &e_) {
            base->set_exception(e_);
        };

        bool is_exceptional() {
            return base->is_exceptional();
        };

        ::std::exception &get_exception() {
            return base->get_exception();
        };
    };


    template<typename T>
    struct function_return_type<T &> {
        T **value_ptr;
        function_return_type_base_implementation *base;

        function_return_type() : base{new function_return_type_base_implementation}, value_ptr{new T *} {};

        function_return_type(function_return_type &other) : value_ptr(other.value_ptr), base{other.base} {
            base->share();
        };

        function_return_type(function_return_type &&other) : value_ptr(other.value_ptr), base{other.base} {
            other.value_ptr = nullptr;
            other.base = nullptr;
        }

        function_return_type(T &value) : function_return_type{} {
            value_ptr = &value;
            base->set();
        };

        function_return_type(T &&value) : function_return_type{} {
            value_ptr = &move(value);
            base->set();
        };

        function_return_type &operator=(function_return_type &other) {
            destroy();
            value_ptr = other.value_ptr;
            base = other.base;
            base->share();
            return *this;
        }

        function_return_type &operator=(function_return_type &&other) {
            destroy();
            value_ptr = other.value_ptr;
            base = other.base;
            other.value_ptr = nullptr;
            other.base = nullptr;
            return *this;
        }

        function_return_type operator=(T &value) {
            *value_ptr = &value;
            base->set();
            return *this;
        };

        function_return_type operator=(T &&value) {
            *value_ptr = &move(value);
            base->set();
            return *this;
        };

        void set(T &value) {
            *value_ptr = &value;
            base->set();
        };

        void set(T &&value) {
            *value_ptr = &move(value);
            base->set();
        };

        operator T &() { return **value_ptr; };

        operator bool() const { return base->is_set(); };

        T &get() const { return **value_ptr; };

        void destroy() {
            if (base) {
                if (base->is_shared()) {
                    base->unshare();
                } else {
                    delete value_ptr;
                    delete base;
                }
            }
        };

        ~function_return_type() {
            destroy();
        };

        bool is_set() { return base->is_set(); };

        bool is_shared() { return base->is_shared(); };

        size_t shared_count() { return base->shared_count(); };

        // exception handling.
        void set_exception(::std::exception &e_) {
            base->set_exception(e_);
        };

        bool is_exceptional() {
            return base->is_exceptional();
        };

        ::std::exception &get_exception() {
            return base->get_exception();
        };
    };


    template<>
    struct function_return_type<void> {
        function_return_type_base_implementation *base;

        function_return_type() : base{new function_return_type_base_implementation} {};

        function_return_type(function_return_type &other) : base{other.base} {
            base->share();
        };

        function_return_type(function_return_type &&other) : base{other.base} {
            other.base = nullptr;
        };

        function_return_type &operator=(function_return_type &other) {
            destroy();
            base = other.base;
            base->share();
            return *this;
        };

        function_return_type &operator=(function_return_type &&other) {
            destroy();
            base = other.base;
            other.base = nullptr;
            return *this;
        };

        void set() { base->set(); };

        operator bool() const { return base->is_set(); };

        void destroy() {
            if (base) {
                if (base->is_shared()) {
                    base->unshare();
                } else {
                    delete base;
                }
            }
        };

        ~function_return_type() {
            destroy();
        };

        bool is_set() { return base->is_set(); };

        bool is_shared() { return base->is_shared(); };

        size_t shared_count() { return base->shared_count(); };

        // exception handling.
        void set_exception(::std::exception &e_) {
            base->set_exception(e_);
        };

        bool is_exceptional() {
            return base->is_exceptional();
        };

        ::std::exception &get_exception() {
            return base->get_exception();
        };
    };


// This is the main class.
    class thread_task {
    public:
        virtual void operator()() = 0;

        virtual ~thread_task() {};
    };


// Thread_task helper.
    template<size_t N, typename H, typename... T>
    struct thread_task_helper {
    };
#define T_T_H_MACRO(N) template<typename H, typename... T> struct thread_task_helper<N, H, T...> : thread_task_helper<N+1, T...> { using arg##N##_type = H; H arg##N; template<typename HH, typename... TT> thread_task_helper(HH&& head, TT&&... tail) : thread_task_helper<N+1, T...>{::juwhan::forward<TT>(tail)...}, arg##N(::juwhan::forward<HH>(head)) {}; };

    T_T_H_MACRO(1);

    T_T_H_MACRO(2);

    T_T_H_MACRO(3);

    T_T_H_MACRO(4);

    T_T_H_MACRO(5);

    T_T_H_MACRO(6);

    T_T_H_MACRO(7);

    T_T_H_MACRO(8);

    T_T_H_MACRO(9);

    T_T_H_MACRO(10);
#undef T_T_H_MACRO

// Base helper.
#define T_T_H_MACRO(N) template<typename H> struct thread_task_helper<N, H> { static constexpr size_t arity = N; using arg##N##_type = H; H arg##N; template<typename HH> thread_task_helper(HH&& head) : arg##N(::juwhan::forward<HH>(head)) {}; };

    T_T_H_MACRO(1);

    T_T_H_MACRO(2);

    T_T_H_MACRO(3);

    T_T_H_MACRO(4);

    T_T_H_MACRO(5);

    T_T_H_MACRO(6);

    T_T_H_MACRO(7);

    T_T_H_MACRO(8);

    T_T_H_MACRO(9);

    T_T_H_MACRO(10);
#undef T_T_H_MACRO


// thread_task implementation, where the function takes parameters.
    template<typename F, typename... A>
    struct thread_task_implementation : thread_task_helper<1, A...>, public thread_task {
        using this_type = thread_task_implementation<F, A...>;
        using result_type = typename function_traits<F>::result_type;
        function_return_type<result_type> ret;
        fast_function<typename function_type_deduction<F>::simplified_traits> func;

        // Use perfect ::juwhan::forwarding in constructors.
        // There are 3 executable types.
        // 1. Static function.
        // 2. Member class function.
        // 3. A class with overridden operator(); including lambda functions.


        // 1. The static and the functor case.
        template<typename FF, typename... AA, typename = typename enable_if<
                function_type_deduction<typename decay<FF>::type>::is_static ||
                function_type_deduction<typename decay<FF>::type>::is_functor>::type>
        thread_task_implementation(FF &&func_, AA &&... args)
                : thread_task_helper<1, A...>(::juwhan::forward<AA>(args)...), func(::juwhan::forward<FF>(func_)) {};

        // 2. The member function case.
        template<typename FF, typename T, typename... AA, typename = typename enable_if<function_type_deduction<typename decay<FF>::type>::is_member>::type>
        thread_task_implementation(FF &&func_, T &&this_, AA &&... args)
                :  thread_task_helper<1, A...>(::juwhan::forward<AA>(args)...),
                   func(::juwhan::forward<T>(this_), ::juwhan::forward<FF>(func_)) {};



        // execute().
#define EXE_MACRO(N) template<typename V = this_type> typename enable_if<V::arity==N && !is_same<void, result_type>::value>::type execute() { try { ret = FUNC_MACRO(N); } catch(::std::exception& e) { ret.set_exception(e); } catch(...) {} };

        EXE_MACRO(1);

        EXE_MACRO(2);

        EXE_MACRO(3);

        EXE_MACRO(4);

        EXE_MACRO(5);

        EXE_MACRO(6);

        EXE_MACRO(7);

        EXE_MACRO(8);

        EXE_MACRO(9);

        EXE_MACRO(10);
#undef EXE_MACRO
#define EXE_MACRO(N) template<typename V = this_type> typename enable_if<V::arity==N && is_same<void, result_type>::value>::type execute() { try{ FUNC_MACRO(N); ret.set(); } catch(::std::exception& e) { ret.set_exception(e); } catch(...) {} };

        EXE_MACRO(1);

        EXE_MACRO(2);

        EXE_MACRO(3);

        EXE_MACRO(4);

        EXE_MACRO(5);

        EXE_MACRO(6);

        EXE_MACRO(7);

        EXE_MACRO(8);

        EXE_MACRO(9);

        EXE_MACRO(10);
#undef EXE_MACRO


        // Operator().
        void operator()() {
            execute();
        };


    };


// thread_task implementation, where the function DOES NOT take parameters.
    template<typename F>
    struct thread_task_implementation<F> : public thread_task {
        static constexpr size_t arity = 0;
        using this_type = thread_task_implementation<F>;
        using result_type = typename function_traits<F>::result_type;;
        function_return_type<result_type> ret;
        fast_function<typename function_type_deduction<F>::simplified_traits> func;


        // Use perfect ::juwhan::forwarding in constructors.
        // There are 3 executable types.
        // 1. Static function.
        // 2. Member class function.
        // 3. A class with overridden operator(); including lambda functions.

        // 1. The static and the functor case.
        template<typename FF, typename = typename enable_if<
                function_type_deduction<typename decay<FF>::type>::is_static ||
                function_type_deduction<typename decay<FF>::type>::is_functor>::type>
        thread_task_implementation(FF &&func_)
                : func(::juwhan::forward<FF>(func_)) {};

        // 2. The member function case.
        template<typename FF, typename T, typename = typename enable_if<function_type_deduction<typename decay<FF>::type>::is_member>::type>
        thread_task_implementation(FF &&func_, T &&this_)
                :  func(::juwhan::forward<T>(this_), ::juwhan::forward<FF>(func_)) {};


        // Execute().
        // Returns something.
        template<typename V = this_type>
        typename enable_if<!is_same<void, typename V::result_type>::value>::type execute() {
            // This function returns a value.
            try { ret = func(); }
            catch (::std::exception &e) { ret.set_exception(e); }
            catch (...) {}
        };

        // Returns nothing(void).
        template<typename V = this_type>
        typename enable_if<is_same<void, typename V::result_type>::value>::type execute() {
            // This function returns NOTHING.
            try {
                func();
                ret.set();
            }
            catch (::std::exception &e) { ret.set_exception(e); }
            catch (...) {}
        };

        // Operator().
        void operator()() {
            execute();
        };

    };

// Note I am NOT using shared_ptr<thread_task>, to make sure atomic of the returned pointer is truly atomic. shared_ptr is a class and an atomic deduced from it may not be truly atomic at machine level.
//
// When F is a static function or a functor.
    template<typename F, typename... A>
    inline
    typename enable_if<function_type_deduction<typename decay<F>::type>::is_static ||
                       function_type_deduction<typename decay<F>::type>::is_functor, thread_task *>::type
    make_task(F &&func_, A &&... args) {
        // Use malloc to void* free.
        auto thread_task_pointer = malloc(
                sizeof(thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>));
        new(thread_task_pointer) thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>{
                ::juwhan::forward<F>(func_), ::juwhan::forward<A>(args)...};
        return reinterpret_cast<thread_task *>(thread_task_pointer);
    };

// When F is a member function of a class.
    template<typename F, typename T, typename... A>
    inline
    typename enable_if<function_type_deduction<typename decay<F>::type>::is_member, thread_task *>::type
    make_task(F &&func_, T &&this_, A &&... args) {
        // Use malloc to void* free.
        auto thread_task_pointer = malloc(
                sizeof(thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>));
        new(thread_task_pointer) thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>{
                ::juwhan::forward<F>(func_), ::juwhan::forward<T>(this_), ::juwhan::forward<A>(args)...};
        return reinterpret_cast<thread_task *>(thread_task_pointer);
    };


} // End of namespace juwhan.

// Undef
#undef PARAM0
#undef PARAM1
#undef PARAM2
#undef PARAM3
#undef PARAM4
#undef PARAM5
#undef PARAM6
#undef PARAM7
#undef PARAM8
#undef PARAM9
#undef PARAM10
#undef FUNC_MACRO

#endif