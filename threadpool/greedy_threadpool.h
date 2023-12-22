#ifndef juwhan_greedy_greedy_threadpool_h
#define juwhan_greedy_greedy_threadpool_h

#include <atomic>
#include <condition_variable>
#include <exception>
#include <stdexcept>
#include <vector>

#include "thread.h"
#include "thread_task.h"
#include "threadlocal.h"
#include "aligned_circular_array.h"
#include "work_stealing_queue.h"

#include "include_me.h"

#define grd_tp_info(...)
#define grd_tp_info_if(...)

// #define grd_tp_info cinfo
// #define grd_tp_info_if cinfo_if

namespace juwhan {

// Forward declaration.
    template<typename T>
    struct greedy_threadpool_receit;

    struct greedy_threadpool {
        static greedy_threadpool instance;

        template<typename T>
        using receipt_type = greedy_threadpool_receit<T>;

        struct join_guard {
            ::std::vector<thread> &threads;

            explicit join_guard(::std::vector<thread> &threads_) : threads(threads_) {};


            ~join_guard() {
                for (auto i = 0; i < threads.size(); ++i) {
                    if (threads[i].joinable()) threads[i].join();
                }
            }


        };

        using queue_type = work_stealing_queue<thread_task *>;
        using queue_type_ptr = work_stealing_queue<thread_task *> *;
        ::std::atomic<bool> active; // Stop and go according to this.
        char pad0[JUWHAN_CACHELINE_SIZE];
        ::std::atomic<bool> done;
        char pad1[JUWHAN_CACHELINE_SIZE];
        ::std::vector<thread> threads;
        char pad2[JUWHAN_CACHELINE_SIZE];
        ::std::mutex mut;
        char pad3[JUWHAN_CACHELINE_SIZE];
        ::std::condition_variable cond;
        char pad4[JUWHAN_CACHELINE_SIZE];
        // The following are read only. No need to prevent false sharing.
        ::std::vector<queue_type_ptr> master_queues;
        ::std::vector<::std::vector<queue_type_ptr>> master_neighboring_queues;
        threadlocal<queue_type_ptr> my_queue;
        threadlocal<::std::vector<queue_type_ptr> *> neighboring_queues;
        join_guard joiner;

        void make_master_queues(size_t thread_count) {
            grd_tp_info("Building queue structure for a thread pool ...");
            for (auto i = 0; i < thread_count; ++i) {
                master_queues.push_back(new queue_type{});
            }
            grd_tp_info("Master queue is made.");
            // Make neighboring queus and put them in the master.
            for (auto i = 0; i < thread_count; ++i) {
                ::std::vector<queue_type_ptr> tmp_neighboring_queues;
                for (auto j = 0; j < thread_count; ++j) {
                    if (j != i) {
                        tmp_neighboring_queues.push_back(master_queues[j]);
                    }
                }
                master_neighboring_queues.push_back(tmp_neighboring_queues);
            }
            grd_tp_info("Neighbor queues are made.");
        };


        void worker(size_t me) {
            grd_tp_info("A worker (" + to_string(me) + ") has entered.");
            // Initialize threadlocal variables.
            my_queue.set(master_queues[me]);
            grd_tp_info("I (" + to_string(me) + ") just set my master queue.");
            neighboring_queues.set(&master_neighboring_queues[me]);
            grd_tp_info("I (" + to_string(me) + ") just set my neighboring queues.");
            thread_task *fetched_task;
            while (!done) {
                while (!done && active) {
                    grd_tp_info("I (" + to_string(me) + ") just entered the main working loop.");
                    fetched_task = fetch_task();
                    grd_tp_info_if(fetched_task, "I(" + to_string(me) + ") fetched a job.");
                    if (fetched_task) {
                        (*fetched_task)();
                        // Delete the done-with task.
                        delete fetched_task;
                    } else {
                        this_thread::yield();  // This is a greedy threadpool. Just yield for a moment and keep crunching.
                    }
                }
                if (!done) // Done has priority. For example, if the condition was done:true && active::false, then honor done:false.
                {
                    // Someone stoppped the thread.
                    grd_tp_info("I am thread(" + ::juwhan::to_string(me) +
                                "). Someone ordered me to stop crunching. I will stop until further notice.");
                    // Wait until some work is added or done flag is raised.
                    ::std::unique_lock<::std::mutex> lock{mut};
                    cond.wait(lock, [this] { return (active.load() || done); });
                    lock.unlock();
                }
            }
            grd_tp_info("A worker (" + to_string(me) + ") is about to finish...");
            // The done flag is raised. However, there may be remaining tasks in the queue. Flush it.
            flush();
            grd_tp_info("The main queue related to thread (" + to_string(me) + ") has been flushed.");
            // Now, release the queue. Note that the threadlocal queue is just an alias to the thread specific queue.
            my_queue.release();
            grd_tp_info("The main queue related to thread (" + to_string(me) + ") has been released.");
            neighboring_queues.release();
            grd_tp_info("The neighbor queues related to thread (" + to_string(me) + ") has been released.");
        };


