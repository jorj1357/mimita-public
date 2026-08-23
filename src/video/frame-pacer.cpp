#include "frame-pacer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

#include <chrono>

FramePacer* ScopedFrameTimer::sPacer = nullptr;

uint64_t FramePacer::nowUs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}

float FramePacer::targetFrameTimeMs() const
{
    if (mMaxFrames <= 0) return 0.0f;
    return 1000.0f / (float)mMaxFrames;
}

float FramePacer::p99FrameTimeMs() const
{
    return mP99Ms;
}

void FramePacer::beginFrame()
{
    mSubsystemCount = 0;

    uint64_t now = nowUs();

    if (mFrameStartUs != 0)
        mLastFrameUs = now - mFrameStartUs;
    else
        mLastFrameUs = 0;

    mFrameStartUs = now;

    mDt = (float)((double)mLastFrameUs / 1000000.0);
    if (mDt < 0.0001f) mDt = 0.0001f;
    if (mDt > 0.1f)    mDt = 0.1f;

    mLastFrameMs = (float)((double)mLastFrameUs / 1000.0);
}

void FramePacer::endFrame()
{
    mFrameEndUs = nowUs();
    uint64_t elapsedUs = mFrameEndUs - mFrameStartUs;

    float targetMs = targetFrameTimeMs();
    if (targetMs > 0.0f && !mVSync)
    {
        uint64_t targetUs = (uint64_t)(targetMs * 1000.0f);
        if (elapsedUs < targetUs)
        {
            uint64_t remainingUs = targetUs - elapsedUs;
            if (remainingUs > 1000)
            {
                uint64_t sleepUs = remainingUs - 500;
                std::this_thread::sleep_for(
                    std::chrono::microseconds(sleepUs));
            }
            // Removed spin-wait: the sleep above is sufficient for frame pacing.
            // The spin-wait burned CPU and caused GPU timing interference.
        }
    }

    updateStats();

    // Update 300-sample history copy for external access
    if (mHistoryCount < MAX_HISTORY)
        mHistoryCopy[mHistoryCount++] = mLastFrameMs;
    else
    {
        for (int i = 1; i < MAX_HISTORY; ++i)
            mHistoryCopy[i - 1] = mHistoryCopy[i];
        mHistoryCopy[MAX_HISTORY - 1] = mLastFrameMs;
    }

    // Hitch detection
    if (mHitchDebug && targetMs > 0.0f)
    {
        float hitchThreshold = targetMs * 1.5f;
        if (mLastFrameMs > hitchThreshold)
        {
            printf("[FRAME HITCH] frame=%d  total=%.2fms  (threshold=%.2fms, target=%.2fms)\n",
                   mFrameCount, mLastFrameMs, hitchThreshold, targetMs);
            for (int i = 0; i < mSubsystemCount; ++i)
            {
                if (mSubsystems[i].name)
                    printf("  %s=%.2f\n", mSubsystems[i].name, (double)mSubsystems[i].totalUs / 1000.0);
            }
        }
    }
}

void FramePacer::beginSubsystem(const char* name)
{
    if (mSubsystemCount >= MAX_SUBSYSTEMS) return;
    mSubsystems[mSubsystemCount].name = name;
    mSubsystems[mSubsystemCount].startUs = nowUs();
    mSubsystems[mSubsystemCount].totalUs = 0;
    mSubsystemCount++;
}

void FramePacer::endSubsystem()
{
    uint64_t now = nowUs();
    for (int i = mSubsystemCount - 1; i >= 0; --i)
    {
        if (mSubsystems[i].startUs != 0)
        {
            mSubsystems[i].totalUs += now - mSubsystems[i].startUs;
            mSubsystems[i].startUs = 0;
            return;
        }
    }
}

const char* FramePacer::subsystemName(int i) const
{
    return i < mSubsystemCount ? mSubsystems[i].name : nullptr;
}

double FramePacer::subsystemTimeMs(int i) const
{
    return i < mSubsystemCount ? (double)mSubsystems[i].totalUs / 1000.0 : 0.0;
}

void FramePacer::setMaxFrames(int fps)
{
    mMaxFrames = std::clamp(fps, 10, 999);
    printf("[FRAME PACER] maxFrames=%d (target=%.2f ms)\n",
           mMaxFrames, targetFrameTimeMs());
}

void FramePacer::setVSync(bool on)
{
    mVSync = on;
    printf("[FRAME PACER] vsync=%s\n", on ? "ON" : "OFF");
}

void FramePacer::updateStats()
{
    float ms = mLastFrameMs;

    if (mHistorySum == 0.0f)
    {
        for (int i = 0; i < HISTORY_SIZE; ++i)
            mHistory[i] = ms;
        mHistorySum = ms * HISTORY_SIZE;
    }
    else
    {
        mHistorySum -= mHistory[mHistoryIdx];
        mHistory[mHistoryIdx] = ms;
        mHistorySum += ms;
    }
    mHistoryIdx = (mHistoryIdx + 1) % HISTORY_SIZE;
    mAvgFrameMs = mHistorySum / HISTORY_SIZE;

    if (mFrameCount % HISTORY_SIZE == 0)
    {
        mMinFrameMs = ms;
        mMaxFrameMs = ms;
    }
    else
    {
        if (ms < mMinFrameMs) mMinFrameMs = ms;
        if (ms > mMaxFrameMs) mMaxFrameMs = ms;
    }

    mLastVarianceMs = std::fabs(ms - mAvgFrameMs);

    if (mFrameCount % 60 == 0 && mFrameCount > 0)
    {
        std::copy(mHistory, mHistory + HISTORY_SIZE, mSortedHistory);
        std::sort(mSortedHistory, mSortedHistory + HISTORY_SIZE);
        int idx99 = (int)(HISTORY_SIZE * 0.99f);
        if (idx99 >= HISTORY_SIZE) idx99 = HISTORY_SIZE - 1;
        mP99Ms = mSortedHistory[idx99];
    }

    ++mFrameCount;

    if (mShowFPS)
    {
        int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
        int avgFps = mAvgFrameMs > 0.0f ? (int)(1000.0f / mAvgFrameMs + 0.5f) : 0;

        if (mFrameDebug)
        {
            snprintf(mFpsText, sizeof(mFpsText),
                     "FPS: %d  AVG: %d", fps, avgFps);
            snprintf(mDebugText, sizeof(mDebugText),
                     "FRAME: %.2fms  TARGET: %.2fms  VAR: %.2fms\n"
                     "MIN: %.2fms  MAX: %.2fms  P99: %.2fms",
                     ms, targetFrameTimeMs(), mLastVarianceMs,
                     mMinFrameMs, mMaxFrameMs, mP99Ms);
        }
        else
        {
            snprintf(mFpsText, sizeof(mFpsText),
                     "FPS: %d  AVG: %d\nFRAME: %.2fms",
                     fps, avgFps, ms);
            mDebugText[0] = '\0';
        }
    }
}
