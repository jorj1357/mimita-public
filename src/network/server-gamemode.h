// 09 01 2026, 00 00
/* purpose
* Declares the authoritative JSON-defined gamemode state and tick entry points.
* Runs shared waiting, intermission, countdown, active, results, and rematch lifecycle.
* Supports duel, FFA, TDM, Bomb Tag, sandbox, and future gamemode rule sets.
* Does NOT simulate players, apply damage, or render anything.
* Does NOT own the client queue/matchmaking or the coordinator protocol.
* Does NOT create team spawns - it reads them from the loaded headless world.
*/

#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "network/server.h"
#include "network/actor-match.h"
#include "game/objective-state.h"
#include "combat/area-effect.h"
#include "npc/team-brain.h"
#include "procedural/procedural-world.h"
#include "gamemode/action-graph.h"
#include "gamemode/disaster-runtime.h"

namespace MimitaNet {

enum class ServerMode { Sandbox, Duel, TeamDeathmatch, FreeForAll };

struct ServerGamemodeKillEvent
{
    uint32_t killerId = 0;
    uint32_t victimId = 0;
    uint8_t killerEntityType = ENTITY_PLAYER;
    uint8_t victimEntityType = ENTITY_PLAYER;
    std::string killerName;
    std::string victimName;
    std::string weaponId;
    std::string weaponDisplayName;
    glm::vec3 killerPos{0.0f};
    glm::vec3 victimPos{0.0f};
    uint32_t eventId = 0;
    uint64_t correlationId = 0;
    uint32_t serverTick = 0;
};

struct ServerGamemodeState
{
    bool enabled = false;
    ServerMode mode = ServerMode::Sandbox;
    bool mapOnly = false;
    std::string communityMode = "sandbox";
    int communityWeaponSetId = 1;
    int appliedCommunityWeaponSetId = 0;
    bool communityWeaponSetExplicit = false;
    std::unordered_map<uint32_t, int> communityScores;
    std::unordered_map<uint32_t, int> communityTeams;
    int communityTeamScore[2] = {0, 0};
    bool communityRoundOver = false;
    uint64_t communityRoundResetMs = 0;
    // DuelStatePhase (packets.h)
    uint8_t phase = DUEL_PHASE_WAITING;
    bool matchOver = false;
    int scoreA = 0;
    int scoreB = 0;
    int goalValue = 20;
    float countdown = 0.0f;
    float countdownSeconds = 3.0f;
    float goSeconds = 1.0f;
    float rematchLeft = 0.0f;
    float rematchSeconds = 5.0f;
    std::string teamAName = "RED";
    std::string teamBName = "BLUE";
    uint32_t playerAId = 0;
    uint32_t playerBId = 0;
    uint32_t winnerPlayerId = 0;
    // The single match anchor: both teams always spawn near this one point
    // (picked from the map's spawn points, fixed for the whole match), with a
    // fresh random XY offset on every spawn/respawn. Floating on purpose.
    glm::vec3 spawnA{0.0f};
    glm::vec3 spawnB{0.0f};
    // The original random map anchor, kept separate from spawnA/spawnB (which
    // are overwritten by the CT/T cluster fronts). Used as the neutral fallback
    // for an actor whose team cannot be resolved, so an unknown team never
    // silently spawns on the CT cluster.
    glm::vec3 sharedAnchor{0.0f};
    // Per-team spawn clusters resolved from the active mode's map-node filters
    // (with legacy CT/T tag support). Empty means shared-anchor fallback.
    std::vector<glm::vec3> teamSpawnPoints[2];
    bool teamSpawnsResolved = false;
    // Random XY offset radius around the anchor (meters).
    float spawnOffsetRadius = 5.0f;
    bool spawnsAssigned = false;
    // The last DuelStatePacket sent, to avoid re-broadcasting identical state.
    bool stateSent = false;
    // Pending kill deferred from applyServerDamage (no socket there). Processed
    // at the next serverGamemodeTick, which has the socket + packet counters.
    bool hasPendingKill = false;
    uint32_t pendingKillerId = 0;
    uint32_t pendingVictimId = 0;
    bool pendingKillerIsNpc = false;
    bool pendingVictimIsNpc = false;
    std::deque<ServerGamemodeKillEvent> pendingKillEvents;
    // The kill promoted from pendingKillEvents this tick. Carries names and
    // positions so scoring, persistence, and the killfeed share one event.
    ServerGamemodeKillEvent currentKill;
    // Periodic DuelState broadcast cadence so clients can detect a dead server.
    uint32_t lastBroadcastTick = 0;
    // Forces the first authoritative community-match state to reach clients
    // immediately after modestart/modestartnow changes the runtime mode.
    bool stateBroadcastPending = false;
    // Live map rotation (auto on rematch) + manual changemap request.
    std::vector<std::string> mapPool;
    bool rotateMaps = false;
    bool autoMapRotation = false;
    uint32_t mapRotationMinutes = 15;
    uint64_t nextMapRotationMs = 0;
    uint64_t mapChangeCountdownStartMs = 0;
    std::string pendingAutomaticMap;
    bool hasPendingManualMap = false;
    std::string pendingManualMap;
    // Maps already used this rotation cycle (so each new duel picks a map we
    // weren't just on, and never repeats until the whole pool is used).
    std::unordered_set<std::string> usedMaps;
    // Server's current loaded map (name only, for HUD/logging).
    std::string mapId;
    uint32_t duelId = 0;
    uint32_t mapVersion = 0;
    uint32_t spawnAnchorVersion = 0;
    uint32_t respawnSequence = 0;
    uint32_t stateVersion = 0;
    uint32_t spawnAnchorIndex = 0;

