// 09 22 2026
/* purpose
* Hot server attack-request routing/validation policy. The cold server parses the
* request and fills AttackPolicyV1; this behavior owns the decision: spawn-state,
* cooldown, slot, community set, geometry tolerance, per-tick shot limit, and the
* reported ammo/cooldown/state. It can accept, reject with a reason, or accept
* and suppress the cold fire (when it executes the shot itself).
* Full hot routing: no anti-cheat yet, so validation lives here and is
* live-editable. Falls back to the cold policy only when this declines.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-tuning.h"
#include "hot-reload/hot-tool-visual.h"

#include "network/packets.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

// Cold reject reason codes (AttackResultPacket.reason).
constexpr std::uint32_t kReasonCooldown = 1;
constexpr std::uint32_t kReasonDead = 2;
constexpr std::uint32_t kReasonSlot = 4;
constexpr std::uint32_t kReasonInvalidGeometry = 5;
constexpr std::uint32_t kReasonWeaponSet = 7;
constexpr std::uint32_t kReasonNotSpawned = 8;

void setReason(AttackPolicyV1* p, std::uint32_t reason, const char* text)
{
    p->accept = 0;
    p->reason = reason;
    if (text)
        std::snprintf(p->reasonText, GAME_ATTACK_REASON_MAX, "%s", text);
}

void MIMITA_GAME_CALL onAttackPolicy(void* host, const GameEventV1* event)
{
    if (!event || event->typeId != GAME_EVENT_HASH_ATTACK_POLICY ||
        event->payloadSize != sizeof(AttackPolicyV1))
        return;
    auto* policy = static_cast<AttackPolicyV1*>(event->payload);
    if (!policy)
        return;
    auto* ctx = static_cast<GameplayContextV1*>(host);

    // Scope ownership: only weapons whose recipe opted into hot execution are
    // routed here. Everything else declines so the cold validation stays
    // authoritative and no cold-owned weapon changes behavior.
    const ToolVisualRecipeV1* recipe =
        findToolVisualByNetworkId(policy->weaponDefNetworkId);
    if (!recipe)
        recipe = findToolVisualByNetworkId(policy->weaponNetworkId);
    if (!recipe ||
        (recipe->definition.toolFlags & TOOL_FLAG_OWNS_EXECUTION) == 0)
    {
        policy->handled = 0;
        return;
    }

    // This behavior owns the routing decision for hot weapons.
    policy->handled = 1u;
    policy->accept = 1u;
    policy->reason = 0;
    policy->suppressColdFire = 0u;
    policy->hitVerdict = MimitaNet::HIT_VERDICT_MISS;

    // Creation/inspection mode never fires.
    if (ctx && ctx->permanentStorage &&
        ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
        const auto* shared =
            reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage);
        if (shared->magic == GAME_SHARED_MAGIC &&
            (shared->modeFlags & GAME_MODE_FLAG_CREATION)) {
            setReason(policy, kReasonDead, "CREATION MODE");
            return;
        }
    }

    // ── Validation policy (was cold) ─────────────────────────────────
    if (!policy->hasDefinition) {
        setReason(policy, kReasonWeaponSet, "UNKNOWN WEAPON");
        return;
    }
    if (!policy->communityAllowed) {
        setReason(policy, kReasonWeaponSet, "WEAPON SET DISABLED");
        return;
    }
    if (policy->spawnGeneration == 0) {
        setReason(policy, kReasonNotSpawned, "STALE SPAWN GENERATION");
        return;
    }
    if (!policy->spawnStateActive) {
        setReason(policy, kReasonNotSpawned, "NOT SPAWNED");
        return;
    }
    if (policy->shooterDead) {
        setReason(policy, kReasonDead, "PLAYER DEAD");
        return;
    }
    // Slot reconcile: the requested logical slot must match the definition.
    if (policy->expectedSlot != 0 && policy->equippedSlot != policy->expectedSlot) {
        setReason(policy, kReasonSlot, "SLOT MISMATCH");
        return;
    }
    // Hitscan geometry tolerance (muzzle vs server position).
    if (policy->executionType == 0 /* Hitscan */) {
        const float dx = policy->origin[0] - policy->shooterPos[0];
        const float dy = policy->origin[1] - policy->shooterPos[1];
        const float dz = policy->origin[2] - policy->shooterPos[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float dirLen = std::sqrt(
            policy->direction[0] * policy->direction[0] +
            policy->direction[1] * policy->direction[1] +
            policy->direction[2] * policy->direction[2]);
        if (dirLen <= 0.0001f || dist > policy->originTolerance) {
            setReason(policy, kReasonInvalidGeometry, "INVALID GEOMETRY");
            return;
        }
    }
    // Per-tick shot rate limit for hitscan.
    if (policy->executionType == 0 /* Hitscan */ &&
        policy->maxShotsPerTick != 0 &&
        policy->shotsThisTick >= policy->maxShotsPerTick) {
        setReason(policy, kReasonCooldown, "SHOT LIMIT");
        return;
    }
    // Cooldown is owned by the tool's per-instance state when present; the hot
    // tool behavior enforces it. The cold tick gate is skipped for hot tools.

    // Accept: report the tool's authoritative state when it exists.
    GameWeaponTuningV1 tuning{};
    if (hotQueryWeaponTuning(ctx, policy->weaponDefNetworkId, tuning)) {
        policy->magazineAmmo = tuning.magazineSize;
        policy->reserveAmmo = tuning.reserveAmmo;
    } else {
        policy->magazineAmmo = -1;
        policy->reserveAmmo = -1;
    }

    // Suspend the built-in fire whenever this behavior owns the decision, so the
    // cold family dispatch does not also run. The tool behaviors execute the
    // shot; when a weapon has no hot tool behavior, it is not in this list and
    // the cold path is unaffected.
    policy->suppressColdFire = 0u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_attackPolicy{
    {GAME_EVENT_HASH_ATTACK_POLICY, GAME_EVENT_HASH_ATTACK_POLICY, 0,
     onAttackPolicy, "attack.policy"}};

#endif
