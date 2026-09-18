// 09 17 2026
/* purpose
* The one generic tool action-event vocabulary plus the hot-side bridge into the
* kernel's single JSONL stream (log.event capability). A tool behavior emits a
* ToolActionEventV1 instead of calling animation/audio/effects directly; the
* animation bridge sets HotActorActionState, and any subscriber (audio, effects,
* replay, tests) can consume the same event. Editing this file changes the
* running stream with no EXE rebuild.
* Plain data only: the kernel owns events.jsonl; the behavior just describes the
* event. No kernel enum, no animation call, no per-weapon branch.
* Does NOT own animation playback, the logger, or file lifetime.
*/
#pragma once

#include <cstdint>
#include <cstdio>

#include "hot-reload/game-api.h"

// ── Action ids (runtime hashes; never a kernel enum) ────────────────
static constexpr std::uint64_t TOOL_ACTION_EQUIPPED =
    gameHash("tool.action.equipped");
static constexpr std::uint64_t TOOL_ACTION_UNEQUIPPED =
    gameHash("tool.action.unequipped");
static constexpr std::uint64_t TOOL_ACTION_PRIMARY_STARTED =
    gameHash("tool.action.primary-started");
static constexpr std::uint64_t TOOL_ACTION_PRIMARY_ACCEPTED =
    gameHash("tool.action.primary-accepted");
static constexpr std::uint64_t TOOL_ACTION_FIRED =
    gameHash("tool.action.fired");
static constexpr std::uint64_t TOOL_ACTION_HIT =
    gameHash("tool.action.hit");
static constexpr std::uint64_t TOOL_ACTION_RELOAD_STARTED =
    gameHash("tool.action.reload-started");
static constexpr std::uint64_t TOOL_ACTION_RELOAD_FINISHED =
    gameHash("tool.action.reload-finished");
static constexpr std::uint64_t TOOL_ACTION_RELOAD_CANCELLED =
    gameHash("tool.action.reload-cancelled");
static constexpr std::uint64_t TOOL_ACTION_SECONDARY_STARTED =
    gameHash("tool.action.secondary-started");
static constexpr std::uint64_t TOOL_ACTION_DRY_FIRE =
    gameHash("tool.action.dry-fire");
static constexpr std::uint64_t TOOL_ACTION_LUNGE =
    gameHash("tool.action.lunge");
static constexpr std::uint64_t TOOL_ACTION_MELEE_CONTACT =
    gameHash("tool.action.melee-contact");

// One action fact. All fields are output; the event is animation-neutral.
struct ToolActionEventV1 {
    std::uint64_t actorEntity;      // userEntity
    std::uint64_t toolEntity;       // equipped tool entity (0 if unknown)
    std::uint64_t toolId;           // runtime tool key (network id or hash)
    std::uint64_t behaviorId;       // behavior family that produced the fact
    std::uint64_t action;           // TOOL_ACTION_* hash
    std::uint64_t simulationTick;
    std::uint32_t actionSequence;   // per-actor monotonic sequence
    std::uint32_t strength100;      // action strength * 100 (0 = unused)
    float origin[3];
    float direction[3];
    std::int32_t amount;            // damage/pellet count/ammo delta, 0 unused
    std::uint32_t result;           // 0 = accepted, else caller result code
};

