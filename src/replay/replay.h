// C:\important\mimita-priv-v8\src\replay\replay.h
// 6 7 2026
/** purpose
 * fragmovies
 */

#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>
#include <string>
#include "input/input-frame.h"

#include "replay-scene.h"

#include <string>

class Player;
class Camera;

struct ReplayFrame {
    uint32_t tick = 0;
    InputFrame inputs;
};

struct ReplayHeader {
    char magic[8] = {'M', 'I', 'R', 'P', 'L', 'A', 'Y', 0};
    uint32_t version = 1;
    uint32_t tickCount = 0;
    uint32_t tickRate = 60;
    float randomSeed = 0.0f;
    char mapName[64] = {};
    uint64_t timestamp = 0;
    char playerName[32] = {};
};

struct ReplayClip {
    ReplayHeader header{};
    std::string mapPath;
    std::string killerId;
    std::string victimId;
    uint32_t killTick = 0;
    std::vector<ReplayFrame> frames;
    std::vector<ReplaySceneFrame> sceneFrames;
    std::vector<ReplaySoundEvent> soundEvents;

    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

class ReplayRecorder {
public:
    void beginRecording(float randomSeed, const char* mapName);

    void recordFrame(const InputFrame& frame);

    void recordSceneFrame(const ReplaySceneFrame& frame);

    void stopRecording();

    void registerAsset(
        const std::string& id,
        const std::string& type,
        const std::string& path,
        const std::vector<ReplayMaterialReference>& materials = {},
        const std::string& shaderName = {},
        const std::string& source = {});

    void setWorldMetadata(const ReplayWorldMetadata& world);
    void setLighting(const ReplayLightingState& lighting);
    void recordEffectEvent(const ReplayEffectEvent& event);
    void recordSoundEvent(const ReplaySoundEvent& event);
    void setMaxTicks(uint32_t maxTicks) { mMaxTicks = maxTicks; }
    ReplayClip makeClip(uint32_t startTick, uint32_t endTick,
                        uint32_t killTick,
                        const std::string& killerId,
                        const std::string& victimId) const;

    bool isRecording() const {
        return mRecording;
    }

    uint32_t currentTick() const {
        return mTick;
    }

    bool exportToJSON(const std::string& path) const;

    bool exportToBinary(const std::string& path) const;

    const ReplayHeader& header() const {
        return mHeader;
    }

    const std::vector<ReplayFrame>& frames() const {
        return mFrames;
    }

    const std::vector<ReplaySceneFrame>& sceneFrames() const {
        return mSceneFrames;
    }

    const std::vector<ReplayAsset>& assets() const {
        return mAssets;
    }

    const std::vector<ReplaySoundEvent>& soundEvents() const {
        return mSoundEvents;
    }

private:
    bool mRecording = false;

    uint32_t mTick = 0;
    uint32_t mEventTick = 0;

    ReplayHeader mHeader{};

    std::vector<ReplayFrame> mFrames;

    std::vector<ReplaySceneFrame> mSceneFrames;

    std::vector<ReplayAsset> mAssets;
    ReplayWorldMetadata mWorld;
    ReplayLightingState mLighting;
    std::vector<ReplaySoundEvent> mSoundEvents;
    std::vector<ReplayEffectEvent> mPendingEffects;
    uint32_t mMaxTicks = 0;
};

class ReplayRingBuffer : public ReplayRecorder {
public:
    static constexpr uint32_t TickRate = 60;
    static constexpr uint32_t DurationSeconds = 60;

