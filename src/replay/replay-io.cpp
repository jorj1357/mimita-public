#include "replay.h"
#include "replay-io.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <glm/gtx/quaternion.hpp>

using json = nlohmann::json;

json vec3Json(const glm::vec3& value)
{
    return {value.x, value.y, value.z};
}

json vec4Json(const glm::vec4& value)
{
    return {value.x, value.y, value.z, value.w};
}

glm::vec3 jsonVec3(const json& value, const glm::vec3& fallback)
{
    if (!value.is_array() || value.size() < 3)
        return fallback;
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

ReplayEffectEvent parseEffect(const json& value)
{
    ReplayEffectEvent effect;
    effect.type = value.value("type", "");
    effect.label = value.value("label", "");
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
    actor.characterName = value.value("characterName", "");
    actor.avatarName = value.value("avatarName", "");
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
        {"characterName", actor.characterName}, {"avatarName", actor.avatarName},
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
    json entry = {
        {"type", effect.type},
        {"label", effect.label},
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
    return entry;
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

    for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir, ec)) {
        if (ec || !entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        if (name.find("-validation") != std::string::npos)
            continue;
        if (name == "replay_editor_session.json")
            continue;
        if (name.rfind(".rple.json") != std::string::npos)
            continue;
        if (name.find(".autosave.json") != std::string::npos)
            continue;
        if (name == "replay-export.json")
            continue;
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


