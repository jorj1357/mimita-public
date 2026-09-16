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

// Generic match facts. `actor.killed` describes the occurrence; a hot handler
// that sets `handled` owns the scoring decision for that kill. `match.evaluate`
// asks the active mode to decide the outcome; a handler that sets `handled`
// suppresses the cold mode-specific win branch. Both return whether handled.
bool dispatchActorKilled(GameActorKilledV1& payload, std::uint64_t tick);
bool dispatchMatchEvaluate(GameMatchEvaluateV1& payload, std::uint64_t tick);

// Generic combat policy facts. `projectile.impact` lets a hot behavior own the
// consequence of a projectile hit; `tool.primary-use`/`tool.alt-use` lets a hot
// behavior own one held use of a tool. Both return whether a handler handled it.
bool dispatchProjectileImpact(ProjectileImpactPolicyV1& payload, std::uint64_t tick);
bool dispatchToolUse(ToolUsePolicyV1& payload, std::uint64_t tick);
// Generic effect request: a hot effect behavior may own the composition. Returns
// true when handled (the cold fallback must not also compose).
bool dispatchEffectRequest(EffectRequestV1& payload, std::uint64_t tick);

// Generic dispatch for any event type with a mutable POD payload. This is the
// single call site shape for all hot behavior seams: the kernel fills base
// values, calls this, then applies the payload's `handled`/out fields. New
// behavior for an existing event needs no new EXE call site.
bool dispatchPayload(std::uint32_t typeId, void* payload,
                     std::uint32_t payloadSize, std::uint64_t tick,
                     std::uint64_t sourceEntity = 0,
                     std::uint64_t targetEntity = 0,
                     std::uint64_t projectileEntity = 0);

// Dispatch an arbitrary runtime event by full 64-bit id with a valid
// GameplayContextV1 host, so domain-scoped hot handlers can resolve capabilities
// and use dynamic components. Returns true when a handler ran.
bool dispatchGameplayEvent64(std::uint64_t typeId, void* payload,
                             std::uint32_t payloadSize, std::uint64_t tick,
                             std::uint64_t sourceEntity = 0,
                             std::uint64_t targetEntity = 0);

// Per-entity behavior bindings: read the entity's BehaviorBindingsComponent and
// emit the bound behaviorId (a runtime event id) for the matching event type.
// This is how a tool/projectile is composed without a global weapon switch.
bool runBehaviorBindings(std::uint64_t entity, std::uint32_t eventType,
                         void* payload, std::uint32_t payloadSize,
                         std::uint64_t tick);

// World used by the queryWorldRay capability while a dispatch is in flight.
void setDispatchWorld(const void* world);

// Server-side collision world (HeadlessWorld). When bound, the physics.move /
// moveCapsule capabilities resolve against it, so hot movement works on the
// dedicated/listen server with the same primitive as the client.
void setDispatchHeadlessWorld(const void* world);

// Flush generic presentation geometry submitted through the render.debug
// capability during the render.frame domain run. Called once per frame by the
// UI/render pass so hot presentation appears in the same frame.
void flushRenderDebug();

// Total skeleton.apply invocations (headless evidence that hot pose generation
// reached the generic skeleton mechanism).
std::uint64_t skeletonApplyCount();

// Total legacy procedural animation bridge invocations. Zero while hot
// animation owns gameplay; non-zero only when the fallback switch is off.
std::uint64_t animationUpdateCount();

// Total audio.play invocations (headless evidence that hot audio policy reached
// the cold audio backend).
std::uint64_t audioPlayCount();

// Total surface.effect invocations (headless evidence that hot decal policy
// reached the cold surface mechanism).
std::uint64_t surfaceEffectCount();

// Total camera.effect invocations (headless evidence that hot camera-effect
// policy reached the cold camera mechanism).
std::uint64_t cameraEffectCount();

// Kernel event queue used by the emitEvent capability. Bounded and FIFO.
void enqueueEvent(const GameEventV1& event);
int drainEvents(int maxEvents);

// True when the active gameplay module provides an event handler.
bool available();

// Kernel-owned capability context for generic runtime systems (dynamic
// component read/write, entity find, log, emit). Valid for the current tick.
GameplayContextV1* hostContext(std::uint64_t tick);
} // namespace LiveBehavior
