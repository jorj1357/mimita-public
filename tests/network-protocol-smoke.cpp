// 07 19 2026, 09 29
/* purpose
* Local UDP protocol smoke test for server/client networking behavior.
* Verifies joins, snapshots, movement, lifecycle, respawn, reconnect, and projectile flow.
* Exercises grenade projectile accept, retry, rejection, second fire, and state packets.
* Does NOT launch the server process or configure deployment services.
* Does NOT test ICE relay behavior, browser UI, or rendering.
* Does NOT replace focused deterministic unit tests for individual subsystems.
*/

#include "network/net_common.h"
#include "network/packets.h"
#include "network/snapshot-chunks.h"
#include "network/movement-validation.h"

#include <cstdio>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
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
    std::string reconnectToken;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    uint32_t movementSequence = 1;
    bool hasReconnectRestore = false;
    float reconnectRestoreX = 0.0f;
    float reconnectRestoreY = 0.0f;
    float reconnectRestoreZ = 0.0f;
    bool hasLastInputPosition = false;
    float lastInputX = 0.0f;
    float lastInputY = 0.0f;
    float lastInputZ = 30.0f;
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
    if (client.socket == INVALID_SOCKET ||
        !MimitaNet::setNonBlocking(client.socket))
        return false;
    MimitaNet::disableUdpConnReset(client.socket);

    sockaddr_in bindAddress{};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddress.sin_port = htons(0);
    if (bind(client.socket, (sockaddr*)&bindAddress, sizeof(bindAddress)) ==
        SOCKET_ERROR)
    {
        std::printf("[network-protocol-smoke] bind failed error=%d\n",
                    WSAGetLastError());
        return false;
    }
    return true;
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
    client.movementSequence = 1;

    MimitaNet::SpawnAckPacket ack{};
    ack.header.type = MimitaNet::PACKET_SPAWN_ACK;
    ack.header.playerId = client.id;
    ack.header.transformEpoch = client.transformEpoch;
    ack.spawnGeneration = client.spawnGeneration;
    ack.transformEpoch = client.transformEpoch;
    sendto(client.socket, (const char*)&ack, sizeof(ack), 0,
           (const sockaddr*)&server, sizeof(server));
}

void syncMovementSequenceToSnapshot(TestClient& client, uint32_t serverTick)
{
    if (serverTick != 0)
        client.movementSequence = std::max(client.movementSequence, serverTick + 1u);
}

