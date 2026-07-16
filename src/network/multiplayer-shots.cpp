#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "network/packets.h"
#include "combat/weapon-fire.h"
#include "combat/death-system.h"
#include "effects/effect-part.h"

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

} // namespace MimitaNet
