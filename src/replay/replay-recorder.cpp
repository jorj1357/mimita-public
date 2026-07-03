#include "replay.h"
#include "replay-io.h"

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

std::vector<ReplayBodyPartState> captureReplayBodyParts(const Player& player)
{
    std::vector<ReplayBodyPartState> states;
    states.reserve(player.physicalBody.parts.size());

    glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), player.pos)
        * glm::mat4_cast(glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat4 invRootWorld = glm::inverse(rootWorld);

    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name != "head" && part.name != "torso" &&
            part.name != "leftArm" && part.name != "rightArm" &&
            part.name != "leftLeg" && part.name != "rightLeg")
            continue;

        glm::mat4 localTransform = invRootWorld * part.worldTransform;

        glm::vec3 scale(1.0f);
        glm::quat orientation;
        glm::vec3 translation(0.0f);
        glm::vec3 skew(0.0f);
        glm::vec4 perspective(0.0f);
        glm::decompose(localTransform,
                       scale, orientation, translation, skew, perspective);

        ReplayBodyPartState state;
        state.name = part.name;
        state.position = translation;
        state.rotation = glm::normalize(orientation);
        state.scale = scale;
        states.push_back(state);
    }

    return states;
}

// ============================================================
// ReplayRecorder
// ============================================================

void ReplayRecorder::beginRecording(float randomSeed, const char* mapName) {
    mFrames.clear();
    mSceneFrames.clear();
    mAssets.clear();
    mSoundEvents.clear();
    mKillfeedEvents.clear();
    mPendingEffects.clear();
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

void ReplayRecorder::recordFrame(const InputFrame& frame) {
    if (!mRecording) return;

    ReplayFrame rf;
    rf.tick = mTick++;
    mEventTick = rf.tick;
    rf.inputs = frame;
    if (mMaxTicks > 0 && mFrames.size() >= mMaxTicks)
        mFrames.erase(mFrames.begin());
    mFrames.push_back(rf);
    mHeader.tickCount = (uint32_t)mFrames.size();
}

void ReplayRecorder::recordSceneFrame(const ReplaySceneFrame& inputFrame)
{
    if (!mRecording) return;

    ReplaySceneFrame frame = inputFrame;
    frame.effects.insert(frame.effects.end(), mPendingEffects.begin(), mPendingEffects.end());
    mPendingEffects.clear();
    if (mMaxTicks > 0 && mSceneFrames.size() >= mMaxTicks)
        mSceneFrames.erase(mSceneFrames.begin());
    mSceneFrames.push_back(frame);
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
    mWorld = world;
}

void ReplayRecorder::setLighting(const ReplayLightingState& lighting)
{
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
    j["header"]["sceneFrameCount"] = mSceneFrames.size();
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
    for (const ReplaySceneFrame& sceneFrame : mSceneFrames) {
        for (const ReplayEffectEvent& effect : sceneFrame.effects) {
            json event = effectJson(effect);
            event["tick"] = effect.spawnTick;
            j["events"].push_back(event);
            eventCounts[effect.type] = eventCounts.value(effect.type, 0) + 1;
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

    for (const auto& sf : mSceneFrames) {
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
            a["collidable"] = actor.collidable;
            a["fade"] = actor.fade;
            a["blackness"] = actor.blackness;
            a["animationState"] = actor.animationState;
            a["bodyParts"] = json::object();
            for (const ReplayBodyPartState& part : actor.bodyParts) {
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

    const json validation =
        buildValidationJson(path, mHeader, mSceneFrames);
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

    printf("[REPLAY] Exported %u input frames and %zu scene frames to %s\n",
           mHeader.tickCount, mSceneFrames.size(), path.c_str());
    printf("[REPLAY VALIDATION] Exported %zu authoritative frames to %s\n",
           mSceneFrames.size(), validationPath.c_str());
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
