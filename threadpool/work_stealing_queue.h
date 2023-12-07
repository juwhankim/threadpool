#ifndef juwhan_work_stealing_queue_h
#define juwhan_work_stealing_queue_h

#include <atomic>
#include <cstring>
#include <pthread.h>
#include <exception>
#include <stdexcept>


#include "juwhan_std.h"
#include "aligned_circular_array.h"

#include "include_me.h"

// The default array size is 2^4.
#define DEFAULT_ARRAY_SIZE 4

#define wsq_info(...)
#define wsq_info_if(...)

// #define wsq_info cinfo
// #define wsq_info_if cinfo_if

namespace juwhan {

    enum struct return_state {
        valid = 0,
        empty = 1,
        abort = 2
    };

    template<typename T>
    struct return_type {
        T value;
        return_state state;

        // Conversion to T.
        operator T() const { return value; };

        // Conversion to bool.
        operator bool() const { return (state == return_state::valid) ? true : false; };

        return_type &operator=(T &_value) {
            value = _value;
            state = return_state::valid;
            return *this;
        };

        return_type &operator=(T &&_value) {
            value = ::juwhan::move(_value);
            state = return_state::valid;
            return *this;
        };

        return_type &operator=(return_state _state) {
            state = _state;
            return *this;
        };
    };


    template<typename T>
    struct return_type<T *> {
        T *value;
        return_state state;

        // Conversion to T*.
        operator T *() const { return value; };

        // Conversion to bool.
        operator bool() const { return (state == return_state::valid) ? true : false; };

        // Dereferencing.
        T &operator*() { return *value; };

        T *operator->() { return value; };

        // Assignment.
        return_type &operator=(T *_value) {
            value = _value;
            state = return_state::valid;
            return *this;
        };

        return_type &operator=(return_state _state) {
            state = _state;
            return *this;
        };
    };

    template<typename T, size_t Align = JUWHAN_CACHELINE_SIZE>
    class work_stealing_queue {
        // Private section.
        char pad0[JUWHAN_CACHELINE_SIZE];   // Add padding to prevent false sharing.
        ::std::atomic<aligned_circular_array<T, Align> *> array;
        char pad1[JUWHAN_CACHELINE_SIZE];   // Add padding to prevent false sharing.
        ::std::atomic <size_t> top;
        char pad2[JUWHAN_CACHELINE_SIZE];   // Add padding to prevent false sharing.
        ::std::atomic <size_t> bottom;
        char pad3[JUWHAN_CACHELINE_SIZE];   // Add padding to prevent false sharing.
        pthread_rwlock_t rwlock;
    public:
        using array_type = aligned_circular_array<T, Align>;
        using element_type = aligned_element<::std::atomic < T>, Align>;


        work_stealing_queue(size_t requested_size) : array{new array_type{requested_size}}, top{1}, bottom{1} {
            auto init_result = pthread_rwlock_init(&rwlock, NULL);
            if (init_result)
                throw ::std::runtime_error(
                        "An unknown error occurred while trying to initialize a rwlock in a work stealing queue: error code " +
                        to_string(init_result) + ".");
        };

        work_stealing_queue()
                : work_stealing_queue{1 << DEFAULT_ARRAY_SIZE} {};

        ~work_stealing_queue() {
            // Delete the array without destroying the elements in it.
            *(array.load()) = nullptr;
            delete array.load();
        };


        size_t capacity() { return array.load(::std::memory_order_relaxed)->size(); };


        size_t size() {
            auto b = bottom.load(::std::memory_order_relaxed);
            auto t = top.load(::std::memory_order_relaxed);
            return (b > t) ? b - t : 0;
        };


        void grow(size_t t, size_t b, array_type *a) {
            auto current_size = a->size;
            wsq_info("Queue growth has been requested. Current size of the queue is " + to_string(current_size) + ".");
            auto new_a = new array_type{current_size << 1};
            // Innocent copy.
            // The order is top to bottom.
            for (auto i = t; i <= b; ++i) (*new_a)[i] = (*a)[i];

/*
    // Copy contents using memcpy. The only occasion grow is called when the array is full, i.e. b = t.
    auto offset0 = (t & (current_size - 1));
    auto n0 = (current_size - offset0);
    auto offset1 = 0;
    auto offset2 = offset0 + n0;
    auto n1 = (current_size - n0);
    wsq_info("Size of the new array is " + to_string(new_a->size) + ".");
    wsq_info("We will copy the head from index " + to_string(offset0) + ", " + to_string(n0) + " elements(" + to_string(n0*sizeof(element_type)) + " bytes), and tail from index "  + to_string(offset1) + ", " + to_string(n1) + " elements(" + to_string(n1*sizeof(element_type)) + " bytes) to the end of the destined new array at " + to_string(offset2) + "." );
    auto new_addr = new_a->address();
    auto old_addr = a->address();
    if (n0) memcpy(new_addr + offset0, old_addr + offset0, n0*sizeof(element_type));
    if (n1) memcpy(new_addr + offset2, old_addr + offset1, n1*sizeof(element_type));

*/

            // Write lock here.
            auto lock_result = pthread_rwlock_wrlock(&rwlock);
            if (lock_result)
                throw ::std::runtime_error(
                        "An unknown error occurred while trying to acquire a write lock while growing the array size: error code " +
                        to_string(lock_result) + ".");
            array.store(new_a, ::std::memory_order_seq_cst);
            auto release_result = pthread_rwlock_unlock(&rwlock);
            if (release_result)
                throw ::std::runtime_error(
                        "An unknown error occurred while trying to release a write lock while growing the array size: error code " +
                        to_string(lock_result) + ".");
            // We want to delete the array without destroying the tasks that the elements(pointers) are pointing to.
            *a = nullptr;
            delete a;
        };


