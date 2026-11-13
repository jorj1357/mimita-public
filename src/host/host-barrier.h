// 2026-11-12
// Purpose: resident generation barrier for one replaceable game.dll.
//
// A swap must stop new calls and task submissions, unregister callbacks, cancel
// or drain and join jobs, and wait until in-flight calls reach zero before the
// old library is freed. The barrier returns an explicit result so the loader can
// keep the previous generation active on BUSY or ERROR.
//
// Header-only scaffolding: it is not wired into the EXE yet and adds no
// translation unit. The host owns one barrier for the whole process.
//
// Protocol:
//   beginGeneration(g)                  // new generation accepting work
//   enter(g) / leave()                  // guard every host->game call
//   registerCallback / unregisterCallback
//   submitJob                           // generation-stamped, runs on the barrier worker
//   beginQuiesce(mode)                  // refuse new work, clear callbacks
//   finishQuiesce(timeoutMs) -> result  // Ok only when in-flight == 0 and jobs idle
//   commit or reject                    // loader decision

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

class GenerationBarrier {
public:
    using CallbackToken = std::uint64_t;
    using JobId = std::uint64_t;

    GenerationBarrier() = default;

    ~GenerationBarrier() { shutdown(); }

    GenerationBarrier(const GenerationBarrier&) = delete;
    GenerationBarrier& operator=(const GenerationBarrier&) = delete;

    // Start accepting work for a new generation. Clears callbacks and pending
    // jobs from the previous generation.
    void beginGeneration(std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mMutex);
        mGeneration.store(generation, std::memory_order_release);
        mCallbacks.clear();
        mPending.clear();
        mNextCallback = 1;
        mNextJob = 1;
        mQuiescing.store(false, std::memory_order_release);
        mAccepting.store(true, std::memory_order_release);
        mStop = false;
        ensureWorker();
    }

    std::uint64_t generation() const {
        return mGeneration.load(std::memory_order_acquire);
    }

    // Guard a host->game call. Returns false when the generation is stale, the
    // barrier is quiescing, or the process is stopping.
    bool enter(std::uint64_t generation) {
        if (!mAccepting.load(std::memory_order_acquire)) {
            return false;
        }
        if (generation != mGeneration.load(std::memory_order_acquire)) {
            return false;
        }
        mInFlight.fetch_add(1, std::memory_order_acq_rel);
        if (!mAccepting.load(std::memory_order_acquire) ||
            generation != mGeneration.load(std::memory_order_acquire)) {
            leave();
            return false;
        }
        return true;
    }

    void leave() {
        if (mInFlight.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(mMutex);
            mIdleCv.notify_all();
        }
    }

    std::uint64_t inFlight() const {
        return mInFlight.load(std::memory_order_acquire);
    }

    CallbackToken registerCallback(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mAccepting.load(std::memory_order_acquire) || !callback) {
            return 0;
        }
        const CallbackToken token = mNextCallback++;
        mCallbacks.emplace(token, std::move(callback));
        return token;
    }

    void unregisterCallback(CallbackToken token) {
        std::lock_guard<std::mutex> lock(mMutex);
        mCallbacks.erase(token);
    }

    // Invoke one registered callback under the entry guard. Returns false if the
    // token is unknown or the generation is no longer accepting calls.
    bool invokeCallback(CallbackToken token) {
        std::function<void()> callback;
        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            const auto found = mCallbacks.find(token);
            if (found == mCallbacks.end()) {
                return false;
            }
            callback = found->second;
            generation = mGeneration.load(std::memory_order_acquire);
        }
        if (!enter(generation)) {
            return false;
        }
        callback();
        leave();
        return true;
    }

    // Submit a generation-stamped job. Returns 0 when the barrier is quiescing.
    JobId submitJob(std::function<void()> job) {
        if (!job) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mAccepting.load(std::memory_order_acquire)) {
            return 0;
        }
        const JobId id = mNextJob++;
        mPending.push_back(Job{id, mGeneration.load(std::memory_order_acquire), std::move(job)});
        mJobCv.notify_one();
        return id;
    }

    std::uint64_t pendingJobs() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return static_cast<std::uint64_t>(mPending.size());
    }

    std::uint64_t runningJobs() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mRunning;
    }

    void cancelPendingJobs() {
        std::lock_guard<std::mutex> lock(mMutex);
        mPending.clear();
    }

    void beginQuiesce(QuiesceMode /*mode*/) {
        std::lock_guard<std::mutex> lock(mMutex);
        mAccepting.store(false, std::memory_order_release);
        mQuiescing.store(true, std::memory_order_release);
        // Keep callbacks registered until a successful commit. If quiesce
        // fails, resumeGeneration() makes the old generation usable again.
        mPending.clear();
        mJobCv.notify_all();
    }

    // Resume the current generation after a failed quiesce/load. Callbacks that
    // were registered before beginQuiesce remain valid.
    void resumeGeneration() {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mStop) {
            return;
        }
        mAccepting.store(true, std::memory_order_release);
        mQuiescing.store(false, std::memory_order_release);
    }

    // Ok only when no call, callback, or job can still enter the generation.
    // timeoutMs <= 0 waits indefinitely; a positive value returns Busy on
    // timeout, keeping the caller free to retain the old generation.
    HostResult finishQuiesce(int timeoutMs = 0) {
        cancelPendingJobs();
        std::unique_lock<std::mutex> lock(mMutex);
        const auto idle = [this] {
            return mInFlight.load(std::memory_order_acquire) == 0 && mRunning == 0 &&
                   mPending.empty();
        };
        if (timeoutMs > 0) {
            if (!mIdleCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), idle)) {
                return HostResult::Busy;
            }
        } else {
            mIdleCv.wait(lock, idle);
        }
        return HostResult::Ok;
    }

    bool quiescing() const {
        return mQuiescing.load(std::memory_order_acquire);
    }

    bool accepting() const {
        return mAccepting.load(std::memory_order_acquire);
    }

    void shutdown() {
        beginQuiesce(QuiesceMode::Shutdown);
        finishQuiesce(0);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallbacks.clear();
            mStop = true;
            mAccepting.store(false, std::memory_order_release);
            mJobCv.notify_all();
        }
        if (mWorker.joinable()) {
            mWorker.join();
        }
    }

