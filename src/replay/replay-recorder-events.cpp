#include "replay.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "combat/shot-profiler.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"

using json = nlohmann::json;

void ReplayRecorder::recordEffectEvent(const ReplayEffectEvent& inputEvent)
{
    if (!mRecording) return;
    MIMITA_PERF_SCOPE("Replay::Event::Effect");
    std::lock_guard<std::mutex> lock(mRingMutex);
    Perf::state().replayPerf.effectsRecorded++;
    if (gShotProfiler) gShotProfiler->replayEventsCreated++;
    {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->replayRecordMs : nullptr);
        ReplayEffectEvent event = inputEvent;
        event.spawnTick = (int)mEventTick;
        event.spawnTime = (float)mEventTick / (float)std::max(mHeader.tickRate, 1u);
        size_t oldCap = mPendingEffects.capacity();
        {
            MIMITA_PERF_SCOPE("Replay::Event::EffectVectorPush");
            mPendingEffects.push_back(event);
        }
        if (mPendingEffects.capacity() > oldCap) {
            if (gShotProfiler) gShotProfiler->replayVectorGrows++;
            Perf::state().replayPerf.vectorCapacityGrowths++;
        }
        if (!event.texturePath.empty()) {
            MIMITA_PERF_SCOPE("Replay::Event::EffectAssetRegistration");
            registerAsset("texture:" + event.texturePath, "texture", event.texturePath, {}, {}, "effect");
        }
        Debug::logThrottled(Debug::Category::Replay, "replay-effect-recorded", 1.0f,
            "[REPLAY EFFECT] recorded events (latest type=%s tick=%d)\n",
            event.type.c_str(), event.spawnTick);
    }
}

void ReplayRecorder::recordSoundEvent(const ReplaySoundEvent& inputEvent)
{
    if (!mRecording) return;
    MIMITA_PERF_SCOPE("Replay::Event::Sound");
    std::lock_guard<std::mutex> lock(mRingMutex);
    Perf::state().replayPerf.soundsRecorded++;
    ReplaySoundEvent event = inputEvent;
    event.tick = (int)mEventTick;
    {
        MIMITA_PERF_SCOPE("Replay::Event::SoundVectorPush");
        mSoundEvents.push_back(event);
    }
    if (mMaxTicks > 0) {
        const int oldestTick = (int)mTick - (int)mMaxTicks;
        while (!mSoundEvents.empty() && mSoundEvents.front().tick < oldestTick)
            mSoundEvents.erase(mSoundEvents.begin());
    }
    {
        MIMITA_PERF_SCOPE("Replay::Event::SoundAssetRegistration");
        registerAsset("sound:" + event.soundPath, "sound", event.soundPath, {}, {}, "audio");
    }
}

void ReplayRecorder::recordKillfeedEvent(const ReplayKillfeedEvent& inputEvent)
{
    if (!mRecording) return;
    MIMITA_PERF_SCOPE("Replay::Event::Killfeed");
    std::lock_guard<std::mutex> lock(mRingMutex);
    Perf::state().replayPerf.killfeedsRecorded++;
    ReplayKillfeedEvent event = inputEvent;
    event.tick = (int)mEventTick;
    mKillfeedEvents.push_back(event);
    if (mMaxTicks > 0) {
        const int oldestTick = (int)mTick - (int)mMaxTicks;
        while (!mKillfeedEvents.empty() && mKillfeedEvents.front().tick < oldestTick)
            mKillfeedEvents.erase(mKillfeedEvents.begin());
    }
}
