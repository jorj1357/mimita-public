#include "network/server-presets.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "utils/path_utils.h"

namespace MimitaNet {
namespace {

constexpr const char* SERVER_PRESETS_PATH = "config/server-presets.json";

bool readBool(const nlohmann::json& object, const char* key, bool fallback)
{
    if (!object.contains(key))
        return fallback;
    if (!object[key].is_boolean())
        throw std::runtime_error(std::string("'") + key + "' must be boolean");
    return object[key].get<bool>();
}

uint32_t readUInt(const nlohmann::json& object, const char* key, uint32_t fallback,
                 uint32_t minimum, uint32_t maximum)
{
    if (!object.contains(key))
        return fallback;
    if (!object[key].is_number_unsigned() && !object[key].is_number_integer())
        throw std::runtime_error(std::string("'") + key + "' must be an integer");
    const int64_t value = object[key].get<int64_t>();
    if (value < static_cast<int64_t>(minimum) || value > static_cast<int64_t>(maximum))
        throw std::runtime_error(std::string("'") + key + "' is out of range");
    return static_cast<uint32_t>(value);
}

std::string readString(const nlohmann::json& object, const char* key,
                       const std::string& fallback)
{
    if (!object.contains(key))
        return fallback;
    if (!object[key].is_string())
        throw std::runtime_error(std::string("'") + key + "' must be a string");
    const std::string value = object[key].get<std::string>();
    if (value.empty())
        throw std::runtime_error(std::string("'") + key + "' cannot be empty");
    return value;
}

} // namespace

bool loadServerPreset(const std::string& id, ServerPreset& out, std::string& error)
{
    error.clear();
    try
    {
        std::ifstream file(resolveAssetPath(SERVER_PRESETS_PATH));
        if (!file.is_open())
            throw std::runtime_error("file not found: " + std::string(SERVER_PRESETS_PATH));

        const nlohmann::json root = nlohmann::json::parse(file, nullptr, true, true);
        if (!root.is_object() || !root.contains("presets") || !root["presets"].is_object())
            throw std::runtime_error("expected an object named 'presets'");
        if (!root["presets"].contains(id))
            throw std::runtime_error("unknown preset '" + id + "'");

        const auto& preset = root["presets"][id];
        if (!preset.is_object())
            throw std::runtime_error("preset '" + id + "' must be an object");

        ServerPreset next;
        next.id = id;
        next.serverName = readString(preset, "server_name", next.serverName);
        next.mapName = readString(preset, "map", next.mapName);
        next.gameMode = readString(preset, "mode", next.gameMode);
        next.maxPlayers = readUInt(preset, "max_players", next.maxPlayers, 1, 999);
        next.startupNpcsEnabled = readBool(preset, "startup_npcs", next.startupNpcsEnabled);
        next.startupNpcCount = readUInt(preset, "npc_count", next.startupNpcCount, 0, 999);
        next.autoMapRotation = readBool(preset, "map_rotation", next.autoMapRotation);
        next.mapRotationMinutes = readUInt(preset, "map_rotation_minutes",
                                            next.mapRotationMinutes, 1, 9999);
        next.discordNotification = readBool(preset, "discord_notification",
                                            next.discordNotification);
        next.joinHost = readBool(preset, "join_host", next.joinHost);
        out = std::move(next);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

} // namespace MimitaNet