    // ── FFA/TDM match mode fields ──────────────────────────────────
    // Match mode: "duel", "ffa", "tdm"
    std::string matchMode = "duel";
    // Host-only runtime actor-preset override. Empty means the gamemode's
    // actor_preset/role assignment remains authoritative.
    std::string actorPresetOverrideId;

    // Authoritative tick references for countdown/start/end
    uint32_t countdownStartTick = 0;
    uint32_t matchStartTick = 0;
    uint32_t matchTimeLimitTick = 0;  // matchStartTick + timeLimitTicks
    uint32_t currentServerTick = 0;

    // Intermission/results phase timers
    float phaseTimer = 0.0f;
    bool startCountdownImmediately = false;
    float intermissionSeconds = 15.0f;
    float resultsSeconds = 8.0f;
    int timeLimitSeconds = 300;

    // FFA scoring: per-player kills/deaths
    std::unordered_map<uint32_t, int> ffaKills;
    std::unordered_map<uint32_t, int> ffaDeaths;

    // TDM scoring
    int redTeamKills = 0;
    int blueTeamKills = 0;

    // Team assignments (persistent per match, 0=red, 1=blue)
    std::unordered_map<uint32_t, int> matchTeams;

    // The single authoritative match identity for every participant (human or
    // NPC), keyed by actor id. Assigned by assignMatchParticipants and updated
    // as actor state changes. Replicated (team/role/state) via DuelStatePacket.
    std::unordered_map<uint32_t, ActorMatchDescriptor> matchActors;

    // All participating player IDs (FFA/TDM can have >2 players)
    std::vector<uint32_t> participants;
    std::unordered_map<uint32_t, std::string> participantNames;

    // Victory info
    int victoryType = 0;  // 0=ScoreLimit, 1=TimeLimit
    int winnerTeam = -1;  // for TDM: 0=red, 1=blue

    // ── Authoritative match-rule values (from Gamemode JSON) ────────
    // respawnSeconds: <0 = unset (legacy instant 0.01s), 0 = one-life (no
    // respawn; dead actors become Spectating), >0 = respawn delay.
    float respawnSeconds = -1.0f;
    bool killHeals = true;
    // Active gamemode policy: false means same-team actors cannot damage one
    // another. Loaded from Gamemode::friendlyFire at mode start.
    bool friendlyFireEnabled = false;
    std::string winCondition;

