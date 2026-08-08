// 07 21 2026, 16 45
/* purpose
* Defines MiMITA network packet IDs, fixed wire structs, flags, and datagram limits.
* Carries compact gameplay state for join, input, snapshots, weapons, projectiles, and lifecycle.
* Keeps packet layouts explicit and protocol-versioned for client/server compatibility.
* Does NOT own transport sockets, validation policy, gameplay simulation, or rendering.
* Does NOT trust client health, damage, death, ammo, score, projectile hits, or weapon outcomes.
* Does NOT encode private runtime-only Player or MovementState object layouts.
*/

#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8
constexpr uint16_t PROTOCOL_VERSION = 28;

// ── Player state flags for remote visual replication ──────────────
enum NetworkPlayerStateFlags : uint16_t
{
    NET_STATE_WALKING      = 1 << 0,
    NET_STATE_JUMPING      = 1 << 1,
    NET_STATE_DASHING      = 1 << 2,
    NET_STATE_DOWN_DASHING = 1 << 3,
    NET_STATE_FREEZING     = 1 << 4,
    NET_STATE_ON_GROUND    = 1 << 5,
    NET_STATE_ATTACKING    = 1 << 6
};
constexpr int MAX_RECONNECT_TOKEN_BYTES = 64;
constexpr int MAX_JOIN_TOKEN_BYTES = 64;
constexpr int MAX_VIP_JOIN_TICKET_BYTES = 64;
constexpr int MAX_PLAYERS = 32;
constexpr int MAX_SNAPSHOT_ENTITIES = 96;
constexpr int MAX_NAME_BYTES = 32;

// Safe datagram size: 1200 bytes ensures no IP fragmentation on internet paths.
// Accounts for IP header (20), UDP header (8), ICE/STUN overhead (~32),
// and leaves ~1140 bytes for game payload.
constexpr int MAX_GAME_DATAGRAM_BYTES = 1200;

enum PacketType : uint8_t
{
    PACKET_HELLO = 1,
    PACKET_WELCOME = 2,
    PACKET_INPUT = 3,
    PACKET_SNAPSHOT = 4,
    PACKET_DISCONNECT = 5,
    PACKET_PING = 6,
    PACKET_PLAYER_LIST = 7,
    PACKET_PROFILE = 8,
    PACKET_ENTITY_SPAWN = 9,
    PACKET_ENTITY_DESPAWN = 10,
    PACKET_SPAWN_NPC_REQUEST = 11,
    PACKET_TELEPORT_REQUEST = 12,
    PACKET_EXPLODE_REQUEST = 13,
    PACKET_SHOT_REQUEST = 14,
    PACKET_SHOT_EVENT = 15,
    PACKET_CHAT_MESSAGE = 16,
    PACKET_NPC_DAMAGE_REQUEST = 17,
    PACKET_NPC_DAMAGE_EVENT = 18,
    PACKET_SERVER_COMMAND = 19,
    // ── Migration packet types ──────────────────────────────────
    PACKET_JOIN_REQUEST = 20,
    PACKET_JOIN_ACCEPT = 21,
    PACKET_JOIN_REJECT = 22,
    PACKET_RECONNECT_REQUEST = 23,
    PACKET_RECONNECT_ACCEPT = 24,
    PACKET_DISAGREEMENT = 25,
    PACKET_CLIENT_MAP_READY = 26,
    PACKET_PROJECTILE_FIRE_REQUEST = 27,
    PACKET_PROJECTILE_SPAWN_EVENT = 28,
    PACKET_PROJECTILE_STATE_EVENT = 29,
    PACKET_PROJECTILE_EXPLODE_EVENT = 30,
    PACKET_PROJECTILE_DESPAWN_EVENT = 31,
    PACKET_MELEE_HIT_REQUEST = 32,
    PACKET_MELEE_HIT_EVENT = 33,
    PACKET_PELLET_BLAST_REQUEST = 34,
    PACKET_PELLET_BLAST_EVENT = 35,
    PACKET_GODBALL_STATE = 36,
    PACKET_PROJECTILE_FIRE_RESULT = 37,
    // ── Generic attack pipeline ─────────────────────────────────────
    PACKET_ATTACK_REQUEST = 38,
    PACKET_ATTACK_RESULT = 39,
    // ── Authoritative reload ────────────────────────────────────────
    PACKET_RELOAD_REQUEST = 40,
    PACKET_RELOAD_RESULT = 41,
    // ── Respawn ─────────────────────────────────────────────────────
    PACKET_RESPAWN_REQUEST = 42,
    PACKET_PLAYER_RESPAWNED = 43,
    PACKET_SPAWN_ACK = 44,
    PACKET_SPAWN_ACTIVATED = 45,
    PACKET_RELIABLE_EVENT_ACK = 46,
    PACKET_DAMAGE_CONFIRMED_EVENT = 47,
    // ── Chat v2 ───────────────────────────────────────────────────────
    PACKET_CHAT_REQUEST = 48,
    PACKET_CHAT_MESSAGE_EVENT = 49,
    PACKET_CHAT_TYPING_STATE_REQUEST = 50,
    PACKET_CHAT_TYPING_STATE_EVENT = 51,
    // ── VIP name style sync ───────────────────────────────────────────
    PACKET_VIP_STYLE_EVENT = 52
};

