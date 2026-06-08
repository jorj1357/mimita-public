#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8
constexpr uint16_t PROTOCOL_VERSION = 2;
constexpr int MAX_PLAYERS = 32;
constexpr int MAX_NAME_BYTES = 32;
constexpr int MAX_SNAPSHOT_PLAYERS = 32;

enum PacketType : uint8_t
{
    PACKET_HELLO = 1,
    PACKET_WELCOME = 2,
    PACKET_INPUT = 3,
    PACKET_SNAPSHOT = 4,
    PACKET_DISCONNECT = 5,
    PACKET_PING = 6,
    PACKET_PLAYER_LIST = 7
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
};

struct SnapshotPlayer
{
    uint32_t playerId = 0;
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float yaw = 0.0f;
    int32_t health = 100;
    uint8_t onGround = 0;
    uint8_t active = 0;
    char name[MAX_NAME_BYTES];
};

struct SnapshotPacket
{
    PacketHeader header;
    uint32_t playerCount = 0;
    SnapshotPlayer players[MAX_SNAPSHOT_PLAYERS];
};

struct DisconnectPacket
{
    PacketHeader header;
};

#pragma pack(pop)

bool validHeader(const PacketHeader& header, uint8_t expectedType);

} // namespace MimitaNet
