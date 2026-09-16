// 08 24 2026, 10 50
/* purpose
* Owns client multiplayer tick IO, packet receive dispatch, snapshot processing, and input sends.
* Converts local shared movement state into Stage 3A client movement reports.
* Keeps local and remote snapshot lifecycle filtering consistent across legacy and chunked snapshots.
* Does NOT define server validation policy, render interpolation math, or packet binary layouts.
* Does NOT own physics simulation, weapon runtime reconciliation internals, or server authority.
* Does NOT apply stale local snapshots before lifecycle checks pass.
*/

#include "network/multiplayer-context.h"
#include "network/dynamic-replication.h"
#include "ecs/dynamic-components.h"
#include "ecs/relationship-store.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/generation-verify.h"
#include "hot-reload/migration-prep.h"
#include "hot-reload/content-artifact.h"
#include "network/constraint-codec.h"
#include "physics/constraints/constraint-store.h"
#include "network/packets.h"
#include "duel/duel-queue.h"
#include "network/community-match-client.h"
#include "network/snapshot-chunks.h"
#include "network/remote-entity-lifecycle.h"
#include "network/badconn/badconn.h"
#include "network/reconnect-visuals.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "config/networking-config.h"
#include "config/camera-config.h"
#include "auth/auth-system.h"
#include "website/api-client.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "terminal/terminal-state.h"
#include "gui/hud/chat-history.h"
#include "gui/hud/chat-window.h"
#include "gui/hud/chat-bubble.h"
#include "world/world.h"
#include "entities/player.h"
#include "notifications/notifications.h"
#include "gui/hud/reward-popup.h"
#include "killfeed/killfeed.h"
#include "npc/npc-avatar.h"
#include "gui/password-popup.h"
#include "utils/time-format.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/artifact-transfer.h"
#include "hot-reload/artifact-cache.h"
#include "hot-reload/generation-switch-mapping.h"
#include "live-code/live-behavior.h"

namespace {
// Client-side distributed artifact acquisition (single in-flight transfer, v1).
MimitaRuntime::ArtifactReceiver gGenerationArtifactReceiver;
std::uint64_t gRequestedArtifactHash = 0;
} // namespace
#include "live-code/live-code-events.h"
#include "live-code/live-identity.h"
#include "ragdoll/ragdoll-entities.h"
#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "ragdoll/ragdoll-presentation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <unordered_set>
#include <utility>

namespace MimitaNet {

namespace {

// A snapshot tick gap larger than this (~500ms of missed snapshots) means a
// blackout/reconnect, not ordinary packet loss. It arms the client's post-gap
// resync so the local player snaps back to the server's authoritative position.
constexpr uint32_t POST_GAP_RESYNC_TICKS = 30;
// How long the post-gap resync stays armed after the gap is detected.
constexpr uint64_t POST_GAP_RESYNC_WINDOW_MS = 1000;

MimitaVip::VipAppearance vipAppearanceFromEntity(const SnapshotEntity& entity)
{
    return MimitaVip::appearanceFromBytes(
        entity.vipTier, entity.vipStyleKind, entity.vipColorR,
        entity.vipColorG, entity.vipColorB, entity.vipFlags);
}

uint32_t movementReportFlagsFromMpInput(const MpInput& input)
{
    uint32_t flags = 0;
    if (input.onGround)
        flags |= MOVEMENT_REPORT_ON_GROUND;
    if (input.stableOnGround)
        flags |= MOVEMENT_REPORT_STABLE_ON_GROUND;
    if (input.hasWorldContact)
        flags |= MOVEMENT_REPORT_HAS_WORLD_CONTACT;
    if (input.realWorldContactThisFrame)
        flags |= MOVEMENT_REPORT_REAL_WORLD_CONTACT;
    if (input.airJumpArmed)
        flags |= MOVEMENT_REPORT_AIR_JUMP_ARMED;
    if (input.airJumpLocked)
        flags |= MOVEMENT_REPORT_AIR_JUMP_LOCKED;
    if (input.dashAvailable)
        flags |= MOVEMENT_REPORT_DASH_AVAILABLE;
    if (input.dashMomentumProtectionActive)
        flags |= MOVEMENT_REPORT_DASH_PROTECTED;
    if (input.downDashAvailable)
        flags |= MOVEMENT_REPORT_DOWN_DASH_AVAILABLE;
    if (input.freezeActive)
        flags |= MOVEMENT_REPORT_FREEZE_ACTIVE;
    if (input.freezeAvailable)
        flags |= MOVEMENT_REPORT_FREEZE_AVAILABLE;
    if (input.groundReturnAvailable)
        flags |= MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE;
    if (input.jumpHeld)
        flags |= MOVEMENT_REPORT_JUMP_HELD;
    if (input.dashPressed)
        flags |= MOVEMENT_REPORT_DASH_PRESSED;
    if (input.downDashPressed)
        flags |= MOVEMENT_REPORT_DOWN_DASH_PRESSED;
    if (input.freezeHeld)
        flags |= MOVEMENT_REPORT_FREEZE_HELD;
    return flags;
}

static void eraseLocalReplica(MultiplayerContext& ctx, uint32_t entityId,
                              const char* reason)
{
    if (entityId == 0)
        return;
    const bool hadPlayer = ctx.remotePlayers.erase(entityId) != 0;
    const bool hadInterp = ctx.remotePlayerInterpolation.erase(entityId) != 0;
    if (hadPlayer || hadInterp)
    {
        printf("[CLIENT LOCAL REPLICA DROP] playerId=%u entityId=%u reason=%s\n",
               ctx.localPlayerId, entityId, reason);
    }
}

// ── Shared snapshot entity processing ──────────────────────────────
// Called by both legacy (SnapshotPacket) and chunked (CompactEntityData)
// snapshot paths.  Handles local-player state, remote-player/NPC creation,
// interpolation, and cleanup of missing entities.
// Generation gating: while a late-join bootstrap is in progress (or has failed),
// this peer must not consume ordinary world snapshots. Idle means steady state.
static bool mpGenerationWorldAllowed(const MultiplayerContext& ctx)
{
    const MimitaRuntime::GenerationBootstrapV1& b = ctx.generationBootstrap;
    if (b.state == MimitaRuntime::BootstrapState::Idle)
        return true;
    const std::uint64_t local = (std::uint64_t)
        HotReloadSystem::instance().status().activeGeneration;
    const std::uint64_t server = (std::uint64_t)ctx.serverCodeGeneration;
    return b.worldParticipationAllowed(server, local);
}

static void processSnapshotEntities(
    MultiplayerContext& ctx,
    const SnapshotEntity* entities,
    uint32_t entityCount,
    uint32_t serverTick,
    float dt,
    const char* sourceName,
    uint32_t logicalGenerationId = 0)
{
    if (!mpGenerationWorldAllowed(ctx))
    {
        Debug::logThrottled(Debug::Category::General, "generation-bootstrap", 2.0f,
                            "world snapshot ignored: generation bootstrap %s",
                            MimitaRuntime::bootstrapStateName(
                                ctx.generationBootstrap.state));
        return;
    }
    // ── Authoritative membership ordering gate ──────────────────────────
    // A snapshot older than the newest already-applied membership snapshot
    // must NOT create, destroy, remove, or revive remote entities. It may
    // only feed interpolation history (which independently rejects stale
    // transform samples). Out-of-order snapshots are never allowed to
    // reconcile world membership against newer state.
    const bool membershipAllowed = snapshotMayMutateMembership(
        serverTick, ctx.latestAppliedMembershipTick);
    if (!membershipAllowed)
    {
        Debug::logThrottled(
            Debug::Category::Networking,
            "snapshot-membership-skip",
            0.25f,
            "[SNAPSHOT MEMBERSHIP SKIP] tick=%u latestAppliedMembershipTick=%u "
            "reason=older-than-current-membership\n",
            serverTick, ctx.latestAppliedMembershipTick);
    }

    const auto& lifecycleCfg =
        NetworkingConfig::instance().data().remoteEntityLifecycle;

    std::unordered_map<uint32_t, bool> seenPlayers;
    std::unordered_map<uint32_t, bool> seenNpcs;

    for (uint32_t i = 0; i < entityCount; ++i)
    {
        const SnapshotEntity& entity = entities[i];
        if (!entity.active || entity.networkEntityId == 0)
        {
            printf("[CLIENT ENTITY SKIP] entityId=%u reason=inactive-or-zero-id\n",
                   entity.networkEntityId);
            continue;
        }

        const bool isLocal =
            entity.entityType == ENTITY_PLAYER &&
            (entity.ownerClientId == ctx.localPlayerId ||
             entity.networkEntityId == ctx.localPlayerId);
        if (isLocal)
        {
            eraseLocalReplica(ctx, entity.networkEntityId, "authoritative-local-snapshot");

            const bool olderEpoch = entity.transformEpoch != 0 &&
                ctx.localServerEpoch != 0 &&
                (uint32_t)entity.transformEpoch < (uint32_t)ctx.localServerEpoch;
            const bool sameEpochOlderTick = entity.transformEpoch == ctx.localServerEpoch &&
                serverTick <= ctx.latestLocalSnapshotTick;
            const bool acceptLifecycle = !olderEpoch && !sameEpochOlderTick;

            if (!acceptLifecycle)
            {
                Debug::logThrottled(
                    Debug::Category::Networking,
                    "client-local-stale-snapshot",
                    0.5f,
                    "[CLIENT SNAPSHOT DROP] entityId=%u reason=stale-local "
                    "tick=%u latestTick=%u epoch=%u localEpoch=%u spawnGen=%u "
                    "knownSpawnGen=%u\n",
                    entity.networkEntityId,
                    serverTick,
                    ctx.latestLocalSnapshotTick,
                    (unsigned)entity.transformEpoch,
                    (unsigned)ctx.localServerEpoch,
                    entity.spawnGeneration,
                    ctx.lastKnownSpawnGeneration);
                continue;
            }

            ctx.localServerPosition = {entity.px, entity.py, entity.pz};
            ctx.localServerVelocity = {entity.vx, entity.vy, entity.vz};
            ctx.localServerYaw = entity.yaw;
            ctx.localServerOnGround = entity.onGround != 0;
            ctx.hasLocalServerPosition = true;
            ctx.localServerHealth = entity.health;
            ctx.localServerEpoch = entity.transformEpoch;
            ctx.localPingMs = entity.pingMs;
            ctx.latestLocalSnapshotTick = serverTick;
            // Spawn generations are monotonic (the server never decrements
            // them), so only advance. A stale pre-respawn snapshot reordered
            // past a newer spawn must NOT revert lastKnownSpawnGeneration,
            // or the SpawnActivated generation check would silently fail and
            // leave gameplay permanently disabled.
            if (entity.spawnGeneration != 0 &&
                entity.spawnGeneration > ctx.lastKnownSpawnGeneration)
                ctx.lastKnownSpawnGeneration = entity.spawnGeneration;
            if (entity.health > 0)
                ctx.latestAliveSnapshotTick = serverTick;

            if (entity.transformEpoch != 0 &&
                (uint32_t)entity.transformEpoch > ctx.transformEpoch)
            {
                const uint32_t oldEpoch = ctx.transformEpoch;
                ctx.transformEpoch = entity.transformEpoch;
                ctx.teleportResync = true;
                static uint64_t lastEpochSyncLogMs = 0;
                uint64_t nowSnapshot = nowMs();
                if (nowSnapshot - lastEpochSyncLogMs >= 500)
                {
                    printf("[NET EPOCH SYNC] player=%u oldOutgoingEpoch=%u newServerEpoch=%u "
                           "position=(%.2f,%.2f,%.2f)\n",
                           ctx.localPlayerId, oldEpoch,
                           (uint32_t)entity.transformEpoch,
                           entity.px, entity.py, entity.pz);
                    lastEpochSyncLogMs = nowSnapshot;
                }
            }
            if (ctx.awaitingTeleportAck &&
                glm::length(ctx.localServerPosition -
                            ctx.pendingTeleportPosition) <= 1.0f)
            {
                ctx.awaitingTeleportAck = false;
                ctx.teleportResync = true;
                printf("[NET TELEPORT ACK] position=%.1f,%.1f,%.1f\n",
                       ctx.localServerPosition.x, ctx.localServerPosition.y,
                       ctx.localServerPosition.z);
            }
            if (ctx.awaitingExplodeDeath && entity.health <= 0)
            {
                ctx.awaitingExplodeDeath = false;
                ctx.explodeRequestLastSendMs = 0;
            }
            PlayerInfo& localInfo = ctx.playerRegistry[entity.networkEntityId];
            localInfo.name = entity.displayName;
            localInfo.id = entity.networkEntityId;
            localInfo.pingMs = entity.pingMs;
            localInfo.vipAppearance = vipAppearanceFromEntity(entity);
            if (entity.vipStyleEpoch != 0)
                localInfo.vipStyleEpoch = entity.vipStyleEpoch;
            auto pendingLocal = ctx.pendingVipStyles.find(entity.networkEntityId);
            if (pendingLocal != ctx.pendingVipStyles.end())
            {
                localInfo.vipStyleDetail = pendingLocal->second;
                localInfo.vipStyleEpoch = pendingLocal->second.styleEpoch;
                ctx.pendingVipStyles.erase(pendingLocal);
            }
            static uint64_t lastLocalSnapshotLogMs = 0;
            uint64_t nowLocalSnap = nowMs();
            if (nowLocalSnap - lastLocalSnapshotLogMs >= 250)
            {
                lastLocalSnapshotLogMs = nowLocalSnap;
                DBG(Network, "%s tick=%u local pos=(%.2f,%.2f,%.2f) hp=%d epoch=%u",
                    sourceName, serverTick,
                    entity.px, entity.py, entity.pz, entity.health, entity.transformEpoch);
            }
            continue;
        }

        std::unordered_map<uint32_t, Player>* replicas = nullptr;
        std::unordered_map<uint32_t, EntityInterpolationState>* interpolationMap = nullptr;
        std::unordered_map<uint32_t, bool>* seen = nullptr;
        const char* typeName = nullptr;
        if (entity.entityType == ENTITY_PLAYER)
        {
            if (entity.networkEntityId == ctx.localPlayerId ||
                entity.ownerClientId == ctx.localPlayerId)
            {
                eraseLocalReplica(ctx, entity.networkEntityId, "local-identity-guard");
                continue;
            }
            replicas = &ctx.remotePlayers;
            interpolationMap = &ctx.remotePlayerInterpolation;
            seen = &seenPlayers;
            typeName = "Player";
            PlayerInfo& remoteInfo = ctx.playerRegistry[entity.networkEntityId];
            remoteInfo.name = entity.displayName;
            remoteInfo.id = entity.networkEntityId;
            remoteInfo.pingMs = entity.pingMs;
            remoteInfo.vipAppearance = vipAppearanceFromEntity(entity);
            if (entity.vipStyleEpoch != 0)
                remoteInfo.vipStyleEpoch = entity.vipStyleEpoch;
            auto pendingRemote = ctx.pendingVipStyles.find(entity.networkEntityId);
            if (pendingRemote != ctx.pendingVipStyles.end())
            {
                remoteInfo.vipStyleDetail = pendingRemote->second;
                remoteInfo.vipStyleEpoch = pendingRemote->second.styleEpoch;
                ctx.pendingVipStyles.erase(pendingRemote);
            }
        }
        else if (entity.entityType == ENTITY_NPC)
        {
            replicas = &ctx.remoteNpcs;
            interpolationMap = &ctx.remoteNpcInterpolation;
            seen = &seenNpcs;
            typeName = "NPC";
        }
        else
        {
            printf("[CLIENT ENTITY SKIP] entityId=%u reason=unknown-entity-type-%u\n",
                   entity.networkEntityId, entity.entityType);
            continue;
        }

        bool existsBefore = replicas->find(entity.networkEntityId) != replicas->end();
        // A stale snapshot must never create new entities: creation implies
        // authoritative membership that only the newest complete snapshot
        // may assert. Existing entities may still receive interpolation
        // samples, which are independently freshness-rejected.
        if (!membershipAllowed && !existsBefore)
            continue;
        Player& p = (*replicas)[entity.networkEntityId];
        bool isNew = !existsBefore;
        EntityInterpolationState& interpolation = (*interpolationMap)[entity.networkEntityId];
        const bool npcNewLife = entity.entityType == ENTITY_NPC && existsBefore &&
            entity.transformEpoch != 0 &&
            interpolation.lastSnapshotTransformEpoch != 0 &&
            entity.transformEpoch != interpolation.lastSnapshotTransformEpoch;
        if (npcNewLife)
        {
            // A reused NPC id is a new life, not a continuation of the old
            // prediction. Drop old damage/death overlays before this snapshot
            // can render the respawned body.
            ctx.predictedNpcHitMs.erase(entity.networkEntityId);
            ctx.predictedNpcDamage.erase(entity.networkEntityId);
            interpolation.pendingPredictedDamage = 0;
            interpolation.predictedHealthCap = -1;
            interpolation.predictedHealthUpdatedMs = 0;
            p.netPredictedDead = false;
            p.dead = false;
            const std::string npcAvatar = entity.avatarName[0] != '\0'
                ? std::string(entity.avatarName)
                : npcAvatarNameForLife(entity.networkEntityId, entity.transformEpoch);
            if (!npcAvatar.empty()) {
                if (!AvatarSystem::instance().applyAvatarToPlayer(p, npcAvatar))
                    p.loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
                p.setAvatarName(npcAvatar);
            }
            Debug::warn(Debug::Category::Networking,
                "[NPC LIFE RESET] npcId=%u oldEpoch=%u newEpoch=%u reason=respawn",
                entity.networkEntityId,
                (unsigned)interpolation.lastSnapshotTransformEpoch,
                (unsigned)entity.transformEpoch);
        }
        if (isNew)
        {
            if (entity.entityType != ENTITY_NPC)
            {
                std::string remoteAvatar(entity.avatarName);
                if (remoteAvatar.empty()) {
                    AvatarSystem::applySingleTexture(p, GetPlayerSettings().outfitPath);
                } else if (!AvatarSystem::instance().applyAvatarToPlayer(p, remoteAvatar)) {
                    AvatarSystem::applySingleTexture(p, GetPlayerSettings().outfitPath);
                }
                p.setAvatarName(remoteAvatar);
                Debug::warn(Debug::Category::Avatar,
                    "[REMOTE AVATAR] entityId=%u name='%s' avatar='%s' atlas=%u\n",
                    entity.networkEntityId, entity.displayName, remoteAvatar.c_str(),
                    p.avatarInstance ? p.avatarInstance->atlasTexture : 0);
            }
            else
            {
                p.username = entity.displayName;
                std::string npcAvatar = entity.avatarName[0] != '\0'
                    ? std::string(entity.avatarName)
                    : npcAvatarNameForLife(entity.networkEntityId, entity.transformEpoch);
                if (!npcAvatar.empty()) {
                    if (!AvatarSystem::instance().applyAvatarToPlayer(p, npcAvatar)) {
                        p.loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
                    }
                } else {
                    p.loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
                }
                p.setAvatarName(npcAvatar);
                Debug::warn(Debug::Category::Avatar,
                    "[NPC AVATAR CLIENT] entityId=%u avatar='%s' atlas=%u\n",
                    entity.networkEntityId, npcAvatar.c_str(),
                    p.avatarInstance ? p.avatarInstance->atlasTexture : 0);
            }
            interpolation.renderRegistered = true;
            printf("[CLIENT ENTITY CREATE] entityId=%u type=%s ownerClientId=%u "
                   "mesh=%s position=(%.2f,%.2f,%.2f)\n",
                   entity.networkEntityId, typeName, entity.ownerClientId,
                   p.modelLoaded ? "player-glb" : "fallback-capsule",
                   entity.px, entity.py, entity.pz);
        }

        // Avatar identity can change while a replica is alive (for example
        // after an editor save or reconnect). Re-run the shared async avatar
        // application only when the network identity actually changes.
        if (!isNew && entity.entityType != ENTITY_NPC) {
            const std::string networkAvatar(entity.avatarName);
            if (networkAvatar != p.avatarName()) {
                if (!networkAvatar.empty())
                    AvatarSystem::instance().applyAvatarToPlayer(p, networkAvatar);
                else
                    p.setCosmetics({});
                p.setAvatarName(networkAvatar);
                Debug::warn(Debug::Category::Avatar,
                    "[REMOTE AVATAR] entityId=%u changed avatar='%s' ready=%d\n",
                    entity.networkEntityId, networkAvatar.c_str(), (int)p.modelLoaded);
            }
        }

        if (!pushInterpolationTarget(interpolation, entity, serverTick,
                                     logicalGenerationId))
            continue;
        p.spawnGeneration = entity.spawnGeneration;
        if (isNew)
        {
            // Seed serial baselines so creating a replica does not replay
            // already-occurred presentation events (dash, jumps, freeze).
            baselinePresentationSerials(p, interpolation.target);
            updateRenderedReplica(p, interpolation, ctx.interpolationRenderTick, dt, true);
        }
        if (membershipAllowed)
        {
            (*seen)[entity.networkEntityId] = true;
            interpolation.missingTracker.noteSeen();
        }

        static uint64_t lastEntityLogMs = 0;
        uint64_t nowEnt = nowMs();
        if (isNew || nowEnt - lastEntityLogMs >= 1000)
        {
            lastEntityLogMs = nowEnt;
            DBG(Network, "CLIENT ENTITY entityId=%u type=%s ownerId=%u isLocal=0 existsBefore=%d "
                "createdReplica=%d renderRegistered=%d position=(%.2f,%.2f,%.2f) rot=%.2f name=%s",
                entity.networkEntityId, typeName, entity.ownerClientId,
                (int)existsBefore, (int)isNew, (int)interpolation.renderRegistered,
                entity.px, entity.py, entity.pz, entity.yaw, entity.displayName);
        }
    }

    if (!membershipAllowed)
    {
        // Old snapshot: interpolation history may have been updated, but the
        // live entity registry must not be reconciled. Advance the membership
        // tick only for snapshots that are actually applied as members.
        return;
    }

    ctx.latestAppliedMembershipTick = serverTick;
    const uint64_t nowMissingMs = nowMs();

    // Clean up missing entities — only after a complete, newer membership
    // snapshot, and only after the entity has been absent across enough
    // snapshots for long enough to be authoritative rather than packet loss.
    for (auto it = ctx.remotePlayers.begin(); it != ctx.remotePlayers.end(); )
    {
        if (!seenPlayers[it->first])
        {
            const uint32_t eid = it->first;
            EntityInterpolationState& interp =
                ctx.remotePlayerInterpolation[eid];
            interp.missingTracker.noteMissing(nowMissingMs);
            if (interp.missingTracker.shouldRemove(
                    lifecycleCfg.missingSnapshotConfirmationCount,
                    lifecycleCfg.missingSnapshotGraceMs, nowMissingMs))
            {
                printf("[ENTITY DESTROY] reason=missing-from-snapshot-confirmed "
                       "entityId=%u type=Player name=\"%s\" tick=%u "
                       "confirmations=%u\n",
                       eid, ctx.playerRegistry[eid].name.c_str(), serverTick,
                       interp.missingTracker.confirmations);
                it = ctx.remotePlayers.erase(it);
                ctx.remotePlayerInterpolation.erase(eid);
                ctx.playerRegistry.erase(eid);
                mpClearRemoteReconnectVisual(ctx, eid);
            }
            else
            {
                Debug::logThrottled(
                    Debug::Category::Networking,
                    "remote-entity-retain-player",
                    0.5f,
                    "[REMOTE ENTITY RETAIN] entityId=%u type=Player "
                    "snapshotTick=%u confirmations=%u reason=grace-not-elapsed\n",
                    eid, serverTick, interp.missingTracker.confirmations);
                ++it;
            }
        }
        else
            ++it;
    }
    for (auto it = ctx.remoteNpcs.begin(); it != ctx.remoteNpcs.end(); )
    {
        if (!seenNpcs[it->first])
        {
            const uint32_t eid = it->first;
            EntityInterpolationState& interp =
                ctx.remoteNpcInterpolation[eid];
            interp.missingTracker.noteMissing(nowMissingMs);
            if (interp.missingTracker.shouldRemove(
                    lifecycleCfg.missingSnapshotConfirmationCount,
                    lifecycleCfg.missingSnapshotGraceMs, nowMissingMs))
            {
                printf("[ENTITY DESTROY] reason=missing-from-snapshot-confirmed "
                       "entityId=%u type=NPC name=\"%s\" tick=%u "
                       "confirmations=%u\n",
                       eid, it->second.username.c_str(), serverTick,
                       interp.missingTracker.confirmations);
                it = ctx.remoteNpcs.erase(it);
                ctx.remoteNpcInterpolation.erase(eid);
                ctx.predictedNpcHitMs.erase(eid);
                ctx.predictedNpcDamage.erase(eid);
            }
            else
            {
                Debug::logThrottled(
                    Debug::Category::Networking,
                    "remote-entity-retain-npc",
                    0.5f,
                    "[REMOTE ENTITY RETAIN] entityId=%u type=NPC "
                    "snapshotTick=%u confirmations=%u reason=grace-not-elapsed\n",
                    eid, serverTick, interp.missingTracker.confirmations);
                ++it;
            }
        }
        else
            ++it;
    }
}

// (anonymous namespace continues below)

bool isSameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_family == b.sin_family &&
        a.sin_port == b.sin_port &&
        a.sin_addr.s_addr == b.sin_addr.s_addr;
}

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

void sendJoinRequest(MultiplayerContext& ctx, const std::string& playerName)
{
    if (ctx.sock == INVALID_SOCKET && !ctx.transport)
    {
        printf("[NET CONNECT] sendJoinRequest skipped: no transport\n");
        return;
    }
    if (ctx.vipJoinTicket.empty() && !ctx.vipJoinTicketRequested)
    {
        ctx.vipJoinTicketRequested = true;
        const AuthUser& authUser = AuthSystem::instance().user();
        const std::string roomCode = !ctx.currentRoomCode.empty()
            ? ctx.currentRoomCode
            : ctx.roomCode;
        if (!authUser.sessionToken.empty())
        {
            ctx.vipJoinTicket = requestVipJoinTicket(authUser.sessionToken, roomCode, "");
            Debug::warn(Debug::Category::Vip,
                "[VIP JOIN] ticket request result=%s room=%s\n",
                ctx.vipJoinTicket.empty() ? "free-fallback" : "issued",
                roomCode.empty() ? "empty" : "present");
        }
    }

    JoinRequestPacket join{};
    join.header.type = PACKET_JOIN_REQUEST;
    join.header.tick = ctx.tick;
    std::memset(join.joinToken, 0, sizeof(join.joinToken));
    std::strncpy(join.joinToken, ctx.joinToken.c_str(), sizeof(join.joinToken) - 1);
    std::memset(join.vipJoinTicket, 0, sizeof(join.vipJoinTicket));
    std::strncpy(join.vipJoinTicket, ctx.vipJoinTicket.c_str(), sizeof(join.vipJoinTicket) - 1);
    const auto& authUser = AuthSystem::instance().user();
    join.accountId = authUser.id > 0 ? static_cast<uint32_t>(authUser.id) : 0;
    std::memset(join.name, 0, sizeof(join.name));
    std::strncpy(join.name, playerName.c_str(), sizeof(join.name) - 1);
    std::memset(join.avatarName, 0, sizeof(join.avatarName));
    std::strncpy(join.avatarName, GetPlayerSettings().avatarName.c_str(), sizeof(join.avatarName) - 1);
    std::memset(join.password, 0, sizeof(join.password));
    std::strncpy(join.password, ctx.serverPassword.c_str(), sizeof(join.password) - 1);
    mpSendPacket(ctx, &join, sizeof(join));
    printf("[NET CONNECT] join request sent token=%s\n", ctx.joinToken.c_str());
}

} // namespace