enum DamageConfirmedSource : uint8_t
{
    DAMAGE_CONFIRMED_HITSCAN = 1,
    DAMAGE_CONFIRMED_MELEE = 2,
    DAMAGE_CONFIRMED_ROCKET_EXPLOSION = 3,
    DAMAGE_CONFIRMED_GRENADE_EXPLOSION = 4,
    DAMAGE_CONFIRMED_PHYSICAL_CONTACT = 5
};

enum EntityType : uint8_t
{
    ENTITY_NONE = 0,
    ENTITY_PLAYER = 1,
    ENTITY_NPC = 2
};

enum NetworkWeaponType : uint8_t
{
    NETWORK_WEAPON_NONE = 0,
    NETWORK_WEAPON_REVOLVER = 1,
    NETWORK_WEAPON_GODBALL = 2,
    NETWORK_WEAPON_SHOTGUN = 3,
    NETWORK_WEAPON_SWORDSWORD = 4,
    NETWORK_WEAPON_ROCKET_LAUNCHER = 5,
    NETWORK_WEAPON_HAFS = 6,
    NETWORK_WEAPON_GRENADE_LAUNCHER = 7,
    NETWORK_WEAPON_AA12 = 8
};

enum NetworkWeaponStateFlags : uint8_t
{
    NET_WEAPON_STATE_FIRING = 1 << 0,
    NET_WEAPON_STATE_RELOADING = 1 << 1,
    NET_WEAPON_STATE_COOLDOWN = 1 << 2,
    NET_WEAPON_STATE_EMPTY = 1 << 3,
    NET_WEAPON_STATE_EQUIPPING = 1 << 4
};

enum ShotImpactType : uint8_t
{
    SHOT_IMPACT_NONE = 0,
    SHOT_IMPACT_WORLD = 1,
    SHOT_IMPACT_ENTITY = 2
};

enum DisagreementReason : uint8_t
{
    DISAGREEMENT_NONE = 0,
    DISAGREEMENT_OCCLUDED_SHOT = 1,
    DISAGREEMENT_INVALID_DAMAGE = 2,
    DISAGREEMENT_POSITION_CORRECTION = 3,
    DISAGREEMENT_INVALID_MOVEMENT = 4,
    DISAGREEMENT_INVALID_STATE = 5,
    DISAGREEMENT_REWIND_MISS = 6,
    DISAGREEMENT_TARGET_NOT_FOUND = 7,
    DISAGREEMENT_TARGET_DEAD = 8,
    DISAGREEMENT_SELF_TARGET = 9
};

