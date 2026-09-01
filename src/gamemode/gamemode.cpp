// 08 10 2026, 14 34
/* purpose
* Loads, hot-reloads, and lists gamemodes from config/gamemodes/*.json.
* Parses every knobs into a Gamemode struct with safe defaults for missing keys.
* Used by the menu (list gamemodes) and the duel engine (read rules).
* Does NOT contain gameplay logic or duel simulation.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "gamemode/gamemode.h"

#include <algorithm>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "game/duel.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

std::string optString(const json& root, const char* key, const std::string& fallback)
{
    if (root.contains(key) && root[key].is_string())
        return root[key].get<std::string>();
    return fallback;
}

int optInt(const json& root, const char* key, int fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<int>();
    return fallback;
}

float optFloat(const json& root, const char* key, float fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<float>();
    return fallback;
}

bool optBool(const json& root, const char* key, bool fallback)
{
    if (root.contains(key) && root[key].is_boolean())
        return root[key].get<bool>();
    return fallback;
}

std::vector<std::string> optStringArray(const json& root, const char* key)
{
    std::vector<std::string> out;
    if (!root.contains(key) || !root[key].is_array())
        return out;
    for (const auto& item : root[key]) {
        if (item.is_string())
            out.push_back(item.get<std::string>());
    }
    return out;
}

} // namespace

GamemodeRegistry& GamemodeRegistry::instance()
{
    static GamemodeRegistry registry;
    return registry;
}

void GamemodeRegistry::loadDirectory(const std::string& dir)
{
    directory_ = dir;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Missing directory %s; no gamemodes loaded.\n", dir.c_str());
        return;
    }

    modes_.clear();
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".json") continue;

        LoadedMode slot;
        slot.path = entry.path().string();
        slot.writeTime = getLastWrite(slot.path);
        loadFile(slot.path, slot);
        if (!slot.mode.id.empty())
            modes_.push_back(std::move(slot));
    }

    Debug::warn(Debug::Category::Duel, "[GAMEMODE] Loaded %zu gamemode(s) from %s\n", modes_.size(), dir.c_str());
}

void GamemodeRegistry::pollReload()
{
    if (directory_.empty()) return;

    bool changed = false;
    for (auto& slot : modes_) {
        const auto writeTime = getLastWrite(slot.path);
        if (writeTime == std::filesystem::file_time_type{} || writeTime == slot.writeTime)
            continue;
        changed = true;
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Detected change: %s\n", fileNameOf(slot.path).c_str());
        slot.writeTime = writeTime;
        loadFile(slot.path, slot);
    }

    if (changed)
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Reload complete\n");
}

void GamemodeRegistry::loadFile(const std::string& path, LoadedMode& slot)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Missing %s; using defaults.\n", path.c_str());
        return;
    }

    try {
        json root;
        file >> root;
        if (!root.is_object() || !root.contains("id") || !root["id"].is_string()) {
            Debug::warn(Debug::Category::Duel, "[GAMEMODE] %s has no string \"id\"; skipped.\n", fileNameOf(path).c_str());
            return;
        }

        Gamemode next;
        next.id = root["id"].get<std::string>();
        next.name = optString(root, "name", next.name);
        next.description = optString(root, "description", next.description);
        next.teamNames = optStringArray(root, "team_names");
        if (next.teamNames.size() < 2)
            next.teamNames = {"RED", "BLUE"};
        next.goalValue = std::max(1, optInt(root, "goal_value", next.goalValue));
        next.timeLimitSeconds = std::max(0, optInt(root, "time_limit_seconds", next.timeLimitSeconds));
        next.respawnSeconds = std::max(0.0f, optFloat(root, "respawn_seconds", next.respawnSeconds));
        next.killHeals = optBool(root, "kill_heals", next.killHeals);
        next.countdownSeconds = std::max(0.0f, optFloat(root, "countdown_seconds", next.countdownSeconds));
        next.goSeconds = std::max(0.0f, optFloat(root, "go_seconds", next.goSeconds));
        next.rematchSeconds = std::max(0.0f, optFloat(root, "rematch_seconds", next.rematchSeconds));
        next.spawnTracerSeconds = std::max(0.0f, optFloat(root, "spawn_tracer_seconds", next.spawnTracerSeconds));
        next.allowRematch = optBool(root, "allow_rematch", next.allowRematch);
        next.spawnOffsetRadius = std::max(0.0f, optFloat(root, "spawn_offset_radius", next.spawnOffsetRadius));
        next.spawnStrategy = optString(root, "spawn_strategy", next.spawnStrategy);
        next.intermissionSeconds = std::max(0, optInt(root, "intermission_seconds", next.intermissionSeconds));
        next.resultsSeconds = std::max(0, optInt(root, "results_seconds", next.resultsSeconds));
        next.maps = optStringArray(root, "maps");

        slot.mode = next;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE] Loaded %s: %s | goal=%d | time=%d | respawn=%.1fs | heal=%d | maps=%zu\n",
            fileNameOf(path).c_str(), next.name.c_str(), next.goalValue,
            next.timeLimitSeconds, next.respawnSeconds, (int)next.killHeals, next.maps.size());
    } catch (const json::parse_error& e) {
        Debug::error(Debug::Category::Duel, "[GAMEMODE] Parse error in %s: %s. Keeping previous valid data.\n",
                     path.c_str(), e.what());
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel, "[GAMEMODE] Error loading %s: %s. Keeping previous valid data.\n",
                     path.c_str(), e.what());
    }
}

GamemodeRegistry::LoadedMode* GamemodeRegistry::findSlot(const std::string& id)
{
    for (auto& slot : modes_) {
        if (slot.mode.id == id)
            return &slot;
    }
    return nullptr;
}

const GamemodeRegistry::LoadedMode* GamemodeRegistry::findSlot(const std::string& id) const
{
    for (const auto& slot : modes_) {
        if (slot.mode.id == id)
            return &slot;
    }
    return nullptr;
}

const Gamemode& GamemodeRegistry::get(const std::string& id) const
{
    static const Gamemode fallback;
    const LoadedMode* slot = findSlot(id);
    if (slot) return slot->mode;
    Debug::warn(Debug::Category::Duel, "[GAMEMODE] Unknown gamemode \"%s\"; returning defaults.\n", id.c_str());
    return fallback;
}

bool GamemodeRegistry::has(const std::string& id) const
{
    return findSlot(id) != nullptr;
}

std::vector<std::string> GamemodeRegistry::ids() const
{
    std::vector<std::string> out;
    out.reserve(modes_.size());
    for (const auto& slot : modes_)
        out.push_back(slot.mode.id);
    return out;
}

void applyGamemodeToDuelConfig(DuelConfig& cfg, const Gamemode& gm)
{
    cfg.gamemodeId = gm.id;
    cfg.teamNames = gm.teamNames;
    cfg.killsToWin = gm.goalValue;
    cfg.duelLengthSeconds = gm.timeLimitSeconds;
    cfg.respawnDelaySeconds = gm.respawnSeconds;
    cfg.killHeals = gm.killHeals;
    cfg.countdownSeconds = gm.countdownSeconds;
    cfg.rematchSeconds = gm.rematchSeconds;
    cfg.spawnTracerSeconds = gm.spawnTracerSeconds;
    cfg.allowRematch = gm.allowRematch;
}
