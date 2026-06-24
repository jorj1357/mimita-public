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

bool ReplayPlayer::loadFromJSON(const std::string& path) {
    printf("[REPLAY] loading clip from %s\n", path.c_str());
    ReplayClip clip;
    if (clip.load(path)) {
        mClip = std::move(clip);
        mHeader = mClip.header;
        mFrames = mClip.frames;
        mCurrentTick = 0;
        mPlaybackTick = 0.0f;
        mLastEventTick = -1;
        mOutfitPath.clear();
        printf("[REPLAY] clip loaded path=%s sceneFrames=%zu frames=%zu header.tickCount=%u\n",
               path.c_str(), mClip.sceneFrames.size(), mFrames.size(), mHeader.tickCount);
        return true;
    }

    printf("[REPLAY] clip.load() returned false, trying JSON parse for %s\n", path.c_str());
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[REPLAY] Could not open %s\n", path.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        auto& h = j["header"];
        mHeader.version = h.value("version", 1);
        mHeader.tickCount = h.value("tickCount", 0);
        mHeader.tickRate = h.value("tickRate", 60);
        mHeader.randomSeed = h.value("randomSeed", 0.0f);
        std::string mapName = h.value("mapName", "");
        std::strncpy(mHeader.mapName, mapName.c_str(), sizeof(mHeader.mapName) - 1);
        mHeader.timestamp = h.value("timestamp", 0ULL);
        std::string playerName = h.value("playerName", "");
        std::strncpy(mHeader.playerName, playerName.c_str(), sizeof(mHeader.playerName) - 1);

        mAssets.clear();
        if (j.contains("assets")) {
            for (const auto& jsonAsset : j["assets"]) {
                ReplayAsset asset;
                asset.id = jsonAsset.value("id", "");
                asset.type = jsonAsset.value("type", "");
                asset.path = jsonAsset.value("path", "");
                asset.shaderName = jsonAsset.value("shader", "");
                asset.source = jsonAsset.value("source", "");
                mAssets.push_back(asset);
            }
        }

        mOutfitPath.clear();
        for (const ReplayAsset& asset : mAssets) {
            if (asset.id.find("outfit") != std::string::npos) {
                mOutfitPath = asset.path;
                printf("[REPLAY] Restoring outfit: %s\n", mOutfitPath.c_str());
                break;
            }
        }

        mFrames.clear();
        for (const auto& f : j["frames"]) {
            ReplayFrame rf;
            rf.tick = f.value("tick", 0);
            rf.inputs.moveX = f.value("moveX", 0.0f);
            rf.inputs.moveY = f.value("moveY", 0.0f);
            rf.inputs.jump = f.value("jump", false);
            rf.inputs.jumpPressed = f.value("jumpPressed", false);
            rf.inputs.dashPressed = f.value("dashPressed", false);
            rf.inputs.groundReturnPressed = f.value("groundReturnPressed", false);
            rf.inputs.freezeHeld = f.value("freezeHeld", false);
            rf.inputs.movementPressed = f.value("movementPressed", false);
            rf.inputs.reloadPressed = f.value("reloadPressed", false);
            rf.inputs.lookYaw = f.value("lookYaw", 0.0f);
            rf.inputs.lookPitch = f.value("lookPitch", 0.0f);
            mFrames.push_back(rf);
        }

        mClip.sceneFrames.clear();
        if (j.contains("sceneFrames")) {
            for (const auto& sf : j["sceneFrames"]) {
                ReplaySceneFrame frame;
                frame.tick = sf.value("tick", 0);
                frame.time = sf.value("time", 0.0f);
                frame.camera.position = {
                    sf["camera"].value("position", std::vector<float>{0,0,0})[0],
                    sf["camera"].value("position", std::vector<float>{0,0,0})[1],
                    sf["camera"].value("position", std::vector<float>{0,0,0})[2]
                };
                frame.camera.rotation = {
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[0],
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[1],
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[2]
                };
                frame.camera.fov = sf["camera"].value("fov", 70.0f);
                if (sf.contains("actors")) {
                    for (const auto& a : sf["actors"]) {
                        ReplayActorState actor;
                        actor.id = a.value("id", "");
                        actor.name = a.value("name", "");
                        actor.type = a.value("type", "");
                        actor.modelPath = a.value("modelPath", "");
                        actor.position = {
                            a.value("position", std::vector<float>{0,0,0})[0],
                            a.value("position", std::vector<float>{0,0,0})[1],
                            a.value("position", std::vector<float>{0,0,0})[2]
                        };
                        actor.rotation = {
                            a.value("rotation", std::vector<float>{0,0,0})[0],
                            a.value("rotation", std::vector<float>{0,0,0})[1],
                            a.value("rotation", std::vector<float>{0,0,0})[2]
                        };
                        actor.velocity = {
                            a.value("velocity", std::vector<float>{0,0,0})[0],
                            a.value("velocity", std::vector<float>{0,0,0})[1],
                            a.value("velocity", std::vector<float>{0,0,0})[2]
                        };
                        actor.health = a.value("health", 100);
                        actor.maxHealth = a.value("maxHealth", 100);
                        actor.currentAmmo = a.value("currentAmmo", 0);
                        actor.reserveAmmo = a.value("reserveAmmo", 0);
                        actor.dead = a.value("dead", false);
                        actor.outfitPath = a.value("outfitPath", "");
                        actor.weaponName = a.value("weaponName", "");
                        actor.weaponModelPath = a.value("weaponModelPath", "");
                        actor.shooting = a.value("shooting", false);
                        actor.reloading = a.value("reloading", false);
                        actor.grounded = a.value("grounded", true);
                        if (a.contains("bodyParts")) {
                            for (auto& bp : a["bodyParts"].items()) {
                                ReplayBodyPartState part;
                                part.name = bp.key();
                                if (bp.value().contains("position")) {
                                    auto& p = bp.value()["position"];
                                    part.position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                                }
                                if (bp.value().contains("rotation") && bp.value()["rotation"].is_array()) {
                                    auto& r = bp.value()["rotation"];
                                    if (r.size() >= 4)
                                        part.rotation = glm::quat(r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
                                }
                                if (bp.value().contains("scale")) {
                                    auto& s = bp.value()["scale"];
                                    part.scale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
                                }
                                actor.bodyParts.push_back(part);
                            }
                        }
                        frame.actors.push_back(actor);
                    }
                }
                mClip.sceneFrames.push_back(frame);
            }
            printf("[REPLAY] Loaded %zu scene frames\n", mClip.sceneFrames.size());
        }

        mClip.soundEvents.clear();
        if (j.contains("soundEvents")) {
            for (const auto& se : j["soundEvents"]) {
                ReplaySoundEvent sound;
                sound.tick = se.value("tick", 0);
                sound.soundPath = se.value("soundPath", "");
                sound.world = se.value("world", false);
                std::vector<float> pos = se.value("position", std::vector<float>{0,0,0});
                sound.position = {pos[0], pos[1], pos[2]};
                sound.volume = se.value("volume", 1.0f);
                sound.pitch = se.value("pitch", 1.0f);
                sound.maxDistance = se.value("maxDistance", 30.0f);
                mClip.soundEvents.push_back(sound);
            }
            printf("[REPLAY] Loaded %zu sound events\n", mClip.soundEvents.size());
        }

        printf("[REPLAY] Loaded %zu frames from %s\n", mFrames.size(), path.c_str());
        return true;

    } catch (const std::exception& e) {
        printf("[REPLAY] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool ReplayPlayer::loadFromBinary(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("[REPLAY] Could not open %s\n", path.c_str());
        return false;
    }

    file.read((char*)&mHeader, sizeof(mHeader));
    if (file.gcount() != sizeof(mHeader)) {
        printf("[REPLAY] Invalid header in %s\n", path.c_str());
        return false;
    }

    mFrames.resize(mHeader.tickCount);
    for (uint32_t i = 0; i < mHeader.tickCount; ++i) {
        file.read((char*)&mFrames[i], sizeof(ReplayFrame));
    }

    printf("[REPLAY] Loaded %zu frames (binary) from %s\n", mFrames.size(), path.c_str());
    return true;
}

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
    printf("[REPLAY] seekToTick(%u) -> mCurrentTick=%u max=%zu frames=%zu scene=%zu playing=1\n",
           tick, mCurrentTick, maxTicks, mFrames.size(), mClip.sceneFrames.size());
}

