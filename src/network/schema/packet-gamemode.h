// 2026-11-14
// MiMITA gamemode packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

enum DuelStatePhase : uint8_t
{
    DUEL_PHASE_WAITING = 0,
    DUEL_PHASE_COUNTDOWN = 1,
    DUEL_PHASE_ACTIVE = 2,
    DUEL_PHASE_MATCH_END = 3,
    DUEL_PHASE_INTERMISSION = 4,
    DUEL_PHASE_PRE_MATCH = 5, // Retained for wire compatibility; never emitted by the active lifecycle.
    DUEL_PHASE_RESULTS = 6
    ,DUEL_PHASE_GO = 7
    ,DUEL_PHASE_AFTERMATH = 8
};

// Procedural-world room state (Infinite Dungeon Slayer). Rides inside the
// existing authoritative match-state packet (DuelStatePacket) so there is one
// replication loop. The client never decides room completion; it only applies
// this server-owned state and reconstructs room geometry from seed + count.
enum ProceduralRoomStateNetwork : uint8_t
{
    PROCEDURAL_ROOM_WAITING = 0,
    PROCEDURAL_ROOM_ACTIVE = 1,
    PROCEDURAL_ROOM_COMPLETE = 2
};

struct ProceduralWorldNetworkState
{
    uint8_t enabled = 0;
    uint8_t roomState = PROCEDURAL_ROOM_WAITING;
    uint8_t exitLocked = 0;
    uint8_t reserved = 0;

    char modeId[32] = {};

    uint32_t seed = 0;
    uint32_t currentRoom = 0;
    uint32_t generatedRooms = 0;
    uint32_t highestAccessibleRoom = 0;
    uint32_t aliveEncounterActors = 0;
    uint32_t stateVersion = 0;
};

