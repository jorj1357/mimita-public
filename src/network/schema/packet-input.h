// 2026-11-14
// Input packet wire structs.
#pragma once

#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

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
    // Distinct interaction press serial. Reuses the former reserved byte, so
    // the packet size and layout stay unchanged.
    uint8_t interactSerial = 0;
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

#pragma pack(pop)

}  // namespace MimitaNet
