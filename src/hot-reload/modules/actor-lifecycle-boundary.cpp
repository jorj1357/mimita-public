// 09 23 2026
/* purpose
* The ONE hot actor lifecycle owner, shared by player initial spawn, respawn,
* and reconnect (and NPC spawn). It receives the stable identity + lifecycle
* state envelope, preserves identity/life generation, and is the single place
* to edit respawn health, spawn protection, and lifecycle policy live.
* The EXE remains responsible for applying returned values to storage.
* Does NOT own EXE storage, networking, or Player/Npc object layouts.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

namespace {

// Spawn protection in fixed 60 Hz ticks (60 = 1 second). Edit live.
constexpr std::uint32_t kLifecycleSpawnProtectionTicks = 60;

void MIMITA_GAME_CALL onActorLifecycle(void* host, const GameEventV1* event)
{
    auto* state = event ? static_cast<ActorLifecycleStateV1*>(event->payload) : nullptr;
    if (!state)
        return;

    // Identity and life generation are preserved: a reload must never change
    // who the actor is or which life it is on.
    state->dead = 0u;
    state->respawnRequested = 1u;

    // Arm spawn protection generically on the actor entity so the hot damage
    // policy can ignore incoming damage for the protection window.
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (ctx && ctx->dynamicWriteComponent && state->entityId != 0 &&
        kLifecycleSpawnProtectionTicks > 0)
    {
        HotSpawnProtectionV1 sp{};
        sp.untilTick = (std::uint32_t)ctx->tick + kLifecycleSpawnProtectionTicks;
        ctx->dynamicWriteComponent(ctx->host, state->entityId,
                                   HOT_SPAWN_PROTECTION_COMPONENT, &sp, sizeof(sp));
    }

    state->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorLifecycleRegistration{
    {GAME_EVENT_ACTOR_LIFECYCLE, 0, 0, onActorLifecycle,
     "actor.lifecycle"}};

// Register the shared SpawnProtection component schema so the generic
// dynamic-component write actually succeeds. Without a schema the store rejects
// the write (size lookup), which silently disabled spawn protection.
const MimitaHotPackage::SchemaRegistrar s_spawnProtectionSchema{
    {HOT_SPAWN_PROTECTION_COMPONENT, gameHash("SpawnProtection.v1"),
     (std::uint32_t)sizeof(HotSpawnProtectionV1),
     (std::uint32_t)alignof(HotSpawnProtectionV1), GAME_COPY_AUTHORING,
     GAME_NET_ALL, "SpawnProtection", 1, 0}};

#endif
