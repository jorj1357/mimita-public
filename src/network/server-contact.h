// 2026-10-29
/* purpose
* One shared server-side contact application path for hitscan, projectile,
* explosion, and deathball contacts.
* Does NOT trace geometry, render, send packets, or own weapon-specific queries.
* Callers produce a contact request from their own geometry/trace owner.
*/

#pragma once

#include "network/server.h"

namespace MimitaNet {

struct ServerContactRequest
{
    ServerActorRef attacker;
    uint32_t victimId = 0;
    ServerActorKind victimKind = ServerActorKind::Player;
    uint32_t victimGeneration = 0; // 0 = skip generation check
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    int damage = 0;
    glm::vec3 knockback{0.0f};
    ServerDamageSource source = ServerDamageSource::Hitscan;
    std::string weaponId;
    std::string weaponDisplayName;
    uint32_t serverTick = 0;
    uint32_t eventId = 0;
    uint32_t contactId = 0;
    bool allowSelfDamage = false;
};

struct ServerContactResult
{
    bool accepted = false;
    std::string rejectionReason;
    ServerDamageResult damage;
};

// Resolves the victim, generation, lifecycle, team/friendly-fire policy, and
// applies the result through the single authoritative damage sink.
ServerContactResult resolveServerContact(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    const ServerContactRequest& request);

} // namespace MimitaNet
