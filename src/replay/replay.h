// C:\important\mimita-priv-v8\src\replay\replay.h
// 6 7 2026
/** purpose
 * fragmovies
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "input/input-frame.h"

#include "replay-scene.h"

#include <string>

class Player;

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
};

class ReplayPlayer {
public:
    bool loadFromJSON(const std::string& path);
    bool loadFromBinary(const std::string& path);

    void beginPlayback();
    void stopPlayback();
    bool isPlaying() const { return mPlaying; }

    bool getFrameAt(uint32_t tick, InputFrame& out) const;
    const InputFrame* advanceTick();

    void seekToTick(uint32_t tick);
    uint32_t currentTick() const { return mCurrentTick; }
    uint32_t totalTicks() const { return mHeader.tickCount; }

    const ReplayHeader& header() const { return mHeader; }

private:
    bool mPlaying = false;
    uint32_t mCurrentTick = 0;
    ReplayHeader mHeader{};
    std::vector<ReplayFrame> mFrames;
};

// outside classes ? 6 7 2026
std::string generateReplayExportPath();

void setActiveReplayRecorder(ReplayRecorder* recorder);
void captureReplayEffect(const ReplayEffectEvent& event);
void captureReplaySound(const ReplaySoundEvent& event);
std::vector<ReplayBodyPartState> captureReplayBodyParts(const Player& player);
