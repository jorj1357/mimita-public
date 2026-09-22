// 09 22 2026
/* purpose
* Hot-side generic gameplay-event transport helper: build any packet in hot
* code and use `event.next-id` / `event.broadcast` to assign the reliable ticket
* and queue/send it. Complements hot-projectile-event.h.
* Does NOT own any specific event's meaning.
*/
#pragma once

#include <cstring>

#include "hot-reload/game-api.h"

inline void hotEventNextId(GameplayContextV1* ctx, GameReliableEventTicketV1& out)
{
    out = GameReliableEventTicketV1{};
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<GameEventNextIdFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EVENT_NEXT_ID));
    if (fn)
        fn(ctx->host, &out);
}

inline void hotEventBroadcast(GameplayContextV1* ctx, GameEventBroadcastV1& request)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<GameEventBroadcastFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EVENT_BROADCAST));
    if (fn)
        fn(ctx->host, &request);
}

inline void hotBroadcastPacket(GameplayContextV1* ctx, const void* packet,
                               std::uint32_t size, std::uint32_t flags,
                               std::uint32_t ownerPlayerId = 0)
{
    if (!packet || size == 0 || size > (std::uint32_t)GAME_EVENT_BROADCAST_MAX_BYTES)
        return;
    GameEventBroadcastV1 request{};
    request.flags = flags;
    request.ownerPlayerId = ownerPlayerId;
    request.payloadSize = size;
    std::memcpy(request.payload, packet, size);
    hotEventBroadcast(ctx, request);
}
