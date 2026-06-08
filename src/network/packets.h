#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8
constexpr uint16_t PROTOCOL_VERSION = 3;
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
    PACKET_EXPLODE_REQUEST = 13
};

enum EntityType : uint8_t
{
    ENTITY_NONE = 0,
    ENTITY_PLAYER = 1,
    ENTITY_NPC = 2
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
    uint8_t jumpHeld = 0;
    uint8_t dashPressed = 0;
    uint8_t attackPressed = 0;
    uint8_t freezeHeld = 0;
    uint8_t spawnNpcPressed = 0;
    uint8_t reserved0 = 0;
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
    uint16_t reserved = 0;
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
    uint8_t reserved1 = 0;
    uint16_t reserved2 = 0;
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

struct DisconnectPacket
{
    PacketHeader header;
};

#pragma pack(pop)

static_assert(sizeof(SnapshotPacket) < 16000, "SnapshotPacket exceeds client receive buffer");

bool validHeader(const PacketHeader& header, uint8_t expectedType);

} // namespace MimitaNet
