#include "replay.h"

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <glm/glm.hpp>

#include "perf/perf-spike.h"

ReplayClip ReplayRecorder::makeClip(
    uint32_t startTick, uint32_t endTick, uint32_t killTick,
    const std::string& killerId, const std::string& victimId) const
{
    MIMITA_PERF_SCOPE("Replay::MakeClip");
    ReplayClip clip;
    clip.header = mHeader;
    clip.header.tickCount = 0;
    clip.mapPath = mWorld.mapPath;
    clip.killerId = killerId;
    clip.victimId = victimId;
    clip.killTick = killTick >= startTick ? killTick - startTick : 0;

    for (const ReplaySceneFrame& frame : mSceneFrames) {
        if ((uint32_t)frame.tick != killTick && (uint32_t)frame.tick < killTick + 2)
            continue;
        if ((uint32_t)frame.tick > killTick + 5) break;
        for (const ReplayActorState& actor : frame.actors) {
            if (actor.id == killerId && !actor.weaponName.empty() && actor.weaponName != "none") {
                clip.weaponId = actor.weaponName;
                break;
            }
        }
        if (!clip.weaponId.empty()) break;
    }

    glm::vec3 killerPos, victimPos;
    bool foundKiller = false, foundVictim = false;
    for (const ReplaySceneFrame& frame : mSceneFrames) {
        if ((uint32_t)frame.tick < killTick || (uint32_t)frame.tick > killTick + 5)
            continue;
        for (const ReplayActorState& actor : frame.actors) {
            if (actor.id == killerId) { killerPos = actor.position; foundKiller = true; }
            if (actor.id == victimId) { victimPos = actor.position; foundVictim = true; }
        }
        if (foundKiller && foundVictim) break;
    }
    if (foundKiller && foundVictim)
        clip.killDistance = glm::length(killerPos - victimPos);

    for (const ReplayFrame& source : mFrames) {
        if (source.tick < startTick || source.tick > endTick)
            continue;
        ReplayFrame frame = source;
        frame.tick -= startTick;
        clip.frames.push_back(std::move(frame));
    }
    for (const ReplaySceneFrame& source : mSceneFrames) {
        if ((uint32_t)source.tick < startTick || (uint32_t)source.tick > endTick)
            continue;
        ReplaySceneFrame frame = source;
        frame.tick -= (int)startTick;
        frame.time = (float)frame.tick / (float)std::max(clip.header.tickRate, 1u);
        for (ReplayEffectEvent& effect : frame.effects) {
            effect.spawnTick = std::max(0, effect.spawnTick - (int)startTick);
            effect.spawnTime = (float)effect.spawnTick /
                (float)std::max(clip.header.tickRate, 1u);
        }
        clip.sceneFrames.push_back(std::move(frame));
    }
    for (const ReplaySoundEvent& source : mSoundEvents) {
        if ((uint32_t)source.tick < startTick || (uint32_t)source.tick > endTick)
            continue;
        ReplaySoundEvent sound = source;
        sound.tick -= (int)startTick;
        clip.soundEvents.push_back(std::move(sound));
    }
    for (const ReplayKillfeedEvent& source : mKillfeedEvents) {
        if ((uint32_t)source.tick < startTick || (uint32_t)source.tick > endTick)
            continue;
        ReplayKillfeedEvent kf = source;
        kf.tick -= (int)startTick;
        clip.killfeedEvents.push_back(std::move(kf));
    }
    clip.header.tickCount = (uint32_t)clip.sceneFrames.size();
    return clip;
}

void ReplayRecorder::stopRecording() {
    if (!mRecording) return;
    if (!mPendingEffects.empty()) {
        ReplaySceneFrame finalFrame;
        finalFrame.tick = (int)mTick;
        finalFrame.time = (float)mTick / (float)std::max(mHeader.tickRate, 1u);
        finalFrame.effects = std::move(mPendingEffects);
        mSceneFrames.push_back(std::move(finalFrame));
        mPendingEffects.clear();
    }
    mRecording = false;
    if (gActiveReplayRecorder == this)
        setActiveReplayRecorder(nullptr);
    mHeader.tickCount = (uint32_t)mFrames.size();
    printf("[REPLAY] Recording stopped  ticks=%u\n", mHeader.tickCount);
}
