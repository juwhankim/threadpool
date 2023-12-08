
#ifndef juwhan_threadpool_h
#define juwhan_threadpool_h

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

#define tp_info(...)
#define tp_info_if(...)

namespace juwhan {

// Forward declaration.
    template<typename T>
    struct threadpool_receit;

    struct threadpool {
        static threadpool instance;

        template<typename T>
        using receipt_type = threadpool_receit<T>;

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
        ::std::atomic<size_t> outstanding_count;
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
            tp_info("Building queue structure for a thread pool ...");
            for (auto i = 0; i < thread_count; ++i) {
                master_queues.push_back(new queue_type{});
            }
            tp_info("Master queue is made.");
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
            tp_info("Neighbor queues are made.");
        };


        void worker(size_t me) {
            tp_info("A worker (" + to_string(me) + ") has entered.");
            // Initialize threadlocal variables.
            my_queue.set(master_queues[me]);
            tp_info("I (" + to_string(me) + ") just set my master queue.");
            neighboring_queues.set(&master_neighboring_queues[me]);
            tp_info("I (" + to_string(me) + ") just set my neighboring queues.");
            thread_task *fetched_task;
            while (!done) {
                tp_info("I (" + to_string(me) + ") just entered the main working loop.");
                fetched_task = fetch_task();
                tp_info_if(fetched_task, "I(" + to_string(me) + ") fetched a job.");
                if (fetched_task) {
                    auto old_outstanding_count = outstanding_count.fetch_sub(1);
                    tp_info("I am about to execute the task. My queue had " + to_string(old_outstanding_count) +
                            " outstanding tasks and now it has " + to_string(outstanding_count.load()) + ".");
                    (*fetched_task)();
                    // Now wake up threads to see if a results they're waiting is ready.
                    {
                        tp_info("I(" + to_string(me) + ") finished a job. I'll notify all.");
                        ::std::lock_guard<::std::mutex> lg{mut};
                        cond.notify_all();
                    }
                    // Delete the done-with task.
                    delete fetched_task;
                } else {
                    tp_info("I(" + to_string(me) + ") could NOT fetch a job. I intend to fall sleep.");
                    // Wait until some work is added or done flag is raised.
                    ::std::unique_lock<::std::mutex> lock{mut};
                    cond.wait
                            (
                                    lock, [this] {
                                        return
                                                (
                                                        (outstanding_count.load() > 0)
                                                        || (done.load())
                                                );
                                    }
                            );
                    lock.unlock();
                    tp_info("I(" + to_string(me) + ") woke up. The wake up condition is outstanding_count: " +
                            to_string(outstanding_count.load()) + ". I'll resume working.");
                }
            }
            tp_info("A worker (" + to_string(me) + ") is about to finish...");
            // The done flag is raised. However, there may be remaining tasks in the queue. Flush it.
            flush();
            tp_info("The main queue related to thread (" + to_string(me) + ") has been flushed.");
            // Now, release the queue. Note that the threadlocal queue is just an alias to the thread specific queue.
            my_queue.release();
            tp_info("The main queue related to thread (" + to_string(me) + ") has been released.");
            neighboring_queues.release();
            tp_info("The neighbor queues related to thread (" + to_string(me) + ") has been released.");
        };


        // A threadpool may not be copied, moved, or assigned to another.
        threadpool(threadpool &other) = delete;

        threadpool(threadpool &&other) = delete;

        threadpool &operator=(threadpool &other) = delete;

        threadpool &operator=(threadpool &&other) = delete;


