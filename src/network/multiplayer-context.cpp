#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "render/outfit-atlas.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"

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

SnapshotTransform transformFromEntity(const SnapshotEntity& entity)
{
    SnapshotTransform transform;
    transform.position = {entity.px, entity.py, entity.pz};
    transform.velocity = {entity.vx, entity.vy, entity.vz};
    transform.yaw = entity.yaw;
    transform.health = entity.health;
    transform.onGround = entity.onGround != 0;
    transform.equippedSlot = entity.equippedSlot;
    transform.weaponState = entity.weaponState;
    transform.aimDirection = {entity.aimX, entity.aimY, entity.aimZ};
    transform.pingMs = entity.pingMs;
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
    player.equippedSlot = interpolation.target.equippedSlot;
    {
        // Look up weapon ID from slot for animation system
        player.equippedWeaponId.clear();
        if (player.equippedSlot >= 1) {
            auto reg = WeaponRegistry::instance().all();
            for (const auto& w : reg) {
                if (w.second.slot == player.equippedSlot) {
                    player.equippedWeaponId = w.first;
                    break;
                }
            }
        }
    }
    player.networkShootEffectTimer =
        std::max(0.0f, player.networkShootEffectTimer - dt);
    player.networkWeaponState = interpolation.target.weaponState;
    if (player.networkShootEffectTimer > 0.0f)
        player.networkWeaponState |= 1u;
    player.aimDirection = interpolation.target.aimDirection;
    player.hasAimData = glm::length(player.aimDirection) > 0.001f;
    player.username = interpolation.displayName;
    // Pass reconstructed aim direction so local animation system
    // produces matching limb positions for the remote player.
    player.updateProceduralAnimation(dt, player.aimDirection, player.pos);
}

} // namespace

void mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes)
{
    if (!ctx.active || ctx.sock == INVALID_SOCKET || !data || bytes <= 0)
        return;

    int delayMs = 0;
    if (ctx.fakeLagMode == 1)
        delayMs = ctx.fakeLagCurrentMs;
    else if (ctx.fakeLagMode == 2)
        delayMs = ctx.fakeLagStaticMs;

    if (delayMs <= 0)
    {
        sendto(ctx.sock, (const char*)data, bytes, 0,
               (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
        ++ctx.packetsSent;
        return;
    }

    QueuedPacket queued;
    queued.bytes.assign((const char*)data, (const char*)data + bytes);
    queued.deliverAtMs = nowMs() + (uint64_t)delayMs;
    ctx.outgoingQueue.push_back(std::move(queued));
    const uint64_t currentMs = nowMs();
    if (currentMs - ctx.lastFakeLagLogMs >= 250)
    {
        printf("[FAKELAG] mode=%d delay=%d packetQueued=%zu\n",
               ctx.fakeLagMode, delayMs, ctx.outgoingQueue.size());
        ctx.lastFakeLagLogMs = currentMs;
    }
}

static void flushOutgoingPackets(MultiplayerContext& ctx)
{
    const uint64_t currentMs = nowMs();
    for (size_t i = 0; i < ctx.outgoingQueue.size(); )
    {
        QueuedPacket& queued = ctx.outgoingQueue[i];
        if (queued.deliverAtMs > currentMs)
        {
            ++i;
            continue;
        }

        sendto(ctx.sock, queued.bytes.data(), (int)queued.bytes.size(), 0,
               (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
        ++ctx.packetsSent;
        ctx.outgoingQueue.erase(ctx.outgoingQueue.begin() + i);
    }
}

void mpSetFakeLagMode(MultiplayerContext& ctx, int mode)
{
    ctx.fakeLagMode = std::clamp(mode, 0, 2);
    ctx.fakeLagNextRandomizeMs = 0;
    if (ctx.fakeLagMode == 0)
    {
        ctx.fakeLagCurrentMs = 0;
        for (QueuedPacket& queued : ctx.outgoingQueue)
            queued.deliverAtMs = 0;
        flushOutgoingPackets(ctx);
    }
}

void mpSetFakeLagStatic(MultiplayerContext& ctx, int milliseconds)
{
    ctx.fakeLagStaticMs = std::clamp(milliseconds, 0, 5000);
}

void mpSetFakeLagRange(MultiplayerContext& ctx, int minimumMs, int maximumMs)
{
    minimumMs = std::clamp(minimumMs, 0, 5000);
    maximumMs = std::clamp(maximumMs, 0, 5000);
    if (minimumMs > maximumMs)
        std::swap(minimumMs, maximumMs);
    ctx.fakeLagMinMs = minimumMs;
    ctx.fakeLagMaxMs = maximumMs;
    ctx.fakeLagNextRandomizeMs = 0;
}

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
    ctx.outgoingQueue.clear();
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastFakeLagLogMs = 0;

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
    ctx.outgoingQueue.clear();
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    netShutdown();
    printf("[NET DISCONNECT] shutdown complete\n");
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt)
{
    if (!ctx.active)
        return;

    uint64_t currentMs = nowMs();
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
        else if (header->type == PACKET_SHOT_EVENT &&
                 bytes >= (int)sizeof(ShotEventPacket))
        {
            const ShotEventPacket* event =
                reinterpret_cast<const ShotEventPacket*>(buffer);
            uint32_t& lastSerial =
                ctx.lastReceivedShotSerial[event->shooterPlayerId];
            if (lastSerial != 0 &&
                (int32_t)(event->shotSerial - lastSerial) <= 0)
            {
                printf("[NET SHOT RECV] shooter=%u serial=%u skipped=duplicate last=%u\n",
                       event->shooterPlayerId, event->shotSerial, lastSerial);
                continue;
            }
            lastSerial = event->shotSerial;

            NetworkShotEvent out;
            out.shotSerial = event->shotSerial;
            out.clientTimeMs = event->clientTimeMs;
            out.shooterPlayerId = event->shooterPlayerId;
            out.targetPlayerId = event->targetPlayerId;
            out.damage = event->damage;
            out.targetHealth = event->targetHealth;
            out.power = event->power;
            out.effectFlags = event->effectFlags;
            out.weapon = event->weapon;
            out.impactType = event->impactType;
            out.killed = event->killed != 0;
            out.damageConfirmed = event->damageConfirmed != 0;
            out.origin = {event->originX, event->originY, event->originZ};
            out.hit = {event->hitX, event->hitY, event->hitZ};
            out.direction = {event->dirX, event->dirY, event->dirZ};
            out.normal = {event->normalX, event->normalY, event->normalZ};
            out.knockback = {event->knockX, event->knockY, event->knockZ};
            ctx.shotEvents.push_back(out);
            printf("[NET SHOT RECV] shooter=%u serial=%u weapon=%u impact=%u "
                   "flags=0x%03x damageConfirmed=%d origin=(%.2f %.2f %.2f) "
                   "hit=(%.2f %.2f %.2f)\n",
                   out.shooterPlayerId, out.shotSerial, out.weapon,
                   out.impactType, out.effectFlags, (int)out.damageConfirmed,
                   out.origin.x, out.origin.y, out.origin.z,
                   out.hit.x, out.hit.y, out.hit.z);
        }
        else if (header->type == PACKET_CHAT_MESSAGE &&
                 bytes >= (int)sizeof(ChatPacket))
        {
            const ChatPacket* chat = reinterpret_cast<const ChatPacket*>(buffer);
            MultiplayerContext::IncomingChatMessage msg;
            msg.senderName = chat->senderName;
            msg.text = chat->text;
            ctx.incomingChatMessages.push_back(msg);
            printf("[NET CHAT RECV] %s: %s\n", chat->senderName, chat->text);
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

void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position, float difficulty)
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
    request.difficulty = difficulty;
    mpSendPacket(ctx, &request, sizeof(request));
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
    mpSendPacket(ctx, &request, sizeof(request));
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
    mpSendPacket(ctx, &request, sizeof(request));
}

uint32_t mpSendShotEvent(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    float power,
    uint16_t effectFlags,
    uint8_t weapon,
    uint8_t impactType,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& direction,
    const glm::vec3& normal,
    const glm::vec3& knockbackImpulse)
{
    if (!ctx.active || !ctx.localPlayerId)
        return 0;

    ShotRequestPacket packet{};
    packet.header.type = PACKET_SHOT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.shotSerial = ctx.nextLocalShotSerial++;
    if (ctx.nextLocalShotSerial == 0)
        ctx.nextLocalShotSerial = 1;
    packet.clientTimeMs = nowMs();
    packet.targetPlayerId = targetPlayerId;
    packet.damage = damage;
    packet.power = power;
    packet.effectFlags = effectFlags;
    packet.weapon = weapon;
    packet.impactType = impactType;
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    packet.hitX = hit.x; packet.hitY = hit.y; packet.hitZ = hit.z;
    packet.dirX = direction.x; packet.dirY = direction.y; packet.dirZ = direction.z;
    packet.normalX = normal.x; packet.normalY = normal.y; packet.normalZ = normal.z;
    packet.knockX = knockbackImpulse.x; packet.knockY = knockbackImpulse.y; packet.knockZ = knockbackImpulse.z;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET SHOT SEND] shooter=%u serial=%u weapon=%u impact=%u "
           "flags=0x%03x target=%u damage=%d origin=(%.2f %.2f %.2f) "
           "hit=(%.2f %.2f %.2f)\n",
           ctx.localPlayerId, packet.shotSerial, weapon, impactType,
           effectFlags, targetPlayerId, damage,
           origin.x, origin.y, origin.z, hit.x, hit.y, hit.z);
    return packet.shotSerial;
}

} // namespace MimitaNet