    // ── Round-based match lifecycle (Counter-Strike and future modes) ──
    // Active only when victoryCondition == "rounds". Owns the ordered
    // round chain, per-team round wins, and the round/version identity used
    // to reject stale packets from a previous round.
    bool objectiveRounds = false;
    std::string victoryCondition;      // "rounds" enables the round chain
    uint32_t roundNumber = 0;          // 1-based current round
    uint32_t roundVersion = 0;         // bumped every round start/end; stale guard
    int roundWins[2] = {0, 0};         // per-team round wins (indexed by team)
    int roundsToWin = 0;               // first to this many rounds wins the match
    int maxRounds = 0;                 // configured round ceiling (JSON-controlled)
    bool endlessRounds = false;        // results always advance to the next round
    float roundSeconds = 0.0f;         // active round time limit
    uint32_t roundEndTick = 0;         // tick the active round times out
    int roundWinnerTeam = -1;          // -1 = no round decided yet
    int roundEndReason = 0;            // 0=none,1=elim,2=objective,3=time
    float freezeSeconds = 0.0f;        // freeze time before each round
    bool rosterLocked = false;         // true once team selection closes
    uint32_t roundNextNpcId = 0;       // next id for roster NPC mirror entries
    // Warmup: during a round mode's intermission the roster is spawned and
    // actors move/fight freely with infinite lives. Warmup ends at countdown.
    bool warmup = false;
    // Countdown freeze: the human cannot move during COUNTDOWN; released at GO.
    // NPCs are held by their own wakeupTimer for the same window.
    bool roundCountdownFreeze = false;
    // ── Mode objective item (bomb; future payload/capture/escort) ──
    // Server-owned. The gamemode JSON declares the kind/carrier team; the
    // runtime owns carrier/drop/pickup state. Never stored in an actor preset.
    ObjectiveInstance objective;
    uint32_t objectiveNextCarrierScanTick = 0;
    uint32_t objectivePickupCounter = 0;
    uint32_t objectiveDropCounter = 0;
    // ── Generic area effects (fire/smoke/dark-bang grenades) ────────
    // Server-authoritative; ticked at fixed 60 Hz. Damage is applied through
    // the shared damage path via the returned damage events.
    std::vector<AreaEffect> areaEffects;
    uint32_t nextAreaEffectId = 1;
    uint32_t areaEffectSpawnCounter = 0;
    // ── Team-level tactical brains (one per fixed team) ─────────────
    // Own assignments, shared enemy reports, and objective targeting. They
    // never teleport or override physics.
    TeamBrain teamBrainA{0};
    TeamBrain teamBrainB{1};

    bool npcWaves = false;
    bool persistentNpcSpawns = false;
    uint32_t waveNumber = 0;
    int waveStartCount = 1;
    int waveIncrement = 1;
    int waveNpcsPerWave = 0;
    int waveLives = 3;
    int waveLivesRemaining = 3;
    uint32_t waveHighest = 0;
    float waveBannerSeconds = 3.0f;
    bool waveStaggerEnabled = true;
    int waveNpcsPerTick = 10;
    uint32_t waveNpcTarget = 0;
    uint32_t waveNpcSpawned = 0;
    uint32_t waveNextNpcId = 100000;
    uint32_t waveBannerUntilTick = 0;
    bool waveBannerVisible = false;
    bool waveRunOver = false;
    int npcSpawnMax = 0;
    int npcSpawnIntervalTicks = 60;
    int npcSpawnPerInterval = 1;
    uint32_t npcSpawnNextTick = 0;
    uint32_t npcSpawnSequence = 0;

    // Match event counter for KillEvent IDs
    uint32_t killEventCounter = 0;

    // ── Bomb Tag fields ─────────────────────────────────────────────
    // Server-authoritative bomb ownership, timer, and inactive state.
    // Only active when matchMode == "bombtag".
    uint8_t bombOwnerType = 0;          // BombTagOwnerType (0=none, 1=player, 2=npc)
    uint32_t bombOwnerPlayerId = 0;     // Player ID if owner is player
    uint32_t bombOwnerNpcIndex = 0;     // NPC index if owner is NPC
    uint32_t bombTimerTicks = 0;        // Remaining ticks until explosion
    uint32_t bombInactiveTicks = 0;     // Ticks remaining in inactive grace
    uint32_t bombTimerTicksMax = 900;   // Config: ticks per bomb cycle (15s * 60)
    uint32_t bombInactiveTicksMax = 60; // Config: inactive grace ticks after pass
    uint32_t bombBlinkTicks = 30;       // Config: ticks per color blink phase
    float bombMaxPassSanityDist = 3.0f; // Config: hard rejection distance (meters)
    bool bombTagActive = false;         // True when bomb tag match is running
    bool hasBombFeature = false;        // True when gamemode declares bomb_holder_text feature
    bool pendingModeSwitch = false;
    bool pendingModeSwitchCountdown = false;
    std::string pendingGamemodeId;
    bool pendingJuggernautSkip = false;
    uint32_t bombPassCounter = 0;       // Total passes this session (for logging)
    uint32_t bombExplosionCounter = 0;  // Total explosions this session
    // ── Gamemode visual overrides ───────────────────────────────────
    float cameraFov = 0.0f;         // 0 = no override
    bool forceFirstPerson = false;
    bool hideHealthbars = false;
    bool ragdollExplicit = false;   // true if gamemode defines ragdoll_enabled
    bool ragdollEnabled = false;    // value when ragdollExplicit is true
    bool bloodExplicit = false;     // true if gamemode defines blood_enabled
    bool bloodEnabled = false;      // value when bloodExplicit is true
    // ── Generic disaster runtime (JSON mode packs) ──────────────────
    // Data-driven: a mode pack declares capabilities/disasters and the server
    // resolves them into an action graph before the match. No mode-name branch.
    uint32_t matchSeed = 0;                       // replicated round seed
    MimitaGamemode::ActionGraph disasterGraph;    // resolved capability schedule
    MimitaGamemode::DisasterState disaster;       // active disaster state
    uint32_t disasterPhaseStartTick = 0;          // tick the active phase began