struct DuelStatePacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t duelId = 0;
    uint32_t mapVersion = 0;
    uint32_t spawnAnchorVersion = 0;
    uint32_t respawnSequence = 0;
    uint32_t stateVersion = 0;
    uint8_t phase = 0;          // DuelStatePhase
    uint8_t matchOver = 0;      // 1 = scores locked; keep fighting, no points
    uint8_t reserved[2] = {};
    int32_t scoreA = 0;
    int32_t scoreB = 0;
    int32_t goalValue = 20;
    float countdownLeft = 0.0f;
    float phaseTimer = 0.0f;
    float rematchLeft = 0.0f;
    uint32_t playerAId = 0;
    uint32_t playerBId = 0;
    uint32_t winnerPlayerId = 0;
    char teamAName[32];
    char teamBName[32];
    char mapId[MAX_MAP_ID_BYTES];
    uint32_t spawnAnchorIndex = 0;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorZ = 0.0f;
    uint32_t playerASpawnGeneration = 0;
    uint32_t playerBSpawnGeneration = 0;
    float playerASpawnX = 0.0f;
    float playerASpawnY = 0.0f;
    float playerASpawnZ = 0.0f;
    float playerBSpawnX = 0.0f;
    float playerBSpawnY = 0.0f;
    float playerBSpawnZ = 0.0f;

    // ── FFA/TDM extension fields ───────────────────────────────────
    // Gamemode ids are content-defined ("funcounterstrike", "survive_disasters").
    // Keep room for the longest shipped id; a 16-byte field silently truncated
    // "funcounterstrike" into a different, nonexistent mode id on the client.
    char matchMode[32] = {};           // "duel", "ffa", "tdm", ...
    uint32_t matchStartTick = 0;       // authoritative tick when match started
    uint32_t serverTick = 0;           // current server tick for sync
    int32_t victoryType = 0;           // 0=ScoreLimit, 1=TimeLimit
    int32_t redTeamKills = 0;          // TDM red team score
    int32_t blueTeamKills = 0;         // TDM blue team score
    int32_t timeLimitSeconds = 0;      // match time limit
    int32_t intermissionSeconds = 0;   // intermission duration
    int32_t resultsSeconds = 0;        // results display duration
    float goSeconds = 0.0f;            // GO! overlay duration; lets the client
                                       // show GO even if the GO-phase packet is
                                       // missed while loading
    uint32_t waveNumber = 0;
    uint32_t waveNpcTarget = 0;
    uint32_t waveNpcSpawned = 0;
    uint32_t waveBannerUntilTick = 0;
    uint8_t waveBannerVisible = 0;
    uint8_t waveReserved[3] = {};
    int32_t waveLivesRemaining = 0;
    uint32_t waveHighest = 0;
    // FFA top-3 leaderboard (for HUD rendering)
    uint32_t ffaLeaderIds[3] = {};
    int32_t ffaLeaderScores[3] = {};
    char ffaLeaderNames[3][64] = {};
    // All participant IDs and teams (for the generic participant/results UI)
    uint32_t participantIds[64] = {};
    uint8_t participantCount = 0;
    uint8_t participantTeams[64] = {};  // 0=red, 1=blue, 0xFF=none
    // Actor identity: 1-based MatchRoleRegistry index (0 = none), and ActorState.
    uint8_t participantRoles[64] = {};
    uint8_t participantStates[64] = {};
    // 1-based index for the actor-specific preset applied to this life; 0 = none.
    uint8_t participantActorPresets[64] = {};
    // ── Gamemode visual overrides ───────────────────────────────────
    float cameraFov = 0.0f;         // 0 = no override
    uint8_t ragdollEnabled = 0;     // 0=no override, 1=disabled, 2=enabled
    uint8_t bloodEnabled = 0;       // 0=no override, 1=disabled, 2=enabled
    uint8_t forceFirstPerson = 0;
    char actorPresetId[64] = {};
    // ── Round-based match fields (Counter-Strike and future modes) ──
    // roundVersion rejects stale packets from a previous round; roundWins is
    // the per-team round tally in the mode's fixed team order.
    uint32_t roundVersion = 0;
    uint32_t roundNumber = 0;
    int32_t roundWins[2] = {0, 0};
    int32_t winnerTeam = -1;        // -1 = none; else fixed team index
    uint8_t roundEndReason = 0;     // 0=none, 1=elimination, 2=objective, 3=time
    uint8_t roundsReserved[3] = {};
    float roundSeconds = 0.0f;      // active round time limit
    float roundTimerLeft = 0.0f;    // seconds left in the active round
    // ── Generic objective item (bomb; future payload/capture) ───────
    uint8_t objectiveActive = 0;    // 1 = a mode objective exists
    uint8_t objectiveKind = 0;      // ObjectiveKind
    uint8_t objectiveState = 0;     // ObjectiveState
    uint8_t objectiveTeam = 0;      // allowed carrier team; 0xFF = any
    uint32_t objectiveCarrierId = 0;// 0 = none
    uint32_t objectiveInteractorId = 0; // active planter/defuser; 0 = none
    uint16_t objectiveRequiredTicks = 0; // active plant/defuse duration
    uint16_t objectiveTapTicks = 0;      // repeated-tap progress per press
    float objectiveX = 0.0f;
    float objectiveY = 0.0f;
    float objectiveZ = 0.0f;
    float objectiveProgress = 0.0f; // 0..1 plant or defuse progress
    float objectiveTimerLeft = 0.0f;// seconds until explosion when planted
    float objectiveInteractionRange = 3.0f; // plant/defuse prompt + server range
    uint8_t objectiveProgressKind = 0; // 0=none, 1=plant, 2=defuse
    uint8_t objectiveInteractionMode = 0; // ObjectiveInteractionMode
    char objectiveId[16] = {};
    char objectiveSite[8] = {};
    // ── Procedural world (Infinite Dungeon Slayer) ──────────────────
    // Appended section: server-owned room state. All-zero means disabled.
    ProceduralWorldNetworkState procedural = {};
    // ── Zombie Tower first-playable run state ───────────────────────
    uint8_t ztAttemptResult = 0;
    uint8_t ztFinalResult = 0;
    uint8_t ztRunWon = 0;
    uint8_t ztBossActive = 0;
    uint8_t ztMusicPlaying = 0;
    uint8_t ztFog = 0;
    uint8_t ztRain = 0;
    uint8_t ztTorchLighting = 0;
    int32_t ztAttemptsRemaining = 0;
    uint32_t ztRunElapsedTicks = 0;
    char ztBossName[64] = {};
    int32_t ztBossHp = 0;
    int32_t ztBossMaxHp = 0;
    char ztMostDamageName[64] = {};
    uint32_t ztMostDamage = 0;
    char ztMostHealingName[64] = {};
    uint32_t ztMostHealing = 0;
    char ztTopSpeedName[64] = {};
    float ztTopSpeed = 0.0f;
    uint32_t ztCheckpointSerial = 0;
    char ztCheckpointName[64] = {};
    uint32_t ztMusicSerial = 0;
    char ztMusicTrack[260] = {};
    // ── Payload HUD (protocol 45) ──────────────────────────────────
    // Overtime is stored in server ticks; the client formats seconds and
    // milliseconds. Progress is 0..1 along the authored route.
    uint8_t payloadActive = 0;
    uint8_t payloadOvertime = 0;
    uint8_t payloadAdvancing = 0;
    uint8_t payloadBlocked = 0;
    uint8_t payloadRollingBack = 0;
    uint8_t payloadAttackers = 0;
    uint8_t payloadDefenders = 0;
    uint8_t payloadReserved = 0;
    uint32_t payloadOvertimeTicksLeft = 0;
    uint32_t payloadOvertimeTicksMax = 0;
    float payloadDistance = 0.0f;
    float payloadTotalDistance = 0.0f;
    float payloadProgress = 0.0f;
    float payloadCartX = 0.0f;
    float payloadCartY = 0.0f;
    float payloadCartZ = 0.0f;
    // ── Loss aftermath (protocol 48) ───────────────────────────────
    // Per-recipient local presentation effect. Non-target recipients receive
    // stage=0/targetActorId=0 and keep normal rendering.
    uint8_t aftermathActive = 0;
    uint8_t aftermathStage = 0;      // 1=approach, 2=scare sequence, 3=done
    uint8_t aftermathHideHud = 0;
    uint8_t aftermathMuteAudio = 0;
    uint8_t aftermathBlackout = 0;
    uint8_t aftermathFreezeLocal = 0;
    uint8_t aftermathFatalWindow = 0;
    uint8_t aftermathReserved = 0;
    uint32_t aftermathTargetActorId = 0;
    uint32_t aftermathMoverActorId = 0;
    uint32_t aftermathStartTick = 0;
    uint32_t aftermathSoundSerial = 0;
    uint8_t aftermathSoundIndex = 0xFF;
    uint8_t aftermathSoundReserved[3] = {};
    float aftermathMoverX = 0.0f;
    float aftermathMoverY = 0.0f;
    float aftermathMoverZ = 0.0f;
};