// ── Pellet blast impact types ──────────────────────────────────────
enum PelletImpactType : uint8_t
{
    PELLET_IMPACT_NONE = 0,
    PELLET_IMPACT_WORLD = 1,
    PELLET_IMPACT_PLAYER = 2
};

enum HitBodyPart : uint8_t
{
    HIT_BODY_HEAD = 0,
    HIT_BODY_TORSO = 1,
    HIT_BODY_ARM = 2,
    HIT_BODY_LEG = 3
};

constexpr int MAX_NETWORK_PELLETS = 16;

enum ShotEffectFlags : uint16_t
{
    SHOT_EFFECT_MUZZLE = 1 << 0,
    SHOT_EFFECT_TRACER = 1 << 1,
    SHOT_EFFECT_SHOOT_SOUND = 1 << 2,
    SHOT_EFFECT_WORLD_IMPACT = 1 << 3,
    SHOT_EFFECT_DEBRIS = 1 << 4,
    SHOT_EFFECT_ENTITY_IMPACT = 1 << 5,
    SHOT_EFFECT_BLOOD = 1 << 6,
    SHOT_EFFECT_HIT_SOUND = 1 << 7,
    SHOT_EFFECT_WEAPON_TRIGGER = 1 << 8
};

#pragma pack(push, 1)

struct PacketHeader
{
    uint32_t magic = PROTOCOL_MAGIC;
    uint16_t version = PROTOCOL_VERSION;
    uint8_t type = 0;
    uint8_t reserved = 0;
    uint32_t tick = 0;
    uint32_t playerId = 0;
    uint32_t transformEpoch = 0;
};

static_assert(sizeof(PacketHeader) == 20, "PacketHeader wire size changed");

struct HelloPacket
{
    PacketHeader header;
    char name[MAX_NAME_BYTES];
};

static_assert(sizeof(HelloPacket) == 52, "HelloPacket wire size changed");

struct WelcomePacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_NAME_BYTES];
};

static_assert(sizeof(WelcomePacket) == 160, "WelcomePacket wire size changed");

struct InputCommandRedundancySlot
{
    uint32_t inputCommandSequence = 0;
    uint64_t clientSimulationTick = 0;
    float wishX = 0.0f;
    float wishY = 0.0f;
    float camForwardX = 1.0f;
    float camForwardY = 0.0f;
    float camForwardZ = 0.0f;
    float yaw = 0.0f;
    float lookPitch = 0.0f;
    uint16_t stateFlags = 0;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
};

static_assert(sizeof(InputCommandRedundancySlot) == 50,
              "InputCommandRedundancySlot wire size changed");

struct InputPacket
{
    PacketHeader header;
    float wishX = 0.0f;
    float wishY = 0.0f;
    float camForwardX = 1.0f;
    float camForwardY = 0.0f;
    float camForwardZ = 0.0f;
    float yaw = 0.0f;
    float clientPx = 0.0f;
    float clientPy = 0.0f;
    float clientPz = 0.0f;
    float clientVx = 0.0f;
    float clientVy = 0.0f;
    float clientVz = 0.0f;
    int16_t equippedSlot = 0;
    uint8_t weaponState = 0;
    uint8_t reservedWeapon = 0;
    int32_t clientPingMs = 0;
    uint16_t stateFlags = 0;
    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t directionChangeSerial = 0;
    uint16_t equipSerial = 0;
    uint16_t freezeSerial = 0;
    uint8_t attackPressed = 0;
    uint8_t spawnNpcPressed = 0;
    float sizeScale = 1.0f;
    uint32_t transformEpoch = 0;
    uint16_t respawnSerial = 0;
    uint32_t movementSequence = 0;
    uint64_t clientSimulationTick = 0;
    uint32_t spawnGeneration = 0;
    float externalImpulseX = 0.0f;
    float externalImpulseY = 0.0f;
    float externalImpulseZ = 0.0f;
    float lookPitch = 0.0f;
    uint32_t movementFlags = 0;
    uint32_t inputCommandSequence = 0;
    // Redundant recent movement commands (badconn loss resilience): the last
    // two commands are resent so a lost input packet still delivers its
    // movement command in the next packet. The server dedups by sequence.
    InputCommandRedundancySlot redundancy[2];
};

