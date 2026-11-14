// 2026-11-14
// MiMITA wire header and protocol identity.
#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8
// 30: ShotEvent/PelletBlastEvent become reliable (eventId+session+ACK) and
// carry real damage/health; every bullet visual is guaranteed delivery.
// 36: DuelStatePacket carries the server-authoritative procedural-world state
// (Infinite Dungeon Slayer) so clients render rooms from server truth.
// 39: Replicated NPC avatars and match actor identities use expanded fields.
// 41: WelcomePacket carries the dev-loop run/build identity so a new client
//      cannot silently attach to an older persistent server.
// 43: snapshots carry stable equipped identity and generic weapon action state.
// 44: DuelStatePacket replicates each participant's actor-preset index.
// 45: DuelStatePacket appends Payload cart, overtime, and progress HUD state.
// 46: MeleeHitEventPacket carries replicated weapon-clash effect flags.
// 47: DuelStatePacket matchMode fits full gamemode IDs (was silently truncated).
// 48: DuelStatePacket adds the shared loss-aftermath phase/effect state.
// 49: AttackRequestPacket carries validated client-first per-target hitscan
//     aggregates so shotgun/revolver damage can match local prediction.
// 50: DuelStatePacket carries the authoritative objective interaction range.
constexpr uint16_t PROTOCOL_VERSION = 52;

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

#pragma pack(pop)

}  // namespace MimitaNet
