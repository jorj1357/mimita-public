#include "replay.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "entities/player.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"
#include "killfeed/killfeed.h"
#include "config/killfeed-config.h"

using json = nlohmann::json;

const ReplayActorState* findActor(
    const ReplaySceneFrame& frame, const std::string& id);

namespace {

struct ReplayHitmarkerConfig {
    bool enableReplayHitmarkers = true;
    bool attackerPOVOnly = true;
};

static ReplayHitmarkerConfig gReplayHitmarkerCfg;
static uint64_t gReplayHitmarkerCfgLastWrite = 0;
static const char* REPLAY_HITMARKER_CFG_PATH = "config/audio/replay-hitmarkers.json";

static uint64_t cfgFileWriteTime(const char* path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

static void reloadReplayHitmarkerConfig()
{
    std::ifstream file(REPLAY_HITMARKER_CFG_PATH);
    if (!file.is_open())
        return;
    try
    {
        nlohmann::json j;
        file >> j;

        ReplayHitmarkerConfig loaded;
        if (j.contains("enableReplayHitmarkers"))
            loaded.enableReplayHitmarkers = j["enableReplayHitmarkers"].get<bool>();
        if (j.contains("attackerPOVOnly"))
            loaded.attackerPOVOnly = j["attackerPOVOnly"].get<bool>();

        gReplayHitmarkerCfg = loaded;
        Debug::log(Debug::Category::Replay,
            "[REPLAY HITMARKER] config reloaded: enable=%d attackerPOVOnly=%d\n",
            (int)gReplayHitmarkerCfg.enableReplayHitmarkers,
            (int)gReplayHitmarkerCfg.attackerPOVOnly);
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Replay,
            "[REPLAY HITMARKER] config reload failed: %s\n", e.what());
    }
}

} // anonymous namespace

void pollReplayHitmarkerConfig()
{
    static double elapsed = 0.0;
    elapsed += 1.0 / 60.0;
    if (elapsed < 0.25)
        return;
    elapsed = 0.0;

    uint64_t wt = cfgFileWriteTime(REPLAY_HITMARKER_CFG_PATH);
    if (wt == 0)
        return;

    if (wt != gReplayHitmarkerCfgLastWrite)
    {
        gReplayHitmarkerCfgLastWrite = wt;
        reloadReplayHitmarkerConfig();
    }
}

bool ReplayShouldPlayHitmarkerAudio(
    const std::string& attackerId,
    ReplayCameraMode cameraMode,
    const std::string& viewedEntity)
{
    if (!gReplayHitmarkerCfg.enableReplayHitmarkers)
        return false;
    if (!gReplayHitmarkerCfg.attackerPOVOnly)
        return true;
    if (cameraMode == ReplayCameraMode::Freecam)
        return false;
    if (viewedEntity != attackerId)
        return false;
    return true;
}

// ============================================================
// ReplayPlayer
// ============================================================
bool ReplayPlayer::preloadAssets()
{
    printf("[REPLAY] Preloading assets...\n");
    bool allOk = true;

    std::vector<std::string> requiredModels;

    for (const ReplaySceneFrame& frame : mClip.sceneFrames) {
        for (const ReplayActorState& actor : frame.actors) {
            if (!actor.modelPath.empty())
                requiredModels.push_back(actor.modelPath);
            if (!actor.weaponModelPath.empty())
                requiredModels.push_back(actor.weaponModelPath);
        }
    }

    std::sort(requiredModels.begin(), requiredModels.end());
    requiredModels.erase(std::unique(requiredModels.begin(), requiredModels.end()),
                         requiredModels.end());

    for (const std::string& modelPath : requiredModels) {
        if (!std::filesystem::exists(modelPath)) {
            printf("[REPLAY] MISSING ASSET: %s\n", modelPath.c_str());
            allOk = false;
        } else {
            printf("[REPLAY] Found asset: %s\n", modelPath.c_str());
        }
    }

    if (!mOutfitPath.empty()) {
        if (!std::filesystem::exists(mOutfitPath)) {
            printf("[REPLAY] MISSING OUTFIT: %s\n", mOutfitPath.c_str());
        } else {
            printf("[REPLAY] Found outfit: %s\n", mOutfitPath.c_str());
        }
    }

    for (const ReplayAsset& asset : mAssets) {
        if (!asset.path.empty() && !std::filesystem::exists(asset.path)) {
            printf("[REPLAY] MISSING ASSET from registry: %s (%s)\n",
                   asset.path.c_str(), asset.id.c_str());
        }
    }

    printf("[REPLAY] Asset preload complete %s\n",
           allOk ? "ALL OK" : "SOME MISSING");
    return allOk;
}

