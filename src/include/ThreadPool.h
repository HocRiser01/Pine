#pragma once
#include <condition_variable>  // NOLINT
#include <functional>
#include <future>  // NOLINT
#include <memory>
#include <mutex>  // NOLINT
#include <queue>
#include <thread>  // NOLINT
#include <utility>
#include <vector>
#include "common.h"

class ThreadPool {
 public:
  DISALLOW_COPY_AND_MOVE(ThreadPool);
  explicit ThreadPool(unsigned int size = std::thread::hardware_concurrency());
  ~ThreadPool();

  template <class F, class... Args>
  auto Add(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>;

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  std::atomic<bool> stop_{false};
};

// 不能放在cpp文件，C++编译器不支持模版的分离编译
template <class F, class... Args>
auto ThreadPool::Add(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type> {
  using return_type = typename std::invoke_result<F, Args...>::type;

  auto task =
      std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mtx_);

    // don't allow enqueueing after stopping the pool
    if (stop_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    tasks_.emplace([task](){ (*task)(); });
  }
  queue_cv_.notify_one();
  return res;
}