bool pump(TestClient& client,
          uint64_t deadline,
          const sockaddr_in* server = nullptr,
          bool returnOnReadySnapshot = true)
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
            client.reconnectToken = welcome->reconnectToken;
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
        else if (header->type == MimitaNet::PACKET_RECONNECT_ACCEPT &&
                 bytes >= (int)sizeof(MimitaNet::ReconnectAcceptPacket))
        {
            auto* accept = reinterpret_cast<MimitaNet::ReconnectAcceptPacket*>(buffer);
            const bool duplicateAccept =
                client.id == accept->assignedPlayerId &&
                client.reconnectToken == accept->reconnectToken &&
                client.hasReconnectRestore;
            client.id = accept->assignedPlayerId;
            client.approvedName = accept->approvedName;
            client.reconnectToken = accept->reconnectToken;
            client.spawnGeneration = accept->spawnGeneration;
            client.transformEpoch = accept->header.transformEpoch;
            if (!duplicateAccept)
            {
                const uint32_t freshSequence =
                    accept->header.tick != 0 ? accept->header.tick + 1u : 1u;
                client.movementSequence =
                    std::max(client.movementSequence, freshSequence);
            }
            client.hasReconnectRestore = true;
            client.reconnectRestoreX = accept->restorePx;
            client.reconnectRestoreY = accept->restorePy;
            client.reconnectRestoreZ = accept->restorePz;
        }
        else if (header->type == MimitaNet::PACKET_SNAPSHOT &&
                 bytes >= (int)sizeof(MimitaNet::SnapshotPacket))
        {
            client.snapshot =
                *reinterpret_cast<MimitaNet::SnapshotPacket*>(buffer);
            syncMovementSequenceToSnapshot(client, client.snapshot.header.tick);
            if (returnOnReadySnapshot &&
                client.id && client.snapshot.playerCount >= 2 &&
                client.snapshot.npcCount >= 3)
                return true;
        }
        else if (header->type == MimitaNet::PACKET_SNAPSHOT)
        {
            MimitaNet::SnapshotChunkPacket chunk{};
            if (!MimitaNet::parseSnapshotChunk(buffer, (size_t)bytes, chunk))
                continue;
            syncMovementSequenceToSnapshot(client, chunk.serverTick);
            MimitaNet::clearSnapshotPacket(client.snapshot, chunk.header.tick);
            if (!MimitaNet::appendSnapshotChunkToPacket(chunk, client.snapshot))
                continue;
            if (returnOnReadySnapshot &&
                client.id && client.snapshot.playerCount >= 2 &&
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
    uint16_t respawnSerial = 0,
    int16_t equippedSlot = 0,
    uint16_t equipSerial = 0)
{
    MimitaNet::InputPacket input{};
    input.header.type = MimitaNet::PACKET_INPUT;
    input.header.playerId = client.id;
    input.header.transformEpoch = client.transformEpoch;
    input.header.tick = client.movementSequence;
    input.camForwardX = 1.0f;
    input.clientPx = x;
    input.clientPy = y;
    input.clientPz = z;
    input.transformEpoch = client.transformEpoch;
    input.spawnGeneration = client.spawnGeneration;
    input.movementSequence = client.movementSequence++;
    input.clientSimulationTick = input.movementSequence;
    input.movementFlags =
        MimitaNet::MOVEMENT_REPORT_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_DOWN_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_FREEZE_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE;
    input.sizeScale = 1.0f;
    input.respawnSerial = respawnSerial;
    input.equippedSlot = equippedSlot;
    input.equipSerial = equipSerial;
    sendto(client.socket, (const char*)&input, sizeof(input), 0,
           (const sockaddr*)&server, sizeof(server));
    client.hasLastInputPosition = true;
    client.lastInputX = x;
    client.lastInputY = y;
    client.lastInputZ = z;
}

void sendExplode(TestClient& client, const sockaddr_in& server)
{
    MimitaNet::ExplodeRequestPacket request{};
    request.header.type = MimitaNet::PACKET_EXPLODE_REQUEST;
    request.header.playerId = client.id;
    sendto(client.socket, (const char*)&request, sizeof(request), 0,
           (const sockaddr*)&server, sizeof(server));
}

void sendReconnect(TestClient& client, const sockaddr_in& server,
                   const std::string& token)
{
    MimitaNet::ReconnectRequestPacket request{};
    request.header.type = MimitaNet::PACKET_RECONNECT_REQUEST;
    request.header.playerId = client.id;
    std::strncpy(request.reconnectToken, token.c_str(),
                 sizeof(request.reconnectToken) - 1);
    sendto(client.socket, (const char*)&request, sizeof(request), 0,
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

    const char* serverText = std::getenv("MIMITA_TEST_SERVER_ADDR");
    if (!serverText || !*serverText)
        serverText = "127.0.0.1:1357";

    sockaddr_in server{};
    if (!MimitaNet::parseAddress(serverText, server))
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
    sendExplode(second, server);
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
    sawDeath = false;
    sendExplode(second, server);
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
    float livingX = 0.0f;
    float livingY = 0.0f;
    float livingZ = 30.0f;
    if (first.hasLastInputPosition)
    {
        livingX = first.lastInputX;
        livingY = first.lastInputY;
        livingZ = first.lastInputZ;
    }
    else if (const MimitaNet::SnapshotEntity* livingEntity = findEntity(first, first.id))
    {
        livingX = livingEntity->px;
        livingY = livingEntity->py;
        livingZ = livingEntity->pz;
    }
    sendPosition(first, server, livingX, livingY, livingZ,
                 99 /* respawnSerial from living player */);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pump(first, MimitaNet::nowMs() + 120);
    pump(second, MimitaNet::nowMs() + 120);
    bool livingNoEffect = entityHealth(first, first.id) == 100;

    // ── Test 5: Reconnect preserves player id and accepts new input ──
    bool reconnectPassed = false;
    bool postReconnectMovement = false;
    bool reconnectAccepted = false;
    const uint32_t secondOriginalId = second.id;
    const std::string previousReconnectToken = second.reconnectToken;
    if (!previousReconnectToken.empty() && secondOriginalId != 0)
    {
        closesocket(second.socket);
        second.socket = INVALID_SOCKET;
        if (openClient(second))
        {
            second.id = secondOriginalId;
            second.reconnectToken = previousReconnectToken;
            sendReconnect(second, server, previousReconnectToken);

            const uint64_t reconnectDeadline = MimitaNet::nowMs() + 2000;
            uint64_t nextReconnectRetry = MimitaNet::nowMs() + 200;
            while (MimitaNet::nowMs() < reconnectDeadline)
            {
                if (MimitaNet::nowMs() >= nextReconnectRetry)
                {
                    sendReconnect(second, server, previousReconnectToken);
                    nextReconnectRetry = MimitaNet::nowMs() + 200;
                }
                pump(first, MimitaNet::nowMs() + 30);
                pump(second, MimitaNet::nowMs() + 30, &server, false);
                if (second.id == secondOriginalId &&
                    !second.reconnectToken.empty() &&
                    second.reconnectToken != previousReconnectToken)
                    break;
            }
            reconnectAccepted =
                second.id == secondOriginalId &&
                !second.reconnectToken.empty() &&
                second.reconnectToken != previousReconnectToken;

            float reconnectBaseX = second.reconnectRestoreX;
            float reconnectBaseY = second.reconnectRestoreY;
            float reconnectBaseZ = second.reconnectRestoreZ;
            bool hasReconnectBase = second.hasReconnectRestore;
            if (!hasReconnectBase)
            {
                const MimitaNet::SnapshotEntity* reconnectEntity =
                    findEntity(second, second.id);
                if (reconnectEntity)
                {
                    reconnectBaseX = reconnectEntity->px;
                    reconnectBaseY = reconnectEntity->py;
                    reconnectBaseZ = reconnectEntity->pz;
                    hasReconnectBase = true;
                }
            }

            if (hasReconnectBase)
            {
                sendPosition(second, server,
                             reconnectBaseX,
                             reconnectBaseY,
                             reconnectBaseZ);
                const uint64_t ackDeadline = MimitaNet::nowMs() + 500;
                while (MimitaNet::nowMs() < ackDeadline)
                {
                    pump(first, MimitaNet::nowMs() + 30);
                }

                const float beforeReconnectMoveX = reconnectBaseX;
                int movementStep = 0;
                const uint64_t moveDeadline = MimitaNet::nowMs() + 2600;
                while (!postReconnectMovement && MimitaNet::nowMs() < moveDeadline)
                {
                    const float offset = std::min(1.25f, 0.04f * (float)++movementStep);
                    sendPosition(second, server,
                                 reconnectBaseX + offset,
                                 reconnectBaseY,
                                 reconnectBaseZ);
                    pump(first, MimitaNet::nowMs() + 30);
                    pump(second, MimitaNet::nowMs() + 30, nullptr, false);
                    const MimitaNet::SnapshotEntity* seen =
                        findEntity(first, second.id);
                    const MimitaNet::SnapshotEntity* selfSeen =
                        findEntity(second, second.id);
                    postReconnectMovement =
                        (seen && std::fabs(seen->px - beforeReconnectMoveX) >= 0.5f) ||
                        (selfSeen && std::fabs(selfSeen->px - beforeReconnectMoveX) >= 0.5f);
                }
            }

            reconnectPassed =
                reconnectAccepted && postReconnectMovement;
        }
    }

    // ── Test 6: Grenade projectile fire + idempotent retry ────────────
    bool grenadeTestPassed = false;
    bool grenadeIdempotentPassed = false;
    bool grenadeRejectionPassed = false;
    bool grenadeSecondAcceptPassed = false;
    uint32_t firstProjectileId = 0;
    uint32_t secondProjectileId = 0;
    constexpr uint16_t GRENADE_LAUNCHER_DEF_NETWORK_ID = 8;

    // Helper: send generic AttackRequestPacket for grenade launcher
    auto sendGrenadeRequest = [&](TestClient& client, uint32_t serial,
                                  float ox, float oy, float oz,
                                  float dx, float dy, float dz) {
        MimitaNet::AttackRequestPacket req{};
        req.header.type = MimitaNet::PACKET_ATTACK_REQUEST;
        req.header.tick = client.movementSequence;
        req.header.playerId = client.id;
        req.requestId = serial;
        req.spawnGeneration = client.spawnGeneration;
        req.clientSimulationTick = client.movementSequence;
        req.basedOnInputSequence = (uint16_t)client.movementSequence;
        req.equippedSlot = 8;
        req.weaponDefNetworkId = GRENADE_LAUNCHER_DEF_NETWORK_ID;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 0.001f) { dx /= len; dy /= len; dz /= len; }
        req.aimOriginX = ox; req.aimOriginY = oy; req.aimOriginZ = oz;
        req.aimDirX = dx; req.aimDirY = dy; req.aimDirZ = dz;
        req.muzzlePosX = ox; req.muzzlePosY = oy; req.muzzlePosZ = oz;
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
            if (header->type == MimitaNet::PACKET_ATTACK_RESULT &&
                bytes >= (int)sizeof(MimitaNet::AttackResultPacket))
            {
                auto* result = reinterpret_cast<MimitaNet::AttackResultPacket*>(buffer);
                if (result->requestId == serial)
                {
                    lastResult.fireSerial = result->requestId;
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

    // Equip grenade launcher (slot 8) through the same accepted movement path
    // used by normal client input, retrying briefly because this smoke runs UDP.
    {
        float equipX = 0.0f;
        float equipY = 0.0f;
        float equipZ = 30.0f;
        if (const MimitaNet::SnapshotEntity* playerEntity = findEntity(first, first.id))
        {
            equipX = playerEntity->px;
            equipY = playerEntity->py;
            equipZ = playerEntity->pz;
        }
        else if (first.hasLastInputPosition)
        {
            equipX = first.lastInputX;
            equipY = first.lastInputY;
            equipZ = first.lastInputZ;
        }
        const uint64_t equipDeadline = MimitaNet::nowMs() + 500;
        uint16_t equipSerial = 42;
        while (MimitaNet::nowMs() < equipDeadline)
        {
            sendPosition(first, server, equipX, equipY, equipZ, 0, 8, equipSerial++);
            pump(first, MimitaNet::nowMs() + 30);
            pump(second, MimitaNet::nowMs() + 30);
        }
    }

    // Test 6a: Send serial 1, expect accepted
    float origin[3] = {0.0f, 0.0f, 32.0f};
    if (const MimitaNet::SnapshotEntity* playerEntity = findEntity(first, first.id))
    {
        origin[0] = playerEntity->px;
        origin[1] = playerEntity->py;
        origin[2] = playerEntity->pz + 0.8f;
    }
    else if (first.hasLastInputPosition)
    {
        origin[0] = first.lastInputX;
        origin[1] = first.lastInputY;
        origin[2] = first.lastInputZ + 0.8f;
    }
    sendGrenadeRequest(first, 1, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 1, 2000) && lastResult.accepted && lastResult.projectileId != 0)
    {
        grenadeTestPassed = true;
        firstProjectileId = lastResult.projectileId;
    }

    // Test 6b: Resend serial 1, expect same projectileId (idempotent)
    sendGrenadeRequest(first, 1, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 1, 2000) && lastResult.accepted)
    {
        grenadeIdempotentPassed = (lastResult.projectileId == firstProjectileId);
    }

    // Test 6c: Send serial 2 immediately (cooldown active), expect rejection
    sendGrenadeRequest(first, 2, origin[0], origin[1], origin[2], 1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 2, 2000) && !lastResult.accepted)
    {
        grenadeRejectionPassed = true;
    }

    // Test 6d: Wait just past cooldown, send serial 3, expect new projectile
    {
        const uint64_t secondFireDeadline = MimitaNet::nowMs() + 600;
        while (MimitaNet::nowMs() < secondFireDeadline)
        {
            pump(first, MimitaNet::nowMs() + 30);
            pump(second, MimitaNet::nowMs() + 30);
        }
    }
    float secondOrigin[3] = {origin[0], origin[1], origin[2]};
    if (const MimitaNet::SnapshotEntity* playerEntity = findEntity(first, first.id))
    {
        secondOrigin[0] = playerEntity->px;
        secondOrigin[1] = playerEntity->py;
        secondOrigin[2] = playerEntity->pz + 0.8f;
    }
    sendGrenadeRequest(first, 3, secondOrigin[0], secondOrigin[1], secondOrigin[2],
                       1.0f, 0.0f, 0.0f);
    if (pumpExpectResult(first, 3, 2000) && lastResult.accepted && lastResult.projectileId != 0)
    {
        grenadeSecondAcceptPassed = (lastResult.projectileId != firstProjectileId);
        secondProjectileId = lastResult.projectileId;
    }

    // Test 6e: Verify state packets arrive with position changes
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
        "movement=%d spawned=%d/%d lifecycleDeath=%d autoRespawn=%d "
        "instantRespawn=%d noDouble=%d livingNoEffect=%d reconnectAccepted=%d reconnect=%d reconnectMovement=%d "
        "grenadeAccept=%d grenadeIdempotent=%d grenadeReject=%d grenadeSecondAccept=%d "
        "statePackets=%d firstProjId=%u secondProjId=%u\n",
        first.id, first.approvedName.c_str(),
        second.id, second.approvedName.c_str(),
        first.snapshot.playerCount, first.snapshot.npcCount,
        (int)movementReplicated,
        (int)firstSawSpawn, (int)secondSawSpawn,
        (int)sawDeath, (int)sawAutoRespawn,
        (int)sawInstantRespawn, (int)noDoubleRespawn, (int)livingNoEffect,
        (int)reconnectAccepted,
        (int)reconnectPassed, (int)postReconnectMovement,
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
           reconnectPassed && postReconnectMovement &&
           grenadeTestPassed && grenadeIdempotentPassed &&
           grenadeRejectionPassed && grenadeSecondAcceptPassed &&
           sawStatePackets ? 0 : 7;
}
