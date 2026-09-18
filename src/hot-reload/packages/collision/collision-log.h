// 09 17 2026
/* purpose
* Hot-side bridge from the collision package to the kernel's single JSONL debug
* stream (`log.event` capability). The collision package and movement.main use
* this to make solve behavior observable live while the EXE runs.
* Plain data only: the kernel owns events.jsonl; the package just describes the
* event. No new EXE field and no direct dependency on EXE symbols.
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

inline void collisionLog(GameplayContextV1* ctx, std::uint32_t level,
                         const char* category, const char* name,
                         const char* message, std::uint64_t tick)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<HotLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;

    GameLogEventV1 ev{};
    ev.level = level;
    ev.simulationTick = (std::uint32_t)tick;
    std::snprintf(ev.category, sizeof(ev.category), "%s", category ? category : "COLLISION");
    std::snprintf(ev.name, sizeof(ev.name), "%s", name ? name : "collision.event");
    std::snprintf(ev.message, sizeof(ev.message), "%s", message ? message : "");
    fn(ctx->host, &ev);
}

// Emit an event with one `result` field, built from a message string that is
// already formatted by the caller.
inline void collisionLogResult(GameplayContextV1* ctx, std::uint32_t level,
                               const char* category, const char* name,
                               const char* message, const char* result,
                               std::uint64_t tick)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<HotLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;

    GameLogEventV1 ev{};
    ev.level = level;
    ev.simulationTick = (std::uint32_t)tick;
    std::snprintf(ev.category, sizeof(ev.category), "%s", category ? category : "COLLISION");
    std::snprintf(ev.name, sizeof(ev.name), "%s", name ? name : "collision.event");
    std::snprintf(ev.message, sizeof(ev.message), "%s", message ? message : "");
    std::snprintf(ev.result, sizeof(ev.result), "%s", result ? result : "");
    fn(ctx->host, &ev);
}

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
