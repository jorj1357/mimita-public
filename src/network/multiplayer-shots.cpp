#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "combat/weapon-fire.h"
#include "combat/death-system.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "audio/audio.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>

namespace MimitaNet {

void mpProcessShotEventPacket(MultiplayerContext& ctx, const ShotEventPacket* event)
{
    uint32_t& lastSerial = ctx.lastReceivedShotSerial[event->shooterPlayerId];
    if (lastSerial != 0 &&
        (int32_t)(event->shotSerial - lastSerial) <= 0)
    {
        printf("[NET SHOT RECV] shooter=%u serial=%u skipped=duplicate last=%u\n",
               event->shooterPlayerId, event->shotSerial, lastSerial);
        return;
    }
    lastSerial = event->shotSerial;

    NetworkShotEvent out;
    out.shotSerial = event->shotSerial;
    out.clientTimeMs = event->clientTimeMs;
    out.shooterPlayerId = event->shooterPlayerId;
    out.targetPlayerId = event->targetPlayerId;
    out.damage = event->damage;
    out.targetHealth = event->targetHealth;
    out.power = event->power;
    out.effectFlags = event->effectFlags;
    out.targetTransformEpoch = event->targetTransformEpoch;
    out.weapon = event->weapon;
    out.impactType = event->impactType;
    out.killed = event->killed != 0;
    out.damageConfirmed = event->damageConfirmed != 0;
    out.origin = {event->originX, event->originY, event->originZ};
    out.hit = {event->hitX, event->hitY, event->hitZ};
    out.direction = {event->dirX, event->dirY, event->dirZ};
    out.normal = {event->normalX, event->normalY, event->normalZ};
    out.knockback = {event->knockX, event->knockY, event->knockZ};
    ctx.shotEvents.push_back(out);
    printf("[NET SHOT RECV] shooter=%u serial=%u weapon=%u impact=%u "
           "flags=0x%03x damageConfirmed=%d origin=(%.2f %.2f %.2f) "
           "hit=(%.2f %.2f %.2f)\n",
           out.shooterPlayerId, out.shotSerial, out.weapon,
           out.impactType, out.effectFlags, (int)out.damageConfirmed,
           out.origin.x, out.origin.y, out.origin.z,
           out.hit.x, out.hit.y, out.hit.z);
}

void mpProcessNpcDamageEventPacket(MultiplayerContext& ctx, const NpcDamageEventPacket* event)
{
    auto npcIt = ctx.remoteNpcs.find(event->npcEntityId);
    if (npcIt != ctx.remoteNpcs.end())
    {
        Player& npc = npcIt->second;
        npc.currentHp = event->npcHealth;
        printf("[NET NPC DAMAGE RECV] npcId=%u damage=%d health=%d killed=%d\n",
               event->npcEntityId, event->damage, event->npcHealth,
               (int)event->killed);

        if (event->killed)
        {
            ctx.remoteNpcs.erase(npcIt);
            ctx.remoteNpcInterpolation.erase(event->npcEntityId);
            printf("[NET NPC KILL RECV] npcId=%u removed\n", event->npcEntityId);
        }
    }
    else
    {
        printf("[NET NPC DAMAGE RECV] npcId=%u not-found\n", event->npcEntityId);
    }
}

uint32_t mpSendShotEvent(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    float power,
    uint16_t effectFlags,
    uint8_t weapon,
    uint8_t impactType,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& direction,
    const glm::vec3& normal,
    const glm::vec3& knockbackImpulse)
{
    if (!ctx.active || !ctx.localPlayerId)
        return 0;

    ShotRequestPacket packet{};
    packet.header.type = PACKET_SHOT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.shotSerial = ctx.nextLocalShotSerial++;
    if (ctx.nextLocalShotSerial == 0)
        ctx.nextLocalShotSerial = 1;
    packet.clientTimeMs = nowMs();
    packet.lastServerTick = ctx.latestServerTick;
    packet.targetPlayerId = targetPlayerId;
    packet.damage = damage;
    packet.power = power;
    packet.effectFlags = effectFlags;
    packet.weapon = weapon;
    packet.impactType = impactType;
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    packet.hitX = hit.x; packet.hitY = hit.y; packet.hitZ = hit.z;
    packet.dirX = direction.x; packet.dirY = direction.y; packet.dirZ = direction.z;
    packet.normalX = normal.x; packet.normalY = normal.y; packet.normalZ = normal.z;
    packet.knockX = knockbackImpulse.x; packet.knockY = knockbackImpulse.y; packet.knockZ = knockbackImpulse.z;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET SHOT SEND] shooter=%u serial=%u weapon=%u impact=%u "
           "flags=0x%03x target=%u damage=%d origin=(%.2f %.2f %.2f) "
           "hit=(%.2f %.2f %.2f)\n",
           ctx.localPlayerId, packet.shotSerial, weapon, impactType,
           effectFlags, targetPlayerId, damage,
           origin.x, origin.y, origin.z, hit.x, hit.y, hit.z);
    return packet.shotSerial;
}

