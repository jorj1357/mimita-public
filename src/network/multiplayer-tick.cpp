#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace MimitaNet {

namespace {

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
    if (ctx.sock == INVALID_SOCKET)
    {
        printf("[NET CONNECT] sendJoinRequest skipped: invalid socket\n");
        return;
    }
    JoinRequestPacket join{};
    join.header.type = PACKET_JOIN_REQUEST;
    join.header.tick = ctx.tick;
    std::memset(join.joinToken, 0, sizeof(join.joinToken));
    std::strncpy(join.joinToken, ctx.joinToken.c_str(), sizeof(join.joinToken) - 1);
    std::memset(join.name, 0, sizeof(join.name));
    std::strncpy(join.name, playerName.c_str(), sizeof(join.name) - 1);
    int sent = sendto(ctx.sock, (const char*)&join, sizeof(join), 0,
           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    if (sent == SOCKET_ERROR)
        printf("[NET TX ERROR] sendJoinRequest sendto failed error=%d\n", WSAGetLastError());
    else
        ++ctx.packetsSent;
    printf("[NET CONNECT] join request sent to %s token=%s\n",
           ctx.serverAddress.c_str(), ctx.joinToken.c_str());
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

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input, const World& world)
{
    if (!ctx.active)
        return;
    if (ctx.sock == INVALID_SOCKET)
    {
        Debug::warn(Debug::Category::Networking,
               "[NET TICK] sock=INVALID_SOCKET state=%s connected=%d active=%d\n",
               connectionStateName(ctx.connectionState), (int)ctx.connected, (int)ctx.active);
        return;
    }

    uint64_t currentMs = nowMs();

    const uint64_t CLIENT_TIMEOUT_MS = MimitaNet::CLIENT_TIMEOUT_MS;
    if (ctx.connected && ctx.lastHeardServerMs > 0 &&
        currentMs - ctx.lastHeardServerMs > CLIENT_TIMEOUT_MS)
    {
        if (currentMs - ctx.lastDisconnectLogMs >= 1000)
        {
            printf("[NET TIMEOUT] player=%u reason=server-silent lastPacket=%llums ago\n",
                   ctx.localPlayerId,
                   (unsigned long long)(currentMs - ctx.lastHeardServerMs));
            ctx.lastDisconnectLogMs = currentMs;
        }
        printf("[NET DISCONNECT] player=%u reason=heartbeat_timeout duration=%llums\n",
               ctx.localPlayerId,
               (unsigned long long)(currentMs - ctx.lastHeardServerMs));

        // Try reconnect if we have a reconnect token
        if (!ctx.reconnectToken.empty())
        {
            ctx.connected = false;
            ctx.connectionState = ConnectionState::Reconnecting;
            mpStartReconnect(ctx);
            return;
        }

        ctx.connected = false;
        ctx.active = false;
        ctx.connectionState = ConnectionState::Disconnected;
        closesocket(ctx.sock);
        ctx.sock = INVALID_SOCKET;
        return;
    }

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

    if (ctx.fakeLagMode == 1 &&
        (ctx.fakeLagNextRandomizeMs == 0 ||
         currentMs >= ctx.fakeLagNextRandomizeMs))
    {
        const int span = std::max(0, ctx.fakeLagMaxMs - ctx.fakeLagMinMs);
        ctx.fakeLagCurrentMs = ctx.fakeLagMinMs +
            (span > 0 ? std::rand() % (span + 1) : 0);
        ctx.fakeLagNextRandomizeMs = currentMs + 1000;
        printf("[FAKELAG] mode=1 delay=%d packetQueued=%zu\n",
               ctx.fakeLagCurrentMs, ctx.outgoingQueue.size());
    }
    flushOutgoingPackets(ctx);
    if (!ctx.connected && !ctx.connectFailed && currentMs - ctx.connectStartMs > 6000)
    {
        ctx.connectFailed = true;
        ctx.connectionStatus = "Connection timed out";
        ctx.connectionState = ConnectionState::Disconnected;
        printf("[NET CONNECT] timeout server=%s\n", ctx.serverAddress.c_str());
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

    char buffer[16384];
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
                       "state=%s connected=%d active=%d iceTransport=%d\n",
                       (int)ctx.sock, connectionStateName(ctx.connectionState),
                       (int)ctx.connected, (int)ctx.active, 0);
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

        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (bytes < (int)sizeof(PacketHeader))
        {
            printf("[NET RX] rejected=packet-too-small bytes=%d\n", bytes);
            continue;
        }
        if (header->magic != PROTOCOL_MAGIC)
        {
            printf("[NET RX] rejected=bad-magic magic=0x%llx\n", (unsigned long long)header->magic);
            continue;
        }
        if (header->version != PROTOCOL_VERSION)
        {
            printf("[NET RX] rejected=bad-version version=%d expected=%d\n",
                   header->version, PROTOCOL_VERSION);
            continue;
        }

        ctx.lastHeardServerMs = nowMs();

        printf("[NET RX] type=%d seq=%u bytes=%d\n",
               header->type, header->tick, bytes);

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
            ctx.requiredMapId = welcome->mapId;
            ctx.transformEpoch = welcome->header.transformEpoch;
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
                0
            };
            printf("[NET CONNECT] player=%u serverTick=%u tickRate=%.0f mapId=%s\n",
                   ctx.localPlayerId, welcome->header.tick, welcome->tickRate,
                   welcome->mapId);
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
            ctx.requiredMapId = accept->mapId;
            ctx.transformEpoch = accept->header.transformEpoch;
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
                0
            };
            printf("[NET CONNECT] join accepted player=%u tickRate=%.0f mapId=%s\n",
                   ctx.localPlayerId, accept->tickRate, accept->mapId);
        }
        else if (header->type == PACKET_JOIN_REJECT && bytes >= (int)sizeof(JoinRejectPacket))
        {
            const JoinRejectPacket* reject = reinterpret_cast<const JoinRejectPacket*>(buffer);
            ctx.connectFailed = true;
            ctx.connectionState = ConnectionState::Disconnected;
            ctx.connectionStatus = "Join rejected";
            printf("[NET CONNECT] join rejected reason=%u\n", reject->reason);
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
            ctx.localServerPosition = {accept->restorePx, accept->restorePy, accept->restorePz};
            ctx.localServerHealth = accept->restoredHealth;
            ctx.hasLocalServerPosition = true;
            ctx.transformEpoch = accept->header.transformEpoch;
            ctx.localServerEpoch = accept->header.transformEpoch;
            ctx.lastAppliedEpoch = 0;
            ctx.localPlayerReconciled = false;
            ctx.teleportResync = false;
            ctx.awaitingTeleportAck = false;
            printf("[NET RECONNECT] accepted player=%u health=%d kills=%d deaths=%d epoch=%u\n",
                   ctx.localPlayerId, accept->restoredHealth,
                   accept->restoredKills, accept->restoredDeaths);
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
            ctx.lastSnapshotTick = snapshot->header.tick;
            ctx.latestServerTick = snapshot->header.tick;
            ctx.lastSnapshotReceivedMs = nowMs();
            uint32_t count = std::min(snapshot->entityCount, (uint32_t)MAX_SNAPSHOT_ENTITIES);
            const bool logSnapshot = snapshot->header.tick % 60 == 0;

            if (logSnapshot)
                printf("[CLIENT SNAPSHOT] entityCount=%u playerCount=%u npcCount=%u bytes=%d tick=%u\n",
                       snapshot->entityCount, snapshot->playerCount,
                       snapshot->npcCount, bytes, snapshot->header.tick);

            std::unordered_map<uint32_t, bool> seenPlayers;
            std::unordered_map<uint32_t, bool> seenNpcs;
            for (uint32_t i = 0; i < count; ++i)
            {
                const SnapshotEntity& entity = snapshot->entities[i];
                if (!entity.active || entity.networkEntityId == 0)
                {
                    printf("[CLIENT ENTITY SKIP] entityId=%u reason=inactive-or-zero-id\n",
                           entity.networkEntityId);
                    continue;
                }

                const bool isLocal =
                    entity.entityType == ENTITY_PLAYER &&
                    entity.ownerClientId == ctx.localPlayerId;
                if (isLocal)
                {
                    ctx.localServerPosition = {entity.px, entity.py, entity.pz};
                    ctx.localServerVelocity = {entity.vx, entity.vy, entity.vz};
                    ctx.localServerYaw = entity.yaw;
                    ctx.localServerOnGround = entity.onGround != 0;
                    ctx.hasLocalServerPosition = true;
                    ctx.localServerHealth = entity.health;
                    ctx.localServerEpoch = entity.transformEpoch;
                    ctx.localPingMs = entity.pingMs;

                    // Sync outgoing epoch if server incremented it (respawn/teleport)
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
                        glm::length(
                            ctx.localServerPosition -
                            ctx.pendingTeleportPosition) <= 1.0f)
                    {
                        ctx.awaitingTeleportAck = false;
                        ctx.teleportResync = true;
                        printf("[NET TELEPORT ACK] position=%.1f,%.1f,%.1f\n",
                               ctx.localServerPosition.x,
                               ctx.localServerPosition.y,
                               ctx.localServerPosition.z);
                    }
                    if (ctx.awaitingExplodeDeath && entity.health <= 0)
                        ctx.awaitingExplodeDeath = false;
                    ctx.playerRegistry[entity.networkEntityId] = {
                        entity.displayName, entity.networkEntityId, entity.pingMs
                    };
                    if (logSnapshot)
                    {
                        printf("[CLIENT ENTITY] entityId=%u type=Player ownerId=%u isLocal=1 existsBefore=1 "
                               "createdReplica=0 renderRegistered=1 position=(%.2f,%.2f,%.2f) rotation=%.2f\n",
                               entity.networkEntityId, entity.ownerClientId,
                               entity.px, entity.py, entity.pz, entity.yaw);
                        printf("[CLIENT ENTITY SKIP] entityId=%u reason=local-prediction-keeps-transform\n",
                               entity.networkEntityId);
                    }
                    continue;
                }

                std::unordered_map<uint32_t, Player>* replicas = nullptr;
                std::unordered_map<uint32_t, EntityInterpolationState>* interpolationMap = nullptr;
                std::unordered_map<uint32_t, bool>* seen = nullptr;
                const char* typeName = nullptr;
                if (entity.entityType == ENTITY_PLAYER)
                {
                    replicas = &ctx.remotePlayers;
                    interpolationMap = &ctx.remotePlayerInterpolation;
                    seen = &seenPlayers;
                    typeName = "Player";
                    ctx.playerRegistry[entity.networkEntityId] = {
                        entity.displayName, entity.networkEntityId, entity.pingMs
                    };
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
                Player& p = (*replicas)[entity.networkEntityId];
                bool isNew = !existsBefore;
                EntityInterpolationState& interpolation = (*interpolationMap)[entity.networkEntityId];
                if (isNew)
                {
                    if (GetPlayerSettings().avatarName.empty()) {
                        AvatarSystem::applySingleTexture(p, GetPlayerSettings().outfitPath);
                    } else {
                        AvatarSystem::instance().applyToPlayer(p);
                    }
                    interpolation.renderRegistered = true;
                    printf("[CLIENT ENTITY CREATE] entityId=%u type=%s ownerClientId=%u "
                           "mesh=%s position=(%.2f,%.2f,%.2f)\n",
                           entity.networkEntityId, typeName, entity.ownerClientId,
                           p.modelLoaded ? "player-glb" : "fallback-capsule",
                           entity.px, entity.py, entity.pz);
                }

                pushInterpolationTarget(interpolation, entity, snapshot->header.tick);
                if (isNew)
                    updateRenderedReplica(p, interpolation, dt);
                (*seen)[entity.networkEntityId] = true;

                if (isNew || logSnapshot)
                {
                    printf("[CLIENT ENTITY] entityId=%u type=%s ownerId=%u isLocal=0 existsBefore=%d "
                           "createdReplica=%d renderRegistered=%d position=(%.2f,%.2f,%.2f) rotation=%.2f\n",
                           entity.networkEntityId, typeName, entity.ownerClientId,
                           (int)existsBefore, (int)isNew, (int)interpolation.renderRegistered,
                           entity.px, entity.py, entity.pz, entity.yaw);
                    printf("[INTERPOLATION] entityId=%u snapshotCount=%d renderPos=(%.2f,%.2f,%.2f) "
                           "targetPos=(%.2f,%.2f,%.2f)\n",
                           entity.networkEntityId,
                           interpolation.hasPrevious && interpolation.hasTarget ? 2 : 1,
                           p.pos.x, p.pos.y, p.pos.z,
                           interpolation.target.position.x,
                           interpolation.target.position.y,
                           interpolation.target.position.z);
                }
            }

            for (auto it = ctx.remotePlayers.begin(); it != ctx.remotePlayers.end(); )
            {
                if (!seenPlayers[it->first])
                {
                    const uint32_t entityId = it->first;
                    printf("[ENTITY DESTROY] reason=missing-from-snapshot entityId=%u type=Player name=\"%s\"\n",
                           it->first, ctx.playerRegistry[it->first].name.c_str());
                    it = ctx.remotePlayers.erase(it);
                    ctx.remotePlayerInterpolation.erase(entityId);
                    ctx.playerRegistry.erase(entityId);
                }
                else
                    ++it;
            }
            for (auto it = ctx.remoteNpcs.begin(); it != ctx.remoteNpcs.end(); )
            {
                if (!seenNpcs[it->first])
                {
                    const uint32_t entityId = it->first;
                    printf("[ENTITY DESTROY] reason=missing-from-snapshot entityId=%u type=NPC name=\"%s\"\n",
                           it->first, it->second.username.c_str());
                    it = ctx.remoteNpcs.erase(it);
                    ctx.remoteNpcInterpolation.erase(entityId);
                }
                else
                    ++it;
            }
        }
        else if (header->type == PACKET_SHOT_EVENT &&
                 bytes >= (int)sizeof(ShotEventPacket))
        {
            mpProcessShotEventPacket(ctx, reinterpret_cast<const ShotEventPacket*>(buffer));
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
        else if (header->type == PACKET_MELEE_HIT_EVENT &&
                 bytes >= (int)sizeof(MeleeHitEventPacket))
        {
            mpProcessMeleeHitEventPacket(
                ctx, reinterpret_cast<const MeleeHitEventPacket*>(buffer));
        }
        else if (header->type == PACKET_NPC_DAMAGE_EVENT &&
                 bytes >= (int)sizeof(NpcDamageEventPacket))
        {
            mpProcessNpcDamageEventPacket(ctx, reinterpret_cast<const NpcDamageEventPacket*>(buffer));
        }
        else if (header->type == PACKET_CHAT_MESSAGE &&
                 bytes >= (int)sizeof(ChatPacket))
        {
            mpProcessChatPacket(ctx, reinterpret_cast<const ChatPacket*>(buffer));
        }
        else if (header->type == PACKET_PING &&
                 bytes >= (int)sizeof(PingPacket))
        {
            const PingPacket* ping =
                reinterpret_cast<const PingPacket*>(buffer);
            ctx.localPingMs = (int)std::min<uint64_t>(
                9999, nowMs() - ping->clientTimeMs);
        }
    }

    if (ctx.connected && ctx.localPlayerId && input)
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
                       (int)(input->position.y < -10.0f ? 1 : 0));
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
        }
        else
        {
            in.clientPx = input->position.x;
            in.clientPy = input->position.y;
            in.clientPz = input->position.z;
            in.clientVx = input->velocity.x;
            in.clientVy = input->velocity.y;
            in.clientVz = input->velocity.z;
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
        in.dashSerial = ctx.nextLocalDashSerial;
        in.groundJumpSerial = ctx.nextLocalGroundJumpSerial;
        in.airJumpSerial = ctx.nextLocalAirJumpSerial;
        in.downDashSerial = ctx.nextLocalDownDashSerial;
        in.directionChangeSerial = ctx.nextLocalMovementDirectionSerial;
        in.equipSerial = ctx.nextLocalEquipSerial;
        in.freezeSerial = ctx.nextLocalFreezeSerial;
        in.respawnSerial = ctx.nextLocalRespawnSerial;
        // Do NOT reset event serials to zero. Serials are persistent monotonic
        // counters. Each event type has one upward-only counter.
        // Respawn serial persists until server confirms (reset in mpReconcileLocalPlayer).
        in.attackPressed = input->attackPressed ? 1 : 0;
        in.sizeScale = input->sizeScale;
        mpSendPacket(ctx, &in, sizeof(in));
    }

    mpUpdateRemoteEntities(ctx, dt);
    mpUpdateNetworkProjectiles(ctx, dt, *gpWorld);

    if (ctx.connected && currentMs - ctx.lastPingSentMs >= 1000)
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


