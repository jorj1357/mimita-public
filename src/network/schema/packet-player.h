// 2026-11-14
// MiMITA player packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

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

struct RespawnRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0; // current generation at time of request
};

struct PlayerRespawnedPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
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
    uint8_t communityWeaponSetId = 0;
    uint16_t equippedWeaponDefNetworkId = 0;
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

// ── Bomb Tag state replication (server → clients) ────────────────────
// Carries authoritative bomb holder, timer ticks, inactive ticks, and
// the match phase so clients can render HUD and bomb visual from one
// authoritative source without running their own simulation.

#pragma pack(pop)

}  // namespace MimitaNet