static_assert(sizeof(InputPacket) == 244, "InputPacket wire size changed");
static_assert(sizeof(InputPacket) <= 260,
              "InputPacket must stay compact enough for one safe datagram");

struct ProfilePacket
{
    PacketHeader header;
    char name[MAX_NAME_BYTES];
};

struct SnapshotEntity
{
    uint32_t networkEntityId = 0;
    uint8_t entityType = ENTITY_NONE;
    uint8_t active = 0;
    uint16_t stateFlags = 0;
    uint16_t transformEpoch = 0;
    uint32_t ownerClientId = 0;
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float yaw = 0.0f;
    int32_t health = 100;
    uint8_t onGround = 0;
    int16_t equippedSlot = 0;
    uint8_t weaponState = 0;
    float aimX = 1.0f;
    float aimY = 0.0f;
    float aimZ = 0.0f;
    int32_t pingMs = 0;
    float sizeScale = 1.0f;
    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t directionChangeSerial = 0;
    uint16_t equipSerial = 0;
    uint16_t freezeSerial = 0;
    uint32_t spawnGeneration = 0;
    char displayName[MAX_NAME_BYTES];
    uint8_t vipTier = 0;
    uint8_t vipStyleKind = 0;
    uint8_t vipColorR = 158;
    uint8_t vipColorG = 158;
    uint8_t vipColorB = 158;
    uint8_t vipFlags = 0;
    uint8_t vipStyleEpoch = 0;
    uint8_t vipReserved = 0;
};

struct SnapshotPacket
{
    PacketHeader header;
    uint32_t entityCount = 0;
    uint32_t playerCount = 0;
    uint32_t npcCount = 0;
    SnapshotEntity entities[MAX_SNAPSHOT_ENTITIES];
};

struct EntitySpawnPacket
{
    PacketHeader header;
    SnapshotEntity entity;
};

struct EntityDespawnPacket
{
    PacketHeader header;
    uint32_t networkEntityId = 0;
    uint8_t entityType = ENTITY_NONE;
    uint8_t reserved[3] = {};
};

#pragma pack(push, 1)
struct CompactEntityData
{
    uint32_t networkEntityId = 0;
    uint8_t entityType = MimitaNet::ENTITY_NONE;
    uint8_t active = 0;
    uint16_t stateFlags = 0;
    uint16_t transformEpoch = 0;
    uint32_t ownerClientId = 0;
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float yaw = 0.0f;
    float aimX = 1.0f, aimY = 0.0f, aimZ = 0.0f;
    int32_t health = 100;
    uint8_t onGround = 0;
    int16_t equippedSlot = 0;
    uint8_t weaponState = 0;
    int32_t pingMs = 0;
    float sizeScale = 1.0f;
    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t directionChangeSerial = 0;
    uint16_t equipSerial = 0;
    uint16_t freezeSerial = 0;
    uint32_t spawnGeneration = 0;
    char displayName[32]; // MAX_NAME_BYTES
    uint8_t vipTier = 0;
    uint8_t vipStyleKind = 0;
    uint8_t vipColorR = 158;
    uint8_t vipColorG = 158;
    uint8_t vipColorB = 158;
    uint8_t vipFlags = 0;
    uint8_t vipStyleEpoch = 0;
    uint8_t vipReserved = 0;
};
#pragma pack(pop)

static_assert(sizeof(CompactEntityData) == 128, "CompactEntityData unexpected size");

struct SnapshotChunkPacket
{
    PacketHeader header;
    uint32_t serverTick = 0;
    uint16_t chunkIndex = 0;
    uint16_t chunkCount = 1;
    uint16_t entityCount = 0;
    uint16_t payloadBytes = 0;
    CompactEntityData entities[9]; // 9 * 128 + header(32) = 1184 < 1200
};

