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
#include <utility>
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
    bool objectiveBomb = false;     // Render the planted/dropped objective bomb
};

// Optional per-mode healthbar override. Explicit=true means the mode forces
// these values for every in-match actor regardless of the player's own
// config/healthbar.json; false leaves the user's settings untouched.
struct GamemodeHealthbarOverride {
    bool explicitValue = false;
    bool aimModeEnabled = true;
    bool showNameInAimMode = false;
    bool showHpTextInAimMode = false;
    bool showBarInAimMode = false;
    float maxDistance = 2000.0f;
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
    float goSeconds = 1.0f;
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
    // ── Forced gameplay overrides (optional per-mode) ───────────────
    // Empty = no override, use the player's own config/gameplay.json.
    std::string aimMode;         // "crosshair", "world_hit", "farpoint", "camforward", "physical"
    std::string movementPreset;  // config/movement/*.json "name" for every actor
    GamemodeHealthbarOverride healthbar;
    // ── Objective bomb timings (used when features.objectiveBomb) ───
    float bombPlantSeconds = 3.0f;
    float bombDefuseSeconds = 5.0f;
    float bombTimerSeconds = 40.0f;
    float bombExplosionRadius = 8.0f;
    float bombExplosionDamage = 500.0f;
    // ── Match role counts (optional) ────────────────────────────────
    // role id -> number of participants to assign that role. Empty means the
    // match falls back to the legacy team assignment and no roles.
    std::vector<std::pair<std::string, int>> roleCounts;
    // ── Elimination / win rules (optional) ──────────────────────────
    // Empty = legacy score/time behavior. "last_team_standing" ends the match
    // when only one team (or, in FFA, one actor) still has an in-play actor.
    // "npc_waves" runs the shared lifecycle as an endless NPC-wave survival.
    std::string winCondition;
    // ── NPC wave rules (used when win_condition == "npc_waves") ─────
    // Round N spawns wave_start_count + (N-1) * wave_increment NPCs.
    int waveStartCount = 1;
    int waveIncrement = 1;
    // When true, the mode plays only the maps in its own `maps` list instead
    // of the shared good-map pool. Opt-in so existing modes are unaffected.
    bool useModeMaps = false;
};

class GamemodeRegistry {
public:
    static GamemodeRegistry& instance();

    void loadDirectory(const std::string& dir);
    void pollReload();

    const Gamemode& get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<std::string> ids() const;

    // Monotonic load counter. Increments whenever any mode file is (re)loaded,
    // so the live server can re-apply JSON-rule changes at a safe boundary
    // without polling every field.
    uint64_t revision() const { return revision_; }

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
    uint64_t revision_ = 0;
};

void applyGamemodeToDuelConfig(DuelConfig& cfg, const Gamemode& gm);
