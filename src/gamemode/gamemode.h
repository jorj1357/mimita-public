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

#include <glm/glm.hpp>

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

// One ordered team in a mode's roster. Team index (0-based) is the
// authoritative team number used by matchTeams and the wire. `capacity` of 0
// means unlimited. `role` names the role assigned to members; `spawnGroup`
// names the spawn group this team uses. Purely data; the runtime owns state.
struct GamemodeTeam {
    std::string id;
    std::string displayName;
    int capacity = 0;
    std::string role;
    std::string spawnGroup;
    // Optional team AI assignment policy (JSON-controlled; 0 = brain default).
    int attackersPerSite = 0;
    int defendersPerSite = 0;
    int oneRotator = -1;  // -1 = unset, 0 = no rotator, 1 = one rotator
};

// A named set of spawn positions for one team. Positions are optional; when
// empty the runtime resolves spawns from the map (map anchors or the shared
// anchor fallback). Kept in the gamemode JSON so modes own their rosters.
struct GamemodeSpawnGroup {
    std::string id;
    std::string team;
    struct Point { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
    std::vector<Point> points;
};

// Optional world-space visual for an objective. Data only; the client render
// owner reads it to draw the pulse sphere. Colors are linear RGBA.
struct GamemodeObjectiveVisual {
    bool enabled = false;
    float radius = 0.35f;           // base world radius (meters)
    float pulseAmplitude = 0.15f;   // additional radius at pulse peak
    float periodSeconds = 1.5f;     // full pulse cycle length
    glm::vec3 color{1.0f, 0.25f, 0.1f};  // RGBA (alpha in `colorA`)
    float alpha = 0.8f;
};

// A generic objective definition (bomb today; future payload/capture/escort).
// Objective runtime state is owned by the objective system, never by this data
// or by an actor preset.
struct GamemodeObjectiveDefinition {
    std::string id;
    std::string kind;
    std::string carrierTeam;
    std::string siteGroup;
    float plantSeconds = 0.0f;
    float defuseSeconds = 0.0f;
    float explosionSeconds = 0.0f;
    GamemodeObjectiveVisual visual;
};

// Round/match rules. Zero means "not configured"; the runtime keeps its
// existing behavior when a value is absent.
struct GamemodeRounds {
    int maxRounds = 0;
    int roundsToWin = 0;
    float roundSeconds = 0.0f;
    float freezeSeconds = 0.0f;
    float countdownSeconds = 0.0f;
    float intermissionSeconds = 0.0f;
    float resultsSeconds = 0.0f;
};

// Mode-level presentation policy. Missing keys mean "no policy" so the
// runtime preserves ordinary behavior. Actor-preset presentation refines
// these where both exist.
struct GamemodePresentation {
    bool hasDamageNumbers = false;       bool damageNumbers = true;
    bool hasHitEffects = false;          bool hitEffects = true;
    bool hasWorldImpactEffects = false;  bool worldImpactEffects = true;
    bool hasHitMarkers = false;          bool hitMarkers = true;
    bool hasHitSounds = false;           bool hitSounds = true;
    bool hasBlood = false;               bool blood = true;
    bool hasKillfeed = false;            bool killfeed = true;
    bool hasRagdolls = false;            bool ragdolls = true;
    bool hasEnemyHealthbars = false;     bool enemyHealthbars = true;
    bool hasPlayerOutlines = false;      bool playerOutlines = true;
};

// Optional mode-level NPC targeting policy. When absent (configured=false) the
// runtime falls back to npc-difficulty.json targetMode/damageOtherNpcs so other
// modes are unchanged.
struct GamemodeNpcTargeting {
    bool configured = false;
    std::string mode = "closest";  // "player" | "closest" | "opposite_team"
    bool includePlayers = true;
    bool includeNpcs = true;
};

// Optional mode-level RMB aim-FOV override. When enabled, the camera uses this
// instead of config/aimbody.json for the held-RMB zoom. Never edits aimbody.json.
struct GamemodeAimFov {
    bool enabled = false;
    std::string input = "right_mouse";
    float multiplier = 0.5f;
    float duration = 0.5f;
    std::string easing = "ease_in_out";
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
    // ── Ordered roster / objective / round data (optional) ───────────
    // When `teams` is non-empty it defines the ordered roster and also
    // populates `teamNames` above for backward compatibility.
    std::vector<GamemodeTeam> teams;
    std::vector<GamemodeSpawnGroup> spawnGroups;
    std::vector<GamemodeObjectiveDefinition> objectives;
    GamemodeRounds rounds;
    GamemodePresentation presentation;
    std::string victoryCondition;  // e.g. "rounds"; empty = legacy
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
    bool forceFirstPerson = false;
    bool hideHealthbars = false;
    bool ragdollEnabled = false;
    bool ragdollExplicit = false;
    bool bloodEnabled = false;
    bool bloodExplicit = false;
    // ── Match role counts (optional) ────────────────────────────────
    // role id -> number of participants to assign that role. Empty means the
    // match falls back to the legacy team assignment and no roles.
    std::vector<std::pair<std::string, int>> roleCounts;
    std::string actorPresetId;
    // Optional NPC-only combat profile used when an NPC has no role-specific
    // behavior profile. Humans are never affected by this setting.
    std::string npcBehaviorProfile;
    // Optional mode-level NPC targeting (team modes) and RMB aim-FOV override.
    GamemodeNpcTargeting npcTargeting;
    GamemodeAimFov aimFov;
    // ── Elimination / win rules (optional) ──────────────────────────
    // Empty = legacy score/time behavior. "last_team_standing" ends the match
    // when only one team (or, in FFA, one actor) still has an in-play actor.
    std::string winCondition;
    int waveStartCount = 1;
    int waveIncrement = 1;
    int waveNpcsPerWave = 0; // >0: target = wave number * this value
    int lives = 3;
    float waveBannerSeconds = 3.0f;
    bool waveStaggerEnabled = true;
    int waveNpcsPerTick = 10;
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