// ============================================================
// ReplayCameraController
// ============================================================

bool ReplayCameraController::setMode(const std::string& name)
{
    if (name == "fp" || name == "firstperson")
        mMode = ReplayCameraMode::FirstPerson;
    else if (name == "victim")
        mMode = ReplayCameraMode::Victim;
    else if (name == "orbit")
        mMode = ReplayCameraMode::Orbit;
    else if (name == "freecam")
        mMode = ReplayCameraMode::Freecam;
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
        case ReplayCameraMode::FirstPerson: return "fp";
        case ReplayCameraMode::Victim: return "victim";
        case ReplayCameraMode::Orbit: return "orbit";
        case ReplayCameraMode::Freecam: return "freecam";
    }
    return "fp";
}

void ReplayCameraController::update(
    Camera& camera, const ReplaySceneFrame& frame,
    const std::string& killerId, const std::string& victimId, float dt)
{
    camera.fov = mFov > 0.0f ? mFov : frame.camera.fov;
    if (mMode == ReplayCameraMode::Freecam) {
        static bool logged = false;
        if (!logged) { logged = true;
            printf("[REPLAY FREECAM] active - camera fully detached from replay\n");
        }
        return;
    }
    if (mMode == ReplayCameraMode::FirstPerson) {
        const ReplayActorState* killer = findActor(frame, killerId);
        camera.pos = killer
            ? killer->position + glm::vec3(0.0f, 0.0f, 1.55f)
            : frame.camera.position;
        camera.pitch = frame.camera.rotation.x;
        camera.yaw = frame.camera.rotation.z;
        camera.updateVectors();
        return;
    }

    const std::string targetId =
        mMode == ReplayCameraMode::Victim ? victimId : killerId;
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

    mOrbitAngle += dt * 35.0f;
    const float radians = glm::radians(mOrbitAngle);
    camera.pos = focus + glm::vec3(std::cos(radians) * 5.5f,
                                   std::sin(radians) * 5.5f, 2.2f);
    camera.front = glm::normalize(focus - camera.pos);
    camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0, 0, 1)));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}

