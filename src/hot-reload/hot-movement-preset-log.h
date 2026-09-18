// 09 17 2026
/* purpose
* Hot-side proof bridge from the movement preset registry to the kernel's single
* JSONL debug stream (`log.event` capability), mirroring collision-log.h. Emits
* one `movement.preset` record when a preset is selected or requested, so the
* active preset, its id, the actor, and the effective tuning are observable live
* in events.jsonl while the game runs.
* Plain data only: the kernel owns events.jsonl; this header just describes the
* event. The `host` argument is the GameplayContextV1*.
* Does NOT own the logger, file lifetime, or aggregation.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>
#include <cstdio>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-presets.h"

namespace MimitaHotMovement {

using HotMovementLogEventFn = void (MIMITA_GAME_CALL *)(void* host,
                                                        const GameLogEventV1* event);

// Actor kinds, matching collision-log.h / GameLogEventV1::actorKind.
enum MovementLogActorKind : std::uint32_t {
    MOVEMENT_LOG_ACTOR_NONE = 0,
    MOVEMENT_LOG_ACTOR_PLAYER = 1,
    MOVEMENT_LOG_ACTOR_NPC = 2,
    MOVEMENT_LOG_ACTOR_REMOTE = 3,
    MOVEMENT_LOG_ACTOR_OTHER = 4,
};

// Fully-specified hot log record. Level 2 == Info, which passes the default
// "important" gate for the MOVEMENT category (and any per-category override).
inline bool movementPresetLogFull(void* host, std::uint32_t level,
                                  const char* category, const char* name,
                                  const char* message, const char* result,
                                  std::uint64_t entityId, std::uint64_t actorId,
                                  std::uint32_t actorKind, std::uint64_t frame,
                                  std::uint64_t serverTick,
                                  std::uint64_t clientTick,
                                  std::uint64_t simulationTick)
{
    if (!host)
        return false;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return false;
    auto fn = reinterpret_cast<HotMovementLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return false;

    GameLogEventV1 ev{};
    ev.level = level;
    ev.simulationTick = static_cast<std::uint32_t>(simulationTick);
    ev.entityId = entityId;
    ev.frame = frame;
    ev.serverTick = serverTick;
    ev.clientTick = clientTick;
    ev.actorId = actorId;
    ev.actorKind = actorKind;
    std::snprintf(ev.category, sizeof(ev.category), "%s",
                  category ? category : "MOVEMENT");
    std::snprintf(ev.name, sizeof(ev.name), "%s",
                  name ? name : "movement.preset");
    std::snprintf(ev.message, sizeof(ev.message), "%s", message ? message : "");
    std::snprintf(ev.result, sizeof(ev.result), "%s", result ? result : "");
    fn(ctx->host, &ev);
    return true;
}

// Formats the effective tuning of a preset so a reader can verify which values
// actually reached the movement step.
inline void movementPresetLogMessage(char* out, std::size_t capacity,
                                     const char* source,
                                     const MovementPresetId id,
                                     std::uint64_t entity,
                                     const MovementPreset& preset)
{
    std::snprintf(
        out, capacity,
        "preset=%s id=%u entity=%llu source=%s walkMode=%u groundSpeed=%.2f "
        "airSpeed=%.2f groundAccel=%.2f airAccel=%.2f friction=%.2f stop=%.2f "
        "airCap=%.2f airGain=%.2f gravity=%.2f jump=%.2f fall=%.1f "
        "groundDash=%.1f airDash=%.1f downDash=%.1f freeze=%.1f "
        "maxAirJumps=%u dash=%u downDashEn=%u freezeEn=%u",
        preset.name, static_cast<std::uint32_t>(id),
        static_cast<unsigned long long>(entity), source ? source : "",
        preset.tuning.walkMode, preset.tuning.groundSpeed, preset.tuning.airSpeed,
        preset.tuning.groundAcceleration, preset.tuning.airAcceleration,
        preset.tuning.groundFriction, preset.tuning.stopspeed,
        preset.tuning.airMaxWishspeed, preset.tuning.airSpeedGainMultiplier,
        preset.tuning.gravityMagnitude, preset.tuning.jumpSpeed,
        preset.tuning.maxFallSpeed, preset.tuning.groundDashImpulse,
        preset.tuning.airDashImpulse, preset.tuning.downDashSpeed,
        preset.tuning.freezeDurationSeconds, preset.tuning.maximumAirJumps,
        preset.tuning.dashEnabled, preset.tuning.downDashEnabled,
        preset.tuning.freezeEnabled);
}

// Tuning-request logging: one record per preset change, plus a slow heartbeat.
struct MovementPresetTuningLogState {
    std::uint32_t lastPresetId = 0xFFFFFFFFu;
    float sinceHeartbeatSeconds = 0.0f;
};

inline void movementPresetLogTuning(void* host, MovementPresetTuningLogState& state,
                                    MovementPresetId id, std::uint64_t tick,
                                    std::uint64_t frame)
{
    state.sinceHeartbeatSeconds += (1.0f / 60.0f);
    const std::uint32_t raw = static_cast<std::uint32_t>(id);
    if (raw == state.lastPresetId && state.sinceHeartbeatSeconds < 5.0f)
        return;
    state.lastPresetId = raw;
    state.sinceHeartbeatSeconds = 0.0f;
    const MovementPreset& preset = getMovementPreset(id);
    char msg[320];
    movementPresetLogMessage(msg, sizeof(msg), "tuning-request", id, 0, preset);
    movementPresetLogFull(host, 2u, "MOVEMENT", "movement.preset.tuning", msg,
                          preset.name, 0, 0, MOVEMENT_LOG_ACTOR_NONE, frame, 0,
                          tick, tick);
}

// Per-actor selection logging: one record per (entity, preset) change, plus a
// slow heartbeat. Uses a fixed-size ring so the hot path allocates nothing.
struct MovementPresetActorLogEntry {
    std::uint64_t entity = 0;
    std::uint32_t presetId = 0xFFFFFFFFu;
    float sinceHeartbeatSeconds = 0.0f;
};

inline MovementPresetActorLogEntry& movementPresetActorLogSlot(
    std::uint64_t entity)
{
    constexpr std::uint32_t kCapacity = 128;
    static MovementPresetActorLogEntry slots[kCapacity];
    std::uint32_t freeIndex = kCapacity;
    for (std::uint32_t i = 0; i < kCapacity; ++i) {
        if (slots[i].entity == entity)
            return slots[i];
        if (freeIndex == kCapacity && slots[i].entity == 0)
            freeIndex = i;
    }
    MovementPresetActorLogEntry& slot =
        slots[freeIndex < kCapacity ? freeIndex : (entity % kCapacity)];
    slot = MovementPresetActorLogEntry{};
    slot.entity = entity;
    slot.presetId = 0xFFFFFFFFu;  // unknown: the first compare is a change
    return slot;
}

inline void movementPresetLogActor(void* host, std::uint64_t entity,
                                   std::uint32_t actorKind, MovementPresetId id,
                                   const char* source, std::uint64_t tick,
                                   std::uint64_t frame)
{
    const std::uint32_t raw = static_cast<std::uint32_t>(id);
    MovementPresetActorLogEntry& slot = movementPresetActorLogSlot(entity);
    const bool changed = slot.presetId != raw;
    slot.sinceHeartbeatSeconds += (1.0f / 60.0f);
    if (!changed && slot.sinceHeartbeatSeconds < 5.0f)
        return;
    slot.presetId = raw;
    slot.sinceHeartbeatSeconds = 0.0f;
    const MovementPreset& preset = getMovementPreset(id);
    char msg[352];
    movementPresetLogMessage(msg, sizeof(msg), source, id, entity, preset);
    movementPresetLogFull(host, 2u, "MOVEMENT", "movement.preset.actor", msg,
                          preset.name, entity, entity, actorKind, frame, 0, tick,
                          tick);
}

} // namespace MimitaHotMovement

#endif // MIMITA_GAME_DLL
