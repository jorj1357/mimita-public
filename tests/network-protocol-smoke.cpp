#include "network/net_common.h"
#include "network/packets.h"

#include <cstdio>
#include <chrono>
#include <cmath>
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

const MimitaNet::SnapshotEntity* findEntity(
    const TestClient& client,
    uint32_t entityId)
{
    for (uint32_t i = 0;
         i < client.snapshot.entityCount &&
         i < MimitaNet::MAX_SNAPSHOT_ENTITIES;
         ++i)
    {
        if (client.snapshot.entities[i].networkEntityId == entityId)
            return &client.snapshot.entities[i];
    }
    return nullptr;
}

void sendPosition(
    TestClient& client,
    const sockaddr_in& server,
    float x,
    float y,
    float z,
    uint16_t respawnSerial = 0)
{
    MimitaNet::InputPacket input{};
    input.header.type = MimitaNet::PACKET_INPUT;
    input.header.playerId = client.id;
    input.camForwardX = 1.0f;
    input.clientPx = x;
    input.clientPy = y;
    input.clientPz = z;
    input.respawnSerial = respawnSerial;
    sendto(client.socket, (const char*)&input, sizeof(input), 0,
           (const sockaddr*)&server, sizeof(server));
}

void sendShot(
    TestClient& client,
    const TestClient& target,
    const sockaddr_in& server,
    uint32_t serial)
{
    const MimitaNet::SnapshotEntity* shooterEntity =
        findEntity(client, client.id);
    const MimitaNet::SnapshotEntity* targetEntity =
        findEntity(client, target.id);
    if (!shooterEntity || !targetEntity)
        return;

    const float originX = shooterEntity->px;
    const float originY = shooterEntity->py;
    const float originZ = shooterEntity->pz + 0.8f;
    const float hitX = targetEntity->px;
    const float hitY = targetEntity->py;
    const float hitZ = targetEntity->pz + 0.8f;
    float dirX = hitX - originX;
    float dirY = hitY - originY;
    float dirZ = hitZ - originZ;
    const float length = std::sqrt(
        dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length <= 0.001f)
        return;
    dirX /= length;
    dirY /= length;
    dirZ /= length;

    MimitaNet::ShotRequestPacket shot{};
    shot.header.type = MimitaNet::PACKET_SHOT_REQUEST;
    shot.header.playerId = client.id;
    shot.shotSerial = serial;
    shot.targetPlayerId = target.id;
    shot.damage = 30;
    shot.power = 30.0f;
    shot.effectFlags =
        MimitaNet::SHOT_EFFECT_ENTITY_IMPACT |
        MimitaNet::SHOT_EFFECT_BLOOD |
        MimitaNet::SHOT_EFFECT_HIT_SOUND;
    shot.weapon = MimitaNet::NETWORK_WEAPON_REVOLVER;
    shot.impactType = MimitaNet::SHOT_IMPACT_ENTITY;
    shot.originX = originX;
    shot.originY = originY;
    shot.originZ = originZ;
    shot.hitX = hitX;
    shot.hitY = hitY;
    shot.hitZ = hitZ;
    shot.dirX = dirX;
    shot.dirY = dirY;
    shot.dirZ = dirZ;
    shot.normalX = -dirX;
    shot.normalY = -dirY;
    shot.normalZ = -dirZ;
    sendto(client.socket, (const char*)&shot, sizeof(shot), 0,
           (const sockaddr*)&server, sizeof(server));
}