void ReplayPlayer::beginPlayback() {
    printf("[REPLAY] beginPlayback called tickCount=%u currentTick=%u\n", mHeader.tickCount, mCurrentTick);
    mPlaying = true;
    mPaused = false;
    mCurrentTick = 0;
    mPlaybackTick = 0.0f;
    mLastEventTick = -1;
    mTriggeredEffects.clear();
    mTriggeredSounds.clear();
    mTriggeredKillfeedEvents.clear();
    KillfeedManager::instance().clear();
    KillfeedConfig::instance().load();
    KillfeedManager::instance().setMode(KillfeedConfig::instance().data().mode);
    rebuildInterpolatedFrameAtTick();
    printf("[REPLAY] Playback started ticks=%u currentTick=%u isPlaying=%d\n",
           mHeader.tickCount, mCurrentTick, (int)mPlaying);
}

void ReplayPlayer::stopPlayback() {
    mPlaying = false;
    mPaused = false;
    printf("[REPLAY] Playback stopped at tick %u\n", mCurrentTick);
}

void ReplayPlayer::pause()
{
    if (mPlaying)
        mPaused = true;
}

void ReplayPlayer::resume()
{
    if (mPlaying)
        mPaused = false;
}

void ReplayPlayer::setTimescale(float value)
{
    mTimescale = glm::clamp(value, 0.05f, 4.0f);
}

void ReplayPlayer::seekToTick(uint32_t tick) {
    size_t maxTicks = mClip.sceneFrames.empty() ? mFrames.size() : mClip.sceneFrames.size();
    mCurrentTick = std::min(tick, (uint32_t)maxTicks);
    mPlaybackTick = (float)tick;
    mLastEventTick = (int)tick - 1;
    mPlaying = true;
    mPaused = false;
    rebuildInterpolatedFrameAtTick();
    if (!isReplayExportActive() || gReplayExportVerbose)
        printf("[REPLAY] seekToTick(%u) -> mCurrentTick=%u max=%zu frames=%zu scene=%zu playing=1\n",
               tick, mCurrentTick, maxTicks, mFrames.size(), mClip.sceneFrames.size());}

// ============================================================
// ReplayCameraController
// ============================================================

bool ReplayCameraController::setMode(const std::string& name)
{
    if (name == "recorded" || name == "rec")
        mMode = ReplayCameraMode::Recorded;
    else if (name == "fp" || name == "firstperson")
        mMode = ReplayCameraMode::FirstPerson;
    else if (name == "victim")
        mMode = ReplayCameraMode::Victim;
    else if (name == "orbit")
        mMode = ReplayCameraMode::Orbit;
    else if (name == "freecam")
        mMode = ReplayCameraMode::Freecam;
    else if (name == "thirdperson" || name == "tp")
        mMode = ReplayCameraMode::ThirdPerson;
    else if (name == "spectator" || name == "spec")
        mMode = ReplayCameraMode::Spectator;
    else if (name == "topdown" || name == "td")
        mMode = ReplayCameraMode::TopDown;
    else
        return false;
    return true;
}

void ReplayCameraController::setFov(float value)
{
    mFov = glm::clamp(value, 20.0f, 160.0f);
}

const char* ReplayCameraController::modeName() const
{
    switch (mMode) {
        case ReplayCameraMode::Recorded: return "recorded";
        case ReplayCameraMode::FirstPerson: return "fp";
        case ReplayCameraMode::Victim: return "victim";
        case ReplayCameraMode::Orbit: return "orbit";
        case ReplayCameraMode::Freecam: return "freecam";
        case ReplayCameraMode::ThirdPerson: return "thirdperson";
        case ReplayCameraMode::Spectator: return "spectator";
        case ReplayCameraMode::TopDown: return "topdown";
    }
    return "recorded";
}

