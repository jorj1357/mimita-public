// 08 08 2026, 22 19
/* purpose
* Hot-reloadable JSON source of truth for NPC difficulty (config/npc-difficulty.json).
* Exposes accuracy, damage, fire rate, and force-hit toggles for NPC combat.
* Follows the GameplayConfig pollReload pattern: re-reads the file when it changes on disk.
* Does NOT own NPC state machines, movement, or weapon switching logic.
* Does NOT write config files unless save() is explicitly called by a terminal command.
* Does NOT touch the hot-reload DLL system.
*/

#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

struct NpcDifficultySettings {
    float maxAngularErrorDegrees = 2.0f;  // aim miss-cone in degrees; lower = deadlier
    float difficultyErrorScale = 0.5f;    // 0-1: how much NPC difficulty shrinks the cone
    float damageMultiplier = 3.0f;        // scales weapon damage when an NPC hits the player
    float fireDelayMin = 0.05f;           // fastest shot interval override (0 = use weapon)
    float fireDelayMax = 0.35f;           // slowest shot interval
    float aggressionBonus = 0.4f;         // pushes shot timing toward fireDelayMin
    bool forceHit = false;                // debug: zero aim error, every shot connects
};

class NpcDifficultyConfig {
public:
    static NpcDifficultyConfig& instance();

    bool load(const std::string& path = "config/npc-difficulty.json");
    bool pollReload();
    bool save(const std::string& path = "config/npc-difficulty.json");

    const NpcDifficultySettings& settings() const { return mData; }
    NpcDifficultySettings& settings() { return mData; }

private:
    NpcDifficultyConfig() = default;

    NpcDifficultySettings mData;
    nlohmann::json mRoot;
    std::string mPath = "config/npc-difficulty.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
