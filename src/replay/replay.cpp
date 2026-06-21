// C:\important\mimita-priv-v8\src\replay\replay.cpp
// 6 7 2026
/** purpose
 * fragmovie
 */

#include "replay.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "entities/player.h"
#include "camera.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {
ReplayRecorder* gActiveReplayRecorder = nullptr;
ReplayClipSaver* gActiveReplayClipSaver = nullptr;
bool gReplayCaptureEnabled = true;
} // anonymous namespace

// Outside anonymous namespace so the extern declaration in replay.h links correctly
ReplayFactoryNotifyFn gReplayFactoryNotifyFn = nullptr;

namespace {

json vec3Json(const glm::vec3& value)
{
    return {value.x, value.y, value.z};
}

json vec4Json(const glm::vec4& value)
{
    return {value.x, value.y, value.z, value.w};
}

glm::vec3 jsonVec3(const json& value, const glm::vec3& fallback = {})
{
    if (!value.is_array() || value.size() < 3)
        return fallback;
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

ReplayEffectEvent parseEffect(const json& value)
{
    ReplayEffectEvent effect;
    effect.type = value.value("type", "");
    effect.position = jsonVec3(value.value("position", json::array()));
    effect.direction = jsonVec3(value.value("direction", json::array()));
    effect.from = jsonVec3(value.value("from", json::array()));
    effect.to = jsonVec3(value.value("to", json::array()));
    effect.rotation = jsonVec3(value.value("rotation", json::array()));
    effect.scale = jsonVec3(value.value("scale", json::array()), glm::vec3(1.0f));
    effect.endScale = jsonVec3(value.value("endScale", json::array()), glm::vec3(1.0f));
    effect.velocity = jsonVec3(value.value("velocity", json::array()));
    effect.normal = jsonVec3(value.value("normal", json::array()), glm::vec3(0, 0, 1));
    if (value.contains("color") && value["color"].is_array() && value["color"].size() >= 4)
        effect.color = {value["color"][0].get<float>(), value["color"][1].get<float>(),
                        value["color"][2].get<float>(), value["color"][3].get<float>()};
    effect.spawnTick = value.value("spawnTick", value.value("tick", 0));
    effect.spawnTime = value.value("spawnTime", 0.0f);
    effect.startDelay = value.value("startDelay", 0.0f);
    effect.lifetime = value.value("lifetime", 0.0f);
    effect.alpha = value.value("alpha", 1.0f);
    effect.radius = value.value("radius", 0.0f);
    effect.thickness = value.value("thickness", 0.0f);
    effect.endThickness = value.value("endThickness", 0.0f);
    effect.gravity = value.value("gravity", 0.0f);
    effect.assetId = value.value("assetId", "");
    effect.assetPath = value.value("assetPath", "");
    effect.soundPath = value.value("soundPath", "");
    effect.sourceActorId = value.value("sourceActorId", "");
    effect.targetActorId = value.value("targetActorId", "");
    effect.texturePath = value.value("texturePath", "");
    effect.materialName = value.value("material", "");
    return effect;
}

ReplayActorState parseActor(const json& value)
{
    ReplayActorState actor;
    actor.id = value.value("id", "");
    actor.name = value.value("name", "");
    actor.type = value.value("type", "");
    actor.modelPath = value.value("modelPath", "");
    actor.weaponModelPath = value.value("weaponModelPath", "");
    actor.outfitPath = value.value("outfitPath", "");
    actor.position = jsonVec3(value.value("position", json::array()));
    actor.rotation = jsonVec3(value.value("rotation", json::array()));
    actor.velocity = jsonVec3(value.value("velocity", json::array()));
    actor.health = value.value("health", 100);
    actor.maxHealth = value.value("maxHealth", 100);
    actor.currentAmmo = value.value("currentAmmo", 0);
    actor.reserveAmmo = value.value("reserveAmmo", 0);
    actor.dead = value.value("dead", false);
    actor.shooting = value.value("shooting", false);
    actor.reloading = value.value("reloading", false);
    actor.grounded = value.value("grounded", false);
    actor.collidable = value.value("collidable", true);
    actor.fade = value.value("fade", 0.0f);
    actor.blackness = value.value("blackness", 0.0f);
    actor.weaponName = value.value("weaponName", "");
    actor.animationState = value.value("animationState", "");
    if (value.contains("bodyParts") && value["bodyParts"].is_object()) {
        for (auto it = value["bodyParts"].begin(); it != value["bodyParts"].end(); ++it) {
            ReplayBodyPartState part;
            part.name = it.key();
            part.position = jsonVec3(it->value("position", json::array()));
            if (it->contains("rotation") && (*it)["rotation"].is_array() && (*it)["rotation"].size() >= 4) {
                auto& r = (*it)["rotation"];
                part.rotation = glm::quat(r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
            }
            part.scale = jsonVec3(it->value("scale", json::array()), glm::vec3(1.0f));
            actor.bodyParts.push_back(std::move(part));
        }
    }
    return actor;
}

json actorJson(const ReplayActorState& actor)
{
    json value = {
        {"id", actor.id}, {"name", actor.name}, {"type", actor.type},
        {"modelPath", actor.modelPath}, {"weaponModelPath", actor.weaponModelPath},
        {"outfitPath", actor.outfitPath},
        {"position", vec3Json(actor.position)}, {"rotation", vec3Json(actor.rotation)},
        {"velocity", vec3Json(actor.velocity)}, {"health", actor.health},
        {"maxHealth", actor.maxHealth}, {"currentAmmo", actor.currentAmmo},
        {"reserveAmmo", actor.reserveAmmo}, {"dead", actor.dead},
        {"shooting", actor.shooting},
        {"reloading", actor.reloading}, {"grounded", actor.grounded},
        {"collidable", actor.collidable}, {"fade", actor.fade},
        {"blackness", actor.blackness}, {"weaponName", actor.weaponName},
        {"animationState", actor.animationState}
    };
    value["bodyParts"] = json::object();
    for (const ReplayBodyPartState& part : actor.bodyParts) {
        value["bodyParts"][part.name] = {
            {"position", vec3Json(part.position)},
            {"rotation", {part.rotation.w, part.rotation.x, part.rotation.y, part.rotation.z}},
            {"scale", vec3Json(part.scale)}
        };
    }
    return value;
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

json transformJson(
    const glm::vec3& position,
    const glm::vec3& rotation,
    const glm::vec3& scale = glm::vec3(1.0f))
{
    return {
        {"position", vec3Json(position)},
        {"rotation", vec3Json(rotation)},
        {"scale", vec3Json(scale)}
    };
}

json buildValidationJson(
    const std::string& replayPath,
    const ReplayHeader& header,
    const std::vector<ReplaySceneFrame>& sceneFrames)
{
    json validation;
    validation["schemaVersion"] = 1;
    validation["sourceReplay"] =
        std::filesystem::path(replayPath).filename().string();
    validation["tickRate"] = header.tickRate;
    validation["coordinateSystem"] = "Z_UP";
    validation["distanceUnit"] = "meters";
    validation["rotationUnit"] = "degrees";
    validation["transformSpaces"] = {
        {"root", "world"},
        {"bodyParts", "actor_root_local"}
    };
    validation["thresholds"] = {
        {"positionMeters", 0.05f},
        {"rotationDegrees", 5.0f}
    };
    validation["channels"] = {
        {"transforms", {{"version", 1}, {"enabled", true}}}
    };
    validation["frames"] = json::array();

    for (const ReplaySceneFrame& sceneFrame : sceneFrames) {
        json frame;
        frame["tick"] = sceneFrame.tick;
        frame["actors"] = json::array();

        for (const ReplayActorState& actor : sceneFrame.actors) {
            json actorJson;
            actorJson["id"] = actor.id;
            actorJson["type"] = actor.type;
            actorJson["root"] = transformJson(
                actor.position, actor.rotation);
            actorJson["bodyParts"] = json::object();
            for (const ReplayBodyPartState& part : actor.bodyParts) {
                glm::vec3 euler = glm::degrees(glm::eulerAngles(part.rotation));
                actorJson["bodyParts"][part.name] = transformJson(
                    part.position, euler, part.scale);
            }
            frame["actors"].push_back(std::move(actorJson));
        }

        validation["frames"].push_back(std::move(frame));
    }

    return validation;
}
}

void setActiveReplayRecorder(ReplayRecorder* recorder)
{
    gActiveReplayRecorder = recorder;
}

void setReplayCaptureEnabled(bool enabled)
{
    gReplayCaptureEnabled = enabled;
}

void setActiveReplayClipSaver(ReplayClipSaver* saver)
{
    gActiveReplayClipSaver = saver;
}

void notifyReplayKill(const std::string& killerId,
                      const std::string& victimId,
                      bool roundWinning)
{
    if (gActiveReplayClipSaver)
        gActiveReplayClipSaver->notifyKill(killerId, victimId, roundWinning);
    if (gReplayFactoryNotifyFn)
        gReplayFactoryNotifyFn(killerId, victimId, false, false, roundWinning);
}

void setReplayFactoryNotifyFn(ReplayFactoryNotifyFn fn)
{
    gReplayFactoryNotifyFn = fn;
}

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
        state.rotation = glm::normalize(orientation);
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

std::string generateReplayValidationPath(const std::string& replayPath)
{
    std::filesystem::path path(replayPath);
    const std::string extension = path.extension().string();
    path.replace_filename(
        path.stem().string() + "-validation" +
        (extension.empty() ? ".json" : extension));
    return path.string();
}

std::string generateReplayClipPath()
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[64];
    std::strftime(fileName, sizeof(fileName), "%Y-%m-%d_%H-%M-%S-kill.mclip.json", &localTime);
    return (std::filesystem::path("replays") / "clips" / fileName).string();
}

std::string generateInstantReplayPath()
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[80];
    std::strftime(fileName, sizeof(fileName), "instant-replay-%Y-%m-%d_%H-%M-%S.json", &localTime);
    return (std::filesystem::path("replays") / "instants" / fileName).string();
}

std::vector<std::string> listReplayClips()
{
    std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;
    std::error_code ec;
    const std::filesystem::path baseDir = std::filesystem::path("replays");
    if (!std::filesystem::exists(baseDir, ec))
        return {};

    // Search recursively through all replay subdirectories (e.g. replays/06-12-2026/)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir, ec)) {
        if (ec || !entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        // Skip validation companion files
        if (name.find("-validation") != std::string::npos)
            continue;
        // Match .json replay files (full replays) and .mclip.json clips
        if ((name.size() > 5 && name.rfind(".json") == name.size() - 5) ||
            (name.size() > 11 && name.rfind(".mclip.json") == name.size() - 11)) {
            found.push_back({entry.last_write_time(ec), entry.path().string()});
        }
    }
    std::sort(found.begin(), found.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::string> paths;
    paths.reserve(found.size());
    for (const auto& item : found)
        paths.push_back(item.second);
    return paths;
}

bool ReplayClip::save(const std::string& path) const
{
    json root;
    root["metadata"] = {
        {"format", "mimita-in-engine-clip"},
        {"version", 1},
        {"mapPath", mapPath},
        {"killerId", killerId},
        {"victimId", victimId},
        {"weaponId", weaponId},
        {"killTick", killTick},
        {"killDistance", killDistance},
        {"roundWinning", roundWinning}
    };
    root["header"] = {
        {"version", header.version},
        {"tickCount", header.tickCount},
        {"tickRate", header.tickRate},
        {"mapName", std::string(header.mapName)},
        {"timestamp", header.timestamp},
        {"playerName", std::string(header.playerName)}
    };
    root["frames"] = json::array();
    for (const ReplayFrame& frame : frames) {
        root["frames"].push_back({
            {"tick", frame.tick},
            {"moveX", frame.inputs.moveX}, {"moveY", frame.inputs.moveY},
            {"jump", frame.inputs.jump}, {"jumpPressed", frame.inputs.jumpPressed},
            {"dashPressed", frame.inputs.dashPressed},
            {"groundReturnPressed", frame.inputs.groundReturnPressed},
            {"freezeHeld", frame.inputs.freezeHeld},
            {"movementPressed", frame.inputs.movementPressed},
            {"reloadPressed", frame.inputs.reloadPressed},
            {"lookYaw", frame.inputs.lookYaw}, {"lookPitch", frame.inputs.lookPitch}
        });
    }
    root["sceneFrames"] = json::array();
    for (const ReplaySceneFrame& frame : sceneFrames) {
        json value = {
            {"tick", frame.tick}, {"time", frame.time},
            {"camera", {
                {"position", vec3Json(frame.camera.position)},
                {"rotation", vec3Json(frame.camera.rotation)},
                {"fov", frame.camera.fov}
            }}
        };
        value["actors"] = json::array();
        for (const ReplayActorState& actor : frame.actors)
            value["actors"].push_back(actorJson(actor));
        value["effects"] = json::array();
        for (const ReplayEffectEvent& effect : frame.effects)
            value["effects"].push_back(effectJson(effect));
        root["sceneFrames"].push_back(std::move(value));
    }
    root["soundEvents"] = json::array();
    for (const ReplaySoundEvent& sound : soundEvents) {
        root["soundEvents"].push_back({
            {"tick", sound.tick}, {"soundPath", sound.soundPath},
            {"world", sound.world}, {"position", vec3Json(sound.position)},
            {"volume", sound.volume}, {"pitch", sound.pitch},
            {"maxDistance", sound.maxDistance}
        });
    }

    std::error_code ec;
    const std::filesystem::path output(path);
    if (output.has_parent_path())
        std::filesystem::create_directories(output.parent_path(), ec);
    std::ofstream file(output);
    if (ec || !file.is_open())
        return false;
    file << root.dump(2);
    return (bool)file;
}

bool ReplayClip::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    try {
        json root;
        file >> root;
        const json metadata = root.value("metadata", json::object());
        mapPath = metadata.value("mapPath", "");
        killerId = metadata.value("killerId", "");
        victimId = metadata.value("victimId", "");
        weaponId = metadata.value("weaponId", "");
        killTick = metadata.value("killTick", 0u);
        killDistance = metadata.value("killDistance", 0.0f);
        roundWinning = metadata.value("roundWinning", false);
        const json h = root.value("header", json::object());
        header = {};
        header.version = h.value("version", 1u);
        header.tickRate = h.value("tickRate", 60u);
        header.timestamp = h.value("timestamp", 0ULL);
        const std::string mapName = h.value("mapName", "");
        const std::string playerName = h.value("playerName", "");
        std::strncpy(header.mapName, mapName.c_str(), sizeof(header.mapName) - 1);
        std::strncpy(header.playerName, playerName.c_str(), sizeof(header.playerName) - 1);

        frames.clear();
        for (const json& value : root.value("frames", json::array())) {
            ReplayFrame frame;
            frame.tick = value.value("tick", 0u);
            frame.inputs.moveX = value.value("moveX", 0.0f);
            frame.inputs.moveY = value.value("moveY", 0.0f);
            frame.inputs.jump = value.value("jump", false);
            frame.inputs.jumpPressed = value.value("jumpPressed", false);
            frame.inputs.dashPressed = value.value("dashPressed", false);
            frame.inputs.groundReturnPressed = value.value("groundReturnPressed", false);
            frame.inputs.freezeHeld = value.value("freezeHeld", false);
            frame.inputs.movementPressed = value.value("movementPressed", false);
            frame.inputs.reloadPressed = value.value("reloadPressed", false);
            frame.inputs.lookYaw = value.value("lookYaw", 0.0f);
            frame.inputs.lookPitch = value.value("lookPitch", 0.0f);
            frames.push_back(frame);
        }

        sceneFrames.clear();
        for (const json& value : root.value("sceneFrames", json::array())) {
            ReplaySceneFrame frame;
            frame.tick = value.value("tick", 0);
            frame.time = value.value("time", 0.0f);
            const json camera = value.value("camera", json::object());
            frame.camera.position = jsonVec3(camera.value("position", json::array()));
            frame.camera.rotation = jsonVec3(camera.value("rotation", json::array()));
            frame.camera.fov = camera.value("fov", 70.0f);
            for (const json& actor : value.value("actors", json::array()))
                frame.actors.push_back(parseActor(actor));
            for (const json& effect : value.value("effects", json::array()))
                frame.effects.push_back(parseEffect(effect));
            sceneFrames.push_back(std::move(frame));
        }

        soundEvents.clear();
        for (const json& value : root.value("soundEvents", json::array())) {
            ReplaySoundEvent sound;
            sound.tick = value.value("tick", 0);
            sound.soundPath = value.value("soundPath", "");
            sound.world = value.value("world", false);
            sound.position = jsonVec3(value.value("position", json::array()));
            sound.volume = value.value("volume", 1.0f);
            sound.pitch = value.value("pitch", 1.0f);
            sound.maxDistance = value.value("maxDistance", 0.0f);
            soundEvents.push_back(std::move(sound));
        }
        header.tickCount = sceneFrames.empty() ? (uint32_t)frames.size()
                                                : (uint32_t)sceneFrames.size();
        return !sceneFrames.empty();
    } catch (const std::exception& e) {
        printf("[REPLAY] clip load failed %s: %s\n", path.c_str(), e.what());
        return false;
    }
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

ReplayClip ReplayRecorder::makeClip(
    uint32_t startTick, uint32_t endTick, uint32_t killTick,
    const std::string& killerId, const std::string& victimId) const
{
    ReplayClip clip;
    clip.header = mHeader;
    clip.header.tickCount = 0;
    clip.mapPath = mWorld.mapPath;
    clip.killerId = killerId;
    clip.victimId = victimId;
    clip.killTick = killTick >= startTick ? killTick - startTick : 0;

    // Determine weapon from the killer actor at kill tick
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

    // Calculate kill distance from positions at kill tick
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

bool ReplayRecorder::exportToJSON(const std::string& path) const {
    json j;
    const std::string validationPath = generateReplayValidationPath(path);

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
        mCurrentSceneFrameIndex = 0;
        if (!mClip.sceneFrames.empty())
            mInterpolatedFrame = mClip.sceneFrames.front();
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

        // Store assets for preload
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

        // Extract outfit path from assets
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
            // ADD THESE
            rf.inputs.movementPressed = f.value("movementPressed", false);
            rf.inputs.reloadPressed = f.value("reloadPressed", false);
            rf.inputs.lookYaw = f.value("lookYaw", 0.0f);
            rf.inputs.lookPitch = f.value("lookPitch", 0.0f);
            mFrames.push_back(rf);
        }

        // Load scene frames (actor positions, camera, etc.)
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
                // Load actors
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
                        // Load body parts
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

        // Load sound events
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
        mCurrentSceneFrameIndex = 0;
        if (!mClip.sceneFrames.empty())
            mInterpolatedFrame = mClip.sceneFrames.front();
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

    // Collect all unique model/weapon paths from actors
    for (const ReplaySceneFrame& frame : mClip.sceneFrames) {
        for (const ReplayActorState& actor : frame.actors) {
            if (!actor.modelPath.empty())
                requiredModels.push_back(actor.modelPath);
            if (!actor.weaponModelPath.empty())
                requiredModels.push_back(actor.weaponModelPath);
        }
    }

    // Deduplicate
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

    // Check outfit texture
    if (!mOutfitPath.empty()) {
        if (!std::filesystem::exists(mOutfitPath)) {
            printf("[REPLAY] MISSING OUTFIT: %s\n", mOutfitPath.c_str());
        } else {
            printf("[REPLAY] Found outfit: %s\n", mOutfitPath.c_str());
        }
    }

    // Check all registered assets
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
    resolveSceneFrameAtPlaybackTick();
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

namespace {
const ReplayActorState* findActor(
    const ReplaySceneFrame& frame, const std::string& id)
{
    auto it = std::find_if(
        frame.actors.begin(), frame.actors.end(),
        [&id](const ReplayActorState& actor) { return actor.id == id; });
    return it == frame.actors.end() ? nullptr : &*it;
}

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
    mPlaybackTick += dt * (float)std::max(mHeader.tickRate, 1u) * mTimescale;
    const int lastTick = mClip.sceneFrames.back().tick;
    if (mPlaybackTick > (float)lastTick) {
        mPlaybackTick = (float)lastTick;
        mCurrentTick = (uint32_t)lastTick;
        mPlaying = false;
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
    if (currentEventTick >= previousTick)
        mLastEventTick = currentEventTick;

    resolveSceneFrameAtPlaybackTick();
    {   static float logTimer = 0.0f; logTimer -= dt;
        if (logTimer <= 0.0f) { logTimer = 1.0f;
            auto* frame = currentSceneFrame();
            printf("[REPLAY] time=%.2f tick=%.1f frame=%d sceneFrameIndex=%u cameraPos=(%.1f %.1f %.1f) actorCount=%zu sceneFrames=%zu\n",
                   mPlaybackTick / (float)std::max(mHeader.tickRate, 1u),
                   mPlaybackTick, frame ? frame->tick : -1,
                   mCurrentSceneFrameIndex,
                   frame ? frame->camera.position.x : 0.0f,
                   frame ? frame->camera.position.y : 0.0f,
                   frame ? frame->camera.position.z : 0.0f,
                   frame ? frame->actors.size() : 0,
                   mClip.sceneFrames.size());
        }
    }
}

void ReplayPlayer::resolveSceneFrameAtPlaybackTick()
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
        mCurrentSceneFrameIndex = 0;
        return;
    }
    if (upper == mClip.sceneFrames.end()) {
        mInterpolatedFrame = mClip.sceneFrames.back();
        mCurrentSceneFrameIndex = (uint32_t)(mClip.sceneFrames.size() - 1);
        return;
    }
    const size_t bIndex = (size_t)std::distance(mClip.sceneFrames.begin(), upper);
    const size_t aIndex = bIndex - 1;
    const ReplaySceneFrame& b = *upper;
    const ReplaySceneFrame& a = *(upper - 1);
    const float span = (float)std::max(1, b.tick - a.tick);
    const float t = glm::clamp((mPlaybackTick - (float)a.tick) / span, 0.0f, 1.0f);
    mCurrentSceneFrameIndex = (uint32_t)(t >= 0.5f ? bIndex : aIndex);
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
    // Clamp against scene frames (visual data) not input frames (which may be empty in clips).
    // Scene frames drive replay rendering; input frames are only for simulation playback.
    size_t maxTicks = mClip.sceneFrames.empty() ? mFrames.size() : mClip.sceneFrames.size();
    uint32_t clampedTick = tick;
    if (!mClip.sceneFrames.empty())
        clampedTick = (uint32_t)std::max(0, std::min((int)tick, mClip.sceneFrames.back().tick));
    else if (maxTicks > 0)
        clampedTick = std::min(tick, (uint32_t)maxTicks - 1u);
    else
        clampedTick = 0;
    mCurrentTick = clampedTick;
    mPlaybackTick = (float)clampedTick;
    mLastEventTick = (int)clampedTick - 1;
    // Ensure playing state so that subsequent update() interpolates frames
    mPlaying = true;
    mPaused = false;
    resolveSceneFrameAtPlaybackTick();
    const ReplaySceneFrame* frame = currentSceneFrame();
    printf("[REPLAY] seekToTick(%u) -> mCurrentTick=%u selectedSceneFrameIndex=%u selectedTick=%d max=%zu frames=%zu scene=%zu playing=1\n",
           tick, mCurrentTick, mCurrentSceneFrameIndex,
           frame ? frame->tick : -1, maxTicks, mFrames.size(), mClip.sceneFrames.size());
}

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
        // Freecam: don't touch camera position/rotation/yaw/pitch.
        // WASD + mouse are handled by the main loop's freecam section.
        // Log once to verify freecam is actually active
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