// Server → a player: their opponent just respawned here. Used to draw a
// short "look over here" tracer at the enemy's spawn.
struct DuelEnemySpawnPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t duelId = 0;
    uint32_t mapVersion = 0;
    uint32_t spawnAnchorVersion = 0;
    uint32_t respawnSequence = 0;
    uint32_t enemyPlayerId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
};


// Client → server: skip the rematch timer and start the next duel now.
struct DuelRematchRequestPacket
{
    PacketHeader header;
};

// Server → all clients: the map changed live; load the new one and re-ready.

struct ServerNotificationPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint16_t durationTicks = 180;
    char title[32] = {};
    char message[192] = {};
};

// Server-owned progression feedback, using the shared reliable event envelope.
struct ProgressionEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint8_t kind = 0;
    char name[128] = {};
    char confirmedAt[40] = {};
};

// ── Batched Spy Knife physical contacts (attacker → server) ───────────
// Damage is never trusted from this packet. The server reconstructs the
// target pose at contactTick and resolves damage from weapon configuration.
enum BombTagOwnerType : uint8_t
{
    BOMB_OWNER_NONE = 0,
    BOMB_OWNER_PLAYER = 1,
    BOMB_OWNER_NPC = 2
};

struct BombTagStatePacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t duelId = 0;
    uint32_t stateVersion = 0;
    uint8_t phase = 0;                  // DuelStatePhase
    uint8_t bombOwnerType = 0;          // BombTagOwnerType
    uint8_t reserved[2] = {};
    uint32_t bombOwnerPlayerId = 0;     // player ID if owner is player
    uint32_t bombOwnerNpcIndex = 0;     // NPC index if owner is NPC
    uint32_t timerTicksRemaining = 0;   // server-authoritative bomb timer
    uint32_t inactiveTicksRemaining = 0;// 60-tick cooldown after pass
    uint32_t serverTick = 0;            // current server tick for sync
    float bombPosX = 0.0f;
    float bombPosY = 0.0f;
    float bombPosZ = 0.0f;
};