        // A greedy_threadpool may not be copied, moved, or assigned to another.
        greedy_threadpool(greedy_threadpool &other) = delete;

        greedy_threadpool(greedy_threadpool &&other) = delete;

        greedy_threadpool &operator=(greedy_threadpool &other) = delete;

        greedy_threadpool &operator=(greedy_threadpool &&other) = delete;


        // The default constructor.
        greedy_threadpool(size_t thread_count = 0)
                : done{false}, joiner{threads}, master_queues{}, my_queue{}, neighboring_queues{},
                  master_neighboring_queues{}, mut{}, cond{}, active{false} {
            if (thread_count == 0) thread_count = thread::hardware_concurrency();
            make_master_queues(thread_count);
            // Initialize threadlocal variables for main.
            my_queue.set(master_queues[0]);
            grd_tp_info("I(0) the master thread set my master queue.");
            neighboring_queues.set(&master_neighboring_queues[0]);
            grd_tp_info("I(0) the master thread set my neighboring queues.");
            try {
                // 0 is this (main) thread.
                for (auto i = 1; i < thread_count; ++i) {
                    threads.push_back(thread(&greedy_threadpool::worker, this, i));
                    grd_tp_info("I just activated the worker(" + to_string(i) + ") thread.");
                }
            }
            catch (...) {
                done = true;
                throw ::std::runtime_error("An error occurred while generating worker threads in a greedy_threadpool.");
            }
        };


        // There are cases where a user want to clear up the greedy_threadpool early.
        // In such cases, call destroy.
        void destroy() {
            // Set done flag.
            grd_tp_info("This greedy_threadpool is about to be destroyed.");
            done.store(true);
            grd_tp_info("I set the done flag up.");
            // Some threads may be sleeping. Wake them up.
            {
                ::std::lock_guard<::std::mutex> lg{mut};
                grd_tp_info("I am waking up threads to flush and exit properly...")
                cond.notify_all();
            }
            // Join.
            for (auto i = 0; i < threads.size(); ++i) threads[i].join();
            grd_tp_info("Now, all threads are joined.");
            // Delete queues.
            for (auto i = 0; i < master_queues.size(); ++i) delete master_queues[i];
            grd_tp_info("Now, all master queues are deleted.");
        };


        // Destructor.
        ~greedy_threadpool() {
            if (!done) destroy();
        };


        void flush() {
            grd_tp_info("I will flush my queue.");
            while (auto task = my_queue->pop()) delete task;
        };


        // Fetch a task.
        thread_task *fetch_task() {
            grd_tp_info("OK, I am about to fetch a task.");
            auto fetched_task = my_queue->pop();
            if (fetched_task) return fetched_task;
            // My queue is empty. Try to steal from neighbors, including the main queue.
            for (auto i = 0; i < neighboring_queues->size(); ++i) {
                auto nq = (*neighboring_queues)[i];
                fetched_task = nq->steal();
                // If a task is found, return it without pushing it to the mq, to avoid racing.
                if (fetched_task) return fetched_task;
            }
            grd_tp_info("I could not steal from my neighbors including the main queues.");
            return nullptr;
        };


        // Submit a task.
        template<typename F, typename... A>
        greedy_threadpool_receit<typename thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>::result_type>
        submit(F &&_func, A &&... args) {
            using task_type = thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>;
            using result_type = typename task_type::result_type;
            grd_tp_info("I'll submit a task");
            // Make a task.
            thread_task *new_task = make_task(juwhan::forward<F>(_func), juwhan::forward<A>(args)...);
            grd_tp_info("I just generated a task.");
            // Compose a receit.
            greedy_threadpool_receit<result_type> receit{(reinterpret_cast<task_type *>(new_task))->ret, *this};
            my_queue->push(new_task);
            return receit;
        };

        // Stop and go.
        void stop() {
            active.store(false);
        };

