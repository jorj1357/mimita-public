#include "replay.h"
#include "replay-io.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        json entry = {
            {"tick", sound.tick}, {"soundPath", sound.soundPath},
            {"world", sound.world}, {"position", vec3Json(sound.position)},
            {"volume", sound.volume}, {"pitch", sound.pitch},
            {"maxDistance", sound.maxDistance}
        };
        if (sound.listenerValid) {
            entry["listenerPosition"] = vec3Json(sound.listenerPosition);
            entry["listenerForward"] = vec3Json(sound.listenerForward);
        }
        root["soundEvents"].push_back(std::move(entry));
    }

    root["killfeedEvents"] = json::array();
    for (const ReplayKillfeedEvent& kf : killfeedEvents) {
        root["killfeedEvents"].push_back({
            {"tick", kf.tick},
            {"killerId", kf.killerId},
            {"killerName", kf.killerName},
            {"victimId", kf.victimId},
            {"victimName", kf.victimName},
            {"weaponName", kf.weaponName}
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
            if (value.contains("listenerPosition")) {
                sound.listenerPosition = jsonVec3(value["listenerPosition"]);
                sound.listenerForward = jsonVec3(value.value("listenerForward", json::array({0.0f, 1.0f, 0.0f})));
                sound.listenerValid = true;
            }
            soundEvents.push_back(std::move(sound));
        }

        killfeedEvents.clear();
        for (const json& value : root.value("killfeedEvents", json::array())) {
            ReplayKillfeedEvent kf;
            kf.tick = value.value("tick", 0);
            kf.killerId = value.value("killerId", "");
            kf.killerName = value.value("killerName", "");
            kf.victimId = value.value("victimId", "");
            kf.victimName = value.value("victimName", "");
            kf.weaponName = value.value("weaponName", "");
            killfeedEvents.push_back(std::move(kf));
        }

        header.tickCount = sceneFrames.empty() ? (uint32_t)frames.size()
                                                : (uint32_t)sceneFrames.size();
        return !sceneFrames.empty();
    } catch (const std::exception& e) {
        printf("[REPLAY] clip load failed %s: %s\n", path.c_str(), e.what());
        return false;
    }
}
