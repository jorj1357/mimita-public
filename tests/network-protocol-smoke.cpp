// 07 19 2026, 09 29
/* purpose
* Local UDP protocol smoke test for server/client networking behavior.
* Verifies joins, snapshots, movement, combat, respawn, and projectile fire flow.
* Exercises grenade projectile accept, retry, rejection, second fire, and state packets.
* Does NOT launch the server process or configure deployment services.
* Does NOT test ICE relay behavior, browser UI, or rendering.
* Does NOT replace focused deterministic unit tests for individual subsystems.
*/

#include "network/net_common.h"
#include "network/packets.h"
#include "network/snapshot-chunks.h"

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
    std::string mapId;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    bool mapReadySent = false;
    MimitaNet::SnapshotPacket snapshot{};
};

const sockaddr_in* gServerAddress = nullptr;

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

void sendMapReady(TestClient& client, const sockaddr_in& server)
{
    if (!client.id || client.mapReadySent)
        return;

    MimitaNet::ClientMapReadyPacket ready{};
    ready.header.type = MimitaNet::PACKET_CLIENT_MAP_READY;
    ready.header.playerId = client.id;
    ready.assignedPlayerId = client.id;
    copyName(ready.mapId, client.mapId.empty() ? "funworld3" : client.mapId.c_str());
    sendto(client.socket, (const char*)&ready, sizeof(ready), 0,
           (const sockaddr*)&server, sizeof(server));
    client.mapReadySent = true;
}

void sendSpawnAck(TestClient& client,
                  const MimitaNet::PlayerRespawnedPacket& spawn,
                  const sockaddr_in& server)
{
    if (!client.id)
        return;

    client.spawnGeneration = spawn.spawnGeneration;
    client.transformEpoch = spawn.transformEpoch;

    MimitaNet::SpawnAckPacket ack{};
    ack.header.type = MimitaNet::PACKET_SPAWN_ACK;
    ack.header.playerId = client.id;
    ack.header.transformEpoch = client.transformEpoch;
    ack.spawnGeneration = client.spawnGeneration;
    ack.transformEpoch = client.transformEpoch;
    sendto(client.socket, (const char*)&ack, sizeof(ack), 0,
           (const sockaddr*)&server, sizeof(server));
}

