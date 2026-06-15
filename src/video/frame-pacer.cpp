#include "frame-pacer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

#include <chrono>

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
    uint64_t now = nowUs();

    if (mFrameStartUs != 0)
        mLastFrameUs = now - mFrameStartUs;
    else
        mLastFrameUs = 0;

    mFrameStartUs = now;

    // Compute dt in seconds from last frame time
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

            // Sleep for most of remaining time (leave ~0.5ms for spin wait)
            if (remainingUs > 1000)
            {
                uint64_t sleepUs = remainingUs - 500;
                std::this_thread::sleep_for(
                    std::chrono::microseconds(sleepUs));
            }

            // Busy-wait for the final fraction
            while (nowUs() - mFrameStartUs < targetUs)
            {
                // spin
            }
        }
    }

    updateStats();
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

    // Rolling history
    if (mHistorySum == 0.0f)
    {
        // First entry: fill buffer with this value
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

    // Min / max (reset every HISTORY_SIZE frames)
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

    // Variance = |current - average|
    mLastVarianceMs = std::fabs(ms - mAvgFrameMs);

    // P99: sort history every 60 frames
    if (mFrameCount % 60 == 0 && mFrameCount > 0)
    {
        std::copy(mHistory, mHistory + HISTORY_SIZE, mSortedHistory);
        std::sort(mSortedHistory, mSortedHistory + HISTORY_SIZE);
        int idx99 = (int)(HISTORY_SIZE * 0.99f);
        if (idx99 >= HISTORY_SIZE) idx99 = HISTORY_SIZE - 1;
        mP99Ms = mSortedHistory[idx99];
    }

    ++mFrameCount;

    // Build display text
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
