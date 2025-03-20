#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include "../logging/logging.h"

class ThreadPool {
public:
    // Get the singleton instance of the ThreadPool
    static ThreadPool& getInstance(size_t numThreads = std::thread::hardware_concurrency()) {
        uint64_t numThreads2 = numThreads / 4;
        numThreads2 *= 3;
        static ThreadPool instance(numThreads);
        return instance;
    }

    // Add a task to the task queue
    void enqueueTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    // Explicit shutdown function (optional)
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    // Constructor is private to enforce singleton pattern
    ThreadPool(size_t numThreads) : stop(false) {
        if (numThreads <= 3) {
            numThreads = 4; // Set a default number of threads if hardware_concurrency returns <= 1
        }

        logging::print("Initializing ThreadPool with", std::to_string(numThreads) + " threads.");

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    // Deleted copy constructor and assignment operator to ensure singleton
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Destructor to clean up threads (only when shutdown is called)
    ~ThreadPool() {
        shutdown();
    }

    // Vector to hold worker threads
    std::vector<std::thread> workers;

    // Task queue
    std::queue<std::function<void()>> tasks;

    // Synchronization
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;

    // Worker thread function
    void workerThread() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this] { return stop || !tasks.empty(); });
                if (stop && tasks.empty()) {
                    return;
                }
                task = std::move(tasks.front());
                tasks.pop();
            }
            try {
                task();
            } catch (const std::exception &e) {
                std::cerr << "Exception in task: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in task" << std::endl;
            }
        }
    }
};

class ValidatorThreadPool {
public:
    // Get the singleton instance of the ThreadPool
    static ValidatorThreadPool& getInstance(size_t numThreads = std::thread::hardware_concurrency()) {
        uint64_t numThreads2 = numThreads / 4;
        static ValidatorThreadPool instance(numThreads);
        return instance;
    }

    // Add a task to the task queue
    void enqueueTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    // Explicit shutdown function (optional)
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    // Constructor is private to enforce singleton pattern
    ValidatorThreadPool(size_t numThreads) : stop(false) {
        if (numThreads <= 3) {
            numThreads = 4; // Set a default number of threads if hardware_concurrency returns <= 1
        }
        logging::print("Initializing ValidatorThreadPool with", std::to_string(numThreads) + " threads.");
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ValidatorThreadPool::workerThread, this);
        }
    }

    // Deleted copy constructor and assignment operator to ensure singleton
    ValidatorThreadPool(const ValidatorThreadPool&) = delete;
    ValidatorThreadPool& operator=(const ValidatorThreadPool&) = delete;

    // Destructor to clean up threads (only when shutdown is called)
    ~ValidatorThreadPool() {
        shutdown();
    }

    // Vector to hold worker threads
    std::vector<std::thread> workers;

    // Task queue
    std::queue<std::function<void()>> tasks;

    // Synchronization
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;

    // Worker thread function
    void workerThread() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this] { return stop || !tasks.empty(); });
                if (stop && tasks.empty()) {
                    return;
                }
                task = std::move(tasks.front());
                tasks.pop();
            }
            try {
                task();
            } catch (const std::exception &e) {
                std::cerr << "Exception in task: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in task" << std::endl;
            }
        }
    }
};