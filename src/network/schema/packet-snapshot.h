// 2026-11-14
// MiMITA snapshot packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

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
    // Authoritative six-part actor body health. Layer/detachment presentation is
    // derived from these values with the shared actor-body thresholds.
    uint8_t partHealth[6] = {100, 100, 100, 100, 100, 100};
    // Melee percent meter (0 outside SSBM).
    float meleePercent = 0.0f;
    // Melee shield remaining fraction, quantized to 0..255.
    uint8_t meleeShield = 0;
    uint8_t onGround = 0;
    int16_t equippedSlot = 0;
    uint8_t weaponState = 0;
    float aimX = 1.0f;
    uint16_t equippedWeaponDefNetworkId = 0;
    uint8_t weaponActionIndex = 255;
    uint32_t weaponActionStartTick = 0;
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
    char avatarName[MAX_AVATAR_NAME_BYTES];
    uint8_t vipTier = 0;
    uint8_t vipStyleKind = 0;
    uint8_t vipColorR = 158;
    uint8_t vipColorG = 158;
    uint8_t vipColorB = 158;
    uint8_t vipFlags = 0;
    uint8_t vipStyleEpoch = 0;
    uint8_t vipReserved = 0;
    // Godball replication (active flag carried in stateFlags NET_STATE_GODBALL_ACTIVE)
    float godballX = 0.0f;
    float godballY = 0.0f;
    float godballZ = 0.0f;
    float godballVx = 0.0f;
    float godballVy = 0.0f;
    float godballVz = 0.0f;
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
    uint32_t targetSpawnGeneration = 0;
};

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
    uint8_t partHealth[6] = {100, 100, 100, 100, 100, 100};
    // Melee percent meter, quantized to 4% steps (0..1020) to fit the fixed
    // CompactEntityData size and stay under the snapshot datagram limit.
    uint8_t meleePercent = 0;
    // Melee shield remaining fraction, 0..255.
    uint8_t meleeShield = 0;
    uint8_t onGround = 0;
    int16_t equippedSlot = 0;
    uint8_t weaponState = 0;
    int32_t pingMs = 0;
    uint16_t equippedWeaponDefNetworkId = 0;
    uint8_t weaponActionIndex = 255;
    uint32_t weaponActionStartTick = 0;
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
    char avatarName[MAX_AVATAR_NAME_BYTES];
    uint8_t vipTier = 0;
    uint8_t vipStyleKind = 0;
    uint8_t vipColorR = 158;
    uint8_t vipColorG = 158;
    uint8_t vipColorB = 158;
    uint8_t vipFlags = 0;
    uint8_t vipStyleEpoch = 0;
    uint8_t vipReserved = 0;
    // Godball replication (active flag in stateFlags NET_STATE_GODBALL_ACTIVE)
    // Quantized: position 0.01m resolution ±327.67m, velocity 0.1 m/s ±3276.7 m/s
    uint16_t godballPosX = 0;  // (raw - (-327.67)) / 0.01
    uint16_t godballPosY = 0;
    uint16_t godballPosZ = 0;
    int16_t godballVelX = 0;   // raw * 0.1
    int16_t godballVelY = 0;
    int16_t godballVelZ = 0;
};

static_assert(sizeof(CompactEntityData) == 187, "CompactEntityData unexpected size");

struct SnapshotChunkPacket
{
    PacketHeader header;
    uint32_t serverTick = 0;
    uint16_t chunkIndex = 0;
    uint16_t chunkCount = 1;
    uint16_t entityCount = 0;
    uint16_t payloadBytes = 0;
    CompactEntityData entities[6]; // 6 * 187 + header(32) = 1154 < 1200
};

static_assert(sizeof(SnapshotChunkPacket) < MAX_GAME_DATAGRAM_BYTES,
              "SnapshotChunkPacket exceeds safe datagram limit");
static_assert(sizeof(SnapshotChunkPacket) == 1154, "SnapshotChunkPacket wire size changed");

#pragma pack(pop)

}  // namespace MimitaNet
