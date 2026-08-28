#include "replay.h"
#include "replay-io.h"
#include "perf/perf.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "entities/player.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

void captureReplayEffect(const ReplayEffectEvent& event)
{
    if (gReplayCaptureEnabled && gActiveReplayRecorder &&
        gActiveReplayRecorder->isRecording())
        gActiveReplayRecorder->recordEffectEvent(event);
}

void captureReplaySound(const ReplaySoundEvent& event)
{
    if (gReplayCaptureEnabled && gActiveReplayRecorder &&
        gActiveReplayRecorder->isRecording())
        gActiveReplayRecorder->recordSoundEvent(event);
}

void captureReplayKillfeed(const ReplayKillfeedEvent& event)
{
    if (gReplayCaptureEnabled && gActiveReplayRecorder &&
        gActiveReplayRecorder->isRecording())
        gActiveReplayRecorder->recordKillfeedEvent(event);
}

BodyPartArray captureReplayBodyParts(const Player& player)
{
    Perf::ScopedTimer _t("ReplayCaptureBodyParts");
    BodyPartArray result;

    const glm::vec3& rootPos = player.pos;
    const float rootYaw = player.yaw;

    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (result.count >= ReplayActorState::MAX_BODY_PARTS)
            break;

        // Skip body parts that aren't in our fixed set
        const char* name = part.name.c_str();
        bool match = (name[0] == 'h' && name[1] == 'e') ||  // head
                     (name[0] == 't' && name[1] == 'o') ||  // torso
                     (name[0] == 'l' && name[1] == 'e') ||  // leftArm/leftLeg
                     (name[0] == 'r' && name[1] == 'i');    // rightArm/rightLeg
        if (!match) continue;

        // Direct extraction from world transform — no glm::decompose needed.
        const glm::mat4& wt = part.worldTransform;

        // Compute local translation relative to root
        glm::vec3 worldPos(wt[3][0], wt[3][1], wt[3][2]);
        glm::vec3 localPos = worldPos - rootPos;

        // Rotate local position back by -rootYaw to get body-local coords
        float negYaw = -glm::radians(rootYaw);
        float cosA = glm::cos(negYaw);
        float sinA = glm::sin(negYaw);
        glm::vec3 bodyLocal(
            localPos.x * cosA - localPos.y * sinA,
            localPos.x * sinA + localPos.y * cosA,
            localPos.z
        );

        // Extract rotation quaternion from world transform's upper-left 3x3
        glm::mat3 rotMat(wt);
        glm::quat worldRot = glm::quat_cast(rotMat);

        // Convert to body-local rotation by removing root yaw
        glm::quat rootRot = glm::angleAxis(glm::radians(rootYaw), glm::vec3(0, 0, 1));
        glm::quat localRot = glm::inverse(rootRot) * worldRot;

        ReplayBodyPartState& state = result.parts[result.count];
        state.name = part.name;  // kept for JSON export compatibility
        state.position = bodyLocal;
        state.rotation = glm::normalize(localRot);
        state.scale = glm::vec3(1.0f);
        result.count++;
    }

    return result;
}

// ============================================================
// ReplayRecorder
// ============================================================

void ReplayRecorder::beginRecording(float randomSeed, const char* mapName) {
    std::lock_guard<std::mutex> lock(mRingMutex);
    mFrames.clear();
    mAssets.clear();
    mSoundEvents.clear();
    mKillfeedEvents.clear();
    mPendingEffects.clear();
    mSceneFrameWriteIndex = 0;
    mSceneFrameCount = 0;
    // Clear all frame slots (preserves vector capacity for reuse)
    for (uint32_t i = 0; i < REPLAY_RING_CAPACITY; ++i) {
        mSceneFrames[i].actors.clear();
        mSceneFrames[i].effects.clear();
        mSceneFrames[i].tick = 0;
        mSceneFrames[i].time = 0.0f;
    }
    mIdentityCache.clear();
    mPreviousTickState.clear();
    mIdentityTable.clear();
    mPreviousCompactState.clear();
    mNpcAvatarCache.clear();
    mWorld = {};
    mLighting = {};
    mTick = 0;
    mEventTick = 0;
    mRecording = true;
    setActiveReplayRecorder(this);

    mHeader = {};
    std::memcpy(mHeader.magic, "MIRPLAY", 8);
    mHeader.version = 1;
    mHeader.tickRate = 60;
    mHeader.randomSeed = randomSeed;
    mHeader.timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    if (mapName) {
        std::strncpy(mHeader.mapName, mapName, sizeof(mHeader.mapName) - 1);
    }
    std::strncpy(mHeader.playerName, "player", sizeof(mHeader.playerName) - 1);

    printf("[REPLAY] Recording started  map=%s seed=%.1f\n",
           mHeader.mapName, mHeader.randomSeed);
}

