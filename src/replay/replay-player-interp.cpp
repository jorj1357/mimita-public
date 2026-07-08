#include "replay.h"
#include "replay-editor.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

const ReplayActorState* findActor(
    const ReplaySceneFrame& frame, const std::string& id)
{
    auto it = std::find_if(
        frame.actors.begin(), frame.actors.end(),
        [&id](const ReplayActorState& actor) { return actor.id == id; });
    return it == frame.actors.end() ? nullptr : &*it;
}

namespace {

ReplayBodyPartState mixPart(
    const ReplayBodyPartState& a, const ReplayBodyPartState& b, float t)
{
    ReplayBodyPartState result = a;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

ReplayActorState mixActor(
    const ReplayActorState& a, const ReplayActorState& b, float t)
{
    ReplayActorState result = a;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::mix(a.rotation, b.rotation, t);
    result.velocity = glm::mix(a.velocity, b.velocity, t);
    result.health = t < 0.5f ? a.health : b.health;
    result.shooting = t < 0.5f ? a.shooting : b.shooting;
    result.reloading = t < 0.5f ? a.reloading : b.reloading;
    result.grounded = t < 0.5f ? a.grounded : b.grounded;
    result.collidable = t < 0.5f ? a.collidable : b.collidable;
    result.weaponName = t < 0.5f ? a.weaponName : b.weaponName;
    result.weaponModelPath = t < 0.5f ? a.weaponModelPath : b.weaponModelPath;
    result.currentAmmo = t < 0.5f ? a.currentAmmo : b.currentAmmo;
    result.reserveAmmo = t < 0.5f ? a.reserveAmmo : b.reserveAmmo;
    result.dead = t < 0.5f ? a.dead : b.dead;
    for (ReplayBodyPartState& part : result.bodyParts) {
        auto it = std::find_if(
            b.bodyParts.begin(), b.bodyParts.end(),
            [&part](const ReplayBodyPartState& other) {
                return other.name == part.name;
            });
        if (it != b.bodyParts.end())
            part = mixPart(part, *it, t);
    }
    return result;
}

}

void ReplayPlayer::update(float dt)
{
    if (!mPlaying || mPaused || mClip.sceneFrames.empty())
        return;

    const int previousTick = (int)std::floor(mPlaybackTick);
    float effectiveTimescale = mTimescale;
    if (gReplayEditor.isLoaded() && gReplayEditor.totalTicks() > 0) {
        effectiveTimescale = gReplayEditor.playbackSpeedAtTick((int)std::floor(mPlaybackTick));
    }
    mPlaybackTick += dt * (float)std::max(mHeader.tickRate, 1u) * effectiveTimescale;
    {   static float logTimer = 0.0f; logTimer -= dt;
        if (logTimer <= 0.0f) { logTimer = 1.0f;
            auto* frame = currentSceneFrame();
            printf("[REPLAY] time=%.2f tick=%.1f frame=%d cameraPos=(%.1f %.1f %.1f) actorCount=%zu sceneFrames=%zu\n",
                   mPlaybackTick / (float)std::max(mHeader.tickRate, 1u),
                   mPlaybackTick, frame ? frame->tick : -1,
                   frame ? frame->camera.position.x : 0.0f,
                   frame ? frame->camera.position.y : 0.0f,
                   frame ? frame->camera.position.z : 0.0f,
                   frame ? frame->actors.size() : 0,
                   mClip.sceneFrames.size());
        }
    }
    const int lastTick = mClip.sceneFrames.back().tick;
    if (mPlaybackTick > (float)lastTick) {
        mPlaybackTick = (float)lastTick;
        mCurrentTick = (uint32_t)lastTick;
        // Pause at end instead of stopping — keeps editor active, Space toggles pause,
        // timeline visible, rple reload works, camera keyframes stay editable.
        mPaused = true;
        Debug::log(Debug::Category::Replay, "[RPLE END] tick=%d paused=1 editorStillOpen=1\n", lastTick);
    } else {
        mCurrentTick = (uint32_t)std::max(0, (int)std::floor(mPlaybackTick));
    }

    const int currentEventTick = (int)std::floor(mPlaybackTick);
    for (const ReplaySceneFrame& frame : mClip.sceneFrames) {
        if (frame.tick <= mLastEventTick || frame.tick > currentEventTick)
            continue;
        mTriggeredEffects.insert(
            mTriggeredEffects.end(), frame.effects.begin(), frame.effects.end());
    }
    for (const ReplaySoundEvent& sound : mClip.soundEvents) {
        if (sound.tick > mLastEventTick && sound.tick <= currentEventTick)
            mTriggeredSounds.push_back(sound);
    }
    for (const ReplayKillfeedEvent& kf : mClip.killfeedEvents) {
        if (kf.tick > mLastEventTick && kf.tick <= currentEventTick)
            mTriggeredKillfeedEvents.push_back(kf);
    }
    if (currentEventTick >= previousTick)
        mLastEventTick = currentEventTick;

    auto upper = std::lower_bound(
        mClip.sceneFrames.begin(), mClip.sceneFrames.end(), mPlaybackTick,
        [](const ReplaySceneFrame& frame, float tick) {
            return (float)frame.tick < tick;
        });
    if (upper == mClip.sceneFrames.begin()) {
        mInterpolatedFrame = *upper;
        return;
    }
    if (upper == mClip.sceneFrames.end()) {
        mInterpolatedFrame = mClip.sceneFrames.back();
        return;
    }
    const ReplaySceneFrame& b = *upper;
    const ReplaySceneFrame& a = *(upper - 1);
    const float span = (float)std::max(1, b.tick - a.tick);
    const float t = glm::clamp((mPlaybackTick - (float)a.tick) / span, 0.0f, 1.0f);
    mInterpolatedFrame = a;
    mInterpolatedFrame.tick = (int)mPlaybackTick;
    mInterpolatedFrame.time = mPlaybackTick / (float)std::max(mHeader.tickRate, 1u);
    mInterpolatedFrame.camera.position =
        glm::mix(a.camera.position, b.camera.position, t);
    mInterpolatedFrame.camera.rotation =
        glm::mix(a.camera.rotation, b.camera.rotation, t);
    mInterpolatedFrame.camera.fov = glm::mix(a.camera.fov, b.camera.fov, t);
    mInterpolatedFrame.actors.clear();
    for (const ReplayActorState& actor : a.actors) {
        const ReplayActorState* next = findActor(b, actor.id);
        mInterpolatedFrame.actors.push_back(
            next ? mixActor(actor, *next, t) : actor);
    }
}

const ReplaySceneFrame* ReplayPlayer::currentSceneFrame() const
{
    if (mClip.sceneFrames.empty())
        return nullptr;
    if (mPlaybackTick <= 0.0f)
        return &mClip.sceneFrames.front();
    return &mInterpolatedFrame;
}

std::vector<ReplayEffectEvent> ReplayPlayer::takeTriggeredEffects()
{
    std::vector<ReplayEffectEvent> result;
    result.swap(mTriggeredEffects);
    return result;
}

std::vector<ReplaySoundEvent> ReplayPlayer::takeTriggeredSounds()
{
    std::vector<ReplaySoundEvent> result;
    result.swap(mTriggeredSounds);
    return result;
}

std::vector<ReplayKillfeedEvent> ReplayPlayer::takeTriggeredKillfeedEvents()
{
    std::vector<ReplayKillfeedEvent> result;
    result.swap(mTriggeredKillfeedEvents);
    return result;
}

bool ReplayPlayer::getFrameAt(uint32_t tick, InputFrame& out) const {
    if (tick >= mFrames.size()) return false;
    out = mFrames[tick].inputs;
    return true;
}

const InputFrame* ReplayPlayer::advanceTick() {
    if (!mPlaying || mCurrentTick >= mFrames.size()) {
        mPlaying = false;
        return nullptr;
    }
    const InputFrame* frame = &mFrames[mCurrentTick].inputs;
    mCurrentTick++;
    return frame;
}