    // ── Procedural world (Infinite Dungeon Slayer) ──────────────────
    // Server-owned room lifecycle state, replicated via DuelStatePacket.
    MimitaProcedural::ProceduralWorldState procedural;
    // Requests queued by server commands and applied on the next server tick,
    // which owns the world, npcWorld, and npcSystem. Kept outside
    // `procedural` so a start/stop reset cannot clobber a same-tick request.
    struct PendingProceduralRequest
    {
        bool start = false;
        std::string modeId;
        uint32_t seed = 0;
        uint32_t requesterId = 0;
        bool stop = false;
        bool generateNext = false;
        bool teleportHighest = false;
        uint32_t teleportRequesterId = 0;
    } pendingProcedural;
};

// Singleton gamemode state for the current server process.
ServerGamemodeState& serverGamemodeState();

// Authoritative match lifecycle-rule queries. These read the active
// ServerGamemodeState so damage/respawn/NPC code has one source of truth.
// serverMatchRespawnsEnabled: false => one-life; dead actors become Spectating.
bool serverMatchRespawnsEnabled();
bool serverPlayerRespawnsEnabled(uint32_t playerId);
bool serverFriendlyFireEnabled();
bool serverFriendlyFireBlocks(
    uint32_t attackerId, bool attackerNpc,
    uint32_t victimId, bool victimNpc,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    const char* path, uint32_t tick);
void serverConsumeNpcWaveLife(uint32_t playerId);
// Effective respawn delay in seconds (unset falls back to the legacy 0.01s).
float serverMatchRespawnSeconds();

// Role-resolved spawn data for one authoritative actor. Values are 0/empty when
// the actor has no role or the role does not override that field, so callers
// keep their legacy/default behavior. Weapon list comes from the referenced
// weapon set (no weapon definitions are duplicated inside roles).
struct ActorSpawnProfile
{
    bool hasRole = false;
    std::string roleId;
    int health = 0;                       // 0 = no override
    int weaponSetId = 0;                  // 0 = no override
    std::string startingWeapon;
    std::string avatarName;
    std::vector<std::string> weapons;     // resolved role loadout (may be empty)
    std::string movementPreset;           // resolved/validated role movement preset
    std::string behaviorProfileId;        // resolved/validated role behavior profile
    std::string actorPresetId;            // resolved actor preset owning the movement policy
};
ActorSpawnProfile serverResolveActorSpawnProfile(uint32_t actorId);
bool serverActivateActorPreset(const std::string& presetId);
void serverResetActorPreset();

// Start the shared server runtime with the given mode rules. Duel, FFA, TDM,
// and sandbox all use this same lifecycle owner.
void serverStartMode(const ServerGamemodeState& rules);

// Compatibility entry point for existing duel callers.
void serverGamemodeStart(const ServerGamemodeState& rules);

// Called every server tick while the server runs a managed gamemode.
// `npcs`/`npcSystem`/`npcIdsAlive` let the engine drop the practice NPC(s)
// the moment the real duel starts (both players active).
void serverGamemodeTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    HeadlessWorld& world,
                    World& npcWorld,
                    std::unordered_map<uint32_t, ServerNpc>& npcs,
                    NpcSystem& npcSystem,
                    std::unordered_set<uint32_t>& npcIdsAlive,
                    uint32_t tick,
                    uint64_t& totalPacketsOut);

// Single authoritative kill owner. Every lethal path calls this once. It
// heals and credits a player killer, broadcasts exactly one reliable
// KillEventPacket to every client (killer, victim, and observers), and queues
// one ServerGamemodeKillEvent for scoring, respawn, and persistence on the next
// serverGamemodeTick. `npcs` may be null when the caller only has players.
void serverGamemodeRecordKill(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>* npcs,
    uint32_t killerId, uint8_t killerEntityType,
    uint32_t victimId, uint8_t victimEntityType,
    const std::string& weaponId,
    const std::string& weaponDisplayName,
    uint64_t correlationId,
    const glm::vec3& killerPos,
    const glm::vec3& victimPos,
    uint32_t tick,
    uint64_t& totalPacketsOut);

