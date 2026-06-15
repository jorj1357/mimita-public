#pragma once

#include <cstdint>

// High-precision frame pacer and FPS diagnostics.
// Owns the frame timing loop — not just a sleep helper.
//
// Usage in main loop:
//   pacer.beginFrame();
//   ... game logic ...
//   pacer.endFrame();  // sleeps + busy-waits to meet target
//   float dt = pacer.dt();
//
class FramePacer {
public:
    void beginFrame();
    void endFrame();

    float dt() const { return mDt; }

    // Config (set from terminal commands)
    void setMaxFrames(int fps);   // 10–999
    int  maxFrames() const { return mMaxFrames; }

    void setVSync(bool on);
    bool vsync() const { return mVSync; }

    // Diagnostics
    void setShowFPS(bool on)      { mShowFPS = on; }
    bool showFPS() const          { return mShowFPS; }

    void setFrameDebug(bool on)   { mFrameDebug = on; }
    bool frameDebug() const       { return mFrameDebug; }

    // Stats (valid after every endFrame)
    float frameTimeMs() const     { return mLastFrameMs; }
    float avgFrameTimeMs() const  { return mAvgFrameMs; }
    float targetFrameTimeMs() const;
    float minFrameTimeMs() const  { return mMinFrameMs; }
    float maxFrameTimeMs() const  { return mMaxFrameMs; }
    float p99FrameTimeMs() const;
    float varianceMs() const      { return mLastVarianceMs; }

    // Display text (populated each frame if showFPS is on)
    const char* fpsText() const   { return mFpsText; }

    // Diagnostics text
    const char* debugText() const { return mDebugText; }

private:
    int  mMaxFrames = 999;
    bool mVSync = false;
    bool mShowFPS = false;
    bool mFrameDebug = false;

    float mDt = 0.0f;

    // Steady-clock timestamps (microseconds)
    uint64_t mFrameStartUs = 0;
    uint64_t mFrameEndUs = 0;
    uint64_t mLastFrameUs = 0;

    float mLastFrameMs = 0.0f;

    // Rolling average (ring buffer of last 120 samples)
    static constexpr int HISTORY_SIZE = 120;
    float mHistory[HISTORY_SIZE] = {};
    int   mHistoryIdx = 0;
    float mHistorySum = 0.0f;

    float mAvgFrameMs = 0.0f;
    float mMinFrameMs = 1000.0f;
    float mMaxFrameMs = 0.0f;
    float mLastVarianceMs = 0.0f;
    int   mFrameCount = 0;

    // P99: sorted history copy (computed every 60 frames)
    float mSortedHistory[HISTORY_SIZE] = {};
    float mP99Ms = 0.0f;

    char mFpsText[64] = {};
    char mDebugText[128] = {};

    static uint64_t nowUs();
    void updateStats();
};
