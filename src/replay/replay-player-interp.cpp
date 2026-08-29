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
    result.weaponName = t < 0.5f ? a.weaponName : b.weaponName;
    result.weaponModelPath = t < 0.5f ? a.weaponModelPath : b.weaponModelPath;
    result.currentAmmo = t < 0.5f ? a.currentAmmo : b.currentAmmo;
    result.reserveAmmo = t < 0.5f ? a.reserveAmmo : b.reserveAmmo;
    result.dead = t < 0.5f ? a.dead : b.dead;
    result.sizeScale = a.sizeScale;
    result.bodyPartCount = a.bodyPartCount;
    for (int i = 0; i < result.bodyPartCount; ++i) {
        ReplayBodyPartState& part = result.bodyParts[i];
        // Find matching part in b by partId
        for (int j = 0; j < b.bodyPartCount; ++j) {
            if (b.bodyParts[j].partId == part.partId) {
                part = mixPart(part, b.bodyParts[j], t);
                break;
            }
        }
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
    float tickAdvance = dt * (float)std::max(mHeader.tickRate, 1u) * effectiveTimescale;
    mPlaybackTick += tickAdvance;

    {   static float logTimer = 0.0f; logTimer -= dt;
        if (logTimer <= 0.0f) { logTimer = 1.0f;
            Debug::log(Debug::Category::Replay,
                "[ReplayPB] Replay delta: real_dt=%.4f speed=%.2f tick_advance=%.2f\n",
                dt, effectiveTimescale, tickAdvance);
        }
    }

    {   static float logTimer2 = 0.0f; logTimer2 -= dt;
        if (logTimer2 <= 0.0f) { logTimer2 = 1.0f;
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

    rebuildInterpolatedFrameAtTick();
}

void ReplayPlayer::rebuildInterpolatedFrameAtTick()
{
    if (mClip.sceneFrames.empty())
        return;

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

void ReplayPlayer::pollPoseInvariant()
{
    // Detects the H1 stall: the authoritative tick advances while the presented
    // interpolated frame/pose stays frozen (a skipped rebuild). Called once per
    // render frame while playing/exporting.
    constexpr int kStallThreshold = 30;

    if (!mPlaying || mClip.sceneFrames.empty()) {
        mInvariantStallFrames = 0;
        mInvariantLastTick = -1;
        mInvariantLastFrameTick = -1;
        mInvariantHadPose = false;
        return;
    }

    const ReplaySceneFrame* f = currentSceneFrame();
    const int frameTick = f ? f->tick : -1;
    glm::vec3 pose(0.0f);
    bool hasPose = false;
    if (f && !f->actors.empty()) {
        pose = f->actors[0].position;
        if (f->actors[0].bodyPartCount > 0)
            pose = f->actors[0].bodyParts[0].position;
        hasPose = true;
    }

    if ((int)mCurrentTick != mInvariantLastTick) {
        const bool frameStale = (frameTick == mInvariantLastFrameTick);
        const bool poseStale = hasPose && mInvariantHadPose &&
            (pose == mInvariantLastPose);
        if (frameStale || poseStale) {
            ++mInvariantStallFrames;
            if (mInvariantStallFrames >= kStallThreshold) {
                Debug::warn(Debug::Category::Replay,
                    "[REPLAY INVARIANT] tick advanced (currentTick=%u) but presented "
                    "frame/pose is stale (frameTick=%d lastFrameTick=%d stallFrames=%d)\n",
                    mCurrentTick, frameTick, mInvariantLastFrameTick, mInvariantStallFrames);
                mInvariantStallFrames = 0;
            }
        } else {
            mInvariantStallFrames = 0;
        }
    }

    mInvariantLastTick = (int)mCurrentTick;
    mInvariantLastFrameTick = frameTick;
    if (hasPose) {
        mInvariantLastPose = pose;
        mInvariantHadPose = true;
    }
}

void ReplayPlayer::takeTriggeredEffects(std::vector<ReplayEffectEvent>& out)
{
    out.swap(mTriggeredEffects);
}

void ReplayPlayer::takeTriggeredSounds(std::vector<ReplaySoundEvent>& out)
{
    out.swap(mTriggeredSounds);
}

void ReplayPlayer::takeTriggeredKillfeedEvents(std::vector<ReplayKillfeedEvent>& out)
{
    out.swap(mTriggeredKillfeedEvents);
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
