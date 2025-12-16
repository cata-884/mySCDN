#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono> 

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    bool stop;
    void WorkerLoop() {
        while(true) {
            std::function<void()> task;
            bool found = false;
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                
                if (this->stop && this->tasks.empty()) 
                    return;

                if (!this->tasks.empty()) {
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
        for(size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ThreadPool::WorkerLoop, this);
        }
    }

    void Enqueue(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if(!stop) {
            tasks.push(task);
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop = true;
        }
        for(std::thread &worker: workers) {
            if(worker.joinable()) worker.join();
        }
    }
};