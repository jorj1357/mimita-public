#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8
constexpr uint16_t PROTOCOL_VERSION = 7;
constexpr int MAX_RECONNECT_TOKEN_BYTES = 64;
constexpr int MAX_JOIN_TOKEN_BYTES = 64;
constexpr int MAX_PLAYERS = 32;
constexpr int MAX_SNAPSHOT_ENTITIES = 96;
constexpr int MAX_NAME_BYTES = 32;

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
    PACKET_DISAGREEMENT = 25
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
    NETWORK_WEAPON_HAFS = 6
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
    DISAGREEMENT_INVALID_STATE = 5
};

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
};

struct HelloPacket
{
    PacketHeader header;
    char name[MAX_NAME_BYTES];
};

struct WelcomePacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_NAME_BYTES];
};

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
    uint8_t jumpHeld = 0;
    uint8_t dashPressed = 0;
    uint8_t attackPressed = 0;
    uint8_t freezeHeld = 0;
    uint8_t spawnNpcPressed = 0;
    float sizeScale = 1.0f;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
};

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
    uint16_t lastDashSerial = 0;
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
    char displayName[MAX_NAME_BYTES];
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

struct ChatPacket
{
    PacketHeader header;
    char senderName[MAX_NAME_BYTES];
    char text[240];
};

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
    char name[MAX_NAME_BYTES];
};

struct JoinAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_NAME_BYTES];
};

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

struct ReconnectAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    float tickRate = 60.0f;
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

struct DisagreementPacket
{
    PacketHeader header;
    uint8_t reason = DISAGREEMENT_NONE;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float correctionX = 0.0f;
    float correctionY = 0.0f;
    float correctionZ = 0.0f;
    char description[64];
};

#pragma pack(pop)

static_assert(sizeof(SnapshotPacket) < 16000, "SnapshotPacket exceeds client receive buffer");
static_assert(sizeof(ShotRequestPacket) <= 132, "ShotRequestPacket is too large");
static_assert(sizeof(ShotEventPacket) <= 132, "ShotEventPacket is too large");

bool validHeader(const PacketHeader& header, uint8_t expectedType);

} // namespace MimitaNet
