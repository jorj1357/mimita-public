#include "replay-factory-worker.h"

#include <cstdio>
#include <functional>
#include <thread>

ReplaySaveWorker::ReplaySaveWorker()
{
    mThread = std::thread(&ReplaySaveWorker::threadLoop, this);
}

ReplaySaveWorker::~ReplaySaveWorker()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mShutdown = true;
    }
    mCv.notify_one();
    if (mThread.joinable())
        mThread.join();
}

void ReplaySaveWorker::enqueue(std::function<void()> job)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mJobs.push(std::move(job));
    }
    mCv.notify_one();
}

void ReplaySaveWorker::drain()
{
    // Wait until the queue is empty
    std::unique_lock<std::mutex> lock(mMutex);
    mCv.wait(lock, [this] { return mJobs.empty(); });
}

void ReplaySaveWorker::threadLoop()
{
    {
        printf("[REPLAY WORKER] thread=%zx started\n",
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }

    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCv.wait(lock, [this] {
                return mShutdown || !mJobs.empty();
            });
            if (mShutdown && mJobs.empty())
                return;
            job = std::move(mJobs.front());
            mJobs.pop();
        }
        // Notify drain waiters
        mCv.notify_all();
        if (job) {
            try {
                job();
            } catch (const std::exception& e) {
                printf("[REPLAY WORKER] job threw exception: %s\n", e.what());
            } catch (...) {
                printf("[REPLAY WORKER] job threw unknown exception\n");
            }
        }
    }
}
