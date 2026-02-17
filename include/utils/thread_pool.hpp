#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace bindiff {

// ============== 线程池 ==============

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 0);  // 0 = auto
    ~ThreadPool();
    
    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // 提交任务
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // 等待所有任务完成
    void wait();
    
    // 获取线程数
    size_t size() const { return workers_.size(); }
    
    // 获取活跃任务数
    size_t active_tasks() const { return active_tasks_.load(); }
    
    // 停止
    void stop();

private:
    void worker_thread();
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    size_t total_tasks_{0};
};

// ============== 模板实现 ==============

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool已停止");
        }
        tasks_.emplace([task]() { (*task)(); });
        total_tasks_++;
    }
    cv_.notify_one();
    
    return result;
}

} // namespace bindiff
