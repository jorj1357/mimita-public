#pragma once

#include <cstdint>

class FramePacer {
public:
    void beginFrame();
    void endFrame();

    float dt() const { return mDt; }

    void setMaxFrames(int fps);
    int  maxFrames() const { return mMaxFrames; }

    void setVSync(bool on);
    bool vsync() const { return mVSync; }

    void setShowFPS(bool on)      { mShowFPS = on; }
    bool showFPS() const          { return mShowFPS; }

    void setFrameDebug(bool on)   { mFrameDebug = on; }
    bool frameDebug() const       { return mFrameDebug; }

    void setHitchDebug(bool on)   { mHitchDebug = on; }
    bool hitchDebug() const       { return mHitchDebug; }

    float frameTimeMs() const     { return mLastFrameMs; }
    float avgFrameTimeMs() const  { return mAvgFrameMs; }
    float targetFrameTimeMs() const;
    float minFrameTimeMs() const  { return mMinFrameMs; }
    float maxFrameTimeMs() const  { return mMaxFrameMs; }
    float p99FrameTimeMs() const;
    float varianceMs() const      { return mLastVarianceMs; }

    const char* fpsText() const   { return mFpsText; }
    const char* debugText() const { return mDebugText; }

    int frameCount() const        { return mFrameCount; }

    // Subsystem timing (always active)
    void beginSubsystem(const char* name);
    void endSubsystem();
    int  subsystemCount() const   { return mSubsystemCount; }
    const char* subsystemName(int i) const;
    double subsystemTimeMs(int i) const;

    // Frame time history (300 samples for graph)
    static constexpr int MAX_HISTORY = 300;
    int  historyCount() const     { return mHistoryCount < MAX_HISTORY ? mHistoryCount : MAX_HISTORY; }
    float historyMs(int i) const  { return i < MAX_HISTORY ? mHistoryCopy[i] : 0.0f; }

private:
    int  mMaxFrames = 999;
    bool mVSync = false;
    bool mShowFPS = false;
    bool mFrameDebug = false;
    bool mHitchDebug = false;

    float mDt = 0.0f;
    uint64_t mFrameStartUs = 0;
    uint64_t mFrameEndUs = 0;
    uint64_t mLastFrameUs = 0;
    float mLastFrameMs = 0.0f;

    static constexpr int HISTORY_SIZE = 120;
    float mHistory[HISTORY_SIZE] = {};
    int   mHistoryIdx = 0;
    float mHistorySum = 0.0f;
    float mAvgFrameMs = 0.0f;
    float mMinFrameMs = 1000.0f;
    float mMaxFrameMs = 0.0f;
    float mLastVarianceMs = 0.0f;
    int   mFrameCount = 0;
    float mSortedHistory[HISTORY_SIZE] = {};
    float mP99Ms = 0.0f;

    // Subsystem timing
    static constexpr int MAX_SUBSYSTEMS = 16;
    struct SubsystemSlot {
        const char* name = nullptr;
        uint64_t startUs = 0;
        uint64_t totalUs = 0;
    };
    SubsystemSlot mSubsystems[MAX_SUBSYSTEMS];
    int mSubsystemCount = 0;

    char mFpsText[64] = {};
    char mDebugText[128] = {};

    // 300-sample history for graph display
    float mHistoryCopy[MAX_HISTORY] = {};
    int   mHistoryCount = 0;

    static uint64_t nowUs();
    void updateStats();
};

struct ScopedFrameTimer {
    explicit ScopedFrameTimer(const char* name) {
        if (FramePacer* p = sPacer) p->beginSubsystem(name);
    }
    ~ScopedFrameTimer() {
        if (FramePacer* p = sPacer) p->endSubsystem();
    }
    ScopedFrameTimer(const ScopedFrameTimer&) = delete;
    ScopedFrameTimer& operator=(const ScopedFrameTimer&) = delete;

    static FramePacer* sPacer;
};
