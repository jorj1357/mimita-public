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

// ── Gamemode Feature Declarations ─────────────────────────────────────
// Each gamemode JSON can declare which features it uses.
// The GamemodeManager reads these to determine what to render.
// Adding a new feature type = adding a field here + JSON key + renderer.
struct GamemodeFeatures {
    bool worldTimer = false;        // Render a countdown timer above a world entity
    bool bombHolderText = false;    // Render "X has the bomb!!!!" at top of screen
    bool bombBlink = false;         // Bomb sphere blinks between two colors
    bool infiniteRounds = false;    // Mode runs indefinitely, no win condition
    bool noWeaponsExceptBomb = false; // Player loadout restricted to bomb only
    // Future features:
    bool bossHealthbar = false;     // Render healthbar above a boss entity
    bool worldText = false;         // Render arbitrary text in world space
    bool timerAboveEntity = false;  // Render timer above any entity
};

struct Gamemode {
    std::string id = "duel";
    std::string name = "Duel";
    std::string description = "First to 20. Instant respawns. Max action, no downtime.";
    std::vector<std::string> teamNames = {"RED", "BLUE"};
    int goalValue = 20;
    int weaponSetId = 1;
    int timeLimitSeconds = 0;
    float respawnSeconds = 0.0f;
    bool killHeals = true;
    float countdownSeconds = 3.0f;
    float goSeconds = 0.75f;
    float rematchSeconds = 5.0f;
    float spawnTracerSeconds = 1.5f;
    bool allowRematch = true;
    // Random XY offset radius around the match anchor that both teams spawn near.
    float spawnOffsetRadius = 5.0f;
    std::string spawnStrategy = "shared";  // "shared", "team_shared", "distributed", "random"
    int intermissionSeconds = 15;
    int resultsSeconds = 8;
    std::vector<std::string> maps;
    // ── Bomb Tag specific fields ─────────────────────────────────────
    int bombTimerTicks = 900;          // Ticks per bomb cycle (15s * 60 = 900)
    int inactiveTicks = 60;            // Ticks of inactive grace after pass
    int blinkTicks = 30;               // Ticks per color blink phase
    float maxPassSanityDistance = 3.0f;// Hard rejection distance for passes (meters)
    // ── Feature declarations ─────────────────────────────────────────
    // Declares what this gamemode renders/uses. Drives GamemodeManager.
    GamemodeFeatures features;
    // ── Visual/settings overrides (optional per-gamemode) ───────────
    // 0 / false + explicit=false means "no override, use user's current setting".
    float cameraFov = 0.0f;
    bool ragdollEnabled = false;
    bool ragdollExplicit = false;
    bool bloodEnabled = false;
    bool bloodExplicit = false;
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