int entityHealth(const TestClient& client, uint32_t entityId)
{
    for (uint32_t i = 0;
         i < client.snapshot.entityCount &&
         i < MimitaNet::MAX_SNAPSHOT_ENTITIES;
         ++i)
    {
        if (client.snapshot.entities[i].networkEntityId == entityId)
            return client.snapshot.entities[i].health;
    }
    return -1;
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
    if (!MimitaNet::parseAddress("127.0.0.1:1357", server))
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

    const MimitaNet::SnapshotEntity* initialFirst =
        findEntity(first, first.id);
    if (!initialFirst)
        return 6;
    const float initialFirstX = initialFirst->px;
    sendPosition(
        first, server,
        initialFirst->px + 1.0f,
        initialFirst->py,
        initialFirst->pz);
    bool movementReplicated = false;
    const uint64_t movementDeadline = MimitaNet::nowMs() + 1500;
    while (!movementReplicated && MimitaNet::nowMs() < movementDeadline)
    {
        pump(first, MimitaNet::nowMs() + 30);
        pump(second, MimitaNet::nowMs() + 30);
        const MimitaNet::SnapshotEntity* moved =
            findEntity(second, first.id);
        movementReplicated =
            moved && std::fabs(moved->px - initialFirstX) >= 0.5f;
    }

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

    bool sawDeath = false;
    for (int shot = 0; shot < 4 && !sawDeath; ++shot)
    {
        sendShot(first, second, server, (uint32_t)shot + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        pump(first, MimitaNet::nowMs() + 120);
        pump(second, MimitaNet::nowMs() + 120);
        sawDeath = entityHealth(first, second.id) == 0 ||
                   entityHealth(second, second.id) == 0;
    }
    const uint64_t deathDeadline = MimitaNet::nowMs() + 1000;
    while (!sawDeath && MimitaNet::nowMs() < deathDeadline)
    {
        pump(first, MimitaNet::nowMs() + 30);
        pump(second, MimitaNet::nowMs() + 30);
        sawDeath = entityHealth(first, second.id) == 0 ||
                   entityHealth(second, second.id) == 0;
    }

    // ── Test 1: Timed auto-respawn (no Space pressed) ────────────────
    bool sawAutoRespawn = false;
    const uint64_t autoRespawnDeadline = MimitaNet::nowMs() + 3500;
    while (sawDeath && !sawAutoRespawn && MimitaNet::nowMs() < autoRespawnDeadline)
    {
        pump(first, MimitaNet::nowMs() + 30);
        pump(second, MimitaNet::nowMs() + 30);
        sawAutoRespawn = entityHealth(first, second.id) == 100 &&
                         entityHealth(second, second.id) == 100;
    }

    // ── Test 2: Instant respawn via respawnSerial ────────────────────
    // Kill second again
    sawDeath = false;
    for (int shot = 0; shot < 4 && !sawDeath; ++shot)
    {
        sendShot(first, second, server, 100 + (uint32_t)shot + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        pump(first, MimitaNet::nowMs() + 120);
        pump(second, MimitaNet::nowMs() + 120);
        sawDeath = entityHealth(first, second.id) == 0;
    }
    const uint64_t death2Deadline = MimitaNet::nowMs() + 1000;
    while (!sawDeath && MimitaNet::nowMs() < death2Deadline)
    {
        pump(first, MimitaNet::nowMs() + 30);
        pump(second, MimitaNet::nowMs() + 30);
        sawDeath = entityHealth(first, second.id) == 0;
    }

    // Send respawnSerial=1 to request instant respawn
    bool sawInstantRespawn = false;
    const uint64_t instantRespawnDeadline = MimitaNet::nowMs() + 1000;
    if (sawDeath)
    {
        const MimitaNet::SnapshotEntity* deadEntity = findEntity(second, second.id);
        if (deadEntity)
        {
            sendPosition(second, server,
                         deadEntity->px, deadEntity->py, deadEntity->pz,
                         1 /* respawnSerial */);
        }
        while (!sawInstantRespawn && MimitaNet::nowMs() < instantRespawnDeadline)
        {
            pump(first, MimitaNet::nowMs() + 30);
            pump(second, MimitaNet::nowMs() + 30);
            sawInstantRespawn = entityHealth(first, second.id) == 100;
        }
    }

    // ── Test 3: Duplicate respawnSerial does not cause second respawn ──
    // Send the same serial again — should be ignored, health stays 100
    if (sawInstantRespawn)
    {
        const MimitaNet::SnapshotEntity* aliveEntity = findEntity(second, second.id);
        if (aliveEntity)
        {
            sendPosition(second, server,
                         aliveEntity->px, aliveEntity->py, aliveEntity->pz,
                         1 /* same serial, should be ignored */);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pump(first, MimitaNet::nowMs() + 120);
        pump(second, MimitaNet::nowMs() + 120);
    }
    bool noDoubleRespawn = entityHealth(first, second.id) == 100;

    // ── Test 4: Living player sending respawnSerial does nothing ──────
    sendPosition(first, server, 0.0f, 0.0f, 30.0f, 99 /* respawnSerial from living player */);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pump(first, MimitaNet::nowMs() + 120);
    pump(second, MimitaNet::nowMs() + 120);
    bool livingNoEffect = entityHealth(first, first.id) == 100;

    std::printf(
        "[PROTOCOL SMOKE] first=%u/%s second=%u/%s players=%u npcs=%u "
        "movement=%d spawned=%d/%d combatDeath=%d autoRespawn=%d "
        "instantRespawn=%d noDouble=%d livingNoEffect=%d\n",
        first.id, first.approvedName.c_str(),
        second.id, second.approvedName.c_str(),
        first.snapshot.playerCount, first.snapshot.npcCount,
        (int)movementReplicated,
        (int)firstSawSpawn, (int)secondSawSpawn,
        (int)sawDeath, (int)sawAutoRespawn,
        (int)sawInstantRespawn, (int)noDoubleRespawn, (int)livingNoEffect);

    disconnect(first, server);
    disconnect(second, server);
    MimitaNet::netShutdown();
    return movementReplicated &&
           firstSawSpawn && secondSawSpawn &&
           sawAutoRespawn && sawInstantRespawn &&
           noDoubleRespawn && livingNoEffect ? 0 : 7;
}
