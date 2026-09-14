// 09 12 2026
/* purpose
* EXE-side generic behavior bridge. Dispatches GameEventV1 events to the active
* hot gameplay module and reports whether a behavior handled them.
* The kernel owns the payload; hot code only reads/writes plain data.
* Does NOT own damage, authority, or networking.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace LiveBehavior {

// Dispatch a GAME_EVENT_DAMAGE_POLICY event. Returns true when a hot behavior
// handled it (payload.handled set). `payload` is filled by the caller with base
// values and may be overridden by the behavior.
bool dispatchDamagePolicy(DamagePolicyV1& payload, std::uint64_t tick);

// Dispatch a GAME_EVENT_FIRE_INTENT event for one held-fire tick. Returns true
// when a hot behavior handled it (payload.handled set).
bool dispatchFireIntent(FireIntentPolicyV1& payload, std::uint64_t tick);

// Dispatch a GAME_EVENT_RAGDOLL_SOLVE event. Returns true when handled.
bool dispatchRagdollPolicy(RagdollPolicyV1& payload, std::uint64_t tick);

// Dispatch an arbitrary event to the active behavior table.
bool dispatchEvent(const GameEventV1& event, std::uint64_t tick);

// Generic dispatch for any event type with a mutable POD payload. This is the
// single call site shape for all hot behavior seams: the kernel fills base
// values, calls this, then applies the payload's `handled`/out fields. New
// behavior for an existing event needs no new EXE call site.
bool dispatchPayload(std::uint32_t typeId, void* payload,
                     std::uint32_t payloadSize, std::uint64_t tick,
                     std::uint64_t sourceEntity = 0,
                     std::uint64_t targetEntity = 0,
                     std::uint64_t projectileEntity = 0);

// World used by the queryWorldRay capability while a dispatch is in flight.
void setDispatchWorld(const void* world);

// Kernel event queue used by the emitEvent capability. Bounded and FIFO.
void enqueueEvent(const GameEventV1& event);
int drainEvents(int maxEvents);

// True when the active gameplay module provides an event handler.
bool available();

// Kernel-owned capability context for generic runtime systems (dynamic
// component read/write, entity find, log, emit). Valid for the current tick.
GameplayContextV1* hostContext(std::uint64_t tick);
} // namespace LiveBehavior
