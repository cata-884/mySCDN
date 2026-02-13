#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::condition_variable cv;
  bool stop;

  void WorkerLoop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock lock(this->queue_mutex);
        this->cv.wait(lock,
                      [this] { return this->stop || !this->tasks.empty(); });
        if (this->stop && this->tasks.empty())
          return;
        task = std::move(this->tasks.front());
        this->tasks.pop();
      }
      task();
    }
  }

public:
  explicit ThreadPool(const size_t numThreads) : stop(false) {
    workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back(&ThreadPool::WorkerLoop, this);
    }
  }

  void Enqueue(std::function<void()> task) {
    {
      std::lock_guard lock(queue_mutex);
      if (!stop)
        tasks.push(std::move(task));
    }
    cv.notify_one();
  }

  ~ThreadPool() {
    {
      std::lock_guard lock(queue_mutex);
      stop = true;
    }
    cv.notify_all();
    for (std::thread &worker : workers) {
      if (worker.joinable())
        worker.join();
    }
  }
};