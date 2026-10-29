// 2026-10-29
/* purpose
* Implements the shared server contact application path.
* Keeps victim lookup, generation/lifecycle/team checks, and the damage sink in
* one place for hitscan, projectile, explosion, and deathball callers.
* Does NOT trace geometry, render, send packets, or own weapon-specific queries.
*/

#include "network/server-contact.h"

#include "network/server-gamemode.h"
#include "debug/structured-log.h"

namespace MimitaNet {

ServerContactResult resolveServerContact(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    const ServerContactRequest& request)
{
    ServerContactResult out;
    if (request.attacker.id == 0)
    {
        out.rejectionReason = "attacker-missing";
        return out;
    }
    if (request.victimId == 0 || request.damage <= 0)
    {
        out.rejectionReason = "victim-or-damage-invalid";
        return out;
    }

    ServerActorRef victim{};
    uint32_t serverGeneration = 0;
    bool alive = false;
    if (request.victimKind == ServerActorKind::Npc)
    {
        auto it = npcs.find(request.victimId);
        if (it == npcs.end())
        {
            out.rejectionReason = "victim-not-found";
            return out;
        }
        victim = {it->first, ServerActorKind::Npc, nullptr, &it->second};
        serverGeneration = it->second.transformEpoch;
        alive = it->second.health > 0;
    }
    else
    {
        auto it = players.find(request.victimId);
        if (it == players.end())
        {
            out.rejectionReason = "victim-not-found";
            return out;
        }
        victim = {it->first, ServerActorKind::Player, &it->second, nullptr};
        serverGeneration = it->second.spawnGeneration;
        alive = !it->second.dead && it->second.spawnState == ServerPlayer::Active;
    }

    if (!alive)
    {
        out.rejectionReason = "victim-inactive";
        return out;
    }
    if (request.victimGeneration != 0 &&
        request.victimGeneration != serverGeneration)
    {
        out.rejectionReason = "victim-generation-changed";
        return out;
    }
    if (!request.allowSelfDamage &&
        request.attacker.id == request.victimId &&
        request.attacker.kind == request.victimKind)
    {
        out.rejectionReason = "self-damage";
        return out;
    }
    if (serverFriendlyFireBlocks(
            request.attacker.id,
            request.attacker.kind == ServerActorKind::Npc,
            request.victimId,
            request.victimKind == ServerActorKind::Npc,
            players, npcs, "server-contact", request.serverTick))
    {
        out.rejectionReason = "friendly-fire";
        return out;
    }

    ServerActorDamageRequest damageRequest;
    damageRequest.attacker = request.attacker;
    damageRequest.victim = victim;
    damageRequest.damage = request.damage;
    damageRequest.knockback = request.knockback;
    damageRequest.source = request.source;
    damageRequest.weaponId = request.weaponId;
    damageRequest.weaponDisplayName = request.weaponDisplayName;
    damageRequest.serverTick = request.serverTick;
    damageRequest.eventId = request.eventId;
    damageRequest.correlationId = request.contactId;

    out.damage = applyActorDamage(players, npcs, damageRequest);
    out.accepted = out.damage.applied;
    if (!out.accepted)
    {
        out.rejectionReason = out.damage.rejectionReason.empty()
            ? "damage-rejected" : out.damage.rejectionReason.c_str();
    }
    return out;
}

} // namespace MimitaNet