        // The default constructor.
        threadpool(size_t thread_count = 0)
                : done{false}, joiner{threads}, master_queues{}, my_queue{}, neighboring_queues{},
                  master_neighboring_queues{}, outstanding_count{0}, mut{}, cond{} {
            if (thread_count == 0) thread_count = thread::hardware_concurrency();
            make_master_queues(thread_count);
            // Initialize threadlocal variables for main.
            my_queue.set(master_queues[0]);
            tp_info("I(0) the master thread set my master queue.");
            neighboring_queues.set(&master_neighboring_queues[0]);
            tp_info("I(0) the master thread set my neighboring queues.");
            try {
                // 0 is this (main) thread.
                for (auto i = 1; i < thread_count; ++i) {
                    threads.push_back(thread(&threadpool::worker, this, i));
                    tp_info("I just activated the worker(" + to_string(i) + ") thread.");
                }
            }
            catch (...) {
                done = true;
                throw ::std::runtime_error("An error occurred while generating worker threads in a threadpool.");
            }
        };


        // There are cases where a user want to clear up the threadpool early.
        // In such cases, call destroy.
        void destroy() {
            // Set done flag.
            tp_info("This threadpool is about to be destroyed.");
            done.store(true);
            tp_info("I set the done flag up.");
            // Some threads may be sleeping. Wake them up.
            {
                ::std::lock_guard<::std::mutex> lg{mut};
                tp_info("I am waking up threads to flush and exit properly...")
                cond.notify_all();
            }
            // Join.
            for (auto i = 0; i < threads.size(); ++i) threads[i].join();
            tp_info("Now, all threads are joined.");
            // Delete queues.
            for (auto i = 0; i < master_queues.size(); ++i) delete master_queues[i];
            tp_info("Now, all master queues are deleted.");
        };


        // Destructor.
        ~threadpool() {
            if (!done) destroy();
        };


        void flush() {
            tp_info("I will flush my queue.");
            while (auto task = my_queue->pop()) delete task;
        };


        // Fetch a task.
        thread_task *fetch_task() {
            tp_info("OK, I am about to fetch a task.");
            bool is_empty{false};
            while (!is_empty) {
                auto fetched_task = my_queue->pop();
                if (fetched_task) return fetched_task;
                tp_info("My queue appears to be empty at this point. I'll try to steal from others.");
                if (fetched_task.state == return_state::empty) is_empty = true;
                // My queue is empty. Try to steal from neighbors, including the main queue.
                for (auto i = 0; i < neighboring_queues->size(); ++i) {
                    tp_info("Trying to steal from my " + to_string(i) + "th neighbor.");
                    auto nq = (*neighboring_queues)[i];
                    fetched_task = nq->steal();
                    // If a task is found, return it without pushing it to the mq, to avoid racing.
                    // All threads may be stealing and if the task is pushed into mq, ping-poinging would occur.
                    if (fetched_task) return fetched_task;
                    if (fetched_task.state == return_state::abort) is_empty = false;
                }
                tp_info("I could not steal from my neighbors including the main queues.");
                tp_info_if(is_empty, "All the queues are truly empty.");
                tp_info_if(!is_empty, "Although I failed, it may be due to race-loss. I'll try again.");
            }
            // At this point, all queues appear empty. Return nullptr.
            return nullptr;
        };


        // Submit a task.
        template<typename F, typename... A>
        threadpool_receit<typename thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>::result_type>
        submit(F &&_func, A &&... args) {
            using task_type = thread_task_implementation<typename decay<F>::type, typename decay<A>::type...>;
            using result_type = typename task_type::result_type;
            tp_info("I'll submit a task");
            // Make a task.
            thread_task *new_task = make_task(juwhan::forward<F>(_func), juwhan::forward<A>(args)...);
            tp_info("I just generated a task.");
            // Compose a receit.
            threadpool_receit<result_type> receit{(reinterpret_cast<task_type *>(new_task))->ret, *this};
            // Incrementing the outstanding_count before pushing in the element prevents negative count.
            auto old_outstanding_count = outstanding_count.fetch_add(1);
            tp_info("Before submitting the task, my queue had " + to_string(old_outstanding_count) +
                    " outstanding tasks and now it has " + to_string(outstanding_count.load()) + ".");
            my_queue->push(new_task);
            if (old_outstanding_count == 0) {
                // We need to guard this block.
                // We want to wake up sleeping threads after they release locks, i.e., went into wait() after the while statement.
                tp_info("I just submitted a task while no pending tasks are lined up. Since some threads may be sleeping, I'll wake them up.");
                ::std::lock_guard<::std::mutex> lg{mut};
                cond.notify_all();
            }
            return receit;
        };


    };