void mpSendNpcDamageRequest(MultiplayerContext& ctx, uint32_t npcEntityId, int damage,
    const glm::vec3& origin, const glm::vec3& hit, const glm::vec3& direction,
    const glm::vec3& normal, const glm::vec3& knockback, uint16_t effectFlags, uint8_t weapon)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    NpcDamageRequestPacket packet{};
    packet.header.type = PACKET_NPC_DAMAGE_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.npcEntityId = npcEntityId;
    packet.damage = std::clamp(damage, 1, 200);
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    packet.hitX = hit.x; packet.hitY = hit.y; packet.hitZ = hit.z;
    packet.dirX = direction.x; packet.dirY = direction.y; packet.dirZ = direction.z;
    packet.normalX = normal.x; packet.normalY = normal.y; packet.normalZ = normal.z;
    packet.knockX = knockback.x; packet.knockY = knockback.y; packet.knockZ = knockback.z;
    packet.effectFlags = effectFlags;
    packet.weapon = weapon;
    packet.impactType = SHOT_IMPACT_ENTITY;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET NPC DAMAGE SEND] npcId=%u damage=%d origin=(%.2f,%.2f,%.2f)\n",
           npcEntityId, damage, origin.x, origin.y, origin.z);
}

void mpSendPelletBlastRequest(MultiplayerContext& ctx, uint8_t weapon,
    const glm::vec3& origin, const glm::vec3& baseDirection, uint32_t spreadSeed)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    PelletBlastRequestPacket packet{};
    packet.header.type = PACKET_PELLET_BLAST_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.shotSerial = ctx.nextLocalShotSerial++;
    if (ctx.nextLocalShotSerial == 0)
        ctx.nextLocalShotSerial = 1;
    packet.clientTimeMs = nowMs();
    packet.lastServerTick = ctx.latestServerTick;
    packet.spreadSeed = spreadSeed;
    packet.weapon = weapon;
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    const glm::vec3 dir = glm::normalize(baseDirection);
    packet.baseDirX = dir.x; packet.baseDirY = dir.y; packet.baseDirZ = dir.z;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[PELLET BLAST CLIENT SEND] shooter=%u serial=%u weapon=%s "
           "origin=(%.2f,%.2f,%.2f) baseDirection=(%.3f,%.3f,%.3f) "
           "seed=%u lastServerTick=%u\n",
           ctx.localPlayerId, packet.shotSerial,
           networkWeaponTypeName(weapon),
           origin.x, origin.y, origin.z,
           dir.x, dir.y, dir.z,
           spreadSeed, packet.lastServerTick);
}

