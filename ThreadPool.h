
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace std;

class ThreadPool {
private:
    vector<thread> workers;
    queue<function<void()>> tasks;

    mutex queue_mutex;
    condition_variable condition;
    bool stop;

public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
};

ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for (size_t i = 0; i < threads; i++) {
        // 'emplace_back' will construct a std::thread with the lambda expression and add it to the 'worker' vector
        workers.emplace_back([this] {

            // the thread just do one thing: try to get a task from 'tasks' and execute it.

            while (true) {
                function<void()> task;

                {
                    unique_lock<mutex> lock(this->queue_mutex);

                    // Unblock when there is a task or 'stop == true'.
                    // That means it will never block as if 'stop == true'.
                    // We should never worry there will be thread blocked when 'stop == true'.
                    condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); }); 

                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
    }
};

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    // Wake up all thread, and all the thread is waiting to acquire lock, maybe one thread already get the  lock and enter the wait state.
    condition.notify_all();
    for (std::thread& worker : workers)
        worker.join();
}

template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> future<typename result_of<F(Args...)>::type>
{
    // Tell compile it is a type, not a variable.
    using return_type = typename std::result_of<F(Args...)>::type;

    // Lambda will capture the task, make the task alive as long as the lambda exist.
    auto task = make_shared<packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args...>(args)...));

    future<return_type> res = task->get_future();

    {
        unique_lock<mutex> lock(queue_mutex);
        if(stop) throw runtime_error("enqueue on stopped ThreadPool");

        // In C++, a lambda expression can be implicitly converted to a std::function object.
        // Lambda expresion capture the 'task', it will copy the task, add a reference to shared pointer, so the task will exist until the lambda expression been removed. 
        tasks.emplace([task](){
            (*task)();
        });      
    }

    // Notify one thread to process it.
    condition.notify_one();

    return res;
}