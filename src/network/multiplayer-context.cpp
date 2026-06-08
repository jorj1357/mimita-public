#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "render/outfit-atlas.h"
#include "config/player-settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace MimitaNet {

namespace {

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

SnapshotTransform transformFromEntity(const SnapshotEntity& entity)
{
    SnapshotTransform transform;
    transform.position = {entity.px, entity.py, entity.pz};
    transform.velocity = {entity.vx, entity.vy, entity.vz};
    transform.yaw = entity.yaw;
    transform.health = entity.health;
    transform.onGround = entity.onGround != 0;
    transform.receivedMs = nowMs();
    return transform;
}

float angleLerpDegrees(float from, float to, float t)
{
    float delta = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    return from + delta * t;
}

void pushInterpolationTarget(
    EntityInterpolationState& interpolation,
    const SnapshotEntity& entity,
    uint32_t serverTick)
{
    SnapshotTransform next = transformFromEntity(entity);
    next.serverTick = serverTick;
    if (interpolation.hasTarget) {
        interpolation.previous = interpolation.target;
        interpolation.hasPrevious = true;
    } else {
        interpolation.previous = next;
        interpolation.hasPrevious = true;
    }
    interpolation.target = next;
    interpolation.hasTarget = true;
    interpolation.displayName = entity.displayName;
}

void updateRenderedReplica(
    Player& player,
    EntityInterpolationState& interpolation,
    float dt)
{
    if (!interpolation.hasTarget)
        return;

    constexpr double INTERPOLATION_DELAY_MS = 100.0;
    float t = 1.0f;
    if (interpolation.hasPrevious) {
        const double span = double(interpolation.target.receivedMs - interpolation.previous.receivedMs);
        if (span > 1.0) {
            const double renderTime = double(nowMs()) - INTERPOLATION_DELAY_MS;
            t = std::clamp(
                float((renderTime - double(interpolation.previous.receivedMs)) / span),
                0.0f, 1.0f);
        }
    }

    player.pos = interpolation.previous.position +
        (interpolation.target.position - interpolation.previous.position) * t;
    player.vel = interpolation.target.velocity;
    player.yaw = angleLerpDegrees(interpolation.previous.yaw, interpolation.target.yaw, t);
    player.currentHp = interpolation.target.health;
    player.dead = interpolation.target.health <= 0;
    player.onGround = interpolation.target.onGround;
    player.username = interpolation.displayName;
    player.updateProceduralAnimation(dt);
}

} // namespace

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName)
{
    ctx.serverAddress = address;

    if (!netStartup())
    {
        printf("[NET CONNECT] FATAL: WSAStartup failed\n");
        return false;
    }

    ctx.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx.sock == INVALID_SOCKET)
    {
        printf("[NET CONNECT] FATAL: socket() failed error=%d\n", WSAGetLastError());
        netShutdown();
        return false;
    }
    setNonBlocking(ctx.sock);

    if (!parseAddress(address, ctx.serverAddr))
    {
        printf("[NET CONNECT] FATAL: invalid address: %s\n", address.c_str());
        closesocket(ctx.sock);
        netShutdown();
        return false;
    }

    ctx.active = true;
    ctx.localPlayerId = 0;
    ctx.tick = 0;
    ctx.lastHelloMs = 0;
    ctx.lastSnapshotReceivedMs = 0;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.playerRegistry.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.lastLocalCorrectionLogMs = 0;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.awaitingExplodeDeath = false;
    ctx.localServerVelocity = glm::vec3(0.0f);
    ctx.localServerYaw = 0.0f;
    ctx.localServerOnGround = false;
    ctx.localServerHealth = 100;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connecting...";

    printf("[NET CONNECT] connecting to %s as \"%s\"\n", address.c_str(), playerName.c_str());
    return true;
}

void mpShutdown(MultiplayerContext& ctx)
{
    if (!ctx.active)
        return;

    if (ctx.localPlayerId)
    {
        DisconnectPacket bye{};
        bye.header.type = PACKET_DISCONNECT;
        bye.header.playerId = ctx.localPlayerId;
        bye.header.tick = ctx.tick;
        sendto(ctx.sock, (const char*)&bye, sizeof(bye), 0,
               (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
        printf("[NET DISCONNECT] sent disconnect for id=%u\n", ctx.localPlayerId);
    }

    closesocket(ctx.sock);
    ctx.sock = INVALID_SOCKET;
    ctx.active = false;
    ctx.localPlayerId = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.playerRegistry.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.lastLocalCorrectionLogMs = 0;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.awaitingExplodeDeath = false;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus.clear();
    netShutdown();
    printf("[NET DISCONNECT] shutdown complete\n");
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt)
{
    if (!ctx.active)
        return;

    uint64_t currentMs = nowMs();
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
            sendto(ctx.sock, (const char*)&hello, sizeof(hello), 0,
                   (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
            ++ctx.packetsSent;
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

        PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
        if (bytes < (int)sizeof(PacketHeader) ||
            header->magic != PROTOCOL_MAGIC ||
            header->version != PROTOCOL_VERSION)
            continue;

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
                        entity.displayName, entity.networkEntityId, 0
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
                        entity.displayName, entity.networkEntityId, 0
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
                    OutfitAtlas::instance().apply(p, GetPlayerSettings().outfitPath);
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
    }

    for (auto& kv : ctx.remotePlayers)
    {
        auto interpolation = ctx.remotePlayerInterpolation.find(kv.first);
        if (interpolation != ctx.remotePlayerInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, dt);
    }
    for (auto& kv : ctx.remoteNpcs)
    {
        auto interpolation = ctx.remoteNpcInterpolation.find(kv.first);
        if (interpolation != ctx.remoteNpcInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, dt);
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
        ctx.localServerHealth > 0 && player.dead && !ctx.awaitingExplodeDeath;
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

    player.currentHp = ctx.localServerHealth;
    if (serverKilledPlayer)
        player.currentHp = 0;
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

void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    SpawnNpcRequestPacket request{};
    request.header.type = PACKET_SPAWN_NPC_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    request.px = position.x;
    request.py = position.y;
    request.pz = position.z;
    sendto(ctx.sock, (const char*)&request, sizeof(request), 0,
           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    ++ctx.packetsSent;
}

void mpRequestTeleport(MultiplayerContext& ctx, const glm::vec3& position)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    TeleportRequestPacket request{};
    request.header.type = PACKET_TELEPORT_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    request.px = position.x;
    request.py = position.y;
    request.pz = position.z;
    ctx.pendingTeleportPosition = position;
    ctx.pendingTeleportSentMs = nowMs();
    ctx.awaitingTeleportAck = true;
    sendto(ctx.sock, (const char*)&request, sizeof(request), 0,
           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    ++ctx.packetsSent;
}

void mpRequestExplode(MultiplayerContext& ctx)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    ExplodeRequestPacket request{};
    request.header.type = PACKET_EXPLODE_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    ctx.awaitingExplodeDeath = true;
    sendto(ctx.sock, (const char*)&request, sizeof(request), 0,
           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    ++ctx.packetsSent;
}

} // namespace MimitaNet