void mpProcessPelletBlastEventPacket(MultiplayerContext& ctx, const PelletBlastEventPacket* event)
{
    // Deduplicate by shooter + serial
    uint64_t dedupKey = ((uint64_t)event->shooterPlayerId << 32) | event->shotSerial;
    if (ctx.processedPelletBlastSerials.count(dedupKey))
    {
        printf("[PELLET BLAST CLIENT RECV] shooter=%u serial=%u duplicate=1 skipped=1\n",
               event->shooterPlayerId, event->shotSerial);
        return;
    }
    ctx.processedPelletBlastSerials.insert(dedupKey);
    if (ctx.processedPelletBlastSerials.size() > 256)
        ctx.processedPelletBlastSerials.clear();

    const bool isLocalShooter = event->shooterPlayerId == ctx.localPlayerId;
    const glm::vec3 origin(event->originX, event->originY, event->originZ);
    const glm::vec3 baseDir(event->baseDirX, event->baseDirY, event->baseDirZ);
    const char* weaponName = networkWeaponTypeName(event->weapon);

    printf("[PELLET BLAST CLIENT RECV] shooter=%u serial=%u weapon=%s "
           "pelletCount=%u origin=(%.2f,%.2f,%.2f) localShooter=%d\n",
           event->shooterPlayerId, event->shotSerial, weaponName,
           event->pelletCount, origin.x, origin.y, origin.z, (int)isLocalShooter);

    // For non-shooter: play sound and muzzle flash once
    if (!isLocalShooter)
    {
        playWorldSound("shotgunshoot", origin, 1.0f, 1.0f, 80.0f);
        EffectPartSystem::instance().spawnMuzzleFlash(origin, weaponName);
    }

    // Render each pellet tracer + impact
    for (uint8_t i = 0; i < event->pelletCount && i < MAX_NETWORK_PELLETS; ++i)
    {
        const NetworkPelletResult& pellet = event->pellets[i];
        const glm::vec3 hitPos(pellet.hitX, pellet.hitY, pellet.hitZ);
        const glm::vec3 hitNml(pellet.normalX, pellet.normalY, pellet.normalZ);

        // Only render tracers for non-shooter (shooter already predicted)
        if (!isLocalShooter)
        {
            EffectPartSystem::instance().spawnTracer(origin, hitPos, weaponName);
        }

        if (pellet.impactType == PELLET_IMPACT_WORLD)
        {
            if (!isLocalShooter)
            {
                HitEvent ev;
                ev.position = hitPos;
                ev.normal = hitNml;
                ev.direction = -hitNml;
                ev.hitWorld = true;
                ev.damage = 0;
                ev.attacker = weaponName;
                ev.weaponSource = "pellet_blast";
                HitEffects::onHit(ev);
            }
        }
        else if (pellet.impactType == PELLET_IMPACT_PLAYER && !isLocalShooter)
        {
            HitEvent ev;
            ev.position = hitPos;
            ev.normal = hitNml;
            ev.direction = -hitNml;
            ev.hitEntity = true;
            ev.damage = 0;
            ev.attacker = weaponName;
            ev.weaponSource = "pellet_blast";
            HitEffects::onHit(ev);

            auto remote = ctx.remotePlayers.find(pellet.targetPlayerId);
            if (remote != ctx.remotePlayers.end())
            {
                printf("[PELLET HIT] player=%u damage=pending\n", pellet.targetPlayerId);
            }
        }
    }

    // ── Apply authoritative target results (knockback + death) ──────
    const char* shooterName = "";
    {
        auto si = ctx.playerRegistry.find(event->shooterPlayerId);
        if (si != ctx.playerRegistry.end())
            shooterName = si->second.name.c_str();
    }
    const glm::vec3 baseDirection(event->baseDirX, event->baseDirY, event->baseDirZ);

    for (uint8_t t = 0; t < event->targetCount && t < MAX_PELLET_BLAST_TARGETS; ++t)
    {
        const PelletBlastTargetResult& targetRes = event->targets[t];
        const bool isLocalTarget = targetRes.targetPlayerId == ctx.localPlayerId;

        // ── Knockback ────────────────────────────────────────────────
        glm::vec3 knockback(
            (float)targetRes.knockX,
            (float)targetRes.knockY,
            (float)targetRes.knockZ);

        if (glm::length(knockback) > 0.001f)
        {
            if (isLocalTarget)
            {
                ctx.pendingKnockback += knockback;
                ctx.pendingKnockbackSource = weaponName;
                printf("[PELLET KNOCKBACK APPLY] victim=local "
                       "impulse=(%.2f,%.2f,%.2f) source=%s\n",
                       knockback.x, knockback.y, knockback.z, weaponName);
            }
            else
            {
                auto remote = ctx.remotePlayers.find(targetRes.targetPlayerId);
                if (remote != ctx.remotePlayers.end())
                {
                    remote->second.externalImpulse += knockback;
                    printf("[PELLET KNOCKBACK APPLY] player=%u "
                           "impulse=(%.2f,%.2f,%.2f)\n",
                           targetRes.targetPlayerId,
                           knockback.x, knockback.y, knockback.z);
                }
            }
        }

        // ── Death effect for killed targets (shooter and others) ─────
        if (targetRes.killed && !isLocalTarget)
        {
            NetworkShotEvent deathEvent;
            deathEvent.shotSerial = event->shotSerial;
            deathEvent.shooterPlayerId = event->shooterPlayerId;
            deathEvent.targetPlayerId = targetRes.targetPlayerId;
            deathEvent.damage = targetRes.totalDamage;
            deathEvent.targetHealth = targetRes.healthAfter;
            deathEvent.weapon = event->weapon;
            deathEvent.impactType = SHOT_IMPACT_ENTITY;
            deathEvent.killed = true;
            deathEvent.damageConfirmed = true;
            deathEvent.direction = glm::length(baseDirection) > 0.001f
                ? glm::normalize(baseDirection) : glm::vec3(0.0f, 0.0f, -1.0f);
            deathEvent.knockback = knockback;
            ctx.shotEvents.push_back(deathEvent);

            printf("[PELLET DEATH] shooter=%u target=%u serial=%u\n",
                   event->shooterPlayerId, targetRes.targetPlayerId, event->shotSerial);
        }

    }
}

} // namespace MimitaNet