private:
    struct Job {
        JobId id = 0;
        std::uint64_t generation = 0;
        std::function<void()> callback;
    };

    // The worker starts only when a generation begins, so an idle host that
    // never enables the probe creates no thread.
    void ensureWorker() {
        if (!mWorker.joinable()) {
            mWorker = std::thread([this] { workerLoop(); });
        }
    }

    void workerLoop() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mJobCv.wait(lock, [this] { return mStop || !mPending.empty(); });
                if (mStop && mPending.empty()) {
                    return;
                }
                job = std::move(mPending.front());
                mPending.pop_front();
                ++mRunning;
            }

            const bool runnable =
                job.generation == mGeneration.load(std::memory_order_acquire) &&
                mAccepting.load(std::memory_order_acquire);
            if (runnable && job.callback) {
                job.callback();
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mRunning > 0) {
                    --mRunning;
                }
                if (mRunning == 0 && mPending.empty()) {
                    mIdleCv.notify_all();
                }
            }
        }
    }

    mutable std::mutex mMutex;
    std::condition_variable mIdleCv;
    std::condition_variable mJobCv;
    std::thread mWorker;

    std::atomic<std::uint64_t> mGeneration{0};
    std::atomic<std::uint64_t> mInFlight{0};
    std::atomic<bool> mAccepting{true};
    std::atomic<bool> mQuiescing{false};
    bool mStop = false;
    std::uint64_t mRunning = 0;

    std::uint64_t mNextCallback = 1;
    std::uint64_t mNextJob = 1;
    std::unordered_map<CallbackToken, std::function<void()>> mCallbacks;
    std::deque<Job> mPending;
};

inline GenerationBarrier& hostBarrier() {
    static GenerationBarrier barrier;
    return barrier;
}

}  // namespace mimita::host
