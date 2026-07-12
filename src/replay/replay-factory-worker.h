#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Single persistent worker for background replay save / prune operations.
// Main thread enqueues jobs; worker executes them sequentially.
class ReplaySaveWorker {
public:
    ReplaySaveWorker();
    ~ReplaySaveWorker();

    // Enqueue a job that runs on the worker thread.
    // The function should not touch mutable game state.
    void enqueue(std::function<void()> job);

    // Wait for all queued jobs to complete (for shutdown safety).
    void drain();

private:
    void threadLoop();

    std::thread mThread;
    std::mutex mMutex;
    std::condition_variable mCv;
    std::queue<std::function<void()>> mJobs;
    std::atomic<bool> mShutdown{false};
};