    ReplayRingBuffer() { setMaxTicks(TickRate * DurationSeconds); }
};

enum class ReplayCameraMode {
    FirstPerson,
    Victim,
    Orbit,
    Freecam
};

class ReplayCameraController {
public:
    bool setMode(const std::string& name);
    void setFov(float value);
    float fov() const { return mFov; }
    ReplayCameraMode mode() const { return mMode; }
    const char* modeName() const;
    void update(Camera& camera, const ReplaySceneFrame& frame,
                const std::string& killerId,
                const std::string& victimId, float dt);

private:
    ReplayCameraMode mMode = ReplayCameraMode::FirstPerson;
    float mFov = 0.0f;
    float mOrbitAngle = 0.0f;
};

class ReplayPlayer {
public:
    bool loadFromJSON(const std::string& path);
    bool loadFromBinary(const std::string& path);

    void beginPlayback();
    void stopPlayback();
    bool isPlaying() const { return mPlaying; }
    void pause();
    void resume();
    void update(float dt);
    void setTimescale(float value);
    float timescale() const { return mTimescale; }
    ReplayCameraController& cameraController() { return mCameraController; }
    const ReplaySceneFrame* currentSceneFrame() const;
    std::vector<ReplayEffectEvent> takeTriggeredEffects();
    std::vector<ReplaySoundEvent> takeTriggeredSounds();
    const std::string& killerId() const { return mClip.killerId; }
    const std::string& victimId() const { return mClip.victimId; }
    bool isPaused() const { return mPaused; }
    const std::string& outfitPath() const { return mOutfitPath; }
    void setOutfitPath(const std::string& path) { mOutfitPath = path; }
    size_t totalEffectCount() const { return mClip.sceneFrames.empty() ? 0 : mClip.soundEvents.size(); }

    bool getFrameAt(uint32_t tick, InputFrame& out) const;
    const InputFrame* advanceTick();

    void seekToTick(uint32_t tick);
    uint32_t currentTick() const { return mCurrentTick; }
    uint32_t totalTicks() const { return mHeader.tickCount; }

    const ReplayHeader& header() const { return mHeader; }
    const std::vector<ReplayAsset>& assets() const { return mAssets; }
    bool preloadAssets();

private:
    bool mPlaying = false;
    uint32_t mCurrentTick = 0;
    ReplayHeader mHeader{};
    std::vector<ReplayFrame> mFrames;
    ReplayClip mClip;
    ReplaySceneFrame mInterpolatedFrame;
    bool mPaused = false;
    float mPlaybackTick = 0.0f;
    float mTimescale = 1.0f;
    int mLastEventTick = -1;
    std::vector<ReplayEffectEvent> mTriggeredEffects;
    std::vector<ReplaySoundEvent> mTriggeredSounds;
    ReplayCameraController mCameraController;
    std::string mOutfitPath;
    std::vector<ReplayAsset> mAssets;
};

class ReplayClipSaver {
public:
    explicit ReplayClipSaver(ReplayRingBuffer& ring) : mRing(ring) {}

    void notifyKill(const std::string& killerId,
                    const std::string& victimId,
                    bool roundWinning);
    void update();
    bool saveLastKill(std::string* savedPath = nullptr);
    bool hasLastKill() const { return mLastKill.has_value(); }

private:
    struct KillInfo {
        uint32_t tick = 0;
        std::string killerId;
        std::string victimId;
        bool autoSave = false;
        bool saved = false;
    };

    ReplayRingBuffer& mRing;
    std::optional<KillInfo> mLastKill;
};

// outside classes ? 6 7 2026
std::string generateReplayExportPath();
std::string generateReplayValidationPath(const std::string& replayPath);
std::string generateReplayClipPath();
std::vector<std::string> listReplayClips();

void setActiveReplayRecorder(ReplayRecorder* recorder);
void setReplayCaptureEnabled(bool enabled);
void setActiveReplayClipSaver(ReplayClipSaver* saver);
void notifyReplayKill(const std::string& killerId,
                      const std::string& victimId,
                      bool roundWinning);
void captureReplayEffect(const ReplayEffectEvent& event);
void captureReplaySound(const ReplaySoundEvent& event);
std::vector<ReplayBodyPartState> captureReplayBodyParts(const Player& player);