// Short, searchable name for the action id (stable; used in the JSONL event
// name and message). Returns "tool.action.unknown" for an unknown id.
inline const char* toolActionName(std::uint64_t action)
{
    if (action == TOOL_ACTION_EQUIPPED) return "equipped";
    if (action == TOOL_ACTION_UNEQUIPPED) return "unequipped";
    if (action == TOOL_ACTION_PRIMARY_STARTED) return "primary-started";
    if (action == TOOL_ACTION_PRIMARY_ACCEPTED) return "primary-accepted";
    if (action == TOOL_ACTION_FIRED) return "fired";
    if (action == TOOL_ACTION_HIT) return "hit";
    if (action == TOOL_ACTION_RELOAD_STARTED) return "reload-started";
    if (action == TOOL_ACTION_RELOAD_FINISHED) return "reload-finished";
    if (action == TOOL_ACTION_RELOAD_CANCELLED) return "reload-cancelled";
    if (action == TOOL_ACTION_SECONDARY_STARTED) return "secondary-started";
    if (action == TOOL_ACTION_DRY_FIRE) return "dry-fire";
    if (action == TOOL_ACTION_LUNGE) return "lunge";
    if (action == TOOL_ACTION_MELEE_CONTACT) return "melee-contact";
    return "tool.action.unknown";
}

// ── Structured logging (category WEAPONS, so debuglogger.json owns the level) ──
// Level: 0 trace, 1 debug, 2 info (default-visible), 3 warn, 4 error.
inline void toolLogEvent(void* host, std::uint32_t level, const char* name,
                         const char* message, const char* result,
                         std::uint64_t entityId, std::uint64_t actorId,
                         std::uint32_t actorKind, std::uint64_t tick)
{
    if (!host)
        return;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return;
    using HotLogEventFn = void (MIMITA_GAME_CALL *)(void*,
                                                    const GameLogEventV1*);
    auto fn = reinterpret_cast<HotLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;
    GameLogEventV1 ev{};
    ev.level = level;
    ev.simulationTick = static_cast<std::uint32_t>(tick);
    ev.entityId = entityId;
    ev.actorId = actorId;
    ev.actorKind = actorKind;
    std::snprintf(ev.category, sizeof(ev.category), "%s", "WEAPONS");
    std::snprintf(ev.name, sizeof(ev.name), "%s", name ? name : "tool.event");
    std::snprintf(ev.message, sizeof(ev.message), "%s", message ? message : "");
    std::snprintf(ev.result, sizeof(ev.result), "%s", result ? result : "");
    fn(ctx->host, &ev);
}

// Convenience: one-line tool log with no identity.
inline void toolLog(void* host, std::uint32_t level, const char* name,
                    const char* message, std::uint64_t tick)
{
    toolLogEvent(host, level, name, message, nullptr, 0, 0, 0, tick);
}

// ── Action emit + state bridge ─────────────────────────────────────
// Dispatch the action as a generic `tool.action` fact AND log it. The animation
// bridge (a separate hot subscriber) converts facts into HotActorActionState so
// no behavior touches animation state itself.
inline void emitToolAction(GameplayContextV1* ctx,
                           const ToolActionEventV1& action)
{
    if (!ctx)
        return;
    if (ctx->emitEvent) {
        GameEventV1 event{};
        event.typeId = gameHash("tool.action");
        event.schemaHash = gameHash("tool.action.v1");
        event.payloadVersion = 1;
        event.payloadSize = sizeof(ToolActionEventV1);
        event.sourceEntity = action.actorEntity;
        event.targetEntity = action.toolEntity;
        event.tick = action.simulationTick;
        event.payload = const_cast<ToolActionEventV1*>(&action);
        using EmitFn = void (MIMITA_GAME_CALL *)(GameplayContextV1*,
                                                 const GameEventV1*);
        reinterpret_cast<EmitFn>(ctx->emitEvent)(ctx, &event);
    }
    char message[GAME_LOG_MESSAGE];
    std::snprintf(message, sizeof(message),
                  "tool=%llu behavior=%llu seq=%u strength=%.2f amount=%d",
                  (unsigned long long)action.toolId,
                  (unsigned long long)action.behaviorId,
                  action.actionSequence,
                  action.strength100 / 100.0f, (int)action.amount);
    char result[GAME_LOG_REASON];
    std::snprintf(result, sizeof(result), "action=%s",
                  toolActionName(action.action));
    toolLogEvent(ctx, 2, "tool.action", message, result, action.toolEntity,
                 action.actorEntity, 1 /* player */, action.simulationTick);
}
