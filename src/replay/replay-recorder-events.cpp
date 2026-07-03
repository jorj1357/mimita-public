#include "replay.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "entities/player.h"

using json = nlohmann::json;

void ReplayRecorder::recordEffectEvent(const ReplayEffectEvent& inputEvent)
{
    if (!mRecording) return;
    ReplayEffectEvent event = inputEvent;
    event.spawnTick = (int)mEventTick;
    event.spawnTime = (float)mEventTick / (float)std::max(mHeader.tickRate, 1u);
    mPendingEffects.push_back(event);
    if (!event.texturePath.empty())
        registerAsset("texture:" + event.texturePath, "texture", event.texturePath, {}, {}, "effect");
}

void ReplayRecorder::recordSoundEvent(const ReplaySoundEvent& inputEvent)
{
    if (!mRecording) return;
    ReplaySoundEvent event = inputEvent;
    event.tick = (int)mEventTick;
    mSoundEvents.push_back(event);
    if (mMaxTicks > 0) {
        const int oldestTick = (int)mTick - (int)mMaxTicks;
        while (!mSoundEvents.empty() && mSoundEvents.front().tick < oldestTick)
            mSoundEvents.erase(mSoundEvents.begin());
    }
    registerAsset("sound:" + event.soundPath, "sound", event.soundPath, {}, {}, "audio");
}

void ReplayRecorder::recordKillfeedEvent(const ReplayKillfeedEvent& inputEvent)
{
    if (!mRecording) return;
    ReplayKillfeedEvent event = inputEvent;
    event.tick = (int)mEventTick;
    mKillfeedEvents.push_back(event);
    if (mMaxTicks > 0) {
        const int oldestTick = (int)mTick - (int)mMaxTicks;
        while (!mKillfeedEvents.empty() && mKillfeedEvents.front().tick < oldestTick)
            mKillfeedEvents.erase(mKillfeedEvents.begin());
    }
}