static_assert(sizeof(SnapshotChunkPacket) < MAX_GAME_DATAGRAM_BYTES,
              "SnapshotChunkPacket exceeds safe datagram limit");
static_assert(sizeof(SnapshotChunkPacket) == 1184, "SnapshotChunkPacket wire size changed");

struct SpawnNpcRequestPacket
{
    PacketHeader header;
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float difficulty = 1.0f;
};

struct TeleportRequestPacket
{
    PacketHeader header;
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
};

struct ExplodeRequestPacket
{
    PacketHeader header;
};

struct PingPacket
{
    PacketHeader header;
    uint64_t clientTimeMs = 0;
};

struct ShotRequestPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
};

struct ShotEventPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    int32_t targetHealth = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint16_t targetTransformEpoch = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    uint8_t killed = 0;
    uint8_t damageConfirmed = 0;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
};

constexpr int MAX_PROJECTILE_DAMAGE_RESULTS = 8;

struct ProjectileDamageResultPacket
{
    uint32_t victimPlayerId = 0;
    int32_t damage = 0;
    int32_t healthAfter = 0;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    uint8_t killed = 0;
    uint8_t reserved[3] = {};
};

struct ProjectileFireRequestPacket
{
    PacketHeader header;
    uint32_t fireSerial = 0;
    uint32_t lastServerTick = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved[3] = {};
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
};

struct ProjectileSpawnEventPacket
{
    PacketHeader header;
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved[3] = {};
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float rotW = 1.0f;
    float angVelX = 0.0f;
    float angVelY = 0.0f;
    float angVelZ = 0.0f;
    uint32_t spawnTick = 0;
    float lifetime = 0.0f;
    float radius = 0.0f;
};

struct ProjectileStateEventPacket
{
    PacketHeader header;
    uint32_t projectileId = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved[3] = {};
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float rotW = 1.0f;
    float angVelX = 0.0f;
    float angVelY = 0.0f;
    float angVelZ = 0.0f;
    float age = 0.0f;
};

struct ProjectileExplodeEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t victimCount = 0;
    uint8_t reserved[2] = {};
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float radius = 0.0f;
    ProjectileDamageResultPacket victims[MAX_PROJECTILE_DAMAGE_RESULTS];
};

struct ProjectileDespawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t projectileId = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reason = 0;
    uint8_t reserved[2] = {};
};

struct ReliableEventAckPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
};

struct DamageConfirmedEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t attackerPlayerId = 0;
    uint32_t targetPlayerId = 0;
    uint32_t causeSerial = 0;
    uint32_t projectileId = 0;
    uint32_t attackerSpawnGeneration = 0;
    uint32_t targetSpawnGeneration = 0;
    int32_t damage = 0;
    int32_t healthBefore = 0;
    int32_t healthAfter = 0;
    uint8_t source = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t killed = 0;
    uint8_t reserved = 0;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
};

struct MeleeHitRequestPacket
{
    PacketHeader header;
    uint32_t attackSerial = 0;
    uint32_t lastServerTick = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t attackType = 0;
    uint8_t reserved[2] = {};
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    float weaponSpeed = 0.0f;
};

struct MeleeHitEventPacket
{
    PacketHeader header;
    uint32_t attackSerial = 0;
    uint32_t attackerPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    int32_t targetHealth = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t attackType = 0;
    uint8_t killed = 0;
    uint8_t damageConfirmed = 0;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
};

struct ChatPacket
{
    PacketHeader header;
    char senderName[MAX_NAME_BYTES];
    char text[240];
};

// Chat v2: client request to send a message
struct ChatRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint64_t clientSimulationTick = 0;
    char utf8Message[256] = {};
};

// Chat v2: server broadcasts this to all clients when a message is accepted
struct ChatMessageEventPacket
{
    PacketHeader header;
    uint64_t messageId = 0;
    uint64_t serverTick = 0;
    int64_t utcUnixMilliseconds = 0;
    uint32_t senderEntityId = 0;
    uint32_t senderAccountId = 0;
    uint8_t senderType = 0; // 0=Player, 1=Server
    uint8_t channel = 0;    // 0=Global
    char senderName[MAX_NAME_BYTES] = {};
    char utf8Message[256] = {};
};