// Spawn a generic area effect (fire/smoke/dark-bang) at a world position.
// Server-authoritative; the effect is ticked at fixed 60 Hz. Returns the id.
uint32_t serverSpawnAreaEffect(AreaEffectKind kind, uint32_t ownerActorId,
                               int ownerTeam, const glm::vec3& position,
                               float radius, float height, float durationSeconds,
                               int damagePerTick, int damageIntervalTicks,
                               bool damagesEnemiesOnly);

// Spawn the area effect a grenade leaves behind, if its definition has one.
// Returns 0 for direct-explosion grenades (e.g. frag) or unknown ids.
uint32_t serverSpawnGrenadeAreaEffect(const std::string& grenadeId,
                                      uint32_t ownerActorId, int ownerTeam,
                                      const glm::vec3& position);

// A player pressed Space on the win/lose screen: skip the rematch timer and
// start the next managed match immediately (next tick).
void serverGamemodeRematchNow();

// Host-only changemap command: reload the given map live on the next tick.
void serverGamemodeRequestMapChange(const std::string& mapId);
std::string serverActiveTeamList();
bool serverRequestTeamChange(uint32_t playerId, int requestedTeam,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>& npcs,
                             uint32_t tick, uint64_t& totalPacketsOut,
                             std::string& message);
void serverRespawnAllActors(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut);

// Focused, world-independent selftest for the round-based match lifecycle.
// Validates roster sizing, team ordering, round-win tallying, victory
// threshold, and stale-round versioning without a World/NpcSystem. Returns
// true on success and fills `report` with a human-readable summary.
bool serverCounterStrikeRoundSelfTest(std::string& report);

// Selftest for map spawn-tag classification (spawnpoint.CT / spawnpoint.T).
bool serverSpawnTagSelfTest(std::string& report);

// Starts the shared community map runtime without enabling match scoring.
void serverCommunityMapStart(const std::vector<std::string>& mapPool,
                             const std::string& mapId,
                             bool autoRotation,
                             uint32_t rotationMinutes,
                             int weaponSetId);
void serverCommunitySetMode(const std::string& modeId);
void serverCommunitySetWeaponSet(int weaponSetId);
bool serverCommunityWeaponAllowed(const std::string& weaponId);
int serverCommunityWeaponNativeSlot(int logicalSlot);
int serverCommunityWeaponLogicalSlot(const std::string& weaponId);
int serverCommunityWeaponNativeSlot(const ServerPlayer& player, int logicalSlot);
int serverCommunityWeaponLogicalSlot(const ServerPlayer& player, const std::string& weaponId);
void serverCommunityStartMatch(bool skipIntermission = false,
                               const std::string& requestedMode = {});
void serverGamemodeRequestJuggernautSkip();

// ── Procedural world (Infinite Dungeon Slayer) server API ─────────────
// Starts the mode: loads the room recipe once, appends lobby + room 1
// collision geometry to the headless worlds, spawns room 1's encounter, locks
// its exit, and leaves replication to the match-state broadcast. Call
// serverProceduralWorldStop first when restarting.
bool serverProceduralWorldStart(const std::string& modeId, uint32_t seed,
                                HeadlessWorld& world, World& npcWorld,
                                std::unordered_map<uint32_t, ServerNpc>& npcs);

// Advances the encounter each server tick. Returns true when the replicated
// state changed (room completed / next room generated / door unlocked).
bool serverProceduralWorldTick(SOCKET sock,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               HeadlessWorld& world,
                               World& npcWorld,
                               std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& npcSystem,
                               uint32_t tick);

// Disables procedural mode and removes only procedural rooms, encounter NPCs,
// and barriers. Unrelated sandbox/map/NPC state is left untouched.
void serverProceduralWorldStop(HeadlessWorld& world, World& npcWorld,
                               std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& npcSystem);

// True when the id belongs to the active procedural encounter. Used to disable
// normal NPC respawn for procedural actors.
bool serverProceduralWorldOwnsNpc(uint32_t entityId);

// World-space entrance of the highest accessible room. False when no procedural
// mode is active.
bool serverProceduralWorldTeleportTarget(glm::vec3& outPosition);

// ── Bomb Tag server tick ──────────────────────────────────────────────
// Called every server tick when matchMode == "bombtag".
// Handles bomb timer countdown, physical contact validation with rewind,
// bomb transfer, explosion, shuffle-bag holder selection, and state replication.
void serverBombTagTick(SOCKET sock,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       HeadlessWorld& world,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       NpcSystem& npcSystem,
                       uint32_t tick,
                       uint64_t& totalPacketsOut);

// ── Bomb Tag match start ──────────────────────────────────────────────
// Transitions community mode into bomb tag. Initializes shuffle bag,
// selects first bomb holder, sets timer.
void serverBombTagStartMatch(bool skipIntermission = false);

} // namespace MimitaNet
