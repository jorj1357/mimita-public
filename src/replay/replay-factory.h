#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <functional>

#include <glm/glm.hpp>

#include "replay.h"

class ReplaySaveWorker;

// ----------------------------------------------------------------
// Highlight classification
// ----------------------------------------------------------------
enum class HighlightType : uint8_t {
    Kill,
    RoundWinningKill,
    MultiKill,
    AirKill,
    LongRangeKill,
    ShotgunOneShot,
    RevengeKill,
    FirstKill,
    LastKill
};

const char* highlightTypeName(HighlightType t);

// ----------------------------------------------------------------
// Clip metadata loaded from the saved .mclip.json header
// ----------------------------------------------------------------
struct ReplayClipInfo {
    std::string path;
    std::string filename;
    std::string mapName;
    std::string killerName;
    std::string victimName;
    std::string weaponName;
    HighlightType highlightType = HighlightType::Kill;
    uint32_t durationTicks = 0;
    uint32_t tickRate = 60;
    uint32_t killTick = 0;
    std::string timestamp;
    float distance = 0.0f;
    bool roundWinning = false;

    float durationSeconds() const {
        return tickRate > 0 ? (float)durationTicks / (float)tickRate : 0.0f;
    }
};

// ----------------------------------------------------------------
// Event track items for timeline
// ----------------------------------------------------------------
enum class ReplayEventType : uint8_t {
    Kill,
    Death,
    WeaponSwitch,
    Chat,
    RoundStart,
    RoundEnd,
    FreezeBegin,
    FreezeEnd,
    Dash,
    Jump,
    Land
};

struct ReplayEventItem {
    ReplayEventType type = ReplayEventType::Kill;
    uint32_t tick = 0;
    std::string label;
    std::string detail;
    glm::vec4 color{1.0f};
};

// ----------------------------------------------------------------
// Scans scene frames and builds a time-line of interesting events
// ----------------------------------------------------------------
std::vector<ReplayEventItem> buildEventTimeline(
    const std::vector<ReplaySceneFrame>& frames,
    const std::vector<ReplaySoundEvent>& sounds);

// ----------------------------------------------------------------
// Scans saved clips directory and returns metadata for each
// ----------------------------------------------------------------
std::vector<ReplayClipInfo> scanSavedClips();

bool loadClipInfo(const std::string& path, ReplayClipInfo& info);

// ----------------------------------------------------------------
// Highlight detection logic
// ----------------------------------------------------------------
struct KillContext {
    uint32_t tick;
    std::string killerId;
    std::string victimId;
    std::string weaponId;
    glm::vec3 killerPos;
    glm::vec3 victimPos;
    float distance;
    bool killerWasAirborne;
    bool victimWasAirborne;
    int killCountLast5Sec;
    uint32_t ticksSinceLastKill;
    bool roundWinning;
};

HighlightType classifyHighlight(const KillContext& ctx);

// ----------------------------------------------------------------
// ReplayBrowser - GUI overlay showing saved clips
// ----------------------------------------------------------------
class ReplayBrowser {
public:
    void refresh();
    void draw();
    bool isOpen() const { return mOpen; }
    void setOpen(bool open) { mOpen = open; }
    void toggle() { mOpen = !mOpen; }

    void setPlayCallback(std::function<void(const std::string&)> cb) {
        mPlayCallback = cb;
    }

private:
    bool mOpen = false;
    std::vector<ReplayClipInfo> mClips;
    int mSelectedIndex = -1;
    float mScrollY = 0.0f;
    std::function<void(const std::string&)> mPlayCallback;
    char mRenameBuffer[128] = {};

    void drawClipCard(int index, const ReplayClipInfo& clip, float x, float& y, float w);
};

// ----------------------------------------------------------------
// ReplayTimeline - GUI overlay during playback
// ----------------------------------------------------------------
class ReplayTimeline {
public:
    void setFrames(const std::vector<ReplaySceneFrame>& frames,
                   const std::vector<ReplaySoundEvent>& sounds);
    void draw(uint32_t currentTick, uint32_t totalTicks);
    bool isSeeking() const { return mSeeking; }
    float seekFraction() const { return mSeekFraction; }
    void setSeekCallback(std::function<void(uint32_t)> cb) { mSeekCallback = cb; }

private:
    std::vector<ReplayEventItem> mEvents;
    bool mSeeking = false;
    float mSeekFraction = 0.0f;
    float mTimelineWidth = 0.0f;
    std::function<void(uint32_t)> mSeekCallback;
};

// ----------------------------------------------------------------
// ReplayClipSaver enhancement: auto-saves all kills with metadata
// ----------------------------------------------------------------
class ReplayFactory {
public:
    explicit ReplayFactory(ReplayRingBuffer& ring);

    // Called every tick
    void update();

    // Called when a kill happens
    void notifyKill(const std::string& killerId,
                    const std::string& victimId,
                    bool killerAirborne,
                    bool victimAirborne,
                    bool roundWinning);

    // Manual save (e.g., F8 key)
    bool saveLastKill(std::string* savedPath = nullptr);
    bool hasLastKill() const { return mLastClip.has_value(); }

    const ReplayClipInfo* lastClipInfo() const {
        return mLastClip ? &mLastClip->info : nullptr;
    }

    // Access the ring buffer
    ReplayRingBuffer& ring() { return mRing; }

private:
    struct PendingClip {
        uint32_t killTick;
        std::string killerId;
        std::string victimId;
        std::string weaponId;
        float distance;
        bool killerWasAirborne;
        bool victimWasAirborne;
        bool roundWinning;
        int killCountInWindow;
        uint32_t ticksSinceLastKill;
        ReplayClipInfo info;
    };

    ReplayRingBuffer& mRing;
    ReplaySaveWorker* mWorker = nullptr;
    std::optional<PendingClip> mLastClip;
    uint32_t mLastKillTick = 0;
    int mKillsLast5Sec = 0;
    float mKillWindowTimer = 0.0f;
    std::string mLastWeaponId;

    void finalizeAndSave(PendingClip& pending);
    std::string generateClipFilename();

public:
    void setWorker(ReplaySaveWorker* worker) { mWorker = worker; }
};