// ============================================================
// ReplayClipSaver
// ============================================================

void ReplayClipSaver::notifyKill(
    const std::string& killerId,
    const std::string& victimId,
    bool roundWinning)
{
    KillInfo info;
    info.tick = mRing.currentTick();
    info.killerId = killerId;
    info.victimId = victimId;
    info.autoSave = roundWinning;
    mLastKill = std::move(info);
    printf("[REPLAY] kill marked tick=%u killer=%s victim=%s roundWinning=%d\n",
           mLastKill->tick, killerId.c_str(), victimId.c_str(), (int)roundWinning);
}

void ReplayClipSaver::update()
{
    if (!mLastKill || !mLastKill->autoSave || mLastKill->saved)
        return;
    if (mRing.currentTick() >= mLastKill->tick + 3u * ReplayRingBuffer::TickRate)
        saveLastKill();
}

bool ReplayClipSaver::saveLastKill(std::string* savedPath)
{
    if (!mLastKill)
        return false;
    const uint32_t requestedEnd =
        mLastKill->tick + 3u * ReplayRingBuffer::TickRate;
    if (mRing.currentTick() < requestedEnd) {
        mLastKill->autoSave = true;
        if (savedPath)
            *savedPath = "pending post-kill capture";
        printf("[REPLAY] clip save queued until tick=%u\n", requestedEnd);
        return true;
    }
    const uint32_t startTick =
        mLastKill->tick > 5u * ReplayRingBuffer::TickRate
            ? mLastKill->tick - 5u * ReplayRingBuffer::TickRate
            : 0u;
    ReplayClip clip = mRing.makeClip(
        startTick, requestedEnd, mLastKill->tick,
        mLastKill->killerId, mLastKill->victimId);
    if (clip.sceneFrames.empty())
        return false;
    const std::string path = generateReplayClipPath();
    if (!clip.save(path))
        return false;
    mLastKill->saved = true;
    if (savedPath)
        *savedPath = path;
    printf("[REPLAY] saved clip %s frames=%zu\n",
           path.c_str(), clip.sceneFrames.size());
    return true;
}
