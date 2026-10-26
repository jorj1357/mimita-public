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
#include <nlohmann/json.hpp>

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
    // Melee-style: render each fighter's percent above its head instead of the
    // ordinary healthbar (data-driven per mode).
    bool percentAboveHead = false;
    // Zombie Tower first-slice presentation/run flags.
    bool fog = false;
    bool rain = false;
    bool torchLighting = false;
    bool freezeDuringCountdown = false;
    bool freezeDuringGo = true;
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
    // Optional map-node name filters for this team's spawn group. Matching is
    // case-insensitive; an excluded token wins over an included token.
    std::string spawnTagContains;
    std::string spawnTagExcludes;
    // Optional team AI assignment policy (JSON-controlled; 0 = brain default).
    int attackersPerSite = 0;
    int defendersPerSite = 0;
    int oneRotator = -1;  // -1 = unset, 0 = no rotator, 1 = one rotator
    // Optional NPC-only role for this team (Zombie Tower companions). Empty
    // means the team role is used for both humans and NPCs.
    std::string npcRole;
    // Data-driven default assignment for actors that have no explicit team.
    // Only modes that declare this opt in; other modes keep legacy behavior.
    bool players = false;
    bool npcs = false;
    // When true, a human may take a slot occupied by an NPC on this team.
    bool preferHumans = false;
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
    bool endlessRounds = false;
    float roundSeconds = 0.0f;
    float freezeSeconds = 0.0f;
    float countdownSeconds = 0.0f;
    float intermissionSeconds = 0.0f;
    float resultsSeconds = 0.0f;
};

// Optional mode-level trail override. A null/absent field inherits the local
// config/playervisuals.json value.
struct GamemodeTrailSettings {
    bool enabled = false;
    float minSpeed = 35.0f;
    float sampleIntervalSeconds = 0.035f;
    float lifetimeSeconds = 0.22f;
    int maxSamples = 12;
    float alpha = 0.65f;
    float width = 3.0f;
    glm::vec3 color{255.0f, 80.0f, 40.0f};
};