// ── Bomb Tag pass visualization event (server → all) ─────────────────
// Broadcast on every successful bomb transfer so clients can draw a
// pass beam and display distance information.
struct BombTagPassEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t serverTick = 0;
    uint32_t oldOwnerPlayerId = 0;
    uint32_t newOwnerPlayerId = 0;
    uint32_t oldOwnerNpcIndex = 0;
    uint32_t newOwnerNpcIndex = 0;
    float oldBombPosX = 0.0f;
    float oldBombPosY = 0.0f;
    float oldBombPosZ = 0.0f;
    float newBombPosX = 0.0f;
    float newBombPosY = 0.0f;
    float newBombPosZ = 0.0f;
    float passDistance = 0.0f;
    float serverRewoundDistance = 0.0f;
    uint8_t accepted = 1;
    uint8_t reserved[3] = {};
};

static_assert(sizeof(SnapshotPacket) < MAX_RECEIVE_PACKET_BYTES, "SnapshotPacket exceeds client receive buffer");
static_assert(sizeof(ShotRequestPacket) <= 132, "ShotRequestPacket is too large");
static_assert(sizeof(ShotEventPacket) <= 160, "ShotEventPacket is too large");
static_assert(sizeof(ProjectileSpawnEventPacket) <= 128, "ProjectileSpawnEventPacket is too large");
static_assert(sizeof(ProjectileStateEventPacket) <= 112, "ProjectileStateEventPacket is too large");
static_assert(sizeof(ProjectileExplodeEventPacket) < MAX_GAME_DATAGRAM_BYTES,
              "ProjectileExplodeEventPacket exceeds safe datagram limit");
static_assert(sizeof(MeleeHitEventPacket) <= 96, "MeleeHitEventPacket is too large");
static_assert(sizeof(DamageConfirmedEventPacket) <= 120, "DamageConfirmedEventPacket is too large");
static_assert(sizeof(DisagreementPacket) <= 128, "DisagreementPacket is too large");
static_assert(sizeof(PelletBlastRequestPacket) <= 80, "PelletBlastRequestPacket is too large");
static_assert(sizeof(PelletBlastEventPacket) < MAX_GAME_DATAGRAM_BYTES,
              "PelletBlastEventPacket exceeds safe datagram limit");
static_assert(sizeof(PelletBlastTargetResult) <= 24, "PelletBlastTargetResult is too large");
// 8 claimed-hit targets (multi-pellet shotgun claims) exceed the legacy 128-byte
// bound. Keep the explicit cap but allow the intended claimed-hit payload.
static_assert(sizeof(AttackRequestPacket) <= 384, "AttackRequestPacket is too large");
static_assert(sizeof(AttackResultPacket) <= 96, "AttackResultPacket is too large");
static_assert(sizeof(ReloadRequestPacket) <= 48, "ReloadRequestPacket is too large");
static_assert(sizeof(ReloadResultPacket) <= 72, "ReloadResultPacket is too large");
static_assert(sizeof(RespawnRequestPacket) <= 32, "RespawnRequestPacket is too large");
static_assert(sizeof(PlayerRespawnedPacket) <= 584, "PlayerRespawnedPacket is too large");
static_assert(sizeof(BombTagStatePacket) <= 96, "BombTagStatePacket is too large");
static_assert(sizeof(BombTagPassEventPacket) <= 96, "BombTagPassEventPacket is too large");

// ── Disaster state replication (server → clients) ────────────────────
// One authoritative descriptor for the active mode-pack disaster. `seed` and
// `startTick` let clients reproduce the same deterministic per-actor weapon
// assignment without shipping every assignment over the wire.
struct DisasterStatePacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t duelId = 0;
    uint32_t stateVersion = 0;
    uint32_t seed = 0;              // authoritative round seed
    uint32_t startTick = 0;         // tick the disaster became active
    uint32_t durationTicks = 0;     // bounded duration in 60 Hz ticks
    uint32_t winnerActor = 0;       // resolved winner actor id (0 = none)
    uint8_t phase = 0;              // DuelStatePhase
    uint8_t active = 0;             // 1 while the disaster is running
    uint8_t resolveSource = 0;      // 0=none, 1=last_alive, 2=timeout
    uint8_t reserved = 0;
    char disasterId[32] = {};
    char name[48] = {};
    char description[96] = {};
};

static_assert(sizeof(DisasterStatePacket) <= 256, "DisasterStatePacket is too large");

#pragma pack(pop)

}  // namespace MimitaNet
