// 2026-11-08
/* purpose
* hot-reloadable client/server networking policy decisions
* host owns transport, packet layout, contexts, prediction buffers, and ordering
* this file DOES NOT own sockets, packet framing, or authoritative state
*/
#include "modules/api/network-policy-api.h"

#include <algorithm>

#include <module-identity.h>

// Live-reload activation proof: comment-only edit, no behavior change.

namespace {

const mimita_module::HostServicesV1* gServices = nullptr;

void decideSnapshot(mimita_network_policy::SnapshotPolicyV1& p)
{
    // Queue pressure may reduce duplicate packets, but it must never disable
    // the previous-complete-snapshot resend. Losing that resend makes packet
    // loss and reconnect failures worse, so the safe default is always on.
    const bool pressure = p.queueBytes > 65536ull || p.queueChunks > 8u;
    p.redundancyCopies = pressure ? 0 : (p.chunkCount > 1 ? 0 : 1);
    p.resendPrevious = 1;
    p.partialKeepalive = 1;
    p.maxBufferedTicks = 8;
    p.maxAvatarQueueChunks = 2048;
    p.result = 1;
}

void decideReconnect(mimita_network_policy::ReconnectPolicyV1& p)
{
    const bool partialAlive = p.receivedChunks > 0 ||
        (p.expectedChunks > 0 && p.receivedChunks < p.expectedChunks);
    p.action = partialAlive ? 1 : 0;
    p.backoffMs = 1000;
    p.retryIntervalMs = 1000;
    p.graceMs = 10000;
    p.result = 1;
}

void decideMovementCorrection(
    mimita_network_policy::MovementCorrectionPolicyV1& p)
{
    if (!(p.errorDistance > 0.0f)) {
        p.outClass = 0;
    } else if (p.errorDistance <= p.smallCorrectionDistance) {
        p.outClass = 1;
    } else if (p.errorDistance < p.majorCorrectionDistance) {
        p.outClass = 2;
    } else {
        p.outClass = 3;
    }
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideHitClaim(mimita_network_policy::HitClaimPolicyV1& p)
{
    p.outHitTolerance = std::max(0.0f, p.rewindHitTolerance) +
        std::max(0.0f, p.claimLagAllowance);
    p.outOriginTolerance = std::max(0.0f, p.baseOriginTolerance) +
        std::max(0.0f, p.pingAllowance);
    p.outAccept = 1;
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideReconcile(mimita_network_policy::ReconcilePolicyV1& p)
{
    p.outHardSnap = p.positionError >= p.hardSnapDistance ? 1u : 0u;
    p.outLogCorrection =
        p.positionError >= p.correctionLogDistance ? 1u : 0u;
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideSessionHealth(
    mimita_network_policy::SessionHealthPolicyV1& p)
{
    const bool snapshotWait =
        p.snapshotsEverReceived && p.snapshotAgeMs > p.staleThresholdMs &&
        p.snapshotAgeMs <= p.hardTimeoutMs &&
        p.lastHeardAgeMs <= p.hardTimeoutMs;
    const bool hardTimedOut =
        p.lastHeardAgeMs > p.hardTimeoutMs ||
        (p.snapshotsEverReceived && p.snapshotAgeMs > p.hardTimeoutMs);
    p.outReconnect = hardTimedOut ? 1u : 0u;
    p.outBackoffMs = p.reconnectAttempts > 0 ? 1000u : 0u;
    p.outState = hardTimedOut ? 3u :
        (snapshotWait ? 2u :
         (p.lastHeardAgeMs > p.staleThresholdMs ? 1u : 0u));
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideMapSync(mimita_network_policy::MapSyncPolicyV1& p)
{
    p.outRetryIntervalMs = 500;
    p.outResendReady = (p.connected && p.mapReady && p.readySent &&
                        !p.hasAuthoritativeSpawn) ? 1u : 0u;
    p.outResetReady = 0;
    p.outState = !p.worldLoaded ? 0u :
        (!p.mapReady ? 1u :
         (!p.readySent ? 2u :
          (!p.hasAuthoritativeSpawn ? 3u : 4u)));
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideCombatValidation(
    mimita_network_policy::CombatValidationPolicyV1& p)
{
    p.outCooldownGraceTicks = p.cooldownGraceTicks;
    p.outEquipGraceTicks = p.equipGraceTicks;
    p.outOriginTolerance = std::max(0.0f, p.originTolerance);
    p.outMuzzleTolerance = std::max(0.0f, p.muzzleTolerance);
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideServerPhaseBudget(
    mimita_network_policy::ServerPhaseBudgetPolicyV1& p)
{
    p.outTotalBudgetMs = 16;
    p.outNpcBudgetMs = 10;
    p.outProjectileBudgetMs = 4;
    p.outContactBudgetMs = 4;
    p.outOverBudget =
        (p.npcMs > (double)p.outNpcBudgetMs ||
         p.projectileMs > (double)p.outProjectileBudgetMs ||
         p.contactMs > (double)p.outContactBudgetMs) ? 1u : 0u;
    // Intent only: the host may yield optional NPC/contact work on the next
    // tick. The fixed tick itself is never skipped.
    p.outDeferNpc =
        p.npcCount > 0 && p.npcMs > (double)p.outNpcBudgetMs ? 1u : 0u;
    p.outDeferContacts =
        p.contactMs > (double)p.outContactBudgetMs ? 1u : 0u;
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

void decideInterpolation(
    mimita_network_policy::InterpolationPolicyV1& p)
{
    p.outSnapTicks =
        (p.teleportGapTicks > 0 && p.gapTicks > p.teleportGapTicks) ? 1u : 0u;
    p.outSnapDistance =
        (p.teleportDistance > 0.0f && p.gapDistance > p.teleportDistance)
            ? 1u : 0u;
    p.outMaximumExtrapolationSeconds = p.maximumExtrapolationSeconds;
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

int MIMITA_MODULE_CALL initialize(const mimita_module::HostServicesV1* services)
{
    if (!services || services->version != mimita_module::ABI_VERSION ||
        services->structSize != sizeof(*services) || !services->log)
        return 0;
    gServices = services;
    return 1;
}

void MIMITA_MODULE_CALL shutdown()
{
    gServices = nullptr;
}

int MIMITA_MODULE_CALL onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId == mimita_module::EVENT_NETWORK_SNAPSHOT_POLICY &&
        event->payloadSize == sizeof(mimita_network_policy::SnapshotPolicyV1)) {
        auto& p = *static_cast<mimita_network_policy::SnapshotPolicyV1*>(
            event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideSnapshot(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_RECONNECT_POLICY &&
        event->payloadSize == sizeof(mimita_network_policy::ReconnectPolicyV1)) {
        auto& p = *static_cast<mimita_network_policy::ReconnectPolicyV1*>(
            event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideReconnect(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_MOVEMENT_CORRECTION &&
        event->payloadSize ==
            sizeof(mimita_network_policy::MovementCorrectionPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::MovementCorrectionPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideMovementCorrection(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_HIT_CLAIM_POLICY &&
        event->payloadSize == sizeof(mimita_network_policy::HitClaimPolicyV1)) {
        auto& p = *static_cast<mimita_network_policy::HitClaimPolicyV1*>(
            event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideHitClaim(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_RECONCILE_POLICY &&
        event->payloadSize == sizeof(mimita_network_policy::ReconcilePolicyV1)) {
        auto& p = *static_cast<mimita_network_policy::ReconcilePolicyV1*>(
            event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideReconcile(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_SESSION_HEALTH_POLICY &&
        event->payloadSize ==
            sizeof(mimita_network_policy::SessionHealthPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::SessionHealthPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideSessionHealth(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_MAP_SYNC_POLICY &&
        event->payloadSize ==
            sizeof(mimita_network_policy::MapSyncPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::MapSyncPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideMapSync(p);
        return 1;
    }
    if (event->typeId ==
            mimita_module::EVENT_NETWORK_COMBAT_VALIDATION_POLICY &&
        event->payloadSize ==
            sizeof(mimita_network_policy::CombatValidationPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::CombatValidationPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideCombatValidation(p);
        return 1;
    }
    if (event->typeId ==
            mimita_module::EVENT_NETWORK_SERVER_PHASE_BUDGET_POLICY &&
        event->payloadSize ==
            sizeof(mimita_network_policy::ServerPhaseBudgetPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::ServerPhaseBudgetPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideServerPhaseBudget(p);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NETWORK_INTERPOLATION_POLICY &&
        event->payloadSize ==
            sizeof(mimita_network_policy::InterpolationPolicyV1)) {
        auto& p = *static_cast<
            mimita_network_policy::InterpolationPolicyV1*>(event->payload);
        if (p.version != mimita_network_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decideInterpolation(p);
        return 1;
    }
    return 0;
}

std::uint32_t MIMITA_MODULE_CALL exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int MIMITA_MODULE_CALL importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "network-policy", MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH, MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    return abiVersion == mimita_module::ABI_VERSION ? &gModule.base : nullptr;
}
