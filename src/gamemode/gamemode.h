// 08 10 2026, 14 34
/* purpose
* Defines the Gamemode data structure and registry that load config/gamemodes/*.json files.
* A gamemode is pure data: name, description, team names, goal, time limit, respawn rules.
* Adding a new gamemode = adding a new JSON file, no C++ changes.
* Does NOT contain gameplay logic; the duel engine reads the loaded values.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct DuelConfig;

struct Gamemode {
    std::string id = "duel";
    std::string name = "Duel";
    std::string description = "First to 20. Instant respawns. Max action, no downtime.";
    std::vector<std::string> teamNames = {"RED", "BLUE"};
    int goalValue = 20;
    int timeLimitSeconds = 0;
    float respawnSeconds = 0.0f;
    bool killHeals = true;
    float countdownSeconds = 3.0f;
    float rematchSeconds = 5.0f;
    float spawnTracerSeconds = 1.5f;
    bool allowRematch = true;
    // Random XY offset radius around the match anchor that both teams spawn near.
    float spawnOffsetRadius = 5.0f;
    std::vector<std::string> maps;
};

class GamemodeRegistry {
public:
    static GamemodeRegistry& instance();

    void loadDirectory(const std::string& dir);
    void pollReload();

    const Gamemode& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<std::string> ids() const;

private:
    GamemodeRegistry() = default;

    struct LoadedMode {
        std::string path;
        std::filesystem::file_time_type writeTime;
        Gamemode mode;
    };

    void loadFile(const std::string& path, LoadedMode& slot);
    LoadedMode* findSlot(const std::string& id);
    const LoadedMode* findSlot(const std::string& id) const;

    std::vector<LoadedMode> modes_;
    std::string directory_;
};

void applyGamemodeToDuelConfig(DuelConfig& cfg, const Gamemode& gm);
