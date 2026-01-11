#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks; // void (*task)();
  std::mutex queue_mutex;
  bool stop;
  void WorkerLoop() {
    while (true) {
      std::function<void()> task;
      bool found = false;
      {
        std::lock_guard<std::mutex> lock(this->queue_mutex);

        if (this->stop && this->tasks.empty())
          return;

        if (!this->tasks.empty()) {
          // luam functia cu prioritate maxima
          task = std::move(this->tasks.front());
          this->tasks.pop();
          found = true;
        }
      }
      if (found) {
        task();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

public:
  ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
      // spune explicit ca va executa doar workerLoop si ii dam obiectul pentru
      // a avea acces la tasks
      workers.emplace_back(&ThreadPool::WorkerLoop, this);
    }
  }

  void Enqueue(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (!stop) {
      tasks.push(task);
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      stop = true;
    }
    // un wait pentru threaduri, asteptam ca munca sa se opreasca
    for (std::thread &worker : workers) {
      if (worker.joinable())
        worker.join();
    }
  }
};