// Mode-level presentation policy. Most missing keys mean "no policy";
// player_outlines is opt-in during community matches. Actor-preset
// presentation refines other settings where both exist.
struct GamemodePresentation {
    bool hasDamageNumbers = false;       bool damageNumbers = true;
    bool hasHitEffects = false;          bool hitEffects = true;
    bool hasWorldImpactEffects = false;  bool worldImpactEffects = true;
    bool hasWorldImpactDecals = false;   bool worldImpactDecals = true;
    bool hasWorldHitEffects = false;     bool worldHitEffects = true;
    bool hasHitMarkers = false;          bool hitMarkers = true;
    bool hasHitSounds = false;           bool hitSounds = true;
    bool hasHitFlinch = false;            bool hitFlinch = true;
    bool hasBlood = false;               bool blood = true;
    bool hasKillfeed = false;            bool killfeed = true;
    bool hasRagdolls = false;            bool ragdolls = true;
    bool hasEnemyHealthbars = false;     bool enemyHealthbars = true;
    bool hasFriendlyHealthbars = false;  bool friendlyHealthbars = true;
    bool hasPlayerOutlines = false;      bool playerOutlines = true;
    bool hasFriendlyOutlines = false;     bool friendlyOutlines = false;
    bool hasPlayerWireframes = false;     bool playerWireframes = false;
    bool hasFriendlyWireframes = false;   bool friendlyWireframes = false;
    bool hasEnemyOutlineAlpha = false;   float enemyOutlineAlpha = 0.25f;
    float bloodScale = 1.0f;
    float bloodLifetimeScale = 1.0f;
    float worldImpactDecalScale = 1.0f;
    float worldImpactDecalLifetimeScale = 1.0f;
    float worldImpactDecalAlphaScale = 1.0f;
    float worldHitEffectScale = 1.0f;
    float worldHitEffectLifetimeScale = 1.0f;
    bool hasTeamOutlineColors = false;
    glm::vec3 friendlyOutlineColor{0.0f, 255.0f, 0.0f};
    glm::vec3 enemyOutlineColor{255.0f, 0.0f, 0.0f};
    bool hasTeamWireframeColors = false;
    glm::vec3 friendlyWireframeColor{0.0f, 255.0f, 0.0f};
    glm::vec3 enemyWireframeColor{255.0f, 0.0f, 0.0f};
    // Trail policy. has* stays false when the mode provided no concrete value,
    // so the local playervisuals.json settings remain authoritative.
    bool hasEnemyTrail = false;
    GamemodeTrailSettings enemyTrail;
    bool hasTeammateTrail = false;
    GamemodeTrailSettings teammateTrail;
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

// Optional per-team/mode weighting of NPC travel goals. Scales how strongly an
// NPC prefers moving to an objective vs hunting the team's known enemy area vs
// plain exploration. Values are multipliers (1.0 = neutral).
struct GamemodeNpcTravel {
    bool configured = false;
    float objectiveBias = 1.0f;   // MoveToObjective/DefendSite/... weight
    float huntBias = 0.0f;        // travel toward the team's best enemy report
    float exploreBias = 1.0f;     // reserved: plain exploration weight
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

// Optional mode-wide avatar policy. Role and actor-preset policies still refine
// this per actor; the mode policy is the shared outer filter.
struct GamemodeAvatarPolicy {
    bool configured = false;
    bool forced = false;
    std::string name;
    std::vector<std::string> allowed;
    std::vector<std::string> denied;
    std::string group;
};

// NPC-only selection from the masterlist's named avatar groups. A stable
// assignment belongs to the NPC actor and survives its later lives.
struct GamemodeNpcAvatarPolicy {
    bool configured = false;
    std::string group;
    std::vector<std::string> allowed;
    std::vector<std::string> denied;
    std::string spawnPolicy = "reroll_each_spawn";
};

// Optional Payload rules. Map data owns cart origin/route/interaction geometry;
// this block owns mode-wide timing and movement rules.
struct GamemodePayload {
    bool configured = false;
    std::string attackerTeam = "attackers";
    std::string defenderTeam = "defenders";
    int attackerTeamIndex = 0;
    int defenderTeamIndex = 1;
    float overtimeSeconds = 10.0f;
    float pushMetersPerSecondPerAttacker = 0.2f;
    int maxPushAttackers = 6;
    float unattendedRollbackDelaySeconds = 30.0f;
    float rollbackMetersPerSecond = 0.2f;
    int healHpPer10Ticks = 1;
    bool healDefenders = false;
    int spawnProtectionTicks = 60;
    float teamChangeCooldownSeconds = 300.0f;
    std::vector<std::string> classPool;
};

struct Gamemode {
    std::string id = "duel";
    std::string name = "Duel";
    std::string description = "First to 20. Instant respawns. Max action, no downtime.";
    std::vector<std::string> teamNames = {"RED", "BLUE"};
    int goalValue = 20;
    int weaponSetId = 1;
    int timeLimitSeconds = 0;
    // Outcome when a timed match expires before its score goal is reached.
    // "high_score" preserves the shared default; "stalemate" leaves no winner.
    std::string timeLimitOutcome = "high_score";
    float respawnSeconds = 0.0f;
    bool killHeals = true;
    // Team members are protected from one another unless a mode explicitly
    // opts into friendly fire. Missing JSON keeps the existing safe default.
    bool friendlyFire = false;
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
    // Optional mode-forced movement profile. When set, every actor in the mode
    // uses this movement preset regardless of role, and the client applies it
    // for local prediction. Empty = movement comes from the role/actor preset.
    std::string movementPreset;
    // Optional NPC-only combat profile used when an NPC has no role-specific
    // behavior profile. Humans are never affected by this setting.
    std::string npcBehaviorProfile;
    // Optional mode-level NPC targeting (team modes) and RMB aim-FOV override.
    GamemodeNpcTargeting npcTargeting;
    GamemodeNpcTravel npcTravel;
    GamemodeAimFov aimFov;
    GamemodeAvatarPolicy avatar;
    GamemodeNpcAvatarPolicy npcAvatar;
    GamemodePayload payload;
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
    std::string npcSpawnPolicy;
    int npcSpawnMax = 0;
    int npcSpawnIntervalTicks = 60;
    int npcSpawnPerInterval = 1;
    // Authored-map PvE run attempts (checkpointed modes such as Zombie Tower).
    // Consumed by the map-entity run runtime; 3 is the documented default.
    int runAttempts = 3;
    // Allow a player to change teams while the run is active (Zombie Tower).
    bool allowMidrunTeamChange = false;
    // Enables the authored-map run lifecycle (lobby, attempts, checkpoints,
    // encounter volumes, boss). Generic flag; the mode supplies the map data.
    bool runMode = false;
    // Mode-owned soundtrack. Empty on_go_track means ordinary ingame music.
    std::string musicOnGoTrack;
    std::vector<std::string> musicRandomPool;
};

class GamemodeRegistry {
public:
    static GamemodeRegistry& instance();

    void loadDirectory(const std::string& dir);
    void pollReload();

    // Strict validation against config/gamemodes/masterlist/gamemode-masterlist.json.
    // The registry loads the masterlist with the mode directory. Returns true and
    // fills `errors` with file/setting-path messages when validation is available
    // and the JSON is invalid.
    bool validateFileJson(const std::string& path,
                          const nlohmann::json& root,
                          std::vector<std::string>& errors) const;
    bool masterlistLoaded() const { return mMasterlistLoaded; }
    const std::vector<std::string>& validationErrors() const { return mValidationErrors; }

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

    bool loadFile(const std::string& path, LoadedMode& slot);
    bool loadMasterlist();
    void reportInvalidFile(const std::string& path, const std::vector<std::string>& errors);
    void applyAvatarGroup(GamemodeAvatarPolicy& policy) const;
    LoadedMode* findSlot(const std::string& id);
    const LoadedMode* findSlot(const std::string& id) const;

    std::vector<LoadedMode> modes_;
    std::string directory_;
    nlohmann::json mMasterlist;
    bool mMasterlistLoaded = false;
    std::filesystem::file_time_type mMasterlistWriteTime{};
    std::vector<std::string> mValidationErrors;
};

void applyGamemodeToDuelConfig(DuelConfig& cfg, const Gamemode& gm);
