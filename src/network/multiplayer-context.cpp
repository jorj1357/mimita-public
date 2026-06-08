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
    ctx.playerRegistry.clear();

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
    char buffer[4096];
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
            printf("[NET SPAWN] assigned player id=%u serverTick=%u tickRate=%.0f\n",
                   ctx.localPlayerId, welcome->header.tick, welcome->tickRate);
        }
        else if (header->type == PACKET_SNAPSHOT && bytes >= (int)sizeof(SnapshotPacket))
        {
            SnapshotPacket* snapshot = reinterpret_cast<SnapshotPacket*>(buffer);
            ctx.lastSnapshotTick = snapshot->header.tick;
            uint32_t count = std::min(snapshot->playerCount, (uint32_t)MAX_SNAPSHOT_PLAYERS);

            std::unordered_map<uint32_t, bool> seen;
            for (uint32_t i = 0; i < count; ++i)
            {
                const SnapshotPlayer& sp = snapshot->players[i];
                if (!sp.active || sp.playerId == 0)
                    continue;

                // Update player registry (names)
                ctx.playerRegistry[sp.playerId].id = sp.playerId;
                if (sp.name[0] != '\0')
                    ctx.playerRegistry[sp.playerId].name = sp.name;
                else if (ctx.playerRegistry[sp.playerId].name.empty())
                    ctx.playerRegistry[sp.playerId].name = "player" + std::to_string(sp.playerId);

                // Skip our own player - we render our local player separately
                if (sp.playerId == ctx.localPlayerId)
                {
                    seen[sp.playerId] = true;
                    continue;
                }

                // Create or update remote player
                bool isNew = ctx.remotePlayers.find(sp.playerId) == ctx.remotePlayers.end();
                Player& p = ctx.remotePlayers[sp.playerId];

                if (isNew)
                {
                    // Apply outfit to new remote player
                    OutfitAtlas::instance().apply(p, GetPlayerSettings().outfitPath);
                    printf("[NET SPAWN] remote player id=%u name=\"%s\" spawned\n",
                           sp.playerId, ctx.playerRegistry[sp.playerId].name.c_str());
                }

                p.pos = {sp.px, sp.py, sp.pz};
                p.vel = {sp.vx, sp.vy, sp.vz};
                p.yaw = sp.yaw;
                p.onGround = sp.onGround != 0;
                p.currentHp = sp.health;
                p.username = ctx.playerRegistry[sp.playerId].name;
                p.updateProceduralAnimation(dt);
                seen[sp.playerId] = true;
            }

            // Remove players that disappeared
            for (auto it = ctx.remotePlayers.begin(); it != ctx.remotePlayers.end(); )
            {
                if (!seen[it->first])
                {
                    printf("[NET DESPAWN] remote player id=%u name=\"%s\" removed\n",
                           it->first, ctx.playerRegistry[it->first].name.c_str());
                    it = ctx.remotePlayers.erase(it);
                }
                else
                    ++it;
            }
        }
    }

    ++ctx.tick;
}

} // namespace MimitaNet