// Client → Server: typing state notification
struct ChatTypingStateRequestPacket
{
    PacketHeader header;
    bool isTyping = false;
    uint32_t sequence = 0;
};

// Server → All: typing state broadcast
struct ChatTypingStateEventPacket
{
    PacketHeader header;
    uint32_t playerId = 0;
    bool isTyping = false;
    uint64_t serverTick = 0;
};

// ── VIP name style sync ──────────────────────────────────────────────
// Server broadcasts the full user-chosen style once per join/change so every
// client renders exact colors. Animation state is NEVER sent: each client
// renders the shift locally from its own tick.
constexpr int MAX_VIP_STYLE_COLORS = 32;

struct VipStyleEventPacket
{
    PacketHeader header;
    uint32_t playerId = 0;
    uint32_t styleEpoch = 0;
    uint8_t styleKind = 0;
    uint8_t animation = 0;
    uint8_t direction = 0;
    uint8_t colorCount = 0;
    float rainbowSpeed = 1.0f;
    struct VipStyleColor
    {
        uint8_t r = 158;
        uint8_t g = 158;
        uint8_t b = 158;
    };
    VipStyleColor colors[MAX_VIP_STYLE_COLORS];
};

static_assert(sizeof(VipStyleEventPacket) <= 200, "VipStyleEventPacket is too large");

struct NpcDamageRequestPacket
{
    PacketHeader header;
    uint32_t npcEntityId = 0;
    int32_t damage = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_ENTITY;
};

struct NpcDamageEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t npcEntityId = 0;
    uint32_t shooterPlayerId = 0;
    int32_t damage = 0;
    int32_t npcHealth = 0;
    uint8_t killed = 0;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_ENTITY;
};

struct ServerCommandPacket
{
    PacketHeader header;
    char commandText[240];
};

struct DisconnectPacket
{
    PacketHeader header;
};

struct JoinRequestPacket
{
    PacketHeader header;
    char joinToken[MAX_JOIN_TOKEN_BYTES];
    char vipJoinTicket[MAX_VIP_JOIN_TICKET_BYTES];
    char name[MAX_NAME_BYTES];
};

static_assert(sizeof(JoinRequestPacket) == 180, "JoinRequestPacket wire size changed");

struct JoinAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_NAME_BYTES];
};

static_assert(sizeof(JoinAcceptPacket) == 160, "JoinAcceptPacket wire size changed");

struct JoinRejectPacket
{
    PacketHeader header;
    uint8_t reason = 0; // 1=full, 2=bad-token, 3=banned, 4=version-mismatch
    uint8_t reserved[3] = {};
};

struct ReconnectRequestPacket
{
    PacketHeader header;
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
};

static_assert(sizeof(ReconnectRequestPacket) == 84, "ReconnectRequestPacket wire size changed");

struct ReconnectAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    uint32_t spawnGeneration = 0;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    // Restored state
    int32_t restoredHealth = 100;
    int32_t restoredKills = 0;
    int32_t restoredDeaths = 0;
    float restorePx = 0.0f;
    float restorePy = 0.0f;
    float restorePz = 0.0f;
};

static_assert(sizeof(ReconnectAcceptPacket) == 156, "ReconnectAcceptPacket wire size changed");

struct ClientMapReadyPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    char mapId[MAX_NAME_BYTES];
};

static_assert(sizeof(ClientMapReadyPacket) == 56, "ClientMapReadyPacket wire size changed");

struct DisagreementPacket
{
    PacketHeader header;
    uint8_t reason = DISAGREEMENT_NONE;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
    uint32_t eventId = 0;
    uint32_t relatedSerial = 0;
    uint32_t sourcePlayerId = 0;
    uint32_t targetPlayerId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float correctionX = 0.0f;
    float correctionY = 0.0f;
    float correctionZ = 0.0f;
    char description[64];
};

