// 2026-11-14
// MiMITA physical packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

enum PhysicalEntitySource : uint8_t
{
    PHYSICAL_ENTITY_SOURCE_BOX = 0,
    PHYSICAL_ENTITY_SOURCE_GLB = 1,
    PHYSICAL_ENTITY_SOURCE_SPHERE = 2
};

constexpr int MAX_PHYSICAL_ENTITY_PATH_BYTES = 128;
constexpr int MAX_PHYSICAL_ENTITY_STATE_ENTRIES = 16;

struct PhysicalEntitySpawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t networkId = 0;
    uint8_t motion = 0;      // PhysicalEntityMotion
    uint8_t sourceKind = 0;  // PhysicalEntitySource
    uint16_t reserved = 0;
    uint32_t materialId = 0;
    float halfExtents[3] = {0.5f, 0.5f, 0.5f};
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float angularVelocity[3] = {0.0f, 0.0f, 0.0f};
    float density = 1.0f;
    char modelPath[MAX_PHYSICAL_ENTITY_PATH_BYTES] = {0};
    char texturePath[MAX_PHYSICAL_ENTITY_PATH_BYTES] = {0};
};

struct PhysicalEntityDespawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t networkId = 0;
};

struct PhysicalEntityStateEntry
{
    uint32_t networkId = 0;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float angularVelocity[3] = {0.0f, 0.0f, 0.0f};
};

struct PhysicalEntityStatePacket
{
    PacketHeader header;
    uint16_t entityCount = 0;
    uint16_t reserved = 0;
    PhysicalEntityStateEntry entities[MAX_PHYSICAL_ENTITY_STATE_ENTRIES];
};

struct PhysicalEntityCutEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t networkId = 0;
    uint32_t cutId = 0;
    uint32_t sourceEntityId = 0;
    uint64_t predictionKey = 0;
    uint8_t cutterType = 0;
    uint32_t materialId = 0;
    float localCenter[3] = {0.0f, 0.0f, 0.0f};
    float localDirection[3] = {0.0f, 0.0f, 1.0f};
    float radius = 0.0f;
    float length = 0.0f;
    float damage = 0.0f;
    float energy = 0.0f;
};

// ── Vehicle networking ────────────────────────────────────────────────

#pragma pack(pop)

}  // namespace MimitaNet
