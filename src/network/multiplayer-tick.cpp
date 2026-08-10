// 07 21 2026, 17 10
/* purpose
* Owns client multiplayer tick IO, packet receive dispatch, snapshot processing, and input sends.
* Converts local shared movement state into Stage 3A client movement reports.
* Keeps local and remote snapshot lifecycle filtering consistent across legacy and chunked snapshots.
* Does NOT define server validation policy, render interpolation math, or packet binary layouts.
* Does NOT own physics simulation, weapon runtime reconciliation internals, or server authority.
* Does NOT apply stale local snapshots before lifecycle checks pass.
*/

#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "duel/duel-queue.h"
#include "network/snapshot-chunks.h"
#include "network/remote-entity-lifecycle.h"
#include "network/badconn/badconn.h"
#include "network/reconnect-visuals.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "config/networking-config.h"
#include "auth/auth-system.h"
#include "website/api-client.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "gui/hud/chat-history.h"
#include "gui/hud/chat-bubble.h"
#include "world/world.h"
#include "entities/player.h"
#include "notifications/notifications.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace MimitaNet {

namespace {

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
static void processSnapshotEntities(
    MultiplayerContext& ctx,
    const SnapshotEntity* entities,
    uint32_t entityCount,
    uint32_t serverTick,
    float dt,
    const char* sourceName)
{
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
                ctx.awaitingExplodeDeath = false;
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
                printf("[CLIENT SNAPSHOT] %s tick=%u local pos=(%.2f,%.2f,%.2f) hp=%d epoch=%u\n",
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
        if (isNew)
        {
            // Server NPCs use the default body so they don't clone the local
            // player's avatar; only real player replicas inherit the avatar.
            if (entity.entityType != ENTITY_NPC)
            {
                if (GetPlayerSettings().avatarName.empty()) {
                    AvatarSystem::applySingleTexture(p, GetPlayerSettings().outfitPath);
                } else {
                    AvatarSystem::instance().applyToPlayer(p);
                }
            }
            else
            {
                p.username = entity.displayName;
                // Server NPCs render + collide using the real default player
                // body (populates physicalBody.parts), so what the client sees
                // IS the hitbox the beam checks — instant predicted feedback
                // on actual body parts (head/torso/arms/legs).
                p.loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
            }
            interpolation.renderRegistered = true;
            printf("[CLIENT ENTITY CREATE] entityId=%u type=%s ownerClientId=%u "
                   "mesh=%s position=(%.2f,%.2f,%.2f)\n",
                   entity.networkEntityId, typeName, entity.ownerClientId,
                   p.modelLoaded ? "player-glb" : "fallback-capsule",
                   entity.px, entity.py, entity.pz);
        }

        if (!pushInterpolationTarget(interpolation, entity, serverTick))
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
            printf("[CLIENT ENTITY] entityId=%u type=%s ownerId=%u isLocal=0 existsBefore=%d "
                   "createdReplica=%d renderRegistered=%d position=(%.2f,%.2f,%.2f) rot=%.2f name=%s\n",
                   entity.networkEntityId, typeName, entity.ownerClientId,
                   (int)existsBefore, (int)isNew, (int)interpolation.renderRegistered,
                   entity.px, entity.py, entity.pz, entity.yaw,
                   entity.displayName);
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
    std::memset(join.name, 0, sizeof(join.name));
    std::strncpy(join.name, playerName.c_str(), sizeof(join.name) - 1);
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
    uint32_t oldGen = ctx.lastKnownSpawnGeneration;
    ctx.lastKnownSpawnGeneration = spawn->spawnGeneration;
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
    ctx.pendingFireRequests.clear();
    ctx.pendingReloadRequests.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();

    Debug::log(Debug::Category::Weapons, "[SPAWN SYNC APPLY] oldGen=%u newGen=%u health=%d weapons=%u\n",
               oldGen, spawn->spawnGeneration, spawn->health, spawn->weaponCount);
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input, const World& world)
{
    // ── Async ICE connect job ───────────────────────────────────────────
    // The ICE connect runs on a background thread; poll it every frame so the
    // game never blocks. Progress messages surface on the HUD via
    // ctx.connectionStatus. On success the finished transport is installed
    // here (main thread) and the normal handshake continues below.
    {
        IceConnectStatus connect = mpIceConnectPoll();
        if (connect.active)
        {
            ctx.connectionStatus = connect.message;
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
    if (!ctx.connected && !ctx.connectFailed &&
        currentMs - ctx.connectStartMs > connectTimeoutMs)
    {
        ctx.connectionStatus = "Connection timed out";
        printf("[NET CONNECT] timeout server=%s\n", ctx.serverAddress.c_str());
        NotificationSystem::instance().pushCritical(
            "Connection timed out",
            "Server status: Connection timed out — " + ctx.serverAddress, 0);
        teardownPreviousSession(ctx, DisconnectPolicy::ConnectionFailure);
    }

    // ── Connection state machine ───────────────────────────────────────
    if (ctx.connectionState == ConnectionState::Connecting ||
        ctx.connectionState == ConnectionState::WaitJoinAccept)
    {
        if (currentMs - ctx.lastHelloMs > 500)
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

        printf("[NET RX] type=%d seq=%u bytes=%d\n",
               header->type, header->tick, bytes);

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
            ctx.approvedLocalName = accept->approvedName;
            ctx.reconnectToken = accept->reconnectToken;
            ctx.reliableEventSessionId = accept->reliableEventSessionId;
            ctx.requiredMapId = accept->mapId;
            ctx.transformEpoch = accept->header.transformEpoch;
            ctx.nextMovementSequence = 1;
            ctx.clientMapReadySent = false;
            ctx.clientMapReadySentForMap.clear();
            ctx.clientMapReadySentForPlayerId = 0;
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
            ctx.connectionStatus = "Join rejected";
            printf("[NET CONNECT] join rejected reason=%u\n", reject->reason);
            teardownPreviousSession(ctx, DisconnectPolicy::Rejected);
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
            if (!reassembleSnapshotChunks(sorted, outEntities))
            {
                ctx.snapshotChunkBuffers.erase(chunk.header.tick);
                return;
            }

            // Track missed snapshots
            if (ctx.lastSnapshotTick != 0 &&
                chunk.header.tick > ctx.lastSnapshotTick + 1)
            {
                ctx.snapshotsMissed += chunk.header.tick - ctx.lastSnapshotTick - 1;
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
                                    chunk.header.tick, dt, "chunk");

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
            DuelQueue::instance().onDuelState(
                *reinterpret_cast<const DuelStatePacket*>(buffer));
        }
        else if (header->type == PACKET_DUEL_ENEMY_SPAWN &&
                 bytes >= (int)sizeof(DuelEnemySpawnPacket))
        {
            DuelQueue::instance().onDuelEnemySpawn(
                *reinterpret_cast<const DuelEnemySpawnPacket*>(buffer));
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
                    ctx.transformEpoch = pr->transformEpoch;
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
                ctx.transformEpoch = pr->transformEpoch;
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

            if (gpChatHistory)
                gChatHistory.append(entry);

            // Also add to 3D chat bubble for the sender
            if (ev->senderType == (uint8_t)ChatSenderType::Player)
            {
                bool found = false;
                for (auto& kv : ctx.remotePlayers)
                {
                    if (kv.second.username == ev->senderName)
                    {
                        addChatMessage(kv.second.chatState, ev->utf8Message, ev->senderName);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    // Could be our own message or a new player
                    // Check if it matches local player
                    if (ev->header.playerId == ctx.localPlayerId)
                    {
                        // Already handled locally via requestSendChatMessage
                    }
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
            Debug::log(Debug::Category::Weapons,
                       "[GODBALL LEGACY STATE RX] ignored after physical-contact migration\n");
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
    if (ctx.connected && ctx.localPlayerId && input && inputDue)
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
            : ctx.tick;
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

    mpUpdateRemoteEntities(ctx, dt);
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
