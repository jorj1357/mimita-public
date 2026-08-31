#include "replay-factory-worker.h"

#include <cstdio>
#include <functional>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "debug/debug-log.h"

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

bool ReplaySaveWorker::enqueue(std::function<void()> job)
{
    constexpr size_t kMaxQueuedJobs = 2;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mShutdown || mJobs.size() >= kMaxQueuedJobs)
            return false;
        mJobs.push(std::move(job));
    }
    mCv.notify_one();
    return true;
}

void ReplaySaveWorker::drain()
{
    // Wait until the queue is empty
    std::unique_lock<std::mutex> lock(mMutex);
    mCv.wait(lock, [this] { return mJobs.empty() && mActiveJobs == 0; });
}

size_t ReplaySaveWorker::queueDepth() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mJobs.size() + mActiveJobs;
}

void ReplaySaveWorker::threadLoop()
{
    {
        printf("[REPLAY WORKER] thread=%zx started\n",
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }

#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    Debug::log(Debug::Category::Replay,
               "[PERF][REPLAY_SAVE] worker priority=below_normal\n");
#else
    Debug::log(Debug::Category::Replay,
               "[PERF][REPLAY_SAVE] worker priority=normal\n");
#endif

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
            ++mActiveJobs;
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
        {
            std::lock_guard<std::mutex> lock(mMutex);
            --mActiveJobs;
        }
        mCv.notify_all();
    }
}
