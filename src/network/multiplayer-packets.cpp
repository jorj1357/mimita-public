#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "analytics/analytics-manager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace MimitaNet {

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

void flushOutgoingPackets(MultiplayerContext& ctx)
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
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.lastFakeLagLogMs = 0;

    printf("[NET CONNECT] connecting to %s as \"%s\"\n", address.c_str(), playerName.c_str());
    return true;
}

void mpShutdown(MultiplayerContext& ctx)
{
    if (!ctx.active)
        return;

    AnalyticsManager::instance().trackDisconnect(
        ctx.connected ? "shutdown" : "connection_closed");

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

void mpSendServerCommand(MultiplayerContext& ctx, const std::string& command)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    ServerCommandPacket packet{};
    packet.header.type = PACKET_SERVER_COMMAND;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    std::memset(packet.commandText, 0, sizeof(packet.commandText));
    std::strncpy(packet.commandText, command.c_str(), sizeof(packet.commandText) - 1);
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET SERVER COMMAND SEND] cmd=\"%s\"\n", command.c_str());
}

} // namespace MimitaNet
