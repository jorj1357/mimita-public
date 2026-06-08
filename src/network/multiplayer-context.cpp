#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "render/outfit-atlas.h"
#include "config/player-settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace MimitaNet {

namespace {

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
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
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.playerRegistry.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;

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
    ctx.playerRegistry.clear();
    netShutdown();
    printf("[NET DISCONNECT] shutdown complete\n");
}

void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt)
{
    if (!ctx.active)
        return;

    uint64_t currentMs = nowMs();

    // Send HELLO until we get an ID
    if (ctx.localPlayerId == 0)
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
            ctx.lastSnapshotTick = snapshot->header.tick;
            uint32_t count = std::min(snapshot->entityCount, (uint32_t)MAX_SNAPSHOT_ENTITIES);
            const bool logSnapshot = snapshot->header.tick % 60 == 0;

            if (logSnapshot)
                printf("[CLIENT SNAPSHOT RECV] localClientId=%u bytes=%d entityCount=%u playerCount=%u npcCount=%u\n",
                       ctx.localPlayerId, bytes, snapshot->entityCount,
                       snapshot->playerCount, snapshot->npcCount);

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
                    ctx.hasLocalServerPosition = true;
                    ctx.playerRegistry[entity.networkEntityId] = {
                        entity.displayName, entity.networkEntityId, 0
                    };
                    if (logSnapshot)
                    {
                        printf("[CLIENT ENTITY APPLY] entityId=%u type=Player isLocal=1 existsBefore=1 "
                               "createdNow=0 position=(%.2f,%.2f,%.2f) renderRegistered=1\n",
                               entity.networkEntityId, entity.px, entity.py, entity.pz);
                        printf("[CLIENT ENTITY SKIP] entityId=%u reason=local-prediction-keeps-transform\n",
                               entity.networkEntityId);
                    }
                    continue;
                }

                std::unordered_map<uint32_t, Player>* replicas = nullptr;
                std::unordered_map<uint32_t, bool>* seen = nullptr;
                const char* typeName = nullptr;
                if (entity.entityType == ENTITY_PLAYER)
                {
                    replicas = &ctx.remotePlayers;
                    seen = &seenPlayers;
                    typeName = "Player";
                    ctx.playerRegistry[entity.networkEntityId] = {
                        entity.displayName, entity.networkEntityId, 0
                    };
                }
                else if (entity.entityType == ENTITY_NPC)
                {
                    replicas = &ctx.remoteNpcs;
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
                if (isNew)
                {
                    OutfitAtlas::instance().apply(p, GetPlayerSettings().outfitPath);
                }

                p.pos = {entity.px, entity.py, entity.pz};
                p.vel = {entity.vx, entity.vy, entity.vz};
                p.yaw = entity.yaw;
                p.onGround = entity.onGround != 0;
                p.currentHp = entity.health;
                p.username = entity.displayName[0]
                    ? entity.displayName
                    : std::string(typeName) + std::to_string(entity.networkEntityId);
                p.updateProceduralAnimation(dt);
                (*seen)[entity.networkEntityId] = true;

                if (isNew || logSnapshot)
                    printf("[CLIENT ENTITY APPLY] entityId=%u type=%s isLocal=0 existsBefore=%d "
                           "createdNow=%d position=(%.2f,%.2f,%.2f) renderRegistered=1\n",
                           entity.networkEntityId, typeName, (int)existsBefore, (int)isNew,
                           entity.px, entity.py, entity.pz);
            }

            for (auto it = ctx.remotePlayers.begin(); it != ctx.remotePlayers.end(); )
            {
                if (!seenPlayers[it->first])
                {
                    printf("[NET DESPAWN] remote player id=%u name=\"%s\" removed\n",
                           it->first, ctx.playerRegistry[it->first].name.c_str());
                    it = ctx.remotePlayers.erase(it);
                }
                else
                    ++it;
            }
            for (auto it = ctx.remoteNpcs.begin(); it != ctx.remoteNpcs.end(); )
            {
                if (!seenNpcs[it->first])
                {
                    printf("[NET DESPAWN] NPC entityId=%u name=\"%s\" removed\n",
                           it->first, it->second.username.c_str());
                    it = ctx.remoteNpcs.erase(it);
                }
                else
                    ++it;
            }
        }
    }

    ++ctx.tick;
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

} // namespace MimitaNet