// ── Pellet blast packets ─────────────────────────────────────────────
struct PelletBlastRequestPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t lastServerTick = 0;
    uint32_t spreadSeed = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved0 = 0;
    uint16_t reserved1 = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float baseDirX = 0.0f;
    float baseDirY = 0.0f;
    float baseDirZ = 0.0f;
};

constexpr int MAX_PELLET_BLAST_TARGETS = 8;

struct PelletBlastTargetResult
{
    uint32_t targetPlayerId = 0;
    int16_t totalDamage = 0;
    int16_t healthAfter = 0;
    int16_t knockX = 0, knockY = 0, knockZ = 0;
    uint8_t pelletsHit = 0;
    uint8_t killed = 0;
    uint16_t reserved = 0;
};

struct NetworkPelletResult
{
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    uint32_t targetPlayerId = 0;
    uint8_t impactType = PELLET_IMPACT_NONE;
    uint8_t bodyPart = 0;
    uint8_t pelletIndex = 0;
    uint8_t reserved = 0;
};

struct PelletBlastEventPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t spreadSeed = 0;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float baseDirX = 0.0f;
    float baseDirY = 0.0f;
    float baseDirZ = 0.0f;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t pelletCount = 0;
    uint8_t targetCount = 0;
    uint8_t reserved = 0;
    NetworkPelletResult pellets[MAX_NETWORK_PELLETS];
    PelletBlastTargetResult targets[MAX_PELLET_BLAST_TARGETS];
};

// ── Godball state (position for remote visual replication) ────────────
struct GodballStatePacket
{
    PacketHeader header;
    uint32_t ownerPlayerId = 0;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    uint8_t active = 0;
};

// ── Projectile fire result ────────────────────────────────────────────
enum ProjectileFireResultReason : uint8_t
{
    PROJECTILE_FIRE_ACCEPTED = 0,
    PROJECTILE_FIRE_ALREADY_ACCEPTED = 1,
    PROJECTILE_FIRE_DEAD = 2,
    PROJECTILE_FIRE_COOLDOWN = 3,
    PROJECTILE_FIRE_WEAPON_MISMATCH = 4,
    PROJECTILE_FIRE_ORIGIN_INVALID = 5,
    PROJECTILE_FIRE_DIRECTION_INVALID = 6,
    PROJECTILE_FIRE_CONFIG_MISSING = 7
};

struct ProjectileFireResultPacket
{
    PacketHeader header;
    uint32_t fireSerial = 0;
    uint32_t projectileId = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t accepted = 0;
    uint8_t reason = 0;
    uint8_t reserved = 0;
    float cooldownRemaining = 0.0f;
};

#pragma pack(pop)

// ── Generic attack: one request type for all weapons ─────────────────
// All weapons (hitscan, projectile, melee) share this packet.
// weaponDefNetworkId is a stable uint16 assigned during protocol init.
// basedOnInputSequence ties the attack to a specific equip/input frame.
struct AttackRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t clientSimulationTick = 0;
    uint16_t basedOnInputSequence = 0;
    int16_t equippedSlot = 0;
    uint16_t weaponDefNetworkId = 0;
    float aimOriginX = 0, aimOriginY = 0, aimOriginZ = 0;
    float aimDirX = 0, aimDirY = 0, aimDirZ = 0;
    float muzzlePosX = 0, muzzlePosY = 0, muzzlePosZ = 0;
    uint32_t deterministicSeed = 0;
    uint8_t attackVariant = 0;
    uint8_t reservedAttack[3] = {};
};

struct AttackResultPacket
{
    PacketHeader header;
    uint64_t nextAllowedFireTick = 0;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t projectileId = 0;
    int32_t magazineAmmo = 0;
    int32_t reserveAmmo = 0;
    uint32_t stateRevision = 0;
    uint32_t serverTick = 0;
    uint16_t weaponDefNetworkId = 0;
    uint8_t accepted = 0;
    uint8_t reason = 0;
};