void ReplayCameraController::update(
    Camera& camera, const ReplaySceneFrame& frame,
    const std::string& killerId, const std::string& victimId, float dt,
    uint32_t tick)
{
    // Debug log camera mode once per second
    Debug::logThrottled(Debug::Category::Replay, "replay-cam-mode", 1.0f,
        "[ReplayCamera] Current mode: %s  pos=(%.1f %.1f %.1f)  yaw=%.1f  pitch=%.1f  fov=%.0f  tick=%u\n",
        modeName(), camera.pos.x, camera.pos.y, camera.pos.z,
        camera.yaw, camera.pitch, camera.fov, tick);

    camera.fov = mFov > 0.0f ? mFov : frame.camera.fov;
    if (mMode == ReplayCameraMode::Freecam) {
        Debug::logThrottled(Debug::Category::Replay, "replay-freecam", 2.0f,
            "[ReplayCamera] Freecam active: pos=(%.1f %.1f %.1f) yaw=%.1f pitch=%.1f fov=%.0f\n",
            camera.pos.x, camera.pos.y, camera.pos.z, camera.yaw, camera.pitch, camera.fov);
        return;
    }
    if (mMode == ReplayCameraMode::Recorded) {
        camera.pos = frame.camera.position;
        camera.pitch = frame.camera.rotation.x;
        camera.yaw = frame.camera.rotation.z;
        camera.updateVectors();
        Debug::logThrottled(Debug::Category::Replay, "replay-recorded", 1.0f,
            "[ReplayCam] tick=%u mode=recorded pos=(%.1f %.1f %.1f) pitch=%.1f yaw=%.1f fov=%.0f\n",
            tick, camera.pos.x, camera.pos.y, camera.pos.z, camera.pitch, camera.yaw, camera.fov);
        return;
    }
    if (mMode == ReplayCameraMode::FirstPerson) {
        const ReplayActorState* killer = findActor(frame, killerId);
        camera.pos = killer
            ? killer->position + glm::vec3(0.0f, 0.0f, 1.872f)
            : frame.camera.position;
        camera.pitch = frame.camera.rotation.x;
        camera.yaw = frame.camera.rotation.z;
        camera.updateVectors();
        return;
    }

    const std::string targetId =
        (mMode == ReplayCameraMode::Victim) ? victimId : killerId;
    const ReplayActorState* target = findActor(frame, targetId);
    if (!target && !frame.actors.empty())
        target = &frame.actors.front();
    if (!target)
        return;

    const glm::vec3 focus = target->position + glm::vec3(0.0f, 0.0f, 1.35f);

    if (mMode == ReplayCameraMode::Victim) {
        camera.pos = focus;
        camera.yaw = target->rotation.z;
        camera.pitch = 0.0f;
        camera.updateVectors();
        return;
    }

    if (mMode == ReplayCameraMode::ThirdPerson) {
        const float dist = 4.0f;
        const float height = 2.5f;
        const float yawRad = glm::radians(target->rotation.z);
        camera.pos = focus - glm::vec3(std::cos(yawRad) * dist,
                                        std::sin(yawRad) * dist, -height);
        camera.yaw = target->rotation.z;
        camera.pitch = -15.0f;
        camera.updateVectors();
        return;
    }

    if (mMode == ReplayCameraMode::TopDown) {
        camera.pos = focus + glm::vec3(0.0f, 0.0f, 15.0f);
        camera.pitch = -90.0f;
        camera.yaw = 0.0f;
        camera.updateVectors();
        return;
    }

    if (mMode == ReplayCameraMode::Spectator) {
        mOrbitAngle += dt * 25.0f;
        const float radians = glm::radians(mOrbitAngle);
        const float dist = 8.0f;
        camera.pos = focus + glm::vec3(std::cos(radians) * dist,
                                        std::sin(radians) * dist, 3.0f);
        camera.front = glm::normalize(focus - camera.pos);
        camera.right = glm::normalize(
            glm::cross(camera.front, glm::vec3(0, 0, 1)));
        camera.up = glm::normalize(glm::cross(camera.right, camera.front));
        return;
    }

    mOrbitAngle += dt * 35.0f;
    const float radians = glm::radians(mOrbitAngle);
    camera.pos = focus + glm::vec3(std::cos(radians) * 5.5f,
                                   std::sin(radians) * 5.5f, 2.2f);
    camera.front = glm::normalize(focus - camera.pos);
    camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0, 0, 1)));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}