        void go() {
            active.store(true);
            // Now wake up threads to see if they're sleeping.
            {
                ::std::lock_guard<::std::mutex> lg{mut};
                cond.notify_all();
            }
        };

    };

// greedy_threadpool_receit is read only, hence a class instead of a struct.
    template<typename T>
    struct greedy_threadpool_receit_base_implementation {
        function_return_type<T> ret;
        greedy_threadpool *tp;

        greedy_threadpool_receit_base_implementation() : ret{}, tp{nullptr} {};

        greedy_threadpool_receit_base_implementation(greedy_threadpool_receit_base_implementation &other)
                : ret{other.ret}, tp{other.tp} {};

        greedy_threadpool_receit_base_implementation(greedy_threadpool_receit_base_implementation &&other)
                : ret{::juwhan::move(other.ret)}, tp{other.tp} {};

        // greedy_Threadpool cannot be an rvalue.
        greedy_threadpool_receit_base_implementation(function_return_type<T> &ret_, greedy_threadpool &tp_)
                : ret{ret_}, tp{&tp_} {};

        greedy_threadpool_receit_base_implementation(function_return_type<T> &&ret_, greedy_threadpool &tp_)
                : ret{::juwhan::move(ret_)}, tp{&tp_} {};

        void wait() {
            if (!tp) return;
            grd_tp_info("I am a waiting thread. I will try to process pending tasks while waiting.");
            while (!ret.is_set() && !tp->done) {
                while (!ret.is_set() && tp->active && !tp->done) {
                    auto fetched_task = tp->fetch_task();
                    if (fetched_task) {
                        grd_tp_info("I picked up a task while waiting for a function result to arrive.");
                        (*fetched_task)();
                        // Delete the done-with task.
                        delete fetched_task;
                    } else {
                        this_thread::yield();
                    }
                }
                // is_set has the upper most priority.
                // if (tp->is_set()) break;
                // Done is the second.
                // if (tp->done) break;
                // Someone stoppped the thread.
                if (!ret.is_set() && !tp->done) {
                    ::std::unique_lock<::std::mutex> lock{tp->mut};
                    tp->cond.wait(lock, [this] { return (tp->active.load() || tp->done.load()); });
                    lock.unlock();
                }
            } // Return value is set or the threadpool is done with.
        };
    };


// A receit that returns something.
    template<typename T>
    struct greedy_threadpool_receit : public greedy_threadpool_receit_base_implementation<T> {
        using base_type = greedy_threadpool_receit_base_implementation<T>;
        using base_type::base_type;

        greedy_threadpool_receit() : base_type{} {};

        greedy_threadpool_receit(greedy_threadpool_receit &other)
                : base_type{other} {};

        greedy_threadpool_receit(greedy_threadpool_receit &&other)
                : base_type{::juwhan::move(other)} {};

        greedy_threadpool_receit &operator=(greedy_threadpool_receit &other) {
            this->ret = other.ret;
            this->tp = other.tp;
            return *this;
        };

        greedy_threadpool_receit &operator=(greedy_threadpool_receit &&other) {
            this->ret = ::juwhan::move(other.ret);
            this->tp = other.tp;
            return *this;
        };

        T get() {
            if (!this->ret.is_set()) this->wait();
            if (this->ret.is_exceptional()) {
                ::std::string exception_str(this->ret.get_exception().what());
                throw ::std::runtime_error(exception_str);
            }
            return this->ret.get();
        };
    };


//  A receit that returns nothing(void).
    template<>
    struct greedy_threadpool_receit<void> : public greedy_threadpool_receit_base_implementation<void> {
        using base_type = greedy_threadpool_receit_base_implementation<void>;
        using base_type::base_type;

        greedy_threadpool_receit() : base_type{} {};

        greedy_threadpool_receit(greedy_threadpool_receit &other)
                : base_type{other} {};

        greedy_threadpool_receit(greedy_threadpool_receit &&other)
                : base_type{::juwhan::move(other)} {};

        greedy_threadpool_receit &operator=(greedy_threadpool_receit &other) {
            this->ret = other.ret;
            this->tp = other.tp;
            return *this;
        };

        greedy_threadpool_receit &operator=(greedy_threadpool_receit &&other) {
            this->ret = ::juwhan::move(other.ret);
            this->tp = other.tp;
            return *this;
        };

        void get() {
            if (!this->ret.is_set()) this->wait();
            if (this->ret.is_exceptional()) {
                ::std::string exception_str(this->ret.get_exception().what());
                throw ::std::runtime_error(exception_str);
            }
        };
    };


} // End of namespace juwhan.
#endif