        void push(T x) {
            auto b = bottom.load(::std::memory_order_relaxed);
            auto t = top.load(::std::memory_order_acquire);
            auto a = array.load(::std::memory_order_relaxed);
            wsq_info("Pushing an element at " + to_string(b) + "...");
            if (b > t + a->size - 1) {
                wsq_info("While trying to push an element at the bottom index " + to_string(b) +
                         ", array overflow occured. Corresponding top index was " + to_string(t) + ".");
                grow(t, b, a);
                a = array.load(::std::memory_order_relaxed);
            }
            a->put(b, x);
            wsq_info("A value is successfully added in the array buffer.");
            ::std::atomic_thread_fence(::std::memory_order_release);
            bottom.store(b + 1, ::std::memory_order_relaxed);
            wsq_info("Bottom index of the queue is successfully updated to " + to_string(bottom.load()) + ".");
        };


        return_type<T> pop() {
            auto b = bottom.load(::std::memory_order_relaxed) - 1;
            auto a = array.load(::std::memory_order_relaxed);
            bottom.store(b, ::std::memory_order_relaxed);
            atomic_thread_fence(::std::memory_order_seq_cst);
            auto t = top.load(::std::memory_order_relaxed);
            wsq_info("Popping an element at " + to_string(b) + "...");
            return_type<T> x{};
            if (t <= b) {
                // This queue is not empty.
                wsq_info("OK, this queue appears NOT empty. I'll try to pop.");
                x = a->get(b);
                if (t == b) {
                    wsq_info("This queue has only one element, and we are trying to pop it.");
                    // We fetched the last element in the queue.
                    if (!top.compare_exchange_strong(t, t + 1, ::std::memory_order_seq_cst,
                                                     ::std::memory_order_relaxed)) {
                        wsq_info(
                                "While attempting to pop the last element from the queue, the element disappeared. Perhaps another thread stole it.");
                        // Failed in race.
                        x = return_state::empty;
                    }
                    bottom.store(b + 1, ::std::memory_order_relaxed);
                }
            } else {
                wsq_info("Alas, this is an empty queue.");
                // This is an empty queue.
                x = return_state::empty;
                bottom.store(b + 1, ::std::memory_order_relaxed);
            }
            wsq_info_if(x, "I popped an item successfully. Now, the top index is " + to_string(top.load()) +
                           " and the bottom index is " + to_string(bottom.load()) + ".");
            return x;
        };


        return_type<T> steal() {
            auto t = top.load(::std::memory_order_acquire);
            atomic_thread_fence(::std::memory_order_seq_cst);
            auto b = bottom.load(::std::memory_order_acquire);
            wsq_info("Stealing an element at the top index " + to_string(t) + ", where the bottom index is " +
                     to_string(b));
            auto x = return_type<T>{T{}, return_state::empty};
            if (t < b) {
                // This queue is not empty.
                wsq_info("OK, this queue appears NOT empty. I'll try to steal.");
                // Read lock here.
                auto lock_result = pthread_rwlock_rdlock(&rwlock);
                if (lock_result)
                    throw ::std::runtime_error(
                            "An unknown error occurred while trying to acquire a read lock while stealing an item: error code " +
                            to_string(lock_result) + ".");
                auto a = array.load(::std::memory_order_consume);
                x = a->get(t);
                auto release_result = pthread_rwlock_unlock(&rwlock);
                if (release_result)
                    throw ::std::runtime_error(
                            "An unknown error occurred while trying to release a read lock while stealing an item: error code " +
                            to_string(lock_result) + ".");
                if (!top.compare_exchange_strong(t, t + 1, ::std::memory_order_seq_cst, ::std::memory_order_relaxed)) {
                    // Failed in race.
                    wsq_info(
                            "While attempting to steal an element from the queue, the element disappeared. Perhaps another thread stole it or it was the last element and was popped. Now, the top index is " +
                            to_string(top.load()) + " and the bottom index is " + to_string(bottom.load()) + ".");
                    return return_type<T>{T{}, return_state::abort};
                }
            }
            wsq_info_if(x, "I stole an item successfully. Now, the top index is " + to_string(top.load()) +
                           " and the bottom index is " + to_string(bottom.load()) + ".");
            return x;
        };
    };

}  // End of namespace juwhan.


#endif




















