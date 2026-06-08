#include "network/net_common.h"
#include "network/packets.h"

#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

namespace {

struct TestClient
{
    SOCKET socket = INVALID_SOCKET;
    uint32_t id = 0;
    std::string approvedName;
    MimitaNet::SnapshotPacket snapshot{};
};

void copyName(char (&out)[MimitaNet::MAX_NAME_BYTES], const char* name)
{
    std::memset(out, 0, sizeof(out));
    std::strncpy(out, name, sizeof(out) - 1);
}

bool openClient(TestClient& client)
{
    client.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return client.socket != INVALID_SOCKET &&
           MimitaNet::setNonBlocking(client.socket);
}

void sendHello(TestClient& client, const sockaddr_in& server)
{
    MimitaNet::HelloPacket hello{};
    hello.header.type = MimitaNet::PACKET_HELLO;
    copyName(hello.name, "admin");
    sendto(client.socket, (const char*)&hello, sizeof(hello), 0,
           (const sockaddr*)&server, sizeof(server));
}

bool pump(TestClient& client, uint64_t deadline)
{
    char buffer[16384];
    while (MimitaNet::nowMs() < deadline)
    {
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int bytes = recvfrom(
            client.socket, buffer, sizeof(buffer), 0,
            (sockaddr*)&from, &fromLength);
        if (bytes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        auto* header = reinterpret_cast<MimitaNet::PacketHeader*>(buffer);
        if (bytes < (int)sizeof(*header) ||
            header->magic != MimitaNet::PROTOCOL_MAGIC ||
            header->version != MimitaNet::PROTOCOL_VERSION)
            continue;

        if (header->type == MimitaNet::PACKET_WELCOME &&
            bytes >= (int)sizeof(MimitaNet::WelcomePacket))
        {
            auto* welcome = reinterpret_cast<MimitaNet::WelcomePacket*>(buffer);
            client.id = welcome->assignedPlayerId;
            client.approvedName = welcome->approvedName;
        }
        else if (header->type == MimitaNet::PACKET_SNAPSHOT &&
                 bytes >= (int)sizeof(MimitaNet::SnapshotPacket))
        {
            client.snapshot =
                *reinterpret_cast<MimitaNet::SnapshotPacket*>(buffer);
            if (client.id && client.snapshot.playerCount >= 2 &&
                client.snapshot.npcCount >= 3)
                return true;
        }
    }
    return false;
}

void disconnect(TestClient& client, const sockaddr_in& server)
{
    if (client.id)
    {
        MimitaNet::DisconnectPacket packet{};
        packet.header.type = MimitaNet::PACKET_DISCONNECT;
        packet.header.playerId = client.id;
        sendto(client.socket, (const char*)&packet, sizeof(packet), 0,
               (const sockaddr*)&server, sizeof(server));
    }
    if (client.socket != INVALID_SOCKET)
        closesocket(client.socket);
}

}

int main()
{
    if (!MimitaNet::netStartup())
        return 1;

    sockaddr_in server{};
    if (!MimitaNet::parseAddress("127.0.0.1:2357", server))
        return 2;

    TestClient first;
    TestClient second;
    if (!openClient(first) || !openClient(second))
        return 3;

    sendHello(first, server);
    sendHello(second, server);

    const uint64_t deadline = MimitaNet::nowMs() + 4000;
    const bool firstReady = pump(first, deadline);
    const bool secondReady = pump(second, deadline);
    if (!firstReady || !secondReady)
        return 4;
    if (first.id == second.id || first.approvedName == second.approvedName)
        return 5;

    MimitaNet::SpawnNpcRequestPacket spawn{};
    spawn.header.type = MimitaNet::PACKET_SPAWN_NPC_REQUEST;
    spawn.header.playerId = first.id;
    spawn.px = 10.0f;
    spawn.py = 10.0f;
    spawn.pz = 30.0f;
    sendto(first.socket, (const char*)&spawn, sizeof(spawn), 0,
           (const sockaddr*)&server, sizeof(server));

    bool firstSawSpawn = false;
    bool secondSawSpawn = false;
    const uint64_t spawnDeadline = MimitaNet::nowMs() + 4000;
    while (MimitaNet::nowMs() < spawnDeadline &&
           (!firstSawSpawn || !secondSawSpawn))
    {
        pump(first, MimitaNet::nowMs() + 30);
        pump(second, MimitaNet::nowMs() + 30);
        firstSawSpawn = first.snapshot.npcCount >= 4;
        secondSawSpawn = second.snapshot.npcCount >= 4;
    }

    std::printf(
        "[PROTOCOL SMOKE] first=%u/%s second=%u/%s players=%u npcs=%u spawned=%d/%d\n",
        first.id, first.approvedName.c_str(),
        second.id, second.approvedName.c_str(),
        first.snapshot.playerCount, first.snapshot.npcCount,
        (int)firstSawSpawn, (int)secondSawSpawn);

    disconnect(first, server);
    disconnect(second, server);
    MimitaNet::netShutdown();
    return firstSawSpawn && secondSawSpawn ? 0 : 6;
}