static const char* disconnectReasonStr(MultiplayerContext& ctx)
{
    if (!ctx.active) return "inactive";
    if (ctx.connectFailed) return "connection-timeout";
    if (!ctx.connected) return "not-connected";
    if (ctx.sock == INVALID_SOCKET) return "invalid-socket";
    return "unknown";
}

// ── Apply authoritative spawn state from server ──────────────────────
void applyAuthoritativeSpawn(MultiplayerContext& ctx, const PlayerRespawnedPacket* spawn)
{
    ctx.communityWeaponSetId = spawn->communityWeaponSetId;
    uint32_t oldGen = ctx.lastKnownSpawnGeneration;
    ctx.lastKnownSpawnGeneration = spawn->spawnGeneration;
    // A duel teleport can reuse the same spawn generation while advancing the
    // transform epoch. Seed the complete authoritative transform here so the
    // very next input cannot report the pre-duel local position.
    ctx.localServerPosition = {spawn->posX, spawn->posY, spawn->posZ};
    ctx.localServerVelocity = {spawn->velX, spawn->velY, spawn->velZ};
    ctx.localServerEpoch = spawn->transformEpoch;
    ctx.transformEpoch = spawn->transformEpoch;
    ctx.lastAppliedEpoch = 0;
    ctx.hasLocalServerPosition = true;
    ctx.localPlayerReconciled = false;
    Debug::log(Debug::Category::Duel,
        "[DuelSpawnApply] player=%u generation=%u epoch=%u position=(%.3f,%.3f,%.3f) transformGate=reset\n",
        ctx.localPlayerId, spawn->spawnGeneration, spawn->transformEpoch,
        spawn->posX, spawn->posY, spawn->posZ);
    ctx.nextMovementSequence = 1;
    Debug::log(Debug::Category::Weapons,
               "[SPAWN_GENERATION_CHANGED] playerId=%u oldGen=%u newGen=%u epoch=%u\n",
               ctx.localPlayerId, oldGen, spawn->spawnGeneration, spawn->transformEpoch);

    // Cancel old-life pending attack requests
    for (auto it = ctx.pendingAttackRequests.begin(); it != ctx.pendingAttackRequests.end(); )
    {
        if (it->second.spawnGeneration == oldGen)
        {
            Debug::log(Debug::Category::Weapons, "[SPAWN SYNC CANCEL] requestId=%u oldGen=%u newGen=%u\n",
                       it->second.requestId, oldGen, spawn->spawnGeneration);
            it = ctx.pendingAttackRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Clear old-life predicted state
    ctx.networkProjectiles.clear();
    ctx.predictedProjectileIds.clear();
    ctx.predictedExplosions.clear();
    ctx.predictedSelfKnockbacks.clear();
    ctx.predictedNpcHitMs.clear();
    ctx.predictedNpcDamage.clear();
    ctx.clientSimulationAccumulator = 0.0;
    ctx.clientSimulationTick = 0;
    ctx.clientSimulationStepsThisUpdate = 0;
    ctx.pendingFireRequests.clear();
    ctx.pendingReloadRequests.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();

    // ── Reapply gamemode camera FOV override on spawn ──────────────
    const auto& communityMatch = CommunityMatchClient::instance();
    if (communityMatch.active() && communityMatch.cameraFov() > 0.0f) {
        auto& camCfg = CamConfig::instance().data();
        if (camCfg.fov != communityMatch.cameraFov()) {
            camCfg.fov = communityMatch.cameraFov();
            Debug::log(Debug::Category::Duel,
                "[SPAWN] Reapplied gamemode FOV=%.0f\n", communityMatch.cameraFov());
        }
    }

    Debug::log(Debug::Category::Weapons, "[SPAWN SYNC APPLY] oldGen=%u newGen=%u health=%d weapons=%u\n",
               oldGen, spawn->spawnGeneration, spawn->health, spawn->weaponCount);
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input, const World& world)
{
    // Network gameplay prediction advances on the same fixed 60 Hz clock as
    // server collision/damage. Render FPS and a bad connection must not change
    // how many projectile collision steps are simulated.
    constexpr double kClientSimulationDt = 1.0 / 60.0;
    ctx.clientSimulationAccumulator += std::max(0.0, (double)dt);
    ctx.clientSimulationStepsThisUpdate = 0;
    while (ctx.clientSimulationAccumulator >= kClientSimulationDt &&
           ctx.clientSimulationStepsThisUpdate < 5)
    {
        ctx.clientSimulationAccumulator -= kClientSimulationDt;
        ++ctx.clientSimulationTick;
        ++ctx.clientSimulationStepsThisUpdate;
    }
    if (ctx.clientSimulationStepsThisUpdate == 5 &&
        ctx.clientSimulationAccumulator >= kClientSimulationDt)
        ctx.clientSimulationAccumulator = kClientSimulationDt;

    Debug::logThrottled(Debug::Category::Networking, "client-fixed-step", 1.0,
        "[CLIENT FIXED STEP] tick=%u steps=%u accumulator=%.5f hz=60\n",
        ctx.clientSimulationTick, ctx.clientSimulationStepsThisUpdate,
        ctx.clientSimulationAccumulator);

    // ── Live code generation agreement ──────────────────────────────
    // Report our active generation/hash to the server and surface a mismatch
    // with the server's announced generation. This is the seed of the future
    // multiplayer READY/switch-tick hot-code protocol.
    if (ctx.active && ctx.localPlayerId != 0)
    {
        const HotReloadSystem::Status liveStatus = HotReloadSystem::instance().status();
        if (ctx.reliableEventSessionId != 0)
            LiveIdentity::setSessionId(ctx.reliableEventSessionId);
        if (ctx.clientSimulationTick - ctx.lastCodeGenerationSentTick >= 30)
        {
            ctx.lastCodeGenerationSentTick = ctx.clientSimulationTick;
            CodeGenerationPacket report{};
            report.header.type = PACKET_CODE_GENERATION;
            report.header.tick = ctx.clientSimulationTick;
            report.header.playerId = ctx.localPlayerId;
            report.generation = liveStatus.activeGeneration;
            report.direction = 0;  // client -> server
            // READY only when the local build is loaded AND the server's
            // advertised hot ABI is compatible with our cold kernel ABI.
            const bool abiCompatible = ctx.serverHotAbiVersion == 0 ||
                ctx.serverHotAbiVersion == (uint32_t)MIMITA_GAME_API_VERSION;
            report.phase = (liveStatus.loaded && abiCompatible) ? 1 : 0;  // 1 = READY
            auto hexValue = [](char c) -> uint64_t {
                if (c >= '0' && c <= '9') return (uint64_t)(c - '0');
                if (c >= 'a' && c <= 'f') return (uint64_t)(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return (uint64_t)(c - 'A' + 10);
                return 0;
            };
            for (int i = 0; i + 1 < (int)liveStatus.activeHash.size() && i < 16; i += 2)
                report.codeHash = (report.codeHash << 8) |
                    (hexValue(liveStatus.activeHash[i]) << 4) |
                    hexValue(liveStatus.activeHash[i + 1]);
            report.moduleSetHash =
                MimitaRuntime::GenericRuntime::instance().manifestHash();
            report.logicalCodeHash =
                report.codeHash ^ (report.moduleSetHash * 1099511628211ull);
            report.platformPackageHash = liveStatus.activeGeneration;
            mpSendPacket(ctx, &report, sizeof(report));
        }
        if (ctx.serverCodeGeneration != 0 && liveStatus.activeGeneration != 0 &&
            ctx.serverCodeGeneration != liveStatus.activeGeneration)
        {
            static uint32_t lastLocalGen = 0;
            static uint32_t lastServerGen = 0;
            if (lastLocalGen != liveStatus.activeGeneration ||
                lastServerGen != ctx.serverCodeGeneration)
            {
                LiveCodeEvents::notifyGenerationMismatch(
                    liveStatus.activeGeneration, ctx.serverCodeGeneration, true);
                lastLocalGen = liveStatus.activeGeneration;
                lastServerGen = ctx.serverCodeGeneration;
            }
        }

        // Ragdoll limb replication: bind the live ragdoll into the entity
        // registry and replicate a bounded snapshot every few ticks.
        if (RagdollModeSystem::instance().isActive())
        {
            Ragdoll::RagdollEntities& entities = Ragdoll::RagdollEntities::instance();
            const RagdollBody& body = RagdollModeSystem::instance().aliveBody();
            entities.bind(ctx.localPlayerId, body);
            entities.syncFromBody(ctx.localPlayerId, body);
            entities.setGrab(ctx.localPlayerId, true, RagdollModeSystem::instance().leftGrab());
            entities.setGrab(ctx.localPlayerId, false, RagdollModeSystem::instance().rightGrab());
            // Replication cadence is an editable primitive (snapshot_send_interval_ticks).
            const int sendInterval = std::max(1, RagdollModeConfig::instance().data().snapshotSendIntervalTicks);
            if (ctx.clientSimulationTick - ctx.lastRagdollSentTick >= (std::uint32_t)sendInterval)
            {
                ctx.lastRagdollSentTick = ctx.clientSimulationTick;
                mpSendRagdollSnapshot(ctx);
            }
        }

        // Corpse replication: announce a locally-simulated death once so peers
        // derive the same deterministic corpse (seed = f(owner, tick, eventId)).
        {
            RagdollModeSystem& ragdoll = RagdollModeSystem::instance();
            if (ragdoll.corpseSerial() != ctx.lastCorpseSerial)
            {
                ctx.lastCorpseSerial = ragdoll.corpseSerial();
                const RagdollModeSystem::CorpseSpawnInfo& info = ragdoll.lastCorpseInfo();
                if (RagdollModeConfig::instance().data().replicateCorpses &&
                    (info.ownerId == 0 || info.ownerId == ctx.localPlayerId))
                    mpSendCorpseSpawn(ctx, info.ownerId, info.deathTick,
                        info.deathEventId, info.impulse, info.actorId);
            }
        }

        // Generic constraint reconciliation (client prediction + server
        // authority): advertise locally-active constraints until the server
        // confirms; send a release for any serial that just ended.
        {
            Physics::ConstraintStore& store = Physics::ConstraintStore::instance();
            const int cInterval = std::max(1,
                RagdollModeConfig::instance().data().snapshotSendIntervalTicks);
            const bool requestNow =
                ctx.clientSimulationTick - ctx.lastConstraintRequestTick >=
                (std::uint32_t)cInterval;
            if (requestNow)
                ctx.lastConstraintRequestTick = ctx.clientSimulationTick;
            std::unordered_set<std::uint32_t> current;
            for (std::uint32_t serial : store.activeSerialsForOwner(ctx.localPlayerId)) {
                current.insert(serial);
                if (ctx.confirmedConstraints.find(serial) == ctx.confirmedConstraints.end() &&
                    requestNow) {
                    if (const Physics::ConstraintComponent* c = store.component(serial))
                        mpSendConstraintCreate(ctx, *c);
                }
            }
            for (std::uint32_t serial : ctx.localConstraintsLastTick) {
                if (current.find(serial) == current.end())
                    mpSendConstraintRelease(ctx, serial, 0);
            }
            ctx.localConstraintsLastTick = std::move(current);
        }
    }

    // ── Async ICE connect job ───────────────────────────────────────────
    // The ICE connect runs on a background thread; poll it every frame so the
    // game never blocks. Progress messages surface on the HUD via
    // ctx.connectionStatus. On success the finished transport is installed
    // here (main thread) and the normal handshake continues below.
    {
        IceConnectStatus connect = mpIceConnectPoll();
        if (connect.active)
        {
            // Hot connection status/retry policy. The transport stays kernel, but
            // the visible status text is a behavior so it can be edited live.
            ConnectionStateV1 policy{};
            policy.phase = 0;
            policy.attempt = 0;
            policy.maxAttempts = 5;
            policy.elapsedMs = 0;
            policy.lastError = 0;
            policy.outBackoffScale = 1.0f;
            std::snprintf(policy.outMessage, sizeof(policy.outMessage), "%s",
                          connect.message.c_str());
            LiveBehavior::dispatchPayload(GAME_EVENT_CONNECTION_STATE, &policy,
                                          sizeof(policy), ctx.tick, 0, 0, 0);
            ctx.connectionStatus = (policy.handled && policy.outMessage[0] != '\0')
                ? std::string(policy.outMessage) : connect.message;
        }
        else if (connect.done)
        {
            if (connect.success)
            {
                printf("[ICE CONNECT] transport installed; starting handshake\n");
                mpInstallIceConnectSuccess(ctx, connect);
            }
            else if (!connect.cancelled)
            {
                printf("[ICE CONNECT] failed: %s\n", connect.message.c_str());
                ctx.connectionStatus = connect.message;
                ctx.connectionState = ConnectionState::Disconnected;
                ctx.connectFailed = true;
                NotificationSystem::instance().pushCritical(
                    "Connection failed",
                    "Server status: Connection failed — " + connect.message, 0);
            }
        }
    }

    if (!ctx.active)
        return;
    if (ctx.sock == INVALID_SOCKET && !ctx.transport)
    {
        Debug::warn(Debug::Category::Networking,
               "[NET TICK] sock=INVALID_SOCKET state=%s connected=%d active=%d transport=%d\n",
               connectionStateName(ctx.connectionState), (int)ctx.connected, (int)ctx.active,
               (int)(ctx.transport != nullptr));
        return;
    }

    // Explode is a client request, but it must survive badconn loss. The
    // server operation is idempotent: retries are ignored once already dead.
    if (ctx.awaitingExplodeDeath && ctx.localPlayerId)
    {
        const uint64_t now = nowMs();
        if (ctx.explodeRequestLastSendMs == 0 || now - ctx.explodeRequestLastSendMs >= 200)
        {
            ExplodeRequestPacket request{};
            request.header.type = PACKET_EXPLODE_REQUEST;
            request.header.tick = ctx.tick;
            request.header.playerId = ctx.localPlayerId;
            request.header.transformEpoch = ctx.transformEpoch;
            mpSendPacket(ctx, &request, sizeof(request));
            ctx.explodeRequestLastSendMs = now;
            Debug::log(Debug::Category::Networking,
                "[EXPLODE REQUEST RETRY] player=%u epoch=%u\n",
                ctx.localPlayerId, ctx.transformEpoch);
        }
    }

    uint64_t currentMs = nowMs();

    // ── Honest connection-health machine ───────────────────────────────
    // Packet-freshness drives Connected → WeakConnection → Reconnecting →
    // ReconnectFailed. The server slot stays alive for the grace window, and
    // every transition surfaces a notification (see mpNotifyConnectionStateChange).
    // This replaces the old "silently teardown at clientTimeoutMs" behavior.
    mpUpdateConnectionHealth(ctx);

    // The health machine may have torn the session down on give-up; nothing
    // below is valid with a closed socket / released transport.
    if (!ctx.active)
        return;

    // Decay old disagreement events
    {
        constexpr uint64_t DISAGREEMENT_LIFETIME_MS = 3000;
        for (size_t i = 0; i < ctx.disagreementEvents.size(); )
        {
            if (currentMs - ctx.disagreementEvents[i].timeMs > DISAGREEMENT_LIFETIME_MS)
                ctx.disagreementEvents.erase(ctx.disagreementEvents.begin() + i);
            else
                ++i;
        }
    }

    badconn::tick(ctx.transport.get());
    mpSweepHitClaims(ctx);
    const uint64_t connectTimeoutMs =
        (uint64_t)NetworkingConfig::instance().data().timeouts.connectTimeoutMs;
    // connectStartMs belongs to the initial handshake. Once an established
    // session enters Reconnecting, connected is intentionally false, but the
    // initial-connect timeout must not tear down that session immediately.
    const bool waitingForInitialConnection =
        ctx.connectionState == ConnectionState::Connecting ||
        ctx.connectionState == ConnectionState::WaitJoinAccept;
    if (waitingForInitialConnection && !ctx.connected && !ctx.connectFailed &&
        currentMs - ctx.connectStartMs > connectTimeoutMs)
    {
        ctx.connectionStatus = "Connection timed out";
        printf("[NET CONNECT] timeout server=%s\n", ctx.serverAddress.c_str());
        NotificationSystem::instance().pushCritical(
            "Connection timed out",
            "Server status: Connection timed out — " + ctx.serverAddress, 0);
        teardownPreviousSession(ctx, DisconnectPolicy::ConnectionFailure);
    }

    // ── Password popup result: re-trigger join with new password ──────
    if (ctx.wrongPassword && !PasswordPopup::isOpen() && !PasswordPopup::getPassword().empty())
    {
        ctx.serverPassword = PasswordPopup::getPassword();
        ctx.wrongPassword = false;
        PasswordPopup::clearResult();
        const std::string roomCode = !ctx.currentRoomCode.empty()
            ? ctx.currentRoomCode : ctx.roomCode;
        const bool restarted = mpIceConnectStart(ctx, roomCode, playerName);
        printf("[NET CONNECT] password submitted, restarting ICE join room=%s started=%d\n",
               roomCode.c_str(), (int)restarted);
    }

    // ── Connection state machine ───────────────────────────────────────
    if (ctx.connectionState == ConnectionState::Connecting ||
        ctx.connectionState == ConnectionState::WaitJoinAccept)
    {
        if (!ctx.wrongPassword && currentMs - ctx.lastHelloMs > 500)
        {
            if (!ctx.joinToken.empty())
            {
                sendJoinRequest(ctx, playerName);
                ctx.connectionState = ConnectionState::WaitJoinAccept;
            }
            else
            {
                HelloPacket hello{};
                hello.header.type = PACKET_HELLO;
                hello.header.tick = ctx.tick;
                copyName(hello.name, playerName);
                std::memset(hello.avatarName, 0, sizeof(hello.avatarName));
                std::strncpy(hello.avatarName, GetPlayerSettings().avatarName.c_str(), sizeof(hello.avatarName) - 1);
                mpSendPacket(ctx, &hello, sizeof(hello));
            }
            ctx.lastHelloMs = currentMs;
        }
    }

    if (ctx.connectionState == ConnectionState::Reconnecting)
    {
        mpTickReconnect(ctx);
    }

    // ── Receive loop ──
    // Polls either the ICE transport or raw UDP socket for incoming packets.
    // Processes each packet through the shared dispatch chain.
    char buffer[16384];

    // Lambda to process a single validated packet from buffer[0..bytes)
    auto processPacket = [&](int bytes) {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (bytes < (int)sizeof(PacketHeader) ||
            header->magic != PROTOCOL_MAGIC ||
            header->version != PROTOCOL_VERSION)
            return;

        // Any validated packet from the server resets the real-time heartbeat
        // so connection timeouts are measured with monotonic wall-clock time,
        // never snapshot sequence gaps or entity absence.
        ctx.lastHeardServerMs = nowMs();

        if (header->type == PACKET_WELCOME && bytes >= (int)sizeof(WelcomePacket))
        {
            WelcomePacket* welcome = reinterpret_cast<WelcomePacket*>(buffer);
            ctx.localPlayerId = welcome->assignedPlayerId;
            ctx.connected = true;
            ctx.connectFailed = false;
            ctx.connectionState = ConnectionState::Connected;
            ctx.connectionStatus = "Connected";
            ctx.approvedLocalName = welcome->approvedName;
            ctx.reconnectToken = welcome->reconnectToken;
            ctx.reliableEventSessionId = welcome->reliableEventSessionId;
            ctx.requiredMapId = welcome->mapId;
            ctx.transformEpoch = welcome->header.transformEpoch;
            ctx.nextMovementSequence = 1;
            ctx.clientMapReadySent = false;
            ctx.clientMapReadySentForMap.clear();
            ctx.clientMapReadySentForPlayerId = 0;
            ctx.mapLoadAttempts = 0;
            ctx.lastMapLoadAttemptMs = 0;
            // Reset reconciliation state for new connection
            ctx.hasLocalServerPosition = false;
            ctx.localPlayerReconciled = false;
            ctx.localServerEpoch = 0;
            ctx.lastAppliedEpoch = 0;
            ctx.teleportResync = false;
            ctx.awaitingTeleportAck = false;
            printf("[NET CONNECT] player=%u serverTick=%u tickRate=%.0f mapId=%s epoch=%u\n",
                   ctx.localPlayerId, welcome->header.tick, welcome->tickRate,
                   welcome->mapId, ctx.transformEpoch);
            ctx.playerRegistry[ctx.localPlayerId] = {
                ctx.approvedLocalName.empty() ? playerName : ctx.approvedLocalName,
                ctx.localPlayerId,
                0,
                MimitaVip::freeAppearance()
            };
            eraseLocalReplica(ctx, ctx.localPlayerId, "welcome");
            printf("[NET CONNECT] player=%u serverTick=%u tickRate=%.0f mapId=%s\n",
                   ctx.localPlayerId, welcome->header.tick, welcome->tickRate,
                   welcome->mapId);
            mpNotifyConnectionStateChange(ctx, ConnectionState::Connecting,
                                          ConnectionState::Connected);
        }
        else if (header->type == PACKET_JOIN_ACCEPT && bytes >= (int)sizeof(JoinAcceptPacket))
        {
            const JoinAcceptPacket* accept = reinterpret_cast<const JoinAcceptPacket*>(buffer);
            ctx.localPlayerId = accept->assignedPlayerId;
            ctx.connected = true;
            ctx.connectFailed = false;
            ctx.connectionState = ConnectionState::Connected;
            ctx.connectionStatus = "Connected";
            ctx.wrongPassword = false;
            ctx.approvedLocalName = accept->approvedName;
            ctx.reconnectToken = accept->reconnectToken;
            ctx.reliableEventSessionId = accept->reliableEventSessionId;
            ctx.requiredMapId = accept->mapId;
            ctx.transformEpoch = accept->header.transformEpoch;
            ctx.nextMovementSequence = 1;
            ctx.clientMapReadySent = false;
            ctx.clientMapReadySentForMap.clear();
            ctx.clientMapReadySentForPlayerId = 0;
            ctx.mapLoadAttempts = 0;
            ctx.lastMapLoadAttemptMs = 0;
            // Reset reconciliation state for new connection
            ctx.hasLocalServerPosition = false;
            ctx.localPlayerReconciled = false;
            ctx.localServerEpoch = 0;
            ctx.lastAppliedEpoch = 0;
            ctx.teleportResync = false;
            ctx.awaitingTeleportAck = false;
            printf("[NET CONNECT] join accepted player=%u tickRate=%.0f mapId=%s epoch=%u\n",
                   ctx.localPlayerId, accept->tickRate, accept->mapId, ctx.transformEpoch);
            ctx.playerRegistry[ctx.localPlayerId] = {
                ctx.approvedLocalName.empty() ? playerName : ctx.approvedLocalName,
                ctx.localPlayerId,
                0,
                MimitaVip::freeAppearance()
            };
            eraseLocalReplica(ctx, ctx.localPlayerId, "join-accept");
            printf("[NET CONNECT] join accepted player=%u tickRate=%.0f mapId=%s\n",
                   ctx.localPlayerId, accept->tickRate, accept->mapId);
            mpNotifyConnectionStateChange(ctx, ConnectionState::Connecting,
                                          ConnectionState::Connected);
        }
        else if (header->type == PACKET_JOIN_REJECT && bytes >= (int)sizeof(JoinRejectPacket))
        {
            const JoinRejectPacket* reject = reinterpret_cast<const JoinRejectPacket*>(buffer);
            if (reject->reason == 5)
            {
                ctx.wrongPassword = true;
                ctx.connectionStatus = "Wrong password";
                ctx.connectionState = ConnectionState::Disconnected;
                PasswordPopup::open(ctx.roomCode, "");
                PasswordPopup::setWrongPassword(true);
                printf("[NET CONNECT] join rejected reason=5 wrong-password\n");
            }
            else
            {
                ctx.connectionStatus = "Join rejected";
                printf("[NET CONNECT] join rejected reason=%u\n", reject->reason);
                teardownPreviousSession(ctx, DisconnectPolicy::Rejected);
            }
        }
        else if (header->type == PACKET_RECONNECT_ACCEPT && bytes >= (int)sizeof(ReconnectAcceptPacket))
        {
            const ReconnectAcceptPacket* accept = reinterpret_cast<const ReconnectAcceptPacket*>(buffer);
            ctx.localPlayerId = accept->assignedPlayerId;
            ctx.connected = true;
            ctx.connectFailed = false;
            ctx.connectionState = ConnectionState::Connected;
            ctx.connectionStatus = "Reconnected";
            ctx.approvedLocalName = accept->approvedName;
            ctx.reconnectToken = accept->reconnectToken;
            ctx.reliableEventSessionId = accept->reliableEventSessionId;
            if (accept->spawnGeneration != 0)
                ctx.lastKnownSpawnGeneration = accept->spawnGeneration;
            ctx.localServerPosition = {accept->restorePx, accept->restorePy, accept->restorePz};
            ctx.localServerHealth = accept->restoredHealth;
            ctx.hasLocalServerPosition = true;
            ctx.transformEpoch = accept->header.transformEpoch;
            ctx.localServerEpoch = accept->header.transformEpoch;
            ctx.nextMovementSequence = 1;
            ctx.lastAppliedEpoch = 0;
            ctx.localPlayerReconciled = false;
            ctx.teleportResync = false;
            ctx.awaitingTeleportAck = false;
            eraseLocalReplica(ctx, ctx.localPlayerId, "reconnect-accept");
            printf("[NET RECONNECT] accepted player=%u health=%d kills=%d deaths=%d epoch=%u spawnGen=%u\n",
                   ctx.localPlayerId, accept->restoredHealth,
                   accept->restoredKills, accept->restoredDeaths,
                   ctx.transformEpoch, ctx.lastKnownSpawnGeneration);
            mpNotifyConnectionStateChange(ctx, ConnectionState::Reconnecting,
                                          ConnectionState::Connected);
        }
        else if (header->type == PACKET_DISAGREEMENT && bytes >= (int)sizeof(DisagreementPacket))
        {
            mpProcessDisagreementPacket(ctx, reinterpret_cast<const DisagreementPacket*>(buffer));
        }
        else if (header->type == PACKET_SNAPSHOT && bytes >= (int)sizeof(SnapshotPacket))
        {
            SnapshotPacket* snapshot = reinterpret_cast<SnapshotPacket*>(buffer);
            if (ctx.lastSnapshotTick != 0 &&
                snapshot->header.tick > ctx.lastSnapshotTick + 1)
            {
                ctx.snapshotsMissed +=
                    snapshot->header.tick - ctx.lastSnapshotTick - 1;
                if (snapshot->header.tick - ctx.lastSnapshotTick > POST_GAP_RESYNC_TICKS)
                {
                    ctx.postGapResync = true;
                    ctx.postGapResyncDeadlineMs = nowMs() + POST_GAP_RESYNC_WINDOW_MS;
                }
            }
            if (ctx.connectionState == ConnectionState::WaitJoinAccept ||
                ctx.connectionState == ConnectionState::Connecting)
                ctx.connectionState = ConnectionState::Connected;
            ++ctx.snapshotsReceived;
            // Monotonic: a reordered older snapshot must never regress the
            // newest-seen tick used for missed-snapshot accounting and UI.
            if (snapshot->header.tick > ctx.lastSnapshotTick)
                ctx.lastSnapshotTick = snapshot->header.tick;
            if (snapshot->header.tick > ctx.latestServerTick)
                ctx.latestServerTick = snapshot->header.tick;
            ctx.lastSnapshotReceivedMs = nowMs();
            uint32_t count = std::min(snapshot->entityCount, (uint32_t)MAX_SNAPSHOT_ENTITIES);

            processSnapshotEntities(ctx, snapshot->entities, count,
                                    snapshot->header.tick, dt, "legacy");
        }
        else if (header->type == PACKET_SHOT_EVENT &&
                 bytes >= (int)sizeof(ShotEventPacket))
        {
            mpProcessShotEventPacket(ctx, reinterpret_cast<const ShotEventPacket*>(buffer));
        }
        // Chunked snapshot: smaller than legacy SnapshotPacket = chunk
        else if (header->type == PACKET_SNAPSHOT &&
                 bytes >= (int)sizeof(PacketHeader) + 12 &&
                 bytes < (int)sizeof(SnapshotPacket))
        {
            if (ctx.connectionState == ConnectionState::WaitJoinAccept ||
                ctx.connectionState == ConnectionState::Connecting)
                ctx.connectionState = ConnectionState::Connected;

            SnapshotChunkPacket chunk{};
            if (!parseSnapshotChunk(buffer, (size_t)bytes, chunk))
                return;

            // Buffer the chunk
            // NOTE: ctx.latestServerTick is NOT advanced per chunk — a partial
            // chunk set is not a usable snapshot, and advancing the newest tick
            // would let the monotonic render clock over-run the buffer while
            // the remaining chunks assemble. It advances only on full
            // reassembly below.
            auto& bufMap = ctx.snapshotChunkBuffers[chunk.header.tick];
            bufMap.chunks[chunk.chunkIndex] = chunk;
            bufMap.lastReceiveMs = nowMs();

            // Do NOT increment snapshotsReceived per chunk — wait for full reassembly
            // Do NOT update lastSnapshotTick yet — wait until all chunks arrive

            // Check if all chunks for this tick have arrived
            if (bufMap.chunks.size() != chunk.chunkCount)
            {
                // Clean stale buffers
                uint64_t nowClean = nowMs();
                const uint64_t chunkTimeoutMs = (uint64_t)(
                    NetworkingConfig::instance().data().snapshotBuffer.chunkReassemblyTimeoutSeconds * 1000.0);
                for (auto it = ctx.snapshotChunkBuffers.begin(); it != ctx.snapshotChunkBuffers.end(); )
                {
                    if (nowClean - it->second.lastReceiveMs > chunkTimeoutMs)
                        it = ctx.snapshotChunkBuffers.erase(it);
                    else ++it;
                }
                return;
            }

            // All chunks received — sort and reassemble
            std::vector<SnapshotChunkPacket> sorted;
            sorted.reserve(chunk.chunkCount);
            for (uint16_t ci = 0; ci < chunk.chunkCount; ++ci)
            {
                auto it = bufMap.chunks.find(ci);
                if (it == bufMap.chunks.end()) { sorted.clear(); break; }
                sorted.push_back(it->second);
            }
            if (sorted.empty())
            {
                ctx.snapshotChunkBuffers.erase(chunk.header.tick);
                return;
            }

            std::vector<CompactEntityData> outEntities;
            std::uint32_t snapshotGeneration = 0;
            if (!reassembleSnapshotChunks(sorted, outEntities, nullptr,
                                          &snapshotGeneration))
            {
                ctx.snapshotChunkBuffers.erase(chunk.header.tick);
                return;
            }

            // Track missed snapshots
            if (ctx.lastSnapshotTick != 0 &&
                chunk.header.tick > ctx.lastSnapshotTick + 1)
            {
                ctx.snapshotsMissed += chunk.header.tick - ctx.lastSnapshotTick - 1;
                if (chunk.header.tick - ctx.lastSnapshotTick > POST_GAP_RESYNC_TICKS)
                {
                    ctx.postGapResync = true;
                    ctx.postGapResyncDeadlineMs = nowMs() + POST_GAP_RESYNC_WINDOW_MS;
                }
            }

            // Update stats exactly once per complete snapshot
            ++ctx.snapshotsReceived;
            if (chunk.header.tick > ctx.lastSnapshotTick)
                ctx.lastSnapshotTick = chunk.header.tick;
            if (chunk.header.tick > ctx.latestServerTick)
                ctx.latestServerTick = chunk.header.tick;
            ctx.lastSnapshotReceivedMs = nowMs();

            // Convert compact entities to snapshot entities and process
            std::vector<SnapshotEntity> snapshotEntities;
            snapshotEntities.reserve(outEntities.size());
            for (const auto& ce : outEntities)
                snapshotEntities.push_back(snapshotEntityFromCompact(ce));

            // printf("[CLIENT CHUNK SNAPSHOT] tick=%u chunks=%d entities=%zu snapshotsReceived=%llu\n",
            //        chunk.header.tick, chunk.chunkCount, snapshotEntities.size(),
            //        (unsigned long long)ctx.snapshotsReceived);

            processSnapshotEntities(ctx, snapshotEntities.data(),
                                    (uint32_t)snapshotEntities.size(),
                                    chunk.header.tick, dt, "chunk",
                                    snapshotGeneration);

            ctx.snapshotChunkBuffers.erase(chunk.header.tick);
        }
        else if (header->type == PACKET_PROJECTILE_SPAWN_EVENT &&
                 bytes >= (int)sizeof(ProjectileSpawnEventPacket))
        {
            mpProcessProjectileSpawnEventPacket(
                ctx, reinterpret_cast<const ProjectileSpawnEventPacket*>(buffer));
        }
        else if (header->type == PACKET_PROJECTILE_STATE_EVENT &&
                 bytes >= (int)sizeof(ProjectileStateEventPacket))
        {
            mpProcessProjectileStateEventPacket(
                ctx, reinterpret_cast<const ProjectileStateEventPacket*>(buffer));
        }
        else if (header->type == PACKET_PROJECTILE_EXPLODE_EVENT &&
                 bytes >= (int)sizeof(ProjectileExplodeEventPacket))
        {
            mpProcessProjectileExplodeEventPacket(
                ctx, reinterpret_cast<const ProjectileExplodeEventPacket*>(buffer));
        }
        else if (header->type == PACKET_PROJECTILE_DESPAWN_EVENT &&
                 bytes >= (int)sizeof(ProjectileDespawnEventPacket))
        {
            mpProcessProjectileDespawnEventPacket(
                ctx, reinterpret_cast<const ProjectileDespawnEventPacket*>(buffer));
        }
        else if (header->type == PACKET_AVATAR_MANIFEST &&
                 bytes >= (int)sizeof(AvatarManifestChunkPacket))
        {
            mpProcessAvatarManifestPacket(
                ctx, *reinterpret_cast<const AvatarManifestChunkPacket*>(buffer), bytes);
        }
        else if (header->type == PACKET_AVATAR_ASSET_CHUNK &&
                 bytes >= (int)sizeof(AvatarAssetChunkPacket))
        {
            mpProcessAvatarAssetChunkPacket(
                ctx, *reinterpret_cast<const AvatarAssetChunkPacket*>(buffer), bytes);
        }
        else if (header->type == PACKET_ATTACK_RESULT &&
                 bytes >= (int)sizeof(AttackResultPacket))
        {
            const AttackResultPacket* ar = reinterpret_cast<const AttackResultPacket*>(buffer);
            mpProcessAttackResultPacket(ctx, ar);
        }
        else if (header->type == PACKET_PROJECTILE_FIRE_RESULT &&
                 bytes >= (int)sizeof(ProjectileFireResultPacket))
        {
            mpProcessProjectileFireResultPacket(
                ctx, reinterpret_cast<const ProjectileFireResultPacket*>(buffer));
        }
        else if (header->type == PACKET_PELLET_BLAST_EVENT &&
                 bytes >= (int)sizeof(PelletBlastEventPacket))
        {
            mpProcessPelletBlastEventPacket(
                ctx, reinterpret_cast<const PelletBlastEventPacket*>(buffer));
        }
        else if (header->type == PACKET_MELEE_HIT_EVENT &&
                 bytes >= (int)sizeof(MeleeHitEventPacket))
        {
            mpProcessMeleeHitEventPacket(
                ctx, reinterpret_cast<const MeleeHitEventPacket*>(buffer));
        }
        else if (header->type == PACKET_DAMAGE_CONFIRMED_EVENT &&
                 bytes >= (int)sizeof(DamageConfirmedEventPacket))
        {
            mpProcessDamageConfirmedEventPacket(
                ctx, reinterpret_cast<const DamageConfirmedEventPacket*>(buffer));
        }
        else if (header->type == PACKET_DUEL_STATE &&
                 bytes >= (int)sizeof(DuelStatePacket))
        {
            const DuelStatePacket* duel = reinterpret_cast<const DuelStatePacket*>(buffer);
            Debug::log(Debug::Category::Duel,
                "[DuelPacketRecv] type=DuelStatePacket reliable=1 duel=%u mapVersion=%u anchorVersion=%u state=%u phase=%u score=%d-%d players=%u/%u\n",
                duel->duelId, duel->mapVersion, duel->spawnAnchorVersion, duel->stateVersion,
                (unsigned)duel->phase, duel->scoreA, duel->scoreB,
                duel->playerAId, duel->playerBId);
            if (!mpAcceptReliableEventOnce(ctx, duel->eventId, duel->eventSessionId))
                return;
            // All community match modes (FFA, TDM, Bomb Tag, and any future mode)
            // go to CommunityMatchClient. Only the original 1v1 duel goes to DuelQueue.
            if (std::string(duel->matchMode) == "duel")
                DuelQueue::instance().onDuelState(*duel);
            else
                CommunityMatchClient::instance().onState(*duel);
        }
        else if (header->type == PACKET_BOMB_TAG_STATE &&
                 bytes >= (int)sizeof(BombTagStatePacket))
        {
            const BombTagStatePacket* bt = reinterpret_cast<const BombTagStatePacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, bt->eventId, bt->eventSessionId))
                return;
            CommunityMatchClient::instance().onBombTagState(*bt);
        }
        else if (header->type == PACKET_DUEL_ENEMY_SPAWN &&
                 bytes >= (int)sizeof(DuelEnemySpawnPacket))
        {
            const DuelEnemySpawnPacket* enemy = reinterpret_cast<const DuelEnemySpawnPacket*>(buffer);
            Debug::log(Debug::Category::Duel,
                "[DuelPacketRecv] type=DuelEnemySpawnPacket reliable=1 duel=%u mapVersion=%u anchorVersion=%u respawn=%u enemy=%u pos=(%.3f,%.3f,%.3f)\n",
                enemy->duelId, enemy->mapVersion, enemy->spawnAnchorVersion, enemy->respawnSequence,
                enemy->enemyPlayerId, enemy->posX, enemy->posY, enemy->posZ);
            if (!mpAcceptReliableEventOnce(ctx, enemy->eventId, enemy->eventSessionId))
                return;
            DuelQueue::instance().onDuelEnemySpawn(
                *enemy);
        }
        else if (header->type == PACKET_MAP_CHANGE &&
                 bytes >= (int)sizeof(MapChangePacket))
        {
            const MapChangePacket* mc = reinterpret_cast<const MapChangePacket*>(buffer);
            Debug::log(Debug::Category::Duel,
                "[DuelPacketRecv] type=MapChangePacket reliable=1 duel=%u mapVersion=%u map=%s\n",
                mc->duelId, mc->mapVersion, mc->mapId);
            if (!mpAcceptReliableEventOnce(ctx, mc->eventId, mc->eventSessionId))
                return;
            Debug::log(Debug::Category::Networking, "[NET MAP CHANGE] new=%s\n", mc->mapId);
            // Re-arm the existing map-sync (engine-tick-net) so it loads the
            // new map and re-sends CLIENT_MAP_READY with the fresh id.
            ctx.requiredMapId = mc->mapId;
            ctx.clientMapReadySent = false;
            ctx.clientMapReadySentForMap.clear();
            ctx.clientMapReadySentForPlayerId = 0;
            ctx.mapLoadAttempts = 0;
            ctx.lastMapLoadAttemptMs = 0;
            DuelQueue::instance().onMapChange(mc->mapId, mc->duelId, mc->mapVersion);
        }
        else if (header->type == PACKET_CODE_GENERATION &&
                 bytes >= (int)sizeof(CodeGenerationPacket))
        {
            const CodeGenerationPacket* announce =
                reinterpret_cast<const CodeGenerationPacket*>(buffer);
            if (announce->direction == 1)
            {
                ctx.serverCodeGeneration = announce->generation;
                ctx.serverCodeHash = announce->codeHash;
                ctx.serverCodeSwitchTick = announce->switchTick;
                ctx.serverCodePhase = announce->phase;
                ctx.serverLogicalHash = announce->logicalCodeHash;
                ctx.serverPlatformHash = announce->platformPackageHash;
                ctx.serverHotAbiVersion = announce->hotAbiVersion;
                // Late-join bootstrap: the server advertised the generation its
                // authoritative world is ALREADY running. Enter bootstrap (never a
                // coordinated switch) so we become locally ACTIVE on it before
                // participating. The acquisition path below fetches the artifact.
                if (announce->phase == CODE_GENERATION_PHASE_ACTIVE_BOOTSTRAP)
                    ctx.generationBootstrap.onServerActiveChanged(
                        (std::uint64_t)announce->generation);
                // Coordinated switch: hold our candidate until the shared tick,
                // mapped from the authoritative SERVER tick domain into our local
                // simulation tick domain (delta-based; the counters are not
                // assumed equal). Never activate a candidate we have not
                // validated: ignore SWITCH for an unready generation.
                if (announce->phase == 2 && announce->switchTick != 0 &&
                    (HotReloadSystem::instance().candidateReady() ||
                     MimitaRuntime::ArtifactCache::instance().contains(
                         announce->platformPackageHash)))
                {
                    const std::uint32_t localBoundary =
                        MimitaRuntime::mapServerSwitchTickToClientLocal(
                            announce->header.tick, ctx.clientSimulationTick,
                            announce->switchTick);
                    HotReloadSystem::instance().requestSwitchAtTick(localBoundary);
                }
                // Distributed artifact acquisition: request the artifact by
                // content hash when we do not already have it cached. Acquire
                // never activates; activation happens only at the switch tick.
                if (announce->platformPackageHash != 0 &&
                    !MimitaRuntime::ArtifactCache::instance().contains(
                        announce->platformPackageHash) &&
                    gRequestedArtifactHash != announce->platformPackageHash)
                {
                    gRequestedArtifactHash = announce->platformPackageHash;
                    MimitaNet::ArtifactRequestPacket req{};
                    req.header.type = MimitaNet::PACKET_ARTIFACT_REQUEST;
                    req.header.tick = ctx.clientSimulationTick;
                    req.header.playerId = ctx.localPlayerId;
                    req.logicalGenerationId = announce->logicalCodeHash;
                    req.platformArtifactHash = announce->platformPackageHash;
                    mpSendPacket(ctx, &req, sizeof(req));
                }
            }
        }
        else if (header->type == MimitaNet::PACKET_GENERATION_MANIFEST &&
                 bytes >= (int)sizeof(MimitaNet::GenerationManifestPacket))
        {
            const MimitaNet::GenerationManifestPacket* mp =
                reinterpret_cast<const MimitaNet::GenerationManifestPacket*>(buffer);
            // Never trust the wire: bounded arrays only, explicit version.
            const bool countsOk =
                mp->manifestVersion == MimitaNet::GENERATION_MANIFEST_VERSION &&
                mp->requiredCapabilityCount <=
                    MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS &&
                mp->requiredSchemaCount <=
                    MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS &&
                mp->requiredDependencyCount <=
                    MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS;
            // Supersede safety: bind a manifest only to the generation the server
            // most recently announced. A late manifest for a superseded G cannot
            // validate H.
            if (countsOk &&
                (uint32_t)mp->logicalGenerationId == ctx.serverCodeGeneration)
            {
                ctx.pendingManifest = *mp;
                ctx.pendingManifestGeneration = (uint32_t)mp->logicalGenerationId;
                ctx.pendingManifestValid = true;
                ctx.pendingVerifyFailure = 0;
                if (ctx.generationBootstrap.active())
                    ctx.generationBootstrap.onMetadata(
                        mp->logicalGenerationId, mp->platformArtifactHash,
                        MimitaRuntime::ArtifactCache::instance().contains(
                            mp->platformArtifactHash));
            }
        }
        else if (header->type == MimitaNet::PACKET_CONTENT_ARTIFACT &&
                 bytes >= (int)sizeof(MimitaNet::ContentArtifactPacket))
        {
            const MimitaNet::ContentArtifactPacket* d =
                reinterpret_cast<const MimitaNet::ContentArtifactPacket*>(buffer);
            MimitaRuntime::ContentArtifactV1 art{};
            art.logicalResourceId = d->logicalResourceId;
            art.resourceKind = d->resourceKind;
            art.contentHash = d->contentHash;
            art.byteSize = d->byteSize;
            if (MimitaRuntime::ResourceRegistry::instance().announceCandidate(art))
            {
                if (MimitaRuntime::ArtifactCache::instance().contains(art.contentHash))
                {
                    // Cache hit: validate/publish without any payload transfer.
                    std::string resourceError;
                    MimitaRuntime::publishContentArtifactFromCache(
                        art.logicalResourceId, resourceError);
                }
                else if (gRequestedArtifactHash != art.contentHash)
                {
                    gRequestedArtifactHash = art.contentHash;
                    MimitaNet::ArtifactRequestPacket req{};
                    req.header.type = MimitaNet::PACKET_ARTIFACT_REQUEST;
                    req.header.tick = ctx.clientSimulationTick;
                    req.header.playerId = ctx.localPlayerId;
                    req.logicalGenerationId = art.logicalResourceId;
                    req.platformArtifactHash = art.contentHash;
                    mpSendPacket(ctx, &req, sizeof(req));
                }
            }
        }
        else if (header->type == MimitaNet::PACKET_ARTIFACT_BEGIN &&
                 bytes >= (int)sizeof(MimitaNet::ArtifactBeginPacket))
        {
            const MimitaNet::ArtifactBeginPacket* begin =
                reinterpret_cast<const MimitaNet::ArtifactBeginPacket*>(buffer);
            if (begin->platformArtifactHash != 0 &&
                begin->platformArtifactHash == gRequestedArtifactHash)
                gGenerationArtifactReceiver.begin(*begin);
        }
        else if (header->type == MimitaNet::PACKET_ARTIFACT_CHUNK &&
                 bytes >= (int)sizeof(MimitaNet::ArtifactChunkPacket))
        {
            const MimitaNet::ArtifactChunkPacket* chunk =
                reinterpret_cast<const MimitaNet::ArtifactChunkPacket*>(buffer);
            if (chunk->platformArtifactHash == gRequestedArtifactHash &&
                gGenerationArtifactReceiver.onChunk(*chunk) &&
                gGenerationArtifactReceiver.complete())
            {
                // Reconstruct + verify + store (immutable). Still inactive.
                std::string artifactError;
                const bool verified = gGenerationArtifactReceiver.commit(artifactError);
                // Content artifact: completed bytes for a logical resource route
                // to the resource registry (validate + atomic publish), not the
                // code loader.
                const std::uint64_t contentLogical =
                    MimitaRuntime::ResourceRegistry::instance()
                        .pendingLogicalIdForHash(gGenerationArtifactReceiver.hash());
                if (verified && contentLogical != 0)
                {
                    std::string resourceError;
                    MimitaRuntime::publishContentArtifactFromCache(
                        contentLogical, resourceError);
                }
                // Install the verified bytes as a REAL inactive candidate through
                // the same loader path as a local build (no remote-only loader).
                bool artifactInstalled = false;
                if (verified && contentLogical == 0)
                {
                    std::vector<unsigned char> cachedBytes;
                    if (MimitaRuntime::ArtifactCache::instance().read(
                            gGenerationArtifactReceiver.hash(), cachedBytes))
                    {
                        std::string installError;
                        artifactInstalled =
                            HotReloadSystem::instance().installCandidateArtifact(
                                cachedBytes,
                                (std::uint32_t)
                                    gGenerationArtifactReceiver.logicalGenerationId(),
                                gGenerationArtifactReceiver.hash(), installError);
                        if (!artifactInstalled)
                            Debug::logThrottled(
                                Debug::Category::General, "artifact-install", 2.0f,
                                "remote artifact install failed: %s",
                                installError.c_str());
                    }
                }
                // Hash validity alone is NOT compatibility. Verify the manifest the
                // SERVER associated with this exact generation against REAL local
                // peer facts (kernel ABI, capability registry, schema registry,
                // dependencies) before READY. No manifest => no READY.
                MimitaRuntime::GenerationManifestV1 manifest{};
                bool haveManifest = false;
                if (ctx.pendingManifestValid &&
                    ctx.pendingManifestGeneration ==
                        (uint32_t)gGenerationArtifactReceiver.logicalGenerationId())
                {
                    const MimitaNet::GenerationManifestPacket& mp = ctx.pendingManifest;
                    manifest.logicalGenerationId = mp.logicalGenerationId;
                    manifest.logicalBehaviorHash = mp.logicalBehaviorHash;
                    manifest.platformArtifactHash = mp.platformArtifactHash;
                    manifest.platformArtifactSize = mp.platformArtifactSize;
                    manifest.hotAbiVersion = mp.hotAbiVersion;
                    manifest.requiredCapabilityCount = mp.requiredCapabilityCount;
                    manifest.requiredSchemaCount = mp.requiredSchemaCount;
                    manifest.requiredDependencyCount = mp.requiredDependencyCount;
                    for (uint32_t i = 0;
                         i < mp.requiredCapabilityCount &&
                         i < MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS; ++i)
                        manifest.requiredCapabilities[i] = mp.requiredCapabilities[i];
                    for (uint32_t i = 0;
                         i < mp.requiredSchemaCount &&
                         i < MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS; ++i)
                        manifest.requiredSchemas[i] = mp.requiredSchemas[i];
                    for (uint32_t i = 0;
                         i < mp.requiredDependencyCount &&
                         i < MimitaNet::GENERATION_MANIFEST_MAX_REQUIREMENTS; ++i)
                        manifest.requiredDependencies[i] = mp.requiredDependencies[i];
                    haveManifest = true;
                }
                MimitaRuntime::GenerationLocalFactsV1 facts{};
                facts.artifactHash = gGenerationArtifactReceiver.hash();
                facts.artifactSize = gGenerationArtifactReceiver.totalSize();
                facts.coldAbiVersion = (uint32_t)MIMITA_GAME_API_VERSION;
                facts.hasCapability = [](void*, uint64_t id) {
                    return MimitaRuntime::GenericRuntime::instance().hasCapability(id);
                };
                facts.hasSchema = [](void*, uint64_t id) {
                    return MimitaRuntime::DynamicComponentStore::instance().schema(id) !=
                        nullptr;
                };
                facts.hasDependency = [](void*, uint64_t) { return true; };
                const MimitaRuntime::VerifyFailure vf = haveManifest
                    ? MimitaRuntime::verifyGeneration(manifest, facts)
                    : MimitaRuntime::VerifyFailure::HashMismatch;
                ctx.pendingVerifyFailure = (uint32_t)vf;
                // READY also means "I can safely transition the current active
                // generation F to G". Prepare (never mutate) against live state.
                MimitaRuntime::MigrationPrepareFactsV1 mfacts{};
                mfacts.candidateSchemaCount = haveManifest
                    ? manifest.requiredSchemaCount : 0;
                for (uint32_t i = 0;
                     i < mfacts.candidateSchemaCount &&
                     i < MimitaRuntime::kMaxVerifyRequirements; ++i)
                {
                    mfacts.candidateSchemaIds[i] = manifest.requiredSchemas[i];
                    mfacts.candidateSchemaVersions[i] =
                        manifest.requiredSchemaVersions[i];
                }
                mfacts.storedSchemaVersion = [](void*, std::uint64_t id) {
                    return MimitaRuntime::DynamicComponentStore::instance()
                        .maxStoredVersion(id);
                };
                mfacts.hasMigrationPath =
                    [](void*, std::uint64_t id, std::uint32_t from,
                       std::uint32_t to) {
                        return MimitaRuntime::DynamicComponentStore::instance()
                            .hasMigration(id, from, to);
                    };
                const std::uint64_t activeGeneration =
                    (std::uint64_t)HotReloadSystem::instance()
                        .status().activeGeneration;
                const MimitaRuntime::MigrationPlanV1 migrationPlan =
                    MimitaRuntime::prepareMigration(
                        activeGeneration,
                        haveManifest ? manifest.logicalGenerationId : 0, mfacts);
                ctx.pendingMigrationPrepared =
                    migrationPlan.outcome != MimitaRuntime::MigrationOutcome::Failed;
                ctx.pendingMigrationFailure = (uint32_t)migrationPlan.failure;
                // Bind the prepared plan to the exact candidate so the switch
                // transaction can validate F -> G identity before activation.
                HotReloadSystem::instance().setCandidateMigrationPlan(migrationPlan);
                // Late-join bootstrap: this artifact is the ACTIVE generation we
                // must become locally running. Complete bootstrap (verify + load);
                // do not send READY (that means "prepared for a FUTURE switch").
                if (ctx.generationBootstrap.active())
                {
                    const std::uint64_t bootstrapGen =
                        gGenerationArtifactReceiver.logicalGenerationId();
                    ctx.generationBootstrap.onArtifactAcquired(bootstrapGen);
                    ctx.generationBootstrap.complete(
                        bootstrapGen,
                        haveManifest && artifactInstalled &&
                            HotReloadSystem::instance().hasInstalledCandidate(),
                        manifest, facts);
                    // Activate the installed candidate at the next safe tick so
                    // this peer becomes locally ACTIVE on the server's generation.
                    if (ctx.generationBootstrap.state ==
                        MimitaRuntime::BootstrapState::Ready)
                        HotReloadSystem::instance().requestSwitchAtTick(
                            ctx.clientSimulationTick);
                }
                else if (verified && artifactInstalled &&
                    vf == MimitaRuntime::VerifyFailure::None &&
                    migrationPlan.outcome != MimitaRuntime::MigrationOutcome::Failed)
                {
                    // Report READY for the exact logical generation.
                    CodeGenerationPacket ready{};
                    ready.header.type = PACKET_CODE_GENERATION;
                    ready.header.tick = ctx.clientSimulationTick;
                    ready.header.playerId = ctx.localPlayerId;
                    ready.generation =
                        (uint32_t)gGenerationArtifactReceiver.logicalGenerationId();
                    ready.direction = 0;  // client -> server
                    ready.phase = 1;      // READY
                    ready.platformPackageHash = gGenerationArtifactReceiver.hash();
                    ready.hotAbiVersion = (uint32_t)MIMITA_GAME_API_VERSION;
                    mpSendPacket(ctx, &ready, sizeof(ready));
                }
                gRequestedArtifactHash = 0;
            }
        }
        else if (header->type == PACKET_RAGDOLL_STATE &&
                 bytes >= (int)sizeof(RagdollStatePacket))
        {
            const RagdollStatePacket* state =
                reinterpret_cast<const RagdollStatePacket*>(buffer);
            Ragdoll::Snapshot snapshot;
            snapshot.ownerActorId = state->ownerActorId;
            const std::uint32_t count = std::min(
                (std::uint32_t)state->limbCount, (std::uint32_t)Ragdoll::kMaxSnapshotLimbs);
            snapshot.limbCount = count;
            snapshot.tick = state->sourceTick;
            for (std::uint32_t i = 0; i < count; ++i) {
                snapshot.limbs[i].limbIndex = state->limbs[i].limbIndex;
                for (int k = 0; k < 3; ++k)
                    snapshot.limbs[i].position[k] = state->limbs[i].position[k];
                for (int k = 0; k < 4; ++k)
                    snapshot.limbs[i].rotation[k] = state->limbs[i].rotation[k];
            }
            for (int h = 0; h < 2; ++h) {
                const RagdollGrabStatePacket& in = state->grabs[h];
                Ragdoll::GrabSnapshot& gs = snapshot.grabs[h];
                gs.active = in.active;
                gs.hand = in.hand;
                gs.targetLimb = in.targetLimb;
                gs.strength = in.strength;
                for (int k = 0; k < 3; ++k) {
                    gs.anchor[k] = in.anchor[k];
                    gs.handLocal[k] = in.handLocal[k];
                }
            }
            Ragdoll::RagdollEntities::instance().applySnapshot(snapshot);
            Ragdoll::RagdollPresentation::instance().pushFrame(
                state->ownerActorId, snapshot);
        }
        else if (header->type == PACKET_CORPSE_SPAWN &&
                 bytes >= (int)sizeof(CorpseSpawnPacket))
        {
            const CorpseSpawnPacket* spawn =
                reinterpret_cast<const CorpseSpawnPacket*>(buffer);
            // Reliable-event dedup: a retransmitted corpse spawn must not refresh
            // the death identity repeatedly.
            if (spawn->deathEventId != 0 &&
                !mpAcceptReliableEventOnce(ctx, spawn->deathEventId,
                                           ctx.reliableEventSessionId))
                return;
            RagdollModeSystem::instance().noteNetworkDeath(
                spawn->ownerActorId, spawn->deathTick, spawn->deathEventId);
        }
        else if (header->type == PACKET_CONSTRAINT_CREATE &&
                 bytes >= (int)sizeof(ConstraintCreatePacket))
        {
            const ConstraintCreatePacket* cc =
                reinterpret_cast<const ConstraintCreatePacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, cc->eventId, cc->eventSessionId))
                return;
            Physics::ConstraintComponent component;
            MimitaNet::decodeConstraint(cc->constraint, component);
            Physics::ConstraintStore::instance().create(EntityRealm::Server, component);
            ctx.confirmedConstraints.insert(component.constraintSerial);
        }
        else if (header->type == PACKET_CONSTRAINT_RELEASE &&
                 bytes >= (int)sizeof(ConstraintReleasePacket))
        {
            const ConstraintReleasePacket* cr =
                reinterpret_cast<const ConstraintReleasePacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, cr->eventId, cr->eventSessionId))
                return;
            Physics::ConstraintStore::instance().release(
                cr->constraintSerial, cr->releaseTick, cr->reason);
            ctx.confirmedConstraints.erase(cr->constraintSerial);
        }
        else if (header->type == PACKET_CONSTRAINT_SNAPSHOT &&
                 bytes >= (int)sizeof(ConstraintSnapshotPacket))
        {
            const ConstraintSnapshotPacket* cs =
                reinterpret_cast<const ConstraintSnapshotPacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, cs->eventId, cs->eventSessionId))
                return;
            const std::uint16_t count = std::min(
                (std::uint16_t)cs->constraintCount, (std::uint16_t)MAX_ACTIVE_CONSTRAINTS);
            for (std::uint16_t i = 0; i < count; ++i) {
                Physics::ConstraintComponent component;
                MimitaNet::decodeConstraint(cs->constraints[i], component);
                Physics::ConstraintStore::instance().create(EntityRealm::Server, component);
                ctx.confirmedConstraints.insert(component.constraintSerial);
            }
        }
        else if (header->type == PACKET_KILL_EVENT &&
                 bytes >= (int)sizeof(KillEventPacket))
        {
            const KillEventPacket* kill = reinterpret_cast<const KillEventPacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, kill->eventId, kill->eventSessionId))
                return;
            const std::string killerName(kill->killerName,
                strnlen(kill->killerName, sizeof(kill->killerName)));
            const std::string victimName(kill->victimName,
                strnlen(kill->victimName, sizeof(kill->victimName)));
            std::string weaponDisplay(kill->weaponDisplay,
                strnlen(kill->weaponDisplay, sizeof(kill->weaponDisplay)));
            if (weaponDisplay.empty()) weaponDisplay = "unknown";
            const uint64_t eventKey =
                ((uint64_t)kill->eventSessionId << 32) | kill->eventId;
            KillfeedManager::instance().onKill(
                killerName, victimName, weaponDisplay, false,
                kill->serverTick, eventKey);
            Debug::log(Debug::Category::Networking,
                "[KILL EVENT RX] killer=%s kind=%u victim=%s kind=%u weapon=%s tick=%u event=%u\n",
                killerName.c_str(), (unsigned)kill->killerEntityType,
                victimName.c_str(), (unsigned)kill->victimEntityType,
                weaponDisplay.c_str(), kill->serverTick, kill->eventId);
        }
        else if (header->type == PACKET_PROGRESSION_EVENT &&
                 bytes >= (int)sizeof(ProgressionEventPacket))
        {
            const auto* event = reinterpret_cast<const ProgressionEventPacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, event->eventId, event->eventSessionId)) return;
            if (event->kind <= 4)
                RewardPopupSystem::instance().pushProgression(event->kind,
                    std::string(event->name, strnlen(event->name, sizeof(event->name))),
                    std::string(event->confirmedAt, strnlen(event->confirmedAt, sizeof(event->confirmedAt))));
        }
        else if (header->type == PACKET_DYNAMIC_COMPONENT &&
                 bytes >= (int)(sizeof(PacketHeader) + 16))
        {
            const std::uint32_t eventId = *reinterpret_cast<const std::uint32_t*>(
                buffer + sizeof(PacketHeader));
            const std::uint32_t eventSession = *reinterpret_cast<const std::uint32_t*>(
                buffer + sizeof(PacketHeader) + 4);
            if (!mpAcceptReliableEventOnce(ctx, eventId, eventSession)) return;
            std::vector<MimitaNet::DynamicComponentRecord> records;
            std::vector<MimitaNet::RelationshipRecord> relationships;
            std::vector<MimitaNet::EntityLifecycleRecord> lifecycles;
            std::string error;
            if (MimitaNet::dynamicReplicationDecode(
                    reinterpret_cast<const std::uint8_t*>(buffer),
                    static_cast<std::size_t>(bytes), records, relationships,
                    lifecycles, error)) {
                MimitaNet::dynamicReplicationApply(
                    records, relationships, lifecycles,
                    MimitaRuntime::DynamicComponentStore::instance(),
                    MimitaRuntime::RelationshipStore::instance(), error);
            }
        }
        else if (header->type == PACKET_SERVER_NOTIFICATION &&
                 bytes >= (int)sizeof(ServerNotificationPacket))
        {
            const ServerNotificationPacket* notification =
                reinterpret_cast<const ServerNotificationPacket*>(buffer);
            if (!mpAcceptReliableEventOnce(ctx, notification->eventId,
                                           notification->eventSessionId))
                return;
            NotificationSystem::instance().push(
                notification->title, notification->message,
                notification->durationTicks, {});
            Debug::log(Debug::Category::Networking,
                "[SERVER NOTIFICATION] title=%s message=%s\n",
                notification->title, notification->message);
        }
        else if (header->type == PACKET_RELOAD_RESULT &&
                 bytes >= (int)sizeof(ReloadResultPacket))
        {
            const ReloadResultPacket* rr = reinterpret_cast<const ReloadResultPacket*>(buffer);
            Debug::log(Debug::Category::Weapons, "[RELOAD RESULT RX] playerId=%u requestId=%u accepted=%d reason=%d ammo=%d/%d stateRev=%u\n",
                       ctx.localPlayerId, rr->requestId, (int)rr->accepted, (int)rr->reason,
                       rr->magazineAmmo, rr->reserveAmmo, rr->stateRevision);

            // Store for processing outside mpTick where Player is in scope
            ctx.pendingReloadResults.push_back(*rr);
            if (ctx.pendingReloadResults.size() > 64)
                ctx.pendingReloadResults.erase(ctx.pendingReloadResults.begin());
        }
        else if (header->type == PACKET_NPC_DAMAGE_EVENT &&
                 bytes >= (int)sizeof(NpcDamageEventPacket))
        {
            mpProcessNpcDamageEventPacket(ctx, reinterpret_cast<const NpcDamageEventPacket*>(buffer));
        }
        else if (header->type == PACKET_PLAYER_RESPAWNED &&
                 bytes >= (int)sizeof(PlayerRespawnedPacket))
        {
            const PlayerRespawnedPacket* pr = reinterpret_cast<const PlayerRespawnedPacket*>(buffer);
            Debug::log(Debug::Category::Duel,
                "[DuelPacketRecv] type=PlayerRespawnedPacket reliable=1 player=%u event=%u session=%u spawnGeneration=%u epoch=%u pos=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f)\n",
                ctx.localPlayerId, pr->eventId, pr->eventSessionId,
                pr->spawnGeneration, pr->transformEpoch, pr->posX, pr->posY, pr->posZ,
                pr->velX, pr->velY, pr->velZ);

            // Reliable-event dedup + auto-ack: the server now delivers the
            // spawn sync through the reliable-event transport (like shot
            // events), so a retransmitted copy is dropped here and ACKed.
            if (!mpAcceptReliableEventOnce(ctx, pr->eventId, pr->eventSessionId))
            {
                Debug::log(Debug::Category::Weapons,
                           "[SPAWN RESPAWN DUP EVENT] playerId=%u spawnGen=%u epoch=%u eventId=%u\n",
                           ctx.localPlayerId, pr->spawnGeneration, pr->transformEpoch,
                           pr->eventId);
                return;
            }

            // Reject older spawn generation
            if (pr->spawnGeneration < ctx.lastKnownSpawnGeneration)
            {
                Debug::warn(Debug::Category::Duel,
                    "[DuelStale] type=PlayerRespawnedPacket spawnGeneration=%u current=%u action=ignored\n",
                    pr->spawnGeneration, ctx.lastKnownSpawnGeneration);
                Debug::log(Debug::Category::Weapons, "[SPAWN RESPAWN REJECT] old spawnGen=%u < %u\n",
                           pr->spawnGeneration, ctx.lastKnownSpawnGeneration);
                return;
            }

            // Duplicate spawn sync for the generation already applied (a
            // snapshot already advanced the generation). Re-queue for weapon
            // reconcile + ack so the server can still reach Active, but do NOT
            // re-apply the spawn and do NOT re-disable gameplay.
            if (pr->spawnGeneration == ctx.lastKnownSpawnGeneration)
            {
                if (ctx.active && ctx.localPlayerId)
                {
                    ctx.pendingAuthoritativeSpawn = *pr;
                    // Note: do NOT advance ctx.transformEpoch here. The snapshot
                    // processing advances it when the new-epoch snapshot actually
                    // arrives (right before the local player snaps to it). Advancing
                    // it early made the input gate send the stale pre-teleport
                    // position stamped with the new epoch, which the server accepted
                    // and overwrote the duel spawn anchor.
                    Debug::log(Debug::Category::Weapons,
                               "[SPAWN RESPAWN DUPLICATE] playerId=%u spawnGen=%u epoch=%u — re-queued, gameplay kept %d\n",
                               ctx.localPlayerId, pr->spawnGeneration, pr->transformEpoch,
                               (int)ctx.gameplayActive);
                }
                return;
            }

            applyAuthoritativeSpawn(ctx, pr);

            if (ctx.active && ctx.localPlayerId)
            {
                // Store for weapon reconciliation outside mpTick where Player is in scope.
                // SpawnAck is sent after weapon runtime reconciliation in engineTickNet.
                ctx.pendingAuthoritativeSpawn = *pr;
                ctx.gameplayActive = false;  // waiting for SpawnActivated
                Debug::log(Debug::Category::Weapons, "[SPAWN RESPAWN QUEUE] playerId=%u spawnGen=%u epoch=%u weapons=%u\n",
                           ctx.localPlayerId, pr->spawnGeneration, pr->transformEpoch, pr->weaponCount);
            }
        }
        else if (header->type == PACKET_SPAWN_ACTIVATED &&
                 bytes >= (int)sizeof(SpawnActivatedPacket))
        {
            const SpawnActivatedPacket* act = reinterpret_cast<const SpawnActivatedPacket*>(buffer);
            if (act->spawnGeneration == ctx.lastKnownSpawnGeneration)
            {
                // Server confirmed our SpawnAck — stop retrying it.
                if (ctx.pendingSpawnAckGeneration == act->spawnGeneration)
                {
                    ctx.pendingSpawnAckGeneration = 0;
                    ctx.pendingSpawnAckEpoch = 0;
                    ctx.pendingSpawnAckLastSendMs = 0;
                }
                ctx.gameplayActive = true;
                Debug::log(Debug::Category::Weapons, "[SPAWN ACTIVATED RX] playerId=%u spawnGen=%u epoch=%u — gameplay enabled\n",
                           ctx.localPlayerId, act->spawnGeneration, act->transformEpoch);
            }
            else
            {
                // Visible diagnostic: a stale/delayed activation is silently
                // skipped. If this repeats, gameplay stays disabled and all
                // attacks are blocked.
                Debug::logThrottled(Debug::Category::Weapons, "spawn-activated-skip", 0.5f,
                                    "[SPAWN ACTIVATED SKIPPED] playerId=%u recvGen=%u currentGen=%u\n",
                                    ctx.localPlayerId, act->spawnGeneration,
                                    ctx.lastKnownSpawnGeneration);
            }
        }
        else if (header->type == PACKET_CHAT_MESSAGE &&
                 bytes >= (int)sizeof(ChatPacket))
        {
            mpProcessChatPacket(ctx, reinterpret_cast<const ChatPacket*>(buffer));
        }
        else if (header->type == PACKET_CHAT_MESSAGE_EVENT &&
                 bytes >= (int)sizeof(ChatMessageEventPacket))
        {
            const ChatMessageEventPacket* ev =
                reinterpret_cast<const ChatMessageEventPacket*>(buffer);

            if (ev->requestId != 0)
                ctx.pendingChatRequests.erase(ev->requestId);

            // ACK before message-id deduplication. A duplicate reliable event
            // can arrive after the first ACK was lost; it must be ACKed again
            // even though its chat text must not be rendered twice.
            if (!mpAcceptReliableEventOnce(ctx, ev->eventId, ev->eventSessionId))
                return;

            if (ev->messageId != 0 &&
                !ctx.processedChatMessageIds.insert(ev->messageId).second)
                return;

            noteChatActivity();

            ChatHistoryEntry entry;
            entry.messageId = ev->messageId;
            entry.serverTick = ev->serverTick;
            entry.utcUnixMilliseconds = ev->utcUnixMilliseconds;
            entry.senderEntityId = ev->senderEntityId;
            entry.senderAccountId = ev->senderAccountId;
            entry.senderType = static_cast<ChatSenderType>(ev->senderType);
            entry.senderName = ev->senderName;
            entry.text = ev->utf8Message;
            entry.channel = ev->channel;
            entry.muted = false;
            auto vipIt = ctx.playerRegistry.find(ev->senderEntityId);
            if (vipIt != ctx.playerRegistry.end())
            {
                entry.senderVipAppearance = vipIt->second.vipAppearance;
                entry.senderVipStyleDetail = vipIt->second.vipStyleDetail;
            }

            Debug::log(Debug::Category::Chat,
                       "[CHAT TRACE 2 BEFORE HISTORY GATE] gpChatHistory=%p messageId=%llu text=\"%s\"\n",
                       (void*)gpChatHistory, (unsigned long long)ev->messageId,
                       ev->utf8Message);
            if (gpChatHistory)
            {
                gChatHistory.append(entry);
                Debug::log(Debug::Category::Chat,
                           "[CHAT DEBUG HISTORY ADD] utc=%s messageId=%llu text=\"%s\" sender=%s senderEntityId=%u senderAccountId=%u serverTick=%llu eventUtcMs=%lld count=%zu server=%s room=%s session=%s\n",
                           MiMitaTime::utcIso8601Seconds().c_str(), (unsigned long long)ev->messageId,
                           ev->utf8Message, ev->senderName, ev->senderEntityId,
                           ev->senderAccountId, (unsigned long long)ev->serverTick,
                           (long long)ev->utcUnixMilliseconds, gChatHistory.size(),
                           ctx.serverAddress.c_str(), ctx.roomCode.c_str(),
                           ctx.sessionId.c_str());
            }
            else
            {
                Debug::warn(Debug::Category::Chat,
                            "[CHAT TRACE 2 PROBLEM] gpChatHistory is NULL; received message was NOT added to 2D history messageId=%llu\n",
                            (unsigned long long)ev->messageId);
            }
            Debug::log(Debug::Category::Chat,
                       "[CHAT TRACE 2 AFTER HISTORY GATE] gpChatHistory=%p messageId=%llu\n",
                       (void*)gpChatHistory,
                       (unsigned long long)ev->messageId);

            Debug::log(Debug::Category::Chat,
                       "[CHAT DEBUG RECEIVED] utc=%s text=\"%s\" messageId=%llu sender=%s senderType=%u senderEntityId=%u senderAccountId=%u serverTick=%llu eventUtcMs=%lld bytes=%zu server=%s room=%s session=%s\n",
                        MiMitaTime::utcIso8601Seconds().c_str(), ev->utf8Message,
                       (unsigned long long)ev->messageId, ev->senderName,
                       (unsigned)ev->senderType, ev->senderEntityId,
                       ev->senderAccountId, (unsigned long long)ev->serverTick,
                       (long long)ev->utcUnixMilliseconds,
                       std::strlen(ev->utf8Message), ctx.serverAddress.c_str(),
                       ctx.roomCode.c_str(), ctx.sessionId.c_str());

            // Also add to 3D chat bubble for the sender
            if (ev->senderType == (uint8_t)ChatSenderType::Player)
            {
                bool found = false;
                if (ev->senderEntityId == ctx.localPlayerId)
                {
                    addChatMessage(THE_PLAYER.chatState, ev->utf8Message,
                                   ev->senderName);
                    found = true;
                    Debug::log(Debug::Category::Chat,
                               "[CHAT BUBBLE ASSIGNED] playerId=%u name=%s target=local\n",
                               ev->senderEntityId, ev->senderName);
                }
                if (!found)
                for (auto& kv : ctx.remotePlayers)
                {
                    if (kv.first == ev->senderEntityId ||
                        kv.second.username == ev->senderName)
                    {
                        addChatMessage(kv.second.chatState, ev->utf8Message, ev->senderName);
                        found = true;
                        Debug::log(Debug::Category::Chat,
                                   "[CHAT BUBBLE ASSIGNED] playerId=%u name=%s target=remote\n",
                                   ev->senderEntityId, ev->senderName);
                        break;
                    }
                }
                if (!found)
                {
                    Debug::log(Debug::Category::Chat,
                               "[CHAT BUBBLE LOOKUP FAILED] playerId=%u name=%s\n",
                               ev->senderEntityId, ev->senderName);
                }
                playChatSound((int)std::strlen(ev->utf8Message));

                ReplayEffectEvent chatEvent;
                chatEvent.type = "chat";
                chatEvent.sourceActorId = ev->senderName;
                chatEvent.assetId = ev->utf8Message;
                chatEvent.lifetime = computeChatDuration((int)std::strlen(ev->utf8Message));
                captureReplayEffect(chatEvent);
            }

            Debug::log(Debug::Category::Chat, "[CHAT V2 RECV] messageId=%llu sender=%s tick=%llu\n",
                       (unsigned long long)ev->messageId, ev->senderName,
                       (unsigned long long)ev->serverTick);
        }
        else if (header->type == PACKET_CHAT_TYPING_STATE_EVENT &&
                 bytes >= (int)sizeof(ChatTypingStateEventPacket))
        {
            const ChatTypingStateEventPacket* ev =
                reinterpret_cast<const ChatTypingStateEventPacket*>(buffer);

            auto it = ctx.remotePlayers.find(ev->playerId);
            if (it != ctx.remotePlayers.end())
            {
                it->second.isTyping = ev->isTyping;
                if (ev->isTyping)
                    it->second.typingStartedMs = MimitaNet::nowMs();
            }
        }
        else if (header->type == PACKET_VIP_STYLE_EVENT &&
                 bytes >= (int)sizeof(VipStyleEventPacket))
        {
            const VipStyleEventPacket* ev =
                reinterpret_cast<const VipStyleEventPacket*>(buffer);
            const MimitaVip::VipStyleDetail detail = MimitaVip::styleDetailFromWire(
                ev->styleKind, ev->animation, ev->direction, ev->rainbowSpeed,
                reinterpret_cast<const uint8_t*>(ev->colors), ev->colorCount,
                ev->styleEpoch);

            auto it = ctx.playerRegistry.find(ev->playerId);
            if (it != ctx.playerRegistry.end() &&
                ev->styleEpoch >= it->second.vipStyleEpoch)
            {
                it->second.vipStyleDetail = detail;
                it->second.vipStyleEpoch = ev->styleEpoch;
            }
            else if (it == ctx.playerRegistry.end())
            {
                ctx.pendingVipStyles[ev->playerId] = detail;
            }
            auto rp = ctx.remotePlayers.find(ev->playerId);
            if (rp != ctx.remotePlayers.end())
                rp->second.vipStyleDetail = detail;
            if (ev->playerId == ctx.localPlayerId && gpPlayer)
                gpPlayer->vipStyleDetail = detail;

            Debug::warn(Debug::Category::Vip,
                "[VIP STYLE RX] player=%u epoch=%u kind=%u colors=%u anim=%u\n",
                ev->playerId, ev->styleEpoch, (int)detail.styleKind,
                (int)detail.colorCount(), (int)detail.animation);
        }
        else if (header->type == PACKET_PLAYER_CONNECTION_STATE &&
                 bytes >= (int)sizeof(PlayerConnectionStatePacket))
        {
            // Peer connection-state notice: drive the red reconnect effect on
            // a frozen body (disconnect) or the green effect on recovery.
            const PlayerConnectionStatePacket* pc =
                reinterpret_cast<const PlayerConnectionStatePacket*>(buffer);
            if (pc->header.playerId != ctx.localPlayerId)
            {
                if (pc->connected)
                    mpNoteRemotePlayerReconnected(ctx, pc->header.playerId);
                else
                    mpNoteRemotePlayerDisconnected(ctx, pc->header.playerId,
                                                   pc->disconnectedAtMs);
            }
        }
        else if (header->type == PACKET_GODBALL_STATE &&
                 bytes >= (int)sizeof(GodballStatePacket))
        {
            const GodballStatePacket* gb =
                reinterpret_cast<const GodballStatePacket*>(buffer);
            if (gb->ownerPlayerId != ctx.localPlayerId)
            {
                auto it = ctx.remotePlayers.find(gb->ownerPlayerId);
                if (it != ctx.remotePlayers.end())
                {
                    it->second.godballActive = gb->active != 0;
                    it->second.godballPosition = {gb->posX, gb->posY, gb->posZ};
                    it->second.godballVelocity = {gb->velX, gb->velY, gb->velZ};
                }
            }
        }
        else if (header->type == PACKET_SERVER_COMMAND_RESULT &&
                 bytes >= (int)sizeof(ServerCommandResultPacket))
        {
            // Host-command ack from the server (applied or rejected).
            const ServerCommandResultPacket* res =
                reinterpret_cast<const ServerCommandResultPacket*>(buffer);
            const std::string status(res->statusText,
                                     strnlen(res->statusText, sizeof(res->statusText)));
            if (res->accepted)
            {
                Debug::warn(Debug::Category::Networking,
                    "[HOST COMMAND APPLIED] %s\n", status.c_str());
                NotificationSystem::instance().pushCritical(
                    "Host command applied", status, 0);
            }
            else
            {
                Debug::warn(Debug::Category::Networking,
                    "[HOST COMMAND REJECTED] %s\n", status.c_str());
                NotificationSystem::instance().pushCritical(
                    "Host command rejected", status, 0);
            }
        }
        else if (header->type == PACKET_PING &&
                 bytes >= (int)sizeof(PingPacket))
        {
            const PingPacket* ping =
                reinterpret_cast<const PingPacket*>(buffer);
            ctx.localPingMs = (int)std::min<uint64_t>(
                9999, nowMs() - ping->clientTimeMs);
        }
    };

    // ── Poll ICE transport (if available) ──
    if (ctx.transport)
    {
        std::vector<ReceivedPacket> pkts;
        ctx.transport->poll(pkts);
        badconn::processIncoming(pkts);
        for (const ReceivedPacket& rp : pkts)
        {
            if (rp.bytes.size() < (int)sizeof(PacketHeader))
                continue;
            if (rp.bytes.size() > sizeof(buffer))
                continue;
            memcpy(buffer, rp.bytes.data(), rp.bytes.size());
            int packetBytes = (int)rp.bytes.size();
            processPacket(packetBytes);
        }
    }

    else
    {
        // ── Raw UDP recv loop ──
        std::vector<ReceivedPacket> rawPkts;
        for (;;)
        {
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int bytes = recvfrom(ctx.sock, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
            {
                int wsaErr = WSAGetLastError();
                if (wsaErr == WSAEWOULDBLOCK)
                    break;
                if (wsaErr == WSAEINVAL)
                {
                    Debug::warn(Debug::Category::Networking,
                           "[NET RX SOCKET BUG] recvfrom WSAEINVAL sock=%d "
                           "state=%s connected=%d active=%d\n",
                           (int)ctx.sock, connectionStateName(ctx.connectionState),
                           (int)ctx.connected, (int)ctx.active);
                    break;
                }
                printf("[NET RX ERROR] recvfrom failed error=%d\n", wsaErr);
                break;
            }
            ++ctx.packetsReceived;
            if (!isSameAddress(from, ctx.serverAddr))
            {
                printf("[NET PACKET FILTER] accepted=0 reason=not-server from=%s\n",
                       addressToString(from).c_str());
                continue;
            }
            ReceivedPacket rp;
            rp.bytes.assign(buffer, buffer + bytes);
            rp.receivedAtMs = nowMs();
            rawPkts.push_back(std::move(rp));
        }
        badconn::processIncoming(rawPkts);
        for (const ReceivedPacket& rp : rawPkts)
        {
            if (rp.bytes.size() < (int)sizeof(PacketHeader))
                continue;
            if (rp.bytes.size() > sizeof(buffer))
                continue;
            memcpy(buffer, rp.bytes.data(), rp.bytes.size());
            processPacket((int)rp.bytes.size());
        }
    }

    // Retry chat requests until the server's reliable event confirms them.
    {
        const uint64_t now = nowMs();
        const uint64_t retryMs = (uint64_t)NetworkingConfig::instance().data().reliableEvents.retryMs;
        const uint64_t timeoutMs = (uint64_t)NetworkingConfig::instance().data().reliableEvents.ttlMs;
        for (auto it = ctx.pendingChatRequests.begin(); it != ctx.pendingChatRequests.end(); )
        {
            auto& pending = it->second;
            if (now - pending.firstSentMs > timeoutMs)
            {
                Debug::warn(Debug::Category::Chat,
                            "[CHAT REQUEST TIMEOUT] requestId=%u attempts=%d\n",
                            pending.requestId, pending.attempts);
                it = ctx.pendingChatRequests.erase(it);
                continue;
            }
            if (now - pending.lastSentMs >= retryMs)
            {
                ChatRequestPacket retry{};
                retry.header.type = PACKET_CHAT_REQUEST;
                retry.header.tick = ctx.tick;
                retry.header.playerId = ctx.localPlayerId;
                retry.requestId = pending.requestId;
                retry.clientSimulationTick = ctx.tick;
                std::strncpy(retry.utf8Message, pending.message.c_str(),
                             sizeof(retry.utf8Message) - 1);
                mpSendPacket(ctx, &retry, sizeof(retry));
                pending.lastSentMs = now;
                ++pending.attempts;
            }
            ++it;
        }
    }

    // ── SpawnAck retry (reliable respawn handshake) ───────────────────
    // The initial SpawnAck is sent from engineTickNet after weapon reconcile
    // (it stamps pendingSpawnAckLastSendMs). If the server never confirms
    // (dropped ack or dropped SpawnActivated), re-send the ack until the
    // matching SpawnActivated arrives so the server cannot stay wedged in
    // AwaitingSpawnAck with the player frozen at the spawn point.
    if (ctx.connected && ctx.localPlayerId && ctx.pendingSpawnAckGeneration != 0)
    {
        const uint64_t nowSpawnAck = nowMs();
        if (nowSpawnAck >= ctx.pendingSpawnAckLastSendMs &&
            nowSpawnAck - ctx.pendingSpawnAckLastSendMs >= 100)
        {
            SpawnAckPacket retry{};
            retry.header.type = PACKET_SPAWN_ACK;
            retry.header.tick = ctx.tick;
            retry.header.playerId = ctx.localPlayerId;
            retry.spawnGeneration = ctx.pendingSpawnAckGeneration;
            retry.transformEpoch = ctx.pendingSpawnAckEpoch;
            mpSendPacket(ctx, &retry, sizeof(retry));
            ctx.pendingSpawnAckLastSendMs = nowSpawnAck;
            Debug::log(Debug::Category::Weapons,
                       "[SPAWN ACK RETRY] playerId=%u spawnGen=%u epoch=%u\n",
                       ctx.localPlayerId, ctx.pendingSpawnAckGeneration,
                       ctx.pendingSpawnAckEpoch);
        }
    }

    const double inputIntervalMs = 1000.0 /
        NetworkingConfig::instance().data().runtimeRates.inputSendRateHz;
    const bool inputDue =
        ctx.lastInputSentMs == 0 ||
        (double)(currentMs - ctx.lastInputSentMs) >= inputIntervalMs;
    // Bootstrap completes once the local active generation matches the server's:
    // clear the phase so normal simulation resumes.
    if (ctx.generationBootstrap.state == MimitaRuntime::BootstrapState::Ready &&
        mpGenerationWorldAllowed(ctx))
        ctx.generationBootstrap = MimitaRuntime::GenerationBootstrapV1{};
    if (ctx.connected && ctx.localPlayerId && input && inputDue &&
        mpGenerationWorldAllowed(ctx))
    {
        InputPacket in{};
        in.header.type = PACKET_INPUT;
        in.header.tick = ctx.tick;
        in.header.playerId = ctx.localPlayerId;
        in.header.transformEpoch = ctx.transformEpoch;

        // Rate-limited log around respawn epoch
        {
            static uint64_t lastInputEpochLogMs = 0;
            uint64_t nowInputLog = nowMs();
            if (nowInputLog - lastInputEpochLogMs >= 1000)
            {
                printf("[CLIENT INPUT EPOCH] player=%u packetEpoch=%u serverEpoch=%u lastAppliedEpoch=%u "
                       "pos=(%.2f,%.2f,%.2f) dead=%d\n",
                       ctx.localPlayerId, ctx.transformEpoch,
                       (uint32_t)ctx.localServerEpoch, (uint32_t)ctx.lastAppliedEpoch,
                       input->position.x, input->position.y, input->position.z,
                       (int)(input->position.z < -10.0f ? 1 : 0));
                lastInputEpochLogMs = nowInputLog;
            }
        }

        // Build state flags from MpInput — rebuilt from zero every frame
        uint16_t stateFlags = 0;
        const bool walking =
            std::abs(input->wishX) > 0.001f ||
            std::abs(input->wishY) > 0.001f;
        if (walking) stateFlags |= NET_STATE_WALKING;
        if (input->jumpHeld) stateFlags |= NET_STATE_JUMPING;
        if (input->dashPressed) stateFlags |= NET_STATE_DASHING;
        if (input->downDashPressed) stateFlags |= NET_STATE_DOWN_DASHING;
        if (input->freezeHeld) stateFlags |= NET_STATE_FREEZING;
        if (input->attackPressed) stateFlags |= NET_STATE_ATTACKING;

        // Rate-limited walk send logging
        {
            static uint64_t lastWalkSendLogMs = 0;
            uint64_t nowWalk = nowMs();
            if (nowWalk - lastWalkSendLogMs >= 1000)
            {
                printf("[WALK CLIENT SEND] playerId=%u tick=%u wish=(%.2f,%.2f) "
                       "walking=%d stateFlags=0x%04x walkingBit=%d velocity=(%.2f,%.2f,%.2f)\n",
                       ctx.localPlayerId, ctx.tick,
                       input->wishX, input->wishY,
                       (int)walking, (unsigned)stateFlags,
                       (int)((stateFlags & NET_STATE_WALKING) != 0),
                       input->velocity.x, input->velocity.y, input->velocity.z);
                lastWalkSendLogMs = nowWalk;
            }
        }

        in.wishX = input->wishX;
        in.wishY = input->wishY;
        in.camForwardX = input->camForward.x;
        in.camForwardY = input->camForward.y;
        in.camForwardZ = input->camForward.z;
        in.yaw = input->yaw;
        in.lookPitch = input->lookPitch;

        // ── Client authoritative-transform gate ────────────────────────
        // If we have received a new server epoch but haven't applied it
        // locally yet, send the server's authoritative position instead of
        // our stale local position.  This prevents old local transforms from
        // overwriting the server spawn, respawn, or teleport position.
        const bool authoritativeTransformApplied =
            ctx.hasLocalServerPosition &&
            ctx.localServerEpoch != 0 &&
            ctx.lastAppliedEpoch == ctx.localServerEpoch &&
            ctx.transformEpoch == ctx.localServerEpoch &&
            ctx.localPlayerReconciled;

        if (!authoritativeTransformApplied && ctx.hasLocalServerPosition)
        {
            in.clientPx = ctx.localServerPosition.x;
            in.clientPy = ctx.localServerPosition.y;
            in.clientPz = ctx.localServerPosition.z;
            in.clientVx = ctx.localServerVelocity.x;
            in.clientVy = ctx.localServerVelocity.y;
            in.clientVz = ctx.localServerVelocity.z;
            in.externalImpulseX = 0.0f;
            in.externalImpulseY = 0.0f;
            in.externalImpulseZ = 0.0f;
        }
        else
        {
            in.clientPx = input->position.x;
            in.clientPy = input->position.y;
            in.clientPz = input->position.z;
            in.clientVx = input->velocity.x;
            in.clientVy = input->velocity.y;
            in.clientVz = input->velocity.z;
            in.externalImpulseX = input->externalImpulse.x;
            in.externalImpulseY = input->externalImpulse.y;
            in.externalImpulseZ = input->externalImpulse.z;
        }

        {
            static uint64_t lastTransformGateLogMs = 0;
            uint64_t nowGate = nowMs();
            if (nowGate - lastTransformGateLogMs >= 1000)
            {
                printf("[CLIENT TRANSFORM GATE] playerId=%u "
                       "usingAuthoritative=%d packetEpoch=%u serverEpoch=%u "
                       "lastAppliedEpoch=%u reconciled=%d hasServerPos=%d "
                       "inputPos=(%.2f,%.2f,%.2f) outgoingPos=(%.2f,%.2f,%.2f)\n",
                       ctx.localPlayerId,
                       (int)(!authoritativeTransformApplied && ctx.hasLocalServerPosition),
                       ctx.transformEpoch, (uint32_t)ctx.localServerEpoch,
                       (uint32_t)ctx.lastAppliedEpoch, (int)ctx.localPlayerReconciled,
                       (int)ctx.hasLocalServerPosition,
                       input->position.x, input->position.y, input->position.z,
                       ctx.hasLocalServerPosition
                           ? (in.clientPx == ctx.localServerPosition.x
                               ? ctx.localServerPosition.x : input->position.x)
                           : input->position.x,
                       ctx.hasLocalServerPosition
                           ? (in.clientPy == ctx.localServerPosition.y
                               ? ctx.localServerPosition.y : input->position.y)
                           : input->position.y,
                       ctx.hasLocalServerPosition
                           ? (in.clientPz == ctx.localServerPosition.z
                               ? ctx.localServerPosition.z : input->position.z)
                           : input->position.z);
                lastTransformGateLogMs = nowGate;
            }
        }

        in.equippedSlot = (int16_t)input->equippedSlot;
        in.weaponState = input->weaponState;
        in.clientPingMs = ctx.localPingMs;
        in.stateFlags = stateFlags;
        in.movementSequence = ctx.nextMovementSequence++;
        if (ctx.nextMovementSequence == 0)
            ctx.nextMovementSequence = 1;
        in.inputCommandSequence = ctx.nextInputCommandSequence++;
        if (ctx.nextInputCommandSequence == 0)
            ctx.nextInputCommandSequence = 1;
        in.clientSimulationTick = input->movementSimulationTick != 0
            ? input->movementSimulationTick
            : ctx.clientSimulationTick;
        in.spawnGeneration = ctx.lastKnownSpawnGeneration;
        in.transformEpoch = ctx.transformEpoch;
        in.movementFlags = movementReportFlagsFromMpInput(*input);
        in.dashSerial = ctx.nextLocalDashSerial;
        in.groundJumpSerial = ctx.nextLocalGroundJumpSerial;
        in.airJumpSerial = ctx.nextLocalAirJumpSerial;
        in.downDashSerial = ctx.nextLocalDownDashSerial;
        in.directionChangeSerial = ctx.nextLocalMovementDirectionSerial;
        in.equipSerial = ctx.nextLocalEquipSerial;
        in.freezeSerial = ctx.nextLocalFreezeSerial;
        // Send pending respawn serial repeatedly until confirmed.
        // The pending serial is set in engineTickNet when the user requests
        // an instant respawn, and cleared in mpReconcileLocalPlayer after
        // the server's authoritative new-life snapshot is applied.
        in.respawnSerial = ctx.pendingRespawnSerial;

        // Rate-limited log for pending respawn
        if (ctx.pendingRespawnSerial != 0)
        {
            uint64_t nowRespawnLog = nowMs();
            if (nowRespawnLog - ctx.pendingRespawnLastSendLogMs >= 250)
            {
                printf("[CLIENT RESPAWN SEND] playerId=%u serial=%u packetEpoch=%u "
                       "pendingForMs=%llu playerDead=%d serverHealth=%d\n",
                       ctx.localPlayerId, ctx.pendingRespawnSerial,
                       ctx.transformEpoch,
                       (unsigned long long)(nowRespawnLog - ctx.pendingRespawnStartedMs),
                       (int)(input->position.z < -10.0f ? 1 : 0),
                       ctx.localServerHealth);
                ctx.pendingRespawnLastSendLogMs = nowRespawnLog;
            }
        }
        in.attackPressed = input->attackPressed ? 1 : 0;
        in.sizeScale = input->sizeScale;

        // ── Redundant movement commands (badconn loss resilience) ───────
        // Re-send the last two commands so a lost input packet still delivers
        // its movement command in the next packet. Server dedups by sequence.
        if (ctx.recentInputCommands.size() >= 2)
        {
            in.redundancy[0] =
                ctx.recentInputCommands[ctx.recentInputCommands.size() - 1];
            in.redundancy[1] =
                ctx.recentInputCommands[ctx.recentInputCommands.size() - 2];
        }
        {
            InputCommandRedundancySlot cur;
            cur.inputCommandSequence = in.inputCommandSequence;
            cur.clientSimulationTick = in.clientSimulationTick;
            cur.wishX = in.wishX;
            cur.wishY = in.wishY;
            cur.camForwardX = in.camForwardX;
            cur.camForwardY = in.camForwardY;
            cur.camForwardZ = in.camForwardZ;
            cur.yaw = in.yaw;
            cur.lookPitch = in.lookPitch;
            cur.stateFlags = in.stateFlags;
            cur.spawnGeneration = in.spawnGeneration;
            cur.transformEpoch = in.transformEpoch;
            ctx.recentInputCommands.push_back(cur);
            while (ctx.recentInputCommands.size() > 3)
                ctx.recentInputCommands.erase(ctx.recentInputCommands.begin());
        }

        mpSendPacket(ctx, &in, sizeof(in));
        ctx.lastInputSentMs = currentMs;

        if (input->godballActive)
        {
            GodballStatePacket gb{};
            gb.header.type = PACKET_GODBALL_STATE;
            gb.header.tick = ctx.tick;
            gb.header.playerId = ctx.localPlayerId;
            gb.ownerPlayerId = ctx.localPlayerId;
            gb.posX = input->godballPosition.x;
            gb.posY = input->godballPosition.y;
            gb.posZ = input->godballPosition.z;
            gb.velX = input->godballVelocity.x;
            gb.velY = input->godballVelocity.y;
            gb.velZ = input->godballVelocity.z;
            gb.active = 1;
            mpSendPacket(ctx, &gb, sizeof(gb));
        }

    }

    // ── Retry unacknowledged generic attack requests ────────────────────
    {
        const uint64_t now = nowMs();
        for (auto it = ctx.pendingAttackRequests.begin(); it != ctx.pendingAttackRequests.end(); )
        {
            MultiplayerContext::PendingAttackRequest& p = it->second;
            if (p.accepted || p.rejected)
            {
                it = ctx.pendingAttackRequests.erase(it);
                continue;
            }
            const auto& retryCfg = NetworkingConfig::instance().data().retries;
            if (now - p.lastSentMs >=
                    (uint64_t)retryCfg.attackRetryIntervalMs &&
                p.attempts < (int)retryCfg.attackRetryMaxAttempts)
            {
                AttackRequestPacket retry{};
                retry.header.type = PACKET_ATTACK_REQUEST;
                retry.header.tick = ctx.tick;
                retry.header.playerId = ctx.localPlayerId;
                retry.requestId = p.requestId;
                retry.spawnGeneration = p.spawnGeneration;
                retry.clientSimulationTick = p.clientSimulationTick;
                retry.basedOnInputSequence = p.basedOnInputSequence;
                retry.equippedSlot = p.equippedSlot;
                retry.weaponDefNetworkId = p.weaponDefNetworkId;
                retry.aimOriginX = p.aimOrigin.x;
                retry.aimOriginY = p.aimOrigin.y;
                retry.aimOriginZ = p.aimOrigin.z;
                retry.aimDirX = p.aimDirection.x;
                retry.aimDirY = p.aimDirection.y;
                retry.aimDirZ = p.aimDirection.z;
                retry.muzzlePosX = p.predictedMuzzle.x;
                retry.muzzlePosY = p.predictedMuzzle.y;
                retry.muzzlePosZ = p.predictedMuzzle.z;
                retry.deterministicSeed = p.deterministicSeed;
                retry.attackVariant = p.attackVariant;
                retry.claimedTargetId = p.claimedTargetId;
                retry.claimedHitX = p.claimedHit.x;
                retry.claimedHitY = p.claimedHit.y;
                retry.claimedHitZ = p.claimedHit.z;
                retry.claimedBodyPart = p.claimedBodyPart;
                mpSendPacket(ctx, &retry, sizeof(retry));
                p.lastSentMs = now;
                p.attempts++;
                Debug::log(Debug::Category::Weapons, "[ATTACK RETRY] requestId=%u attempt=%d\n",
                           p.requestId, p.attempts);
            }
            if (now - p.firstSentMs >
                (uint64_t)retryCfg.attackRequestTimeoutMs)
            {
                Debug::log(Debug::Category::Weapons, "[ATTACK TIMEOUT] requestId=%u — removing\n",
                           p.requestId);
                mpCancelPredictedProjectileAttack(ctx, p.requestId);
                it = ctx.pendingAttackRequests.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // ── Retry unacknowledged reload requests (lost-packet resilience) ──
    // Same timing as attack retries. The server dedups by requestId and
    // re-sends its cached result, so a retry can never double-start a reload.
    {
        const uint64_t now = nowMs();
        const auto& retryCfg = NetworkingConfig::instance().data().retries;
        for (auto it = ctx.pendingReloadRequests.begin();
             it != ctx.pendingReloadRequests.end(); )
        {
            MultiplayerContext::PendingReloadRequest& p = it->second;
            if (now - p.lastSentMs >= (uint64_t)retryCfg.attackRetryIntervalMs &&
                p.attempts < (int)retryCfg.attackRetryMaxAttempts)
            {
                ReloadRequestPacket retry{};
                retry.header.type = PACKET_RELOAD_REQUEST;
                retry.header.tick = ctx.tick;
                retry.header.playerId = ctx.localPlayerId;
                retry.requestId = p.requestId;
                retry.spawnGeneration = p.spawnGeneration;
                retry.weaponDefNetworkId = p.weaponDefNetworkId;
                retry.magazineAmmo = p.magazineAmmo;
                retry.reserveAmmo = p.reserveAmmo;
                mpSendPacket(ctx, &retry, sizeof(retry));
                p.lastSentMs = now;
                p.attempts++;
                Debug::log(Debug::Category::Weapons,
                           "[RELOAD RETRY] requestId=%u weaponNetId=%u attempt=%d pending=%zu\n",
                           p.requestId, p.weaponDefNetworkId, p.attempts,
                           ctx.pendingReloadRequests.size());
            }
            if (now - p.firstSentMs >
                (uint64_t)retryCfg.attackRequestTimeoutMs)
            {
                Debug::log(Debug::Category::Weapons,
                           "[RELOAD TIMEOUT] requestId=%u — removing\n", p.requestId);
                it = ctx.pendingReloadRequests.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    mpUpdateRemoteEntities(ctx, dt);
    mpAvatarNetworkTick(ctx);
    mpReleaseTimelineEvents(ctx);
    mpUpdateRemoteSwordStates(ctx, dt);
    mpUpdateNetworkProjectiles(ctx, dt, world);

    if (ctx.connected &&
        currentMs - ctx.lastPingSentMs >=
            (uint64_t)NetworkingConfig::instance().data()
                .runtimeRates.pingIntervalMs)
    {
        PingPacket ping{};
        ping.header.type = PACKET_PING;
        ping.header.tick = ctx.tick;
        ping.header.playerId = ctx.localPlayerId;
        ping.clientTimeMs = currentMs;
        mpSendPacket(ctx, &ping, sizeof(ping));
        ctx.lastPingSentMs = currentMs;
    }

    ++ctx.tick;
}

} // namespace MimitaNet
