// C:\important\mimita-priv-v8\src\replay\replay.cpp
// 6 7 2026
/** purpose
 * fragmovie
 */

#include "replay.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "entities/player.h"

using json = nlohmann::json;

namespace {
ReplayRecorder* gActiveReplayRecorder = nullptr;

json vec3Json(const glm::vec3& value)
{
    return {value.x, value.y, value.z};
}

json vec4Json(const glm::vec4& value)
{
    return {value.x, value.y, value.z, value.w};
}

json materialJson(const ReplayMaterialReference& material)
{
    return {
        {"name", material.materialName},
        {"texturePath", material.texturePath},
        {"shader", material.shaderName}
    };
}

json effectJson(const ReplayEffectEvent& effect)
{
    return {
        {"type", effect.type},
        {"position", vec3Json(effect.position)},
        {"direction", vec3Json(effect.direction)},
        {"from", vec3Json(effect.from)},
        {"to", vec3Json(effect.to)},
        {"rotation", vec3Json(effect.rotation)},
        {"scale", vec3Json(effect.scale)},
        {"endScale", vec3Json(effect.endScale)},
        {"color", vec4Json(effect.color)},
        {"velocity", vec3Json(effect.velocity)},
        {"normal", vec3Json(effect.normal)},
        {"spawnTick", effect.spawnTick},
        {"spawnTime", effect.spawnTime},
        {"startDelay", effect.startDelay},
        {"lifetime", effect.lifetime},
        {"alpha", effect.alpha},
        {"radius", effect.radius},
        {"thickness", effect.thickness},
        {"endThickness", effect.endThickness},
        {"gravity", effect.gravity},
        {"assetId", effect.assetId},
        {"assetPath", effect.assetPath},
        {"soundPath", effect.soundPath},
        {"sourceActorId", effect.sourceActorId},
        {"targetActorId", effect.targetActorId},
        {"texturePath", effect.texturePath},
        {"material", effect.materialName}
    };
}
}

void setActiveReplayRecorder(ReplayRecorder* recorder)
{
    gActiveReplayRecorder = recorder;
}

void captureReplayEffect(const ReplayEffectEvent& event)
{
    if (gActiveReplayRecorder && gActiveReplayRecorder->isRecording())
        gActiveReplayRecorder->recordEffectEvent(event);
}

void captureReplaySound(const ReplaySoundEvent& event)
{
    if (gActiveReplayRecorder && gActiveReplayRecorder->isRecording())
        gActiveReplayRecorder->recordSoundEvent(event);
}

std::vector<ReplayBodyPartState> captureReplayBodyParts(const Player& player)
{
    std::vector<ReplayBodyPartState> states;
    states.reserve(player.physicalBody.parts.size());

    // Compute player root world transform: T(pos) * R_z(yaw)
    glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), player.pos)
        * glm::mat4_cast(glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat4 invRootWorld = glm::inverse(rootWorld);

    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name != "head" && part.name != "torso" &&
            part.name != "leftArm" && part.name != "rightArm" &&
            part.name != "leftLeg" && part.name != "rightLeg")
            continue;

        // Convert from world-space to local-space relative to player root,
        // so Blender can apply them as proper local transforms under the actor root.
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
        // Exaggerate rotations by 1.5x for more readable slow-mo playback
        glm::vec3 euler = glm::degrees(glm::eulerAngles(glm::normalize(orientation)));
        euler *= 1.5f;
        state.rotation = euler;
        state.scale = scale;
        states.push_back(state);
    }

    return states;
}

std::string generateReplayExportPath()
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char dateDirectory[32];
    char fileName[48];
    std::strftime(dateDirectory, sizeof(dateDirectory), "%m-%d-%Y", &localTime);
    std::strftime(fileName, sizeof(fileName), "%H-%M-%S-replay.json", &localTime);
    return (std::filesystem::path("replays") / dateDirectory / fileName).string();
}

// ============================================================
// ReplayRecorder
// ============================================================

void ReplayRecorder::beginRecording(float randomSeed, const char* mapName) {
    mFrames.clear();
    mSceneFrames.clear();
    mAssets.clear();
    mSoundEvents.clear();
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
    mFrames.push_back(rf);
}

void ReplayRecorder::recordSceneFrame(const ReplaySceneFrame& inputFrame)
{
    if (!mRecording) return;

    ReplaySceneFrame frame = inputFrame;
    frame.effects.insert(frame.effects.end(), mPendingEffects.begin(), mPendingEffects.end());
    mPendingEffects.clear();
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
    registerAsset("sound:" + event.soundPath, "sound", event.soundPath, {}, {}, "audio");
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

bool ReplayRecorder::exportToJSON(const std::string& path) const {
    json j;

    // Header
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

    // Frames
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
        // ADD THESE 6 7 2026 
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
                    {"rotation", vec3Json(part.rotation)},
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
    printf("[REPLAY] Exported %u input frames and %zu scene frames to %s\n",
           mHeader.tickCount, mSceneFrames.size(), path.c_str());
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

// ============================================================
// ReplayPlayer
// ============================================================

bool ReplayPlayer::loadFromJSON(const std::string& path) {
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
            // ADD THESE
            rf.inputs.movementPressed = f.value("movementPressed", false);
            rf.inputs.reloadPressed = f.value("reloadPressed", false);
            rf.inputs.lookYaw = f.value("lookYaw", 0.0f);
            rf.inputs.lookPitch = f.value("lookPitch", 0.0f);
            mFrames.push_back(rf);
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

void ReplayPlayer::beginPlayback() {
    mPlaying = true;
    mCurrentTick = 0;
    printf("[REPLAY] Playback started  ticks=%u\n", mHeader.tickCount);
}

void ReplayPlayer::stopPlayback() {
    mPlaying = false;
    printf("[REPLAY] Playback stopped at tick %u\n", mCurrentTick);
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

void ReplayPlayer::seekToTick(uint32_t tick) {
    mCurrentTick = std::min(tick, (uint32_t)mFrames.size());
}