// ── Delta detection ──
uint32_t ReplayRecorder::computeDirtyMask(uint32_t actorId, const ReplayActorTickState& current) const
{
    auto it = mPreviousCompactState.find(actorId);
    if (it == mPreviousCompactState.end())
        return ReplayDirtyAll; // first time seen, everything is new

    const ReplayActorTickState& prev = it->second;
    uint32_t dirty = ReplayDirtyNone;

    // Position: use squared distance epsilon (0.001mm threshold)
    float posDist2 = glm::length2(current.position - prev.position);
    if (posDist2 > 1e-10f) dirty |= ReplayDirtyPosition;

    // Rotation: compare as quaternions (exact match for determinism)
    if (current.rotation.x != prev.rotation.x || current.rotation.y != prev.rotation.y ||
        current.rotation.z != prev.rotation.z) dirty |= ReplayDirtyRotation;

    // Velocity
    float velDist2 = glm::length2(current.velocity - prev.velocity);
    if (velDist2 > 1e-10f) dirty |= ReplayDirtyVelocity;

    // Health/ammo (integer comparison)
    if (current.health != prev.health || current.maxHealth != prev.maxHealth)
        dirty |= ReplayDirtyHealth;

    // Weapon state
    if (current.currentAmmo != prev.currentAmmo || current.reserveAmmo != prev.reserveAmmo)
        dirty |= ReplayDirtyWeapon;

    // Body pose: compare each part's position and rotation
    if (current.bodyPartCount != prev.bodyPartCount) {
        dirty |= ReplayDirtyPose;
    } else {
        for (uint8_t i = 0; i < current.bodyPartCount; ++i) {
            if (glm::length2(current.bodyParts[i].position - prev.bodyParts[i].position) > 1e-10f ||
                current.bodyParts[i].rotation.x != prev.bodyParts[i].rotation.x ||
                current.bodyParts[i].rotation.y != prev.bodyParts[i].rotation.y ||
                current.bodyParts[i].rotation.z != prev.bodyParts[i].rotation.z ||
                current.bodyParts[i].rotation.w != prev.bodyParts[i].rotation.w) {
                dirty |= ReplayDirtyPose;
                break;
            }
        }
    }

    // Flags
    if (current.dead != prev.dead || current.grounded != prev.grounded ||
        current.shooting != prev.shooting || current.reloading != prev.reloading ||
        current.sizeScale != prev.sizeScale)
        dirty |= ReplayDirtyFlags;

    return dirty;
}

// ── NPC avatar cache ──
const std::string& ReplayRecorder::getCachedNpcAvatar(uint32_t npcId, uint16_t epoch) const
{
    static const std::string kEmpty;
    uint64_t key = (uint64_t(npcId) << 16) | uint64_t(epoch);
    auto it = mNpcAvatarCache.find(key);
    return it != mNpcAvatarCache.end() ? it->second : kEmpty;
}

void ReplayRecorder::cacheNpcAvatar(uint32_t npcId, uint16_t epoch, const std::string& name)
{
    uint64_t key = (uint64_t(npcId) << 16) | uint64_t(epoch);
    mNpcAvatarCache[key] = name;
}

void ReplayRecorder::recordFrame(const InputFrame& frame) {
    if (!mRecording) return;
    std::lock_guard<std::mutex> lock(mRingMutex);

    ReplayFrame rf;
    rf.tick = mTick++;
    mEventTick = rf.tick;
    rf.inputs = frame;
    if (mMaxTicks > 0 && mFrames.size() >= mMaxTicks)
        mFrames.erase(mFrames.begin());
    mFrames.push_back(rf);
    mHeader.tickCount = (uint32_t)mFrames.size();
}

