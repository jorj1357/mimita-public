// 09 17 2026
/* purpose
* Hot-side bridge from the collision package to the kernel's single JSONL debug
* stream (`log.event` capability). The collision package and movement.main use
* this to make solve behavior observable live while the EXE runs, including the
* actor identity, frame, client/server ticks, and which triangles touched.
* Plain data only: the kernel owns events.jsonl; the package just describes the
* event. No new EXE field and no direct dependency on EXE symbols.
* The `host` argument is the GameplayContextV1* (see collision-abi.h).
* Does NOT own the logger, file lifetime, or aggregation.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hot-reload/game-api.h"

namespace HotCollisionPackage {

using HotLogEventFn = void (MIMITA_GAME_CALL *)(void* host,
                                                const GameLogEventV1* event);

// Actor kinds for GameLogEventV1::actorKind.
enum CollisionLogActorKind : std::uint32_t {
    COLLISION_LOG_ACTOR_NONE = 0,
    COLLISION_LOG_ACTOR_PLAYER = 1,
    COLLISION_LOG_ACTOR_NPC = 2,
    COLLISION_LOG_ACTOR_REMOTE = 3,
    COLLISION_LOG_ACTOR_OTHER = 4,
};

// Fully-specified hot log record. Callers fill what they know; the kernel adds
// wall_time/t/seq/pid/process/source. Returns false when the capability is
// unavailable (so callers can tell "not reached" from "reached but no contact").
inline bool collisionLogFull(void* host, std::uint32_t level,
                             const char* category, const char* name,
                             const char* message, const char* result,
                             std::uint64_t entityId, std::uint64_t actorId,
                             std::uint32_t actorKind, std::uint64_t frame,
                             std::uint64_t serverTick, std::uint64_t clientTick,
                             std::uint64_t simulationTick)
{
    if (!host)
        return false;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return false;
    auto fn = reinterpret_cast<HotLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return false;

    GameLogEventV1 ev{};
    ev.level = level;
    ev.simulationTick = (std::uint32_t)simulationTick;
    ev.entityId = entityId;
    ev.frame = frame;
    ev.serverTick = serverTick;
    ev.clientTick = clientTick;
    ev.actorId = actorId;
    ev.actorKind = actorKind;
    std::snprintf(ev.category, sizeof(ev.category), "%s", category ? category : "COLLISION");
    std::snprintf(ev.name, sizeof(ev.name), "%s", name ? name : "collision.event");
    std::snprintf(ev.message, sizeof(ev.message), "%s", message ? message : "");
    std::snprintf(ev.result, sizeof(ev.result), "%s", result ? result : "");
    fn(ctx->host, &ev);
    return true;
}

// Convenience wrappers used across the package and movement.

inline void collisionLog(void* host, std::uint32_t level, const char* category,
                         const char* name, const char* message,
                         std::uint64_t tick)
{
    collisionLogFull(host, level, category, name, message, nullptr, 0, 0,
                     COLLISION_LOG_ACTOR_NONE, 0, 0, 0, tick);
}

inline void collisionLogResult(void* host, std::uint32_t level,
                               const char* category, const char* name,
                               const char* message, const char* result,
                               std::uint64_t tick)
{
    collisionLogFull(host, level, category, name, message, result, 0, 0,
                     COLLISION_LOG_ACTOR_NONE, 0, 0, 0, tick);
}

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
