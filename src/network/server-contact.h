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

enum class ServerContactKind : uint8_t
{
    Hitscan,
    Projectile,
    Melee,
    Explosion,
    Deathball,
    WeaponClash
};

struct ServerContactRequest
{
    ServerActorRef attacker;
    uint32_t victimId = 0;
    ServerActorKind victimKind = ServerActorKind::Player;
    uint32_t victimGeneration = 0; // 0 = skip generation check
    ServerContactKind kind = ServerContactKind::Hitscan;
    glm::vec3 origin{0.0f}; // query origin: muzzle, blast, or projectile center
    glm::vec3 point{0.0f};  // authoritative contact point on/near the victim
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    int damage = 0;
    glm::vec3 knockback{0.0f};
    ServerDamageSource source = ServerDamageSource::Hitscan;
    std::string weaponId;
    std::string weaponDisplayName;
    uint32_t serverTick = 0;
    uint32_t contactTick = 0;      // 0 = same as serverTick
    uint32_t maxContactAgeTicks = 0; // 0 = skip age check
    uint32_t eventId = 0;
    uint32_t contactId = 0;
    float maxDistance = 0.0f;      // 0 = skip reach check
    float hitTolerance = 0.0f;     // volume expansion for client/contact claims
    bool validateHitVolume = false;
    bool requireWorldLineOfSight = false;
    const HeadlessWorld* world = nullptr;
    bool dedupe = false;
    bool allowSelfDamage = false;
};

struct ServerContactResult
{
    bool accepted = false;
    std::string rejectionReason;
    ServerDamageResult damage;
};

// Nearest authoritative body point on the victim, used by explosion/splash
// callers so contact geometry and line-of-sight share one body-volume owner.
bool nearestServerActorBodyPoint(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t victimId,
    ServerActorKind victimKind,
    const glm::vec3& from,
    uint32_t contactTick,
    glm::vec3& outPoint);

// Resolves the victim, generation, lifecycle, team/friendly-fire policy,
// shared geometry, duplicate suppression, and applies through the damage sink.
ServerContactResult resolveServerContact(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    const ServerContactRequest& request);

} // namespace MimitaNet