void ReplayRecorder::recordSceneFrame(ReplaySceneFrame inputFrame)
{
    if (!mRecording) return;
    std::lock_guard<std::mutex> lock(mRingMutex);

    // Merge pending effects directly into the frame (no extra copy)
    inputFrame.effects.insert(inputFrame.effects.end(), mPendingEffects.begin(), mPendingEffects.end());
    mPendingEffects.clear();

    // Move into circular buffer slot (reuses existing vector capacity)
    ReplaySceneFrame& slot = mSceneFrames[mSceneFrameWriteIndex];
    slot = std::move(inputFrame);
    mSceneFrameWriteIndex = (mSceneFrameWriteIndex + 1) % REPLAY_RING_CAPACITY;
    if (mSceneFrameCount < REPLAY_RING_CAPACITY)
        mSceneFrameCount++;
    mEventTick = mTick;
}

void ReplayRecorder::registerAsset(
    const std::string& id,
    const std::string& type,
    const std::string& path,
    const std::vector<ReplayMaterialReference>& materials,
    const std::string& shaderName,
    const std::string& source)
{
    for (ReplayAsset& asset : mAssets)
    {
        if (asset.id == id) {
            if (asset.path.empty()) asset.path = path;
            if (asset.materials.empty()) asset.materials = materials;
            if (asset.shaderName.empty()) asset.shaderName = shaderName;
            if (asset.source.empty()) asset.source = source;
            return;
        }
    }

    ReplayAsset asset;
    asset.id = id;
    asset.type = type;
    asset.path = path;
    asset.materials = materials;
    asset.shaderName = shaderName;
    asset.source = source;

    mAssets.push_back(asset);
}

void ReplayRecorder::setWorldMetadata(const ReplayWorldMetadata& world)
{
    std::lock_guard<std::mutex> lock(mRingMutex);
    mWorld = world;
}

void ReplayRecorder::setLighting(const ReplayLightingState& lighting)
{
    std::lock_guard<std::mutex> lock(mRingMutex);
    mLighting = lighting;
}