// threadpool_receit is read only, hence a class instead of a struct.
    template<typename T>
    struct threadpool_receit_base_implementation {
        function_return_type<T> ret;
        threadpool *tp;

        threadpool_receit_base_implementation() : ret{}, tp{nullptr} {};

        threadpool_receit_base_implementation(threadpool_receit_base_implementation &other)
                : ret{other.ret}, tp{other.tp} {};

        threadpool_receit_base_implementation(threadpool_receit_base_implementation &&other)
                : ret{::juwhan::move(other.ret)}, tp{other.tp} {};

        // Threadpool cannot be an rvalue.
        threadpool_receit_base_implementation(function_return_type<T> &ret_, threadpool &tp_)
                : ret{ret_}, tp{&tp_} {};

        threadpool_receit_base_implementation(function_return_type<T> &&ret_, threadpool &tp_)
                : ret{::juwhan::move(ret_)}, tp{&tp_} {};

        void wait() {
            if (!tp) return;
            tp_info("I am a waiting thread. I will try to process pending tasks while waiting.");
            while (!ret.is_set() && !tp->done) {
                tp_info("I just entered task fetching cycle.");
                auto fetched_task = tp->fetch_task();
                if (fetched_task) {
                    tp_info("I picked up a task while waiting for a function result to arrive.");
                    auto old_outstanding_count = tp->outstanding_count.fetch_sub(1);
                    tp_info("I am about to execute the task. My queue had " + to_string(old_outstanding_count) +
                            " outstanding tasks and now it has " + to_string(tp->outstanding_count.load()) + ".");
                    (*fetched_task)();
                    tp_info("A task is done while waiting for a function result. I will wake up waiting threads.");
                    // Now wake up threads to see if a results they're waiting is ready.
                    {
                        ::std::lock_guard<::std::mutex> lg{tp->mut};
                        tp->cond.notify_all();
                    }
                    // Delete the done-with task.
                    delete fetched_task;
                } else {
                    tp_info("I tried to acquire either a function result or a pending task, but could NOT pick up any. I intend to go into sleep.");
                    ::std::unique_lock<::std::mutex> lock{tp->mut};
                    tp->cond.wait
                            (
                                    lock, [this] {
                                        return
                                                (
                                                        (tp->outstanding_count.load() > 0)
                                                        || (tp->done.load())
                                                        || (ret.is_set())
                                                );
                                    }
                            );
                    lock.unlock();
                }
            }
        };
    };


// A receit that returns something.
    template<typename T>
    struct threadpool_receit : public threadpool_receit_base_implementation<T> {
        using base_type = threadpool_receit_base_implementation<T>;
        using base_type::base_type;

        threadpool_receit() : base_type{} {};

        threadpool_receit(threadpool_receit &other)
                : base_type{other} {};

        threadpool_receit(threadpool_receit &&other)
                : base_type{::juwhan::move(other)} {};

        threadpool_receit &operator=(threadpool_receit &other) {
            this->ret = other.ret;
            this->tp = other.tp;
            return *this;
        };

        threadpool_receit &operator=(threadpool_receit &&other) {
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
    struct threadpool_receit<void> : public threadpool_receit_base_implementation<void> {
        using base_type = threadpool_receit_base_implementation<void>;
        using base_type::base_type;

        threadpool_receit() : base_type{} {};

        threadpool_receit(threadpool_receit &other)
                : base_type{other} {};

        threadpool_receit(threadpool_receit &&other)
                : base_type{::juwhan::move(other)} {};

        threadpool_receit &operator=(threadpool_receit &other) {
            this->ret = other.ret;
            this->tp = other.tp;
            return *this;
        };

        threadpool_receit &operator=(threadpool_receit &&other) {
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
















