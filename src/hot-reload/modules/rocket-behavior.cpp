// 09 12 2026
/* purpose
* Hot replaceable rocket/gameplay behavior module.
* Implements the generic behavior event handler: the kernel emits events with
* plain-data payloads and this module owns the gameplay policy (damage,
* knockback), so editing this file changes authoritative behavior live.
* Also exposes rocket flight motion policy.
* Does NOT own projectile state, health storage, authority, or packet flow.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"
#include "hot-reload/hot-movement-policy.h"

namespace {

bool MIMITA_GAME_CALL adjustRocketFlight(
    const RocketFlightStateV1* state, const RocketFlightParamsV1* base,
    RocketFlightParamsV1* out, GameMemory*)
{
    if (!base || !out)
        return false;
    *out = *base;
    // Motion policy is identity by default.
    (void)state;
    return true;
}

// Generic authoritative gameplay policy. The kernel fills base values and the
// behavior decides the result. `handled = 1` tells the kernel this policy owns
// the decision; the kernel then applies `outDamage` and the knockback.
void MIMITA_GAME_CALL onEvent(const GameEventV1* event, GameplayContextV1* context)
{
    if (!event || !event->payload)
        return;

    // Creation/inspection mode suppresses combat without a kernel change: the
    // hot editor sets the shared mode flag; this policy reads it.
    bool creationMode = false;
    if (context && context->structSize >= sizeof(GameplayContextV1) &&
        context->permanentStorage &&
        context->permanentStorageSize >= sizeof(GameSharedStateV1)) {
        const GameSharedStateV1* shared =
            reinterpret_cast<const GameSharedStateV1*>(context->permanentStorage);
        if (shared->magic == GAME_SHARED_MAGIC)
            creationMode = (shared->modeFlags & GAME_MODE_FLAG_CREATION) != 0;
    }

    if (event->typeId == GAME_EVENT_FIRE_INTENT &&
        event->payloadSize == sizeof(FireIntentPolicyV1))
    {
        FireIntentPolicyV1* policy = static_cast<FireIntentPolicyV1*>(event->payload);
        policy->handled = 1;
        policy->outFire = creationMode ? 0u : policy->baseFire;
        policy->ammoCost = 1;
        return;
    }

    if (event->typeId == GAME_EVENT_RAGDOLL_SOLVE &&
        event->payloadSize == sizeof(RagdollPolicyV1))
    {
        RagdollPolicyV1* policy = static_cast<RagdollPolicyV1*>(event->payload);
        policy->handled = 1;
        policy->outStiffness = policy->baseStiffness;
        policy->outDamping = policy->baseDamping;
        policy->outIterations = policy->baseIterations;
        policy->outGravityScale = policy->baseGravityScale;
        // Live proof: change outStiffness/outIterations here and save.
        return;
    }

    // Ragdoll body bind policy: the kernel already produced a reflection-safe
    // base frame; override per-avatar offsets / capsule placement here. Editing
    // this branch changes ragdoll geometry live without relinking the EXE.
    if (event->typeId == GAME_EVENT_RAGDOLL_BIND &&
        event->payloadSize == sizeof(RagdollBindPartV1))
    {
        RagdollBindPartV1* bind = static_cast<RagdollBindPartV1*>(event->payload);
        bind->handled = 1;
        return;
    }

    // Client local-player reconcile policy: return whether the authoritative
    // snap may move the local root. Ragdoll physics owns the root.
    if (event->typeId == GAME_EVENT_MOVEMENT_RECONCILE &&
        event->payloadSize == sizeof(MovementReconcileV1))
    {
        MovementReconcileV1* policy = static_cast<MovementReconcileV1*>(event->payload);
        policy->handled = 1;
        return;
    }

    // Server movement-report validation policy: override the computed decision
    // (for example to let a ragdoll root move under client body authority).
    if (event->typeId == GAME_EVENT_MOVEMENT_VALIDATION &&
        event->payloadSize == sizeof(MovementValidationV1))
    {
        MovementValidationV1* policy = static_cast<MovementValidationV1*>(event->payload);
        policy->handled = 1;
        policy->decision = policy->computedDecision;
        return;
    }

    // Projectile presentation policy: one path for player, NPC, and network
    // projectiles; never gated on the equipped weapon.
    if (event->typeId == GAME_EVENT_PROJECTILE_PRESENT &&
        event->payloadSize == sizeof(ProjectilePresentV1))
    {
        ProjectilePresentV1* policy = static_cast<ProjectilePresentV1*>(event->payload);
        policy->handled = 1;
        policy->visible = policy->exploded ? 0u : 1u;
        return;
    }

    // Death presentation policy: present a corpse exactly once per life.
    if (event->typeId == GAME_EVENT_ACTOR_DEATH &&
        event->payloadSize == sizeof(ActorDeathV1))
    {
        ActorDeathV1* policy = static_cast<ActorDeathV1*>(event->payload);
        policy->handled = 1;
        policy->presentCorpse = policy->alreadyPresented ? 0u : 1u;
        return;
    }

    // Respawn policy: a server-authoritative life must not be locally respawned
    // by the death system (the root cause of the once-per-tick death loop).
    if (event->typeId == GAME_EVENT_ACTOR_RESPAWN &&
        event->payloadSize == sizeof(ActorRespawnV1))
    {
        ActorRespawnV1* policy = static_cast<ActorRespawnV1*>(event->payload);
        policy->handled = 1;
        policy->allowLocalRespawn = policy->authoritative ? 0u : 1u;
        return;
    }

    // Connection status/retry policy. The transport stays kernel-owned.
    if (event->typeId == GAME_EVENT_CONNECTION_STATE &&
        event->payloadSize == sizeof(ConnectionStateV1))
    {
        ConnectionStateV1* policy = static_cast<ConnectionStateV1*>(event->payload);
        policy->handled = 1;
        policy->outBackoffScale = 1.0f;
        return;
    }

    if (event->typeId != GAME_EVENT_DAMAGE_POLICY ||
        event->payloadSize != sizeof(DamagePolicyV1))
        return;

    DamagePolicyV1* policy = static_cast<DamagePolicyV1*>(event->payload);
    policy->handled = 1;

    // Spawn protection (ticks): ignore damage to an actor whose protection
    // window has not elapsed. Applies to every actor (players and NPCs) since
    // the window is stored on the victim entity, not on an actor type.
    if (context && context->dynamicReadComponent && policy->victimEntity != 0) {
        HotSpawnProtectionV1 sp{};
        if (context->dynamicReadComponent(
                context->host, policy->victimEntity,
                HOT_SPAWN_PROTECTION_COMPONENT, &sp, sizeof(sp)) &&
            (std::uint32_t)context->tick < sp.untilTick) {
            policy->outDamage = 0;
            policy->knockbackX = 0.0f;
            policy->knockbackY = 0.0f;
            policy->knockbackZ = 0.0f;
            return;
        }
    }

    // Creation/inspection mode disables damage entirely.
    if (creationMode) {
        policy->outDamage = 0;
        return;
    }

    // Baseline: keep the JSON-derived base damage.
    policy->outDamage = policy->baseDamage;

    // No blanket explosion damage override: the projectile/tool owns the damage
    // and self-damage multiplier (see rocket-tool.cpp / hot-projectiles.cpp).
    // Edit that multiplier live instead of forcing a lethal value here.
}

const GameGameplayModuleV1 gGameplayModuleV1 = {
    2u,
    sizeof(GameGameplayModuleV1),
    adjustRocketFlight,
    onEvent,
};

} // namespace

const GameModuleDescriptor* MimitaGetGameplayModule()
{
    static const GameModuleDescriptor descriptor = {
        "gameplay", 2u, sizeof(GameGameplayModuleV1), &gGameplayModuleV1};
    return &descriptor;
}

#endif
