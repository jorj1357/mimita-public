// 08 27 2026, 14 00
/* purpose
* Replay recording, playback, and export system.
* Records actor state for Blender export and in-engine playback.
* fill in 3rd line
* fill in what this file DOES NOT do
* fill in 2nd line
* fill in 3rd line
*/

#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "input/input-frame.h"

#include "replay-scene.h"

class Player;
struct WeaponViewModel;

class Player;
class Camera;

enum class ReplayState
{
    None,
    ReplayMenu,
    LoadingReplay,
    WatchingReplay,
    PausedReplay,
    ExitingReplay
};

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
    std::string weaponId;
    uint32_t killTick = 0;
    float killDistance = 0.0f;
    bool roundWinning = false;
    std::vector<ReplayFrame> frames;
    std::vector<ReplaySceneFrame> sceneFrames;
    std::vector<ReplaySoundEvent> soundEvents;
    std::vector<ReplayKillfeedEvent> killfeedEvents;
    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

// ── Fixed-capacity circular buffer for replay frames ─────────
// Reuses frame slots to eliminate per-tick heap allocation.
// After warmup, zero allocations for ordinary recording.
static constexpr uint32_t REPLAY_RING_CAPACITY = 3600; // 60 sec * 60 Hz

class ReplayRecorder {
public:
    void beginRecording(float randomSeed, const char* mapName);
    void recordFrame(const InputFrame& frame);
    void recordSceneFrame(ReplaySceneFrame frame);
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
    void recordKillfeedEvent(const ReplayKillfeedEvent& event);
    void setMaxTicks(uint32_t maxTicks) { mMaxTicks = maxTicks; }
    ReplayClip makeClip(uint32_t startTick, uint32_t endTick, uint32_t killTick,
                        const std::string& killerId, const std::string& victimId) const;
    bool isRecording() const {
        return mRecording;
    }

    uint32_t currentTick() const {
        return mTick;
    }

    bool exportToJSON(const std::string& path) const;
    bool exportToBinary(const std::string& path) const;

    // Circular buffer access: get writable frame slot, then commit
    ReplaySceneFrame& getWritableFrame() {
        return mSceneFrames[mSceneFrameWriteIndex];
    }
    void commitFrame() {
        mSceneFrameWriteIndex = (mSceneFrameWriteIndex + 1) % REPLAY_RING_CAPACITY;
        if (mSceneFrameCount < REPLAY_RING_CAPACITY)
            mSceneFrameCount++;
    }

    const ReplayHeader& header() const {
        return mHeader;
    }

    const std::vector<ReplayFrame>& frames() const {
        return mFrames;
    }

    // Ring buffer accessors
    uint32_t sceneFrameCount() const { return mSceneFrameCount; }
    const ReplaySceneFrame& sceneFrameAt(uint32_t i) const {
        uint32_t idx = (mSceneFrameWriteIndex + i) % REPLAY_RING_CAPACITY;
        return mSceneFrames[idx];
    }

    const std::vector<ReplayAsset>& assets() const {
        return mAssets;
    }

    const std::vector<ReplaySoundEvent>& soundEvents() const {
        return mSoundEvents;
    }

    const std::vector<ReplayKillfeedEvent>& killfeedEvents() const {
        return mKillfeedEvents;
    }

    // Identity table access
    const ReplayActorIdentity* getIdentity(uint32_t actorId) const {
        auto it = mIdentityTable.find(actorId);
        return it != mIdentityTable.end() ? &it->second : nullptr;
    }
    ReplayActorIdentity& getOrCreateIdentity(uint32_t actorId) {
        return mIdentityTable[actorId];
    }
    void removeIdentity(uint32_t actorId) {
        mIdentityTable.erase(actorId);
        mPreviousCompactState.erase(actorId);
    }
    void clearIdentities() {
        mIdentityTable.clear();
        mPreviousCompactState.clear();
    }

    // Delta detection: compare current state with previous, return dirty mask
    uint32_t computeDirtyMask(uint32_t actorId, const ReplayActorTickState& current) const;
    bool hasPreviousState(uint32_t actorId) const {
        return mPreviousCompactState.find(actorId) != mPreviousCompactState.end();
    }
    const ReplayActorTickState* getPreviousState(uint32_t actorId) const {
        auto it = mPreviousCompactState.find(actorId);
        return it != mPreviousCompactState.end() ? &it->second : nullptr;
    }
    void storePreviousState(uint32_t actorId, const ReplayActorTickState& state) {
        mPreviousCompactState[actorId] = state;
    }

    // NPC avatar cache
    const std::string& getCachedNpcAvatar(uint32_t npcId, uint16_t epoch) const;
    void cacheNpcAvatar(uint32_t npcId, uint16_t epoch, const std::string& name);

private:
    mutable std::mutex mRingMutex;
    bool mRecording = false;
    uint32_t mTick = 0;
    uint32_t mEventTick = 0;
    ReplayHeader mHeader{};
    std::vector<ReplayFrame> mFrames;