bool pump(TestClient& client, uint64_t deadline, const sockaddr_in* server = nullptr)
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
        const sockaddr_in* serverForResponse = server ? server : gServerAddress;
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
            client.mapId = welcome->mapId[0] ? welcome->mapId : "funworld3";
            if (serverForResponse)
                sendMapReady(client, *serverForResponse);
        }
        else if (header->type == MimitaNet::PACKET_PLAYER_RESPAWNED &&
                 bytes >= (int)sizeof(MimitaNet::PlayerRespawnedPacket))
        {
            auto* spawn = reinterpret_cast<MimitaNet::PlayerRespawnedPacket*>(buffer);
            if (serverForResponse)
                sendSpawnAck(client, *spawn, *serverForResponse);
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
        else if (header->type == MimitaNet::PACKET_SNAPSHOT)
        {
            MimitaNet::SnapshotChunkPacket chunk{};
            if (!MimitaNet::parseSnapshotChunk(buffer, (size_t)bytes, chunk))
                continue;
            MimitaNet::clearSnapshotPacket(client.snapshot, chunk.header.tick);
            if (!MimitaNet::appendSnapshotChunkToPacket(chunk, client.snapshot))
                continue;
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
    input.header.transformEpoch = client.transformEpoch;
    input.camForwardX = 1.0f;
    input.clientPx = x;
    input.clientPy = y;
    input.clientPz = z;
    input.transformEpoch = client.transformEpoch;
    input.sizeScale = 1.0f;
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
    gServerAddress = &server;

    TestClient first;
    TestClient second;
    if (!openClient(first) || !openClient(second))
        return 3;

    sendHello(first, server);
    sendHello(second, server);

    bool firstReady = false;
    bool secondReady = false;
    const uint64_t deadline = MimitaNet::nowMs() + 4000;
    while (MimitaNet::nowMs() < deadline && (!firstReady || !secondReady))
    {
        firstReady = pump(first, MimitaNet::nowMs() + 30, &server) || firstReady;
        secondReady = pump(second, MimitaNet::nowMs() + 30, &server) || secondReady;
    }
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

    // ── Test 5: Grenade projectile fire + idempotent retry ────────────
    bool grenadeTestPassed = false;
    bool grenadeIdempotentPassed = false;
    bool grenadeRejectionPassed = false;
    bool grenadeSecondAcceptPassed = false;
    uint32_t firstProjectileId = 0;
    uint32_t secondProjectileId = 0;

    // Helper: send ProjectileFireRequestPacket for grenade launcher
    auto sendGrenadeRequest = [&](TestClient& client, uint32_t serial,
                                  float ox, float oy, float oz,
                                  float dx, float dy, float dz) {
        MimitaNet::ProjectileFireRequestPacket req{};
        req.header.type = MimitaNet::PACKET_PROJECTILE_FIRE_REQUEST;
        req.header.playerId = client.id;
        req.fireSerial = serial;
        req.lastServerTick = 0;
        req.weapon = MimitaNet::NETWORK_WEAPON_GRENADE_LAUNCHER;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 0.001f) { dx /= len; dy /= len; dz /= len; }
        req.originX = ox; req.originY = oy; req.originZ = oz;
        req.dirX = dx; req.dirY = dy; req.dirZ = dz;
        sendto(client.socket, (const char*)&req, sizeof(req), 0,
               (const sockaddr*)&server, sizeof(server));
    };

    // Helper: pump and collect projectile result
    struct PendingResult {
        uint32_t fireSerial = 0;
        uint32_t projectileId = 0;
        bool accepted = false;
        uint8_t reason = 0;
        bool received = false;
    };
    PendingResult lastResult;
    auto pumpExpectResult = [&](TestClient& client, uint32_t serial,
                                uint64_t timeoutMs) -> bool {
        const uint64_t deadline = MimitaNet::nowMs() + timeoutMs;
        while (MimitaNet::nowMs() < deadline)
        {
            char buffer[16384];
            sockaddr_in from{};
            int fromLength = sizeof(from);
            int bytes = recvfrom(client.socket, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLength);
            if (bytes <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
            auto* header = reinterpret_cast<MimitaNet::PacketHeader*>(buffer);
            if (bytes < (int)sizeof(*header) ||
                header->magic != MimitaNet::PROTOCOL_MAGIC ||
                header->version != MimitaNet::PROTOCOL_VERSION)
                continue;
            if (header->type == MimitaNet::PACKET_PROJECTILE_FIRE_RESULT &&
                bytes >= (int)sizeof(MimitaNet::ProjectileFireResultPacket))
            {
                auto* result = reinterpret_cast<MimitaNet::ProjectileFireResultPacket*>(buffer);
                if (result->fireSerial == serial)
                {
                    lastResult.fireSerial = result->fireSerial;
                    lastResult.projectileId = result->projectileId;
                    lastResult.accepted = result->accepted != 0;
                    lastResult.reason = result->reason;
                    lastResult.received = true;
                    return true;
                }
            }
        }
        return false;
    };

    // Equip grenade launcher (slot 8) by sending input packet with equipSerial
    {
        MimitaNet::InputPacket input{};
        input.header.type = MimitaNet::PACKET_INPUT;
        input.header.playerId = first.id;
        input.header.transformEpoch = first.transformEpoch;
        input.equippedSlot = 8;
        input.equipSerial = 42;
        input.transformEpoch = first.transformEpoch;
        if (const MimitaNet::SnapshotEntity* playerEntity = findEntity(first, first.id))
        {
            input.clientPx = playerEntity->px;
            input.clientPy = playerEntity->py;
            input.clientPz = playerEntity->pz;
        }
        input.camForwardX = 1.0f;
        input.sizeScale = 1.0f;
        sendto(first.socket, (const char*)&input, sizeof(input), 0,
               (const sockaddr*)&server, sizeof(server));
    }

    // Test 5a: Send serial 1, expect accepted
    float origin[3] = {0.0f, 0.0f, 32.0f};
    if (const MimitaNet::SnapshotEntity* playerEntity = findEntity(first, first.id))
    {
        origin[0] = playerEntity->px;
        origin[1] = playerEntity->py;
        origin[2] = playerEntity->pz + 0.8f;
    }
    sendGrenadeRequest(first, 1, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 1, 2000) && lastResult.accepted && lastResult.projectileId != 0)
    {
        grenadeTestPassed = true;
        firstProjectileId = lastResult.projectileId;
    }

    // Test 5b: Resend serial 1, expect same projectileId (idempotent)
    sendGrenadeRequest(first, 1, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 1, 2000) && lastResult.accepted)
    {
        grenadeIdempotentPassed = (lastResult.projectileId == firstProjectileId);
    }

    // Test 5c: Send serial 2 immediately (cooldown active), expect rejection
    sendGrenadeRequest(first, 2, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 2, 2000) && !lastResult.accepted)
    {
        grenadeRejectionPassed = true;
    }

    // Test 5d: Wait for cooldown (~1s), send serial 3, expect new projectile
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    sendGrenadeRequest(first, 3, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 3, 2000) && lastResult.accepted && lastResult.projectileId != 0)
    {
        grenadeSecondAcceptPassed = (lastResult.projectileId != firstProjectileId);
        secondProjectileId = lastResult.projectileId;
    }

    // Test 5e: Verify state packets arrive with position changes
    bool sawStatePackets = false;
    {
        const uint64_t stateDeadline = MimitaNet::nowMs() + 3000;
        int stateCount = 0;
        while (MimitaNet::nowMs() < stateDeadline && stateCount < 3)
        {
            char buffer[16384];
            sockaddr_in from{};
            int fromLength = sizeof(from);
            int bytes = recvfrom(first.socket, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLength);
            if (bytes <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            auto* header = reinterpret_cast<MimitaNet::PacketHeader*>(buffer);
            if (bytes < (int)sizeof(*header) ||
                header->magic != MimitaNet::PROTOCOL_MAGIC)
                continue;
            if (header->type == MimitaNet::PACKET_PROJECTILE_STATE_EVENT &&
                bytes >= (int)sizeof(MimitaNet::ProjectileStateEventPacket))
            {
                auto* state = reinterpret_cast<MimitaNet::ProjectileStateEventPacket*>(buffer);
                if (state->projectileId == secondProjectileId)
                    ++stateCount;
            }
            else if (header->type == MimitaNet::PACKET_PROJECTILE_EXPLODE_EVENT &&
                     bytes >= (int)sizeof(MimitaNet::ProjectileExplodeEventPacket))
            {
                auto* expl = reinterpret_cast<MimitaNet::ProjectileExplodeEventPacket*>(buffer);
                if (expl->projectileId == secondProjectileId)
                    ++stateCount; // count explosion as final state
            }
        }
        sawStatePackets = stateCount >= 3;
    }

    std::printf(
        "[PROTOCOL SMOKE] first=%u/%s second=%u/%s players=%u npcs=%u "
        "movement=%d spawned=%d/%d combatDeath=%d autoRespawn=%d "
        "instantRespawn=%d noDouble=%d livingNoEffect=%d "
        "grenadeAccept=%d grenadeIdempotent=%d grenadeReject=%d grenadeSecondAccept=%d "
        "statePackets=%d firstProjId=%u secondProjId=%u\n",
        first.id, first.approvedName.c_str(),
        second.id, second.approvedName.c_str(),
        first.snapshot.playerCount, first.snapshot.npcCount,
        (int)movementReplicated,
        (int)firstSawSpawn, (int)secondSawSpawn,
        (int)sawDeath, (int)sawAutoRespawn,
        (int)sawInstantRespawn, (int)noDoubleRespawn, (int)livingNoEffect,
        (int)grenadeTestPassed, (int)grenadeIdempotentPassed,
        (int)grenadeRejectionPassed, (int)grenadeSecondAcceptPassed,
        (int)sawStatePackets, firstProjectileId, secondProjectileId);

    disconnect(first, server);
    disconnect(second, server);
    MimitaNet::netShutdown();
    return movementReplicated &&
           firstSawSpawn && secondSawSpawn &&
           sawAutoRespawn && sawInstantRespawn &&
           noDoubleRespawn && livingNoEffect &&
           grenadeTestPassed && grenadeIdempotentPassed &&
           grenadeRejectionPassed && grenadeSecondAcceptPassed &&
           sawStatePackets ? 0 : 7;
}
