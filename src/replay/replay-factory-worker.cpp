#include "replay-factory-worker.h"

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
        if (job)
            job();
    }
}