bool ReplayRecorder::exportToJSON(const std::string& path) const {
    json j;
    const std::string validationPath = generateReplayValidationPath(path);

    j["header"]["version"] = mHeader.version;
    j["header"]["tickCount"] = mHeader.tickCount;
    j["header"]["tickRate"] = mHeader.tickRate;
    j["header"]["randomSeed"] = mHeader.randomSeed;
    j["header"]["mapName"] = std::string(mHeader.mapName);
    j["header"]["timestamp"] = mHeader.timestamp;
    j["header"]["playerName"] = std::string(mHeader.playerName);
    j["header"]["sceneFrameCount"] = mSceneFrameCount;
    j["metadata"]["format"] = "mimita-cinematic-replay";
    j["metadata"]["sceneCaptureVersion"] = 2;
    j["metadata"]["coordinateSystem"] = "Z_UP";
    j["metadata"]["distanceUnit"] = "meters";
    j["metadata"]["timelineFps"] = mHeader.tickRate;
    j["validation"]["schemaVersion"] = 1;
    j["validation"]["path"] =
        std::filesystem::path(validationPath).filename().string();
    j["validation"]["channels"] = json::array({"transforms"});
    j["validation"]["thresholds"] = {
        {"positionMeters", 0.05f},
        {"rotationDegrees", 5.0f}
    };
    json assetsJson = json::array();

    for (const auto& asset : mAssets)
    {
        json a;

        a["id"] = asset.id;
        a["type"] = asset.type;
        a["path"] = asset.path;
        a["shader"] = asset.shaderName;
        a["source"] = asset.source;
        a["materials"] = json::array();
        for (const ReplayMaterialReference& material : asset.materials)
            a["materials"].push_back(materialJson(material));

        assetsJson.push_back(a);
    }

    j["assets"] = assetsJson;
    j["world"]["mapAssetId"] = mWorld.mapAssetId;
    j["world"]["mapPath"] = mWorld.mapPath;
    j["world"]["materials"] = json::array();
    for (const ReplayMaterialReference& material : mWorld.materials)
        j["world"]["materials"].push_back(materialJson(material));

    j["lighting"]["directionalLight"] = vec3Json(mLighting.directionalLight);
    j["lighting"]["ambientStrength"] = mLighting.ambientStrength;
    j["lighting"]["diffuseStrength"] = mLighting.diffuseStrength;
    j["lighting"]["edgeDarkness"] = mLighting.edgeDarkness;
    j["lighting"]["edgeWidth"] = mLighting.edgeWidth;
    j["lighting"]["aoDarkness"] = mLighting.aoDarkness;
    j["lighting"]["aoContrast"] = mLighting.aoContrast;
    j["lighting"]["textureContrast"] = mLighting.textureContrast;
    j["lighting"]["textureBrightness"] = mLighting.textureBrightness;

    j["soundEvents"] = json::array();
    j["killfeedEvents"] = json::array();
    j["events"] = json::array();
    json eventCounts = json::object();
    for (const ReplaySoundEvent& sound : mSoundEvents) {
        json soundJson = {
            {"type", "sound"},
            {"tick", sound.tick},
            {"spawnTick", sound.tick},
            {"soundPath", sound.soundPath},
            {"world", sound.world},
            {"position", vec3Json(sound.position)},
            {"volume", sound.volume},
            {"pitch", sound.pitch},
            {"maxDistance", sound.maxDistance}
        };
        j["soundEvents"].push_back(soundJson);
        j["events"].push_back(soundJson);
        eventCounts["sound"] = eventCounts.value("sound", 0) + 1;
    }
    for (const ReplayKillfeedEvent& kf : mKillfeedEvents) {
        json kfJson = {
            {"type", "killfeed"},
            {"tick", kf.tick},
            {"killerId", kf.killerId},
            {"killerName", kf.killerName},
            {"victimId", kf.victimId},
            {"victimName", kf.victimName},
            {"weaponName", kf.weaponName}
        };
        j["killfeedEvents"].push_back(kfJson);
        j["events"].push_back(kfJson);
        eventCounts["killfeed"] = eventCounts.value("killfeed", 0) + 1;
    }
    for (uint32_t si = 0; si < mSceneFrameCount; ++si) {
        const ReplaySceneFrame& sceneFrame = sceneFrameAt(si);
        for (const ReplayEffectEvent& effect : sceneFrame.effects) {
            json event = effectJson(effect);
            event["tick"] = effect.spawnTick;
            j["events"].push_back(event);
            eventCounts[effect.type] = eventCounts.value(effect.type, 0) + 1;
            Debug::log(Debug::Category::Replay,
                "[REPLAY EFFECT] serialized type=%s tick=%d pos=(%.2f %.2f %.2f) scale=(%.2f %.2f %.2f) alpha=%.2f\n",
                effect.type.c_str(), effect.spawnTick,
                effect.position.x, effect.position.y, effect.position.z,
                effect.scale.x, effect.scale.y, effect.scale.z,
                effect.alpha);
        }
    }
    std::sort(j["events"].begin(), j["events"].end(), [](const json& a, const json& b) {
        return a.value("tick", a.value("spawnTick", 0)) <
               b.value("tick", b.value("spawnTick", 0));
    });
    j["eventCounts"] = eventCounts;

    json framesJson = json::array();
    for (const auto& rf : mFrames) {
        json f;
        f["tick"] = rf.tick;
        f["moveX"] = rf.inputs.moveX;
        f["moveY"] = rf.inputs.moveY;
        f["jump"] = rf.inputs.jump;
        f["jumpPressed"] = rf.inputs.jumpPressed;
        f["dashPressed"] = rf.inputs.dashPressed;
        f["groundReturnPressed"] = rf.inputs.groundReturnPressed;
        f["freezeHeld"] = rf.inputs.freezeHeld;
        f["movementPressed"] = rf.inputs.movementPressed;
        f["reloadPressed"] = rf.inputs.reloadPressed;
        f["lookYaw"] = rf.inputs.lookYaw;
        f["lookPitch"] = rf.inputs.lookPitch;
        framesJson.push_back(f);
    }
    j["frames"] = framesJson;

    json sceneFramesJson = json::array();

    for (uint32_t si = 0; si < mSceneFrameCount; ++si) {
        const auto& sf = sceneFrameAt(si);
        json f;
        f["tick"] = sf.tick;
        f["time"] = sf.time;

        f["camera"]["position"] = {
            sf.camera.position.x,
            sf.camera.position.y,
            sf.camera.position.z
        };

        f["camera"]["rotation"] = {
            sf.camera.rotation.x,
            sf.camera.rotation.y,
            sf.camera.rotation.z
        };

        f["camera"]["fov"] = sf.camera.fov;

        json actorsJson = json::array();

        for (const auto& actor : sf.actors) {
            json a;

            a["id"] = actor.id;
            a["name"] = actor.name;
            a["type"] = actor.type;
            a["modelPath"] = actor.modelPath;
            a["weaponModelPath"] = actor.weaponModelPath;
            a["outfitPath"] = actor.outfitPath;

            a["position"] = {
                actor.position.x,
                actor.position.y,
                actor.position.z
            };

            a["rotation"] = {
                actor.rotation.x,
                actor.rotation.y,
                actor.rotation.z
            };

            a["velocity"] = {
                actor.velocity.x,
                actor.velocity.y,
                actor.velocity.z
            };

            a["health"] = actor.health;
            a["maxHealth"] = actor.maxHealth;
            a["currentAmmo"] = actor.currentAmmo;
            a["reserveAmmo"] = actor.reserveAmmo;
            a["dead"] = actor.dead;
            a["weaponName"] = actor.weaponName;
            a["shooting"] = actor.shooting;
            a["reloading"] = actor.reloading;
            a["grounded"] = actor.grounded;
            a["bodyParts"] = json::object();
            for (int i = 0; i < actor.bodyPartCount; ++i) {
                const ReplayBodyPartState& part = actor.bodyParts[i];
                a["bodyParts"][part.name] = {
                    {"position", vec3Json(part.position)},
                    {"rotation", {part.rotation.w, part.rotation.x, part.rotation.y, part.rotation.z}},
                    {"scale", vec3Json(part.scale)}
                };
            }

            actorsJson.push_back(a);
        }

        f["actors"] = actorsJson;
        f["effects"] = json::array();
        for (const ReplayEffectEvent& effect : sf.effects)
            f["effects"].push_back(effectJson(effect));
        sceneFramesJson.push_back(f);
    }

    j["sceneFrames"] = sceneFramesJson;

    std::error_code ec;
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path())
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        printf("[REPLAY] Failed to create export directory for %s: %s\n",
               path.c_str(), ec.message().c_str());
        return false;
    }

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        printf("[REPLAY] Failed to write %s\n", path.c_str());
        return false;
    }
    file << j.dump(2);
    file.close();
    if (!file) {
        printf("[REPLAY] Failed while writing %s\n", path.c_str());
        return false;
    }

    // Build temporary vector for validation (ring buffer → vector)
    std::vector<ReplaySceneFrame> tempSceneFrames(mSceneFrameCount);
    for (uint32_t i = 0; i < mSceneFrameCount; ++i)
        tempSceneFrames[i] = sceneFrameAt(i);
    const json validation =
        buildValidationJson(path, mHeader, tempSceneFrames);
    std::ofstream validationFile(validationPath);
    if (!validationFile.is_open()) {
        printf("[REPLAY VALIDATION] Failed to write %s\n",
               validationPath.c_str());
        return false;
    }
    validationFile << validation.dump(2);
    validationFile.close();
    if (!validationFile) {
        printf("[REPLAY VALIDATION] Failed while writing %s\n",
               validationPath.c_str());
        return false;
    }

    printf("[REPLAY] Exported %u input frames and %u scene frames to %s\n",
           mHeader.tickCount, mSceneFrameCount, path.c_str());
    printf("[REPLAY VALIDATION] Exported %u authoritative frames to %s\n",
           mSceneFrameCount, validationPath.c_str());
    return true;
}

bool ReplayRecorder::exportToBinary(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("[REPLAY] Failed to write %s\n", path.c_str());
        return false;
    }

    ReplayHeader header = mHeader;
    header.tickCount = (uint32_t)mFrames.size();
    file.write((const char*)&header, sizeof(header));

    for (const auto& rf : mFrames) {
        file.write((const char*)&rf, sizeof(rf));
    }

    printf("[REPLAY] Exported %u frames (binary) to %s\n", header.tickCount, path.c_str());
    return true;
}
