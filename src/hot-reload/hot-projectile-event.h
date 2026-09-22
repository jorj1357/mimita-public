// 09 22 2026
/* purpose
* Hot-side projectile lifecycle event broadcaster. The hot module builds the
* actual packet (contents stay hot-editable) and uses the generic
* `event.next-id` / `event.broadcast` primitives to assign the reliable ticket
* and queue/send it. No cold per-event packet code.
* Does NOT own projectile simulation.
*/
#pragma once

#include <cstring>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-event-broadcast.h"
#include "network/packets.h"
#include "ecs/entity-types.h"

inline std::uint32_t hotOwnerPlayerId(std::uint64_t ownerEntity)
{
    if (ownerEntity == 0)
        return 0;
    const EntityId entity = static_cast<EntityId>(ownerEntity);
    return entityDomain(entity) == EntityDomain::Player ? entityLegacyId(entity) : 0;
}

inline void hotBroadcastProjectileExplode(
    GameplayContextV1* ctx, std::uint32_t projectileId, std::uint64_t ownerEntity,
    std::uint32_t fireSerial, std::uint32_t weaponNetworkId,
    std::uint32_t weaponDefNetworkId, const float position[3], float radius)
{
    GameReliableEventTicketV1 ticket{};
    hotEventNextId(ctx, ticket);

    MimitaNet::ProjectileExplodeEventPacket packet{};
    packet.header.type = MimitaNet::PACKET_PROJECTILE_EXPLODE_EVENT;
    packet.header.tick = ctx ? (std::uint32_t)ctx->tick : 0;
    packet.eventId = ticket.eventId;
    packet.eventSessionId = ticket.eventSessionId;
    packet.projectileId = projectileId;
    packet.ownerPlayerId = hotOwnerPlayerId(ownerEntity);
    packet.fireSerial = fireSerial;
    packet.weapon = (std::uint8_t)weaponNetworkId;
    packet.weaponDefNetworkId = (std::uint16_t)weaponDefNetworkId;
    packet.posX = position[0];
    packet.posY = position[1];
    packet.posZ = position[2];
    packet.radius = radius;

    GameEventBroadcastV1 request{};
    request.flags = GAME_EVENT_BROADCAST_RELIABLE;
    request.eventId = ticket.eventId;
    request.eventSessionId = ticket.eventSessionId;
    request.payloadSize = (std::uint32_t)sizeof(packet);
    std::memcpy(request.payload, &packet, sizeof(packet));
    hotEventBroadcast(ctx, request);
}

inline void hotBroadcastProjectileSpawn(
    GameplayContextV1* ctx, std::uint32_t projectileId, std::uint64_t ownerEntity,
    std::uint32_t fireSerial, std::uint32_t weaponNetworkId,
    std::uint32_t weaponDefNetworkId, const float position[3],
    const float velocity[3], float radius, float lifetime)
{
    MimitaNet::ProjectileSpawnEventPacket packet{};
    packet.header.type = MimitaNet::PACKET_PROJECTILE_SPAWN_EVENT;
    packet.header.tick = ctx ? (std::uint32_t)ctx->tick : 0;
    packet.projectileId = projectileId;
    packet.ownerPlayerId = hotOwnerPlayerId(ownerEntity);
    packet.fireSerial = fireSerial;
    packet.weapon = (std::uint8_t)weaponNetworkId;
    packet.weaponDefNetworkId = (std::uint16_t)weaponDefNetworkId;
    packet.posX = position[0];
    packet.posY = position[1];
    packet.posZ = position[2];
    packet.velX = velocity[0];
    packet.velY = velocity[1];
    packet.velZ = velocity[2];
    packet.rotW = 1.0f;
    packet.spawnTick = ctx ? (std::uint32_t)ctx->tick : 0;
    packet.lifetime = lifetime;
    packet.radius = radius;

    GameEventBroadcastV1 request{};
    request.flags = GAME_EVENT_BROADCAST_EXCLUDE_OWNER;
    request.ownerPlayerId = hotOwnerPlayerId(ownerEntity);
    request.payloadSize = (std::uint32_t)sizeof(packet);
    std::memcpy(request.payload, &packet, sizeof(packet));
    hotEventBroadcast(ctx, request);
}