    // Circular buffer for scene frames — reuses frame slots
    std::array<ReplaySceneFrame, REPLAY_RING_CAPACITY> mSceneFrames{};
    uint32_t mSceneFrameWriteIndex = 0;
    uint32_t mSceneFrameCount = 0;

    std::vector<ReplayAsset> mAssets;
    ReplayWorldMetadata mWorld;
    ReplayLightingState mLighting;
    std::vector<ReplaySoundEvent> mSoundEvents;
    std::vector<ReplayKillfeedEvent> mKillfeedEvents;
    std::vector<ReplayEffectEvent> mPendingEffects;
    uint32_t mMaxTicks = 0;

    // ── Identity table: stores constant strings once per lifetime ──
    // Keys are stable actor IDs (player=0, npc=1000+npc.id, remote=20000+id).
    std::unordered_map<uint32_t, ReplayActorIdentity> mIdentityTable;

    // Previous compact tick state for delta detection (no strings)
    std::unordered_map<uint32_t, ReplayActorTickState> mPreviousCompactState;

    // NPC avatar cache: key = (npcId << 16) | epoch
    std::unordered_map<uint64_t, std::string> mNpcAvatarCache;
};

class ReplayRingBuffer : public ReplayRecorder {
public:
    static constexpr uint32_t TickRate = 60;
    static constexpr uint32_t DurationSeconds = 60;

    ReplayRingBuffer() { setMaxTicks(TickRate * DurationSeconds); }
};

enum class ReplayCameraMode {
    Recorded,
    FirstPerson,
    Victim,
    Orbit,
    Freecam,
    ThirdPerson,
    Spectator,
    TopDown
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
                const std::string& victimId, float dt, uint32_t tick);

private:
    ReplayCameraMode mMode = ReplayCameraMode::Recorded;
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
    void pollPoseInvariant();
    void setTimescale(float value);
    float timescale() const { return mTimescale; }
    ReplayCameraController& cameraController() { return mCameraController; }
    const ReplaySceneFrame* currentSceneFrame() const;
    void takeTriggeredEffects(std::vector<ReplayEffectEvent>& out);
    void takeTriggeredSounds(std::vector<ReplaySoundEvent>& out);
    void takeTriggeredKillfeedEvents(std::vector<ReplayKillfeedEvent>& out);
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
    const std::vector<ReplaySoundEvent>& soundEvents() const { return mClip.soundEvents; }
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
    std::vector<ReplayKillfeedEvent> mTriggeredKillfeedEvents;
    ReplayCameraController mCameraController;
    std::string mOutfitPath;
    std::vector<ReplayAsset> mAssets;

    void rebuildInterpolatedFrameAtTick();

    // Presentation invariant: the authoritative tick may only advance when the
    // presented interpolated frame/pose advances with it. Tracks consecutive
    // stalls so a skipped rebuild (stale mInterpolatedFrame) is reported.
    int mInvariantLastTick = -1;
    int mInvariantLastFrameTick = -1;
    glm::vec3 mInvariantLastPose{0.0f, 0.0f, 0.0f};
    bool mInvariantHadPose = false;
    int mInvariantStallFrames = 0;
};

std::string generateReplayExportPath();
std::string generateReplayValidationPath(const std::string& replayPath);
std::string generateReplayClipPath();
std::string generateInstantReplayPath();
std::vector<std::string> listReplayClips();
std::string saveInstantReplay(ReplayRingBuffer& ring, uint32_t durationSeconds = 15);

void setActiveReplayRecorder(ReplayRecorder* recorder);
void setReplayCaptureEnabled(bool enabled);
void notifyReplayKill(const std::string& killerId,
                      const std::string& victimId,
                      bool roundWinning);

// Callback for ReplayFactory kill notification (set by main.cpp)
using ReplayFactoryNotifyFn = void(*)(const std::string& killerId,
                                       const std::string& victimId,
                                       bool killerAirborne,
                                       bool victimAirborne,
                                       bool roundWinning);
extern ReplayFactoryNotifyFn gReplayFactoryNotifyFn;
void setReplayFactoryNotifyFn(ReplayFactoryNotifyFn fn);

// Global state (defined in replay.cpp, accessible across replay subsystem)
extern ReplayRecorder* gActiveReplayRecorder;
extern bool gReplayCaptureEnabled;

void captureReplayEffect(const ReplayEffectEvent& event);
void captureReplaySound(const ReplaySoundEvent& event);
void captureReplayKillfeed(const ReplayKillfeedEvent& event);

bool ReplayShouldPlayHitmarkerAudio(
    const std::string& attackerId,
    ReplayCameraMode cameraMode,
    const std::string& viewedEntity);

void pollReplayHitmarkerConfig();

// Replay actor/weapon model maps (owned by main.cpp, accessible globally for cleanup)
extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;

struct BodyPartArray {
    std::array<ReplayBodyPartState, ReplayActorState::MAX_BODY_PARTS> parts{};
    uint8_t count = 0;
};
BodyPartArray captureReplayBodyParts(const Player& player);
