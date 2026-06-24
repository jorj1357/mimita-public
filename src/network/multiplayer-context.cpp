#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "render/outfit-atlas.h"
#include "analytics/analytics-manager.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"
#include "effects/effect-part.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace MimitaNet {

bool gNetDamageDebug = false;
bool gNetHitDebug = false;

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

} // namespace

static const char* disconnectReasonStr(MultiplayerContext& ctx)
{
    if (!ctx.active) return "inactive";
    if (ctx.connectFailed) return "connection-timeout";
    if (!ctx.connected) return "not-connected";
    if (ctx.sock == INVALID_SOCKET) return "invalid-socket";
    return "unknown";
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input)
{
    if (!ctx.active)
        return;

    uint64_t currentMs = nowMs();

    // Client-side timeout detection: warn if no server packet for 10s
    constexpr uint64_t CLIENT_TIMEOUT_MS = 10000;
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
        printf("[NET CONNECT] timeout server=%s\n", ctx.serverAddress.c_str());
    }

    // Send HELLO until we get an ID
    if (ctx.localPlayerId == 0 && !ctx.connectFailed)
    {
        if (currentMs - ctx.lastHelloMs > 500)
        {
            HelloPacket hello{};
            hello.header.type = PACKET_HELLO;
            hello.header.tick = ctx.tick;
            copyName(hello.name, playerName);
            mpSendPacket(ctx, &hello, sizeof(hello));
            ctx.lastHelloMs = currentMs;
            printf("[NET CONNECT] hello sent to %s\n", ctx.serverAddress.c_str());
        }
    }

    // Receive packets
    char buffer[16384];
    for (;;)
    {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        int bytes = recvfrom(ctx.sock, buffer, sizeof(buffer), 0,
                             (sockaddr*)&from, &fromLen);
        if (bytes <= 0)
            break;
        ++ctx.packetsReceived;
        if (!isSameAddress(from, ctx.serverAddr))
        {
            printf("[NET PACKET FILTER] accepted=0 reason=not-server from=%s\n",
                   addressToString(from).c_str());
            continue;
        }

        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (bytes < (int)sizeof(PacketHeader) ||
            header->magic != PROTOCOL_MAGIC ||
            header->version != PROTOCOL_VERSION)
            continue;

        ctx.lastHeardServerMs = nowMs();

        if (header->type == PACKET_WELCOME && bytes >= (int)sizeof(WelcomePacket))
        {
            WelcomePacket* welcome = reinterpret_cast<WelcomePacket*>(buffer);
            ctx.localPlayerId = welcome->assignedPlayerId;
            ctx.connected = true;
            ctx.connectFailed = false;
            ctx.connectionStatus = "Connected";
            ctx.approvedLocalName = welcome->approvedName;
            ctx.playerRegistry[ctx.localPlayerId] = {
                ctx.approvedLocalName.empty() ? playerName : ctx.approvedLocalName,
                ctx.localPlayerId,
                0
            };
            printf("[NET SPAWN] assigned player id=%u serverTick=%u tickRate=%.0f\n",
                   ctx.localPlayerId, welcome->header.tick, welcome->tickRate);
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
                    ctx.localPingMs = entity.pingMs;
                    if (ctx.awaitingTeleportAck &&
                        glm::length(
                            ctx.localServerPosition -
                            ctx.pendingTeleportPosition) <= 1.0f)
                    {
                        ctx.awaitingTeleportAck = false;
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
                        OutfitAtlas::instance().apply(p, GetPlayerSettings().outfitPath);
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

    // Send input packet to server every tick when connected
    if (ctx.connected && ctx.localPlayerId && input)
    {
        InputPacket in{};
        in.header.type = PACKET_INPUT;
        in.header.tick = ctx.tick;
        in.header.playerId = ctx.localPlayerId;
        in.wishX = input->wishX;
        in.wishY = input->wishY;
        in.camForwardX = input->camForward.x;
        in.camForwardY = input->camForward.y;
        in.camForwardZ = input->camForward.z;
        in.yaw = input->yaw;
        in.clientPx = input->position.x;
        in.clientPy = input->position.y;
        in.clientPz = input->position.z;
        in.clientVx = input->velocity.x;
        in.clientVy = input->velocity.y;
        in.clientVz = input->velocity.z;
        in.equippedSlot = (int16_t)input->equippedSlot;
        in.weaponState = input->weaponState;
        in.clientPingMs = ctx.localPingMs;
        in.jumpHeld = input->jumpHeld ? 1 : 0;
        in.dashPressed = input->dashPressed ? 1 : 0;
        in.attackPressed = input->attackPressed ? 1 : 0;
        in.freezeHeld = input->freezeHeld ? 1 : 0;
        mpSendPacket(ctx, &in, sizeof(in));
    }

    mpUpdateRemoteEntities(ctx, dt);

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

void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt)
{
    (void)dt;
    if (!ctx.connected || !ctx.hasLocalServerPosition)
        return;

    const glm::vec3 clientPosition = player.pos;
    const glm::vec3 correction = ctx.localServerPosition - player.pos;
    const float error = glm::length(correction);
    constexpr float CATASTROPHIC_DIVERGENCE = 100.0f;
    constexpr float CORRECTION_LOG_DISTANCE = 0.5f;
    constexpr uint64_t TELEPORT_ACK_TIMEOUT_MS = 1500;
    const uint64_t currentMs = nowMs();
    if (ctx.awaitingTeleportAck &&
        currentMs - ctx.pendingTeleportSentMs > TELEPORT_ACK_TIMEOUT_MS)
    {
        ctx.awaitingTeleportAck = false;
    }
    const bool initialSpawn = !ctx.localPlayerReconciled;
    const bool serverKilledPlayer = ctx.localServerHealth <= 0 && !player.dead;
    const bool serverRespawnedPlayer =
        ctx.localServerHealth > 0 && player.dead;
    const bool catastrophicDivergence =
        error > CATASTROPHIC_DIVERGENCE &&
        !ctx.awaitingTeleportAck &&
        !player.dead;
    const bool applyPosition =
        initialSpawn || serverRespawnedPlayer || catastrophicDivergence;

    if (applyPosition)
    {
        player.pos = ctx.localServerPosition;
        player.vel = ctx.localServerVelocity;
        player.yaw = ctx.localServerYaw;
        player.onGround = ctx.localServerOnGround;
        player.externalImpulse = glm::vec3(0.0f);
        player.syncLegacyStateToLayers();
        player.updateModelWorldTransforms();
    }

    if (serverRespawnedPlayer)
        player.currentHp = ctx.localServerHealth;
    else if (serverKilledPlayer)
        player.currentHp = 0;
    else
        player.currentHp = std::min(player.currentHp, ctx.localServerHealth);
    if (serverRespawnedPlayer)
    {
        player.dead = false;
        player.proceduralFrozen = false;
        player.respawnTimer = 0.0f;
        player.killedBy.clear();
    }

    if (!player.dead)
    {
        const Capsule localCapsule = player.getCapsule();
        for (const auto& entry : ctx.remotePlayers)
        {
            const Player& remote = entry.second;
            if (remote.dead)
                continue;

            const Capsule remoteCapsule = remote.getCapsule();
            const float localBottom = localCapsule.a.z - localCapsule.r;
            const float localTop = localCapsule.b.z + localCapsule.r;
            const float remoteBottom = remoteCapsule.a.z - remoteCapsule.r;
            const float remoteTop = remoteCapsule.b.z + remoteCapsule.r;
            if (localTop <= remoteBottom || remoteTop <= localBottom)
                continue;

            glm::vec2 delta(player.pos.x - remote.pos.x, player.pos.y - remote.pos.y);
            float distance = glm::length(delta);
            const float minimumDistance = localCapsule.r + remoteCapsule.r;
            if (distance >= minimumDistance)
                continue;

            glm::vec2 normal(1.0f, 0.0f);
            if (distance > 0.0001f)
                normal = delta / distance;
            const float penetration = minimumDistance - distance;
            player.pos += glm::vec3(normal * penetration, 0.0f);

            glm::vec2 planarVelocity(player.vel.x, player.vel.y);
            const float intoRemote = glm::dot(planarVelocity, normal);
            if (intoRemote < 0.0f)
            {
                planarVelocity -= normal * intoRemote;
                player.vel.x = planarVelocity.x;
                player.vel.y = planarVelocity.y;
            }

            static uint64_t lastCollisionLogMs = 0;
            const uint64_t collisionNowMs = nowMs();
            if (collisionNowMs - lastCollisionLogMs >= 250)
            {
                printf("[CLIENT PLAYER COLLISION] localId=%u remoteId=%u penetration=%.3f "
                       "localPos=(%.2f,%.2f,%.2f) remotePos=(%.2f,%.2f,%.2f)\n",
                       ctx.localPlayerId, entry.first, penetration,
                       player.pos.x, player.pos.y, player.pos.z,
                       remote.pos.x, remote.pos.y, remote.pos.z);
                lastCollisionLogMs = collisionNowMs;
            }
        }
    }

    const bool logCorrection =
        initialSpawn || applyPosition || error >= CORRECTION_LOG_DISTANCE;
    if (logCorrection &&
        (applyPosition || currentMs - ctx.lastLocalCorrectionLogMs >= 500))
    {
        printf("[LOCAL CORRECTION] distance=%.3f "
               "serverPos=(%.2f,%.2f,%.2f) clientPos=(%.2f,%.2f,%.2f) "
               "applied=%d reason=%s\n",
               error,
               ctx.localServerPosition.x,
               ctx.localServerPosition.y,
               ctx.localServerPosition.z,
               clientPosition.x,
               clientPosition.y,
               clientPosition.z,
               (int)applyPosition,
               initialSpawn ? "initial-spawn" :
               serverRespawnedPlayer ? "server-respawn" :
               catastrophicDivergence ? "catastrophic-divergence" :
               serverKilledPlayer ? "server-death" : "within-tolerance");
        ctx.lastLocalCorrectionLogMs = currentMs;
    }
    ctx.localPlayerReconciled = true;
}

} // namespace MimitaNet