// ── Reload request/result ────────────────────────────────────────────
struct ReloadRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint16_t weaponDefNetworkId = 0;
};

struct ReloadResultPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint16_t weaponDefNetworkId = 0;
    uint8_t accepted = 0;
    uint8_t reason = 0;
    int32_t magazineAmmo = 0;
    int32_t reserveAmmo = 0;
    uint64_t reloadCompleteTick = 0;
    uint64_t nextAllowedFireTick = 0;
    uint8_t reloading = 0;
    uint32_t stateRevision = 0;
};

// ── Respawn request/result ───────────────────────────────────────────
struct RespawnRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0; // current generation at time of request
};

struct PlayerRespawnedPacket
{
    PacketHeader header;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    int32_t health = 100;
    // Weapon inventory follows (compact format, up to 16 weapons)
    struct WeaponSlot {
        uint16_t weaponDefNetworkId = 0;
        int32_t magazineAmmo = 0;
        int32_t reserveAmmo = 0;
        uint64_t nextAllowedFireTick = 0;
        uint32_t stateRevision = 0;
        uint8_t reloading = 0;
    };
    WeaponSlot weapons[16];
    uint8_t weaponCount = 0;
};

// ── Spawn acknowledgement ────────────────────────────────────────────
struct SpawnAckPacket
{
    PacketHeader header;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
};

// ── Spawn activated (server confirms ACK was received) ────────────────
struct SpawnActivatedPacket
{
    PacketHeader header;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    uint32_t serverTick = 0;
};

static_assert(sizeof(SnapshotPacket) < 16000, "SnapshotPacket exceeds client receive buffer");
static_assert(sizeof(ShotRequestPacket) <= 132, "ShotRequestPacket is too large");
static_assert(sizeof(ShotEventPacket) <= 132, "ShotEventPacket is too large");
static_assert(sizeof(ProjectileFireRequestPacket) <= 80, "ProjectileFireRequestPacket is too large");
static_assert(sizeof(ProjectileSpawnEventPacket) <= 128, "ProjectileSpawnEventPacket is too large");
static_assert(sizeof(ProjectileStateEventPacket) <= 104, "ProjectileStateEventPacket is too large");
static_assert(sizeof(ProjectileExplodeEventPacket) < MAX_GAME_DATAGRAM_BYTES,
              "ProjectileExplodeEventPacket exceeds safe datagram limit");
static_assert(sizeof(MeleeHitRequestPacket) <= 96, "MeleeHitRequestPacket is too large");
static_assert(sizeof(MeleeHitEventPacket) <= 96, "MeleeHitEventPacket is too large");
static_assert(sizeof(DamageConfirmedEventPacket) <= 104, "DamageConfirmedEventPacket is too large");
static_assert(sizeof(DisagreementPacket) <= 128, "DisagreementPacket is too large");
static_assert(sizeof(PelletBlastRequestPacket) <= 80, "PelletBlastRequestPacket is too large");
static_assert(sizeof(PelletBlastEventPacket) < MAX_GAME_DATAGRAM_BYTES,
              "PelletBlastEventPacket exceeds safe datagram limit");
static_assert(sizeof(PelletBlastTargetResult) <= 24, "PelletBlastTargetResult is too large");
static_assert(sizeof(AttackRequestPacket) <= 96, "AttackRequestPacket is too large");
static_assert(sizeof(AttackResultPacket) <= 64, "AttackResultPacket is too large");
static_assert(sizeof(ReloadRequestPacket) <= 32, "ReloadRequestPacket is too large");
static_assert(sizeof(ReloadResultPacket) <= 72, "ReloadResultPacket is too large");
static_assert(sizeof(RespawnRequestPacket) <= 32, "RespawnRequestPacket is too large");
static_assert(sizeof(PlayerRespawnedPacket) <= 576, "PlayerRespawnedPacket is too large");

bool validHeader(const PacketHeader& header, uint8_t expectedType);

} // namespace MimitaNet
