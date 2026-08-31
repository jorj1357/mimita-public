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
    bool enqueue(std::function<void()> job);

    // Wait for all queued jobs to complete (for shutdown safety).
    void drain();
    size_t queueDepth() const;

private:
    void threadLoop();

    std::thread mThread;
    mutable std::mutex mMutex;
    std::condition_variable mCv;
    std::queue<std::function<void()>> mJobs;
    size_t mActiveJobs = 0;
    std::atomic<bool> mShutdown{false};
};
