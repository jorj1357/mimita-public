// 07 19 2026, 12 20
/* purpose
* Focused UDP smoke for authoritative projectile terminal events.
* Verifies a generic rocket AttackRequest terminal event is broadcast to both clients.
* Confirms the terminal packet carries damage and a single authoritative projectile ID.
* Does NOT launch the server process, render pixels, or inspect OpenGL state.
* Does NOT trust client-reported impact or change server damage behavior.
* Does NOT replace deterministic projectile/unit tests.
*/

#include "network/net_common.h"
#include "network/packets.h"
#include "network/snapshot-chunks.h"
#include "network/movement-validation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

struct Client
{
    SOCKET socket = INVALID_SOCKET;
    uint32_t id = 0;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    uint32_t movementSequence = 1;
    std::string mapId;
    MimitaNet::SnapshotPacket snapshot{};
};

const MimitaNet::SnapshotEntity* findEntity(const Client& c, uint32_t id);

void copyName(char (&out)[MimitaNet::MAX_NAME_BYTES], const char* name)
{
    std::memset(out, 0, sizeof(out));
    std::strncpy(out, name, sizeof(out) - 1);
}

bool openClient(Client& c)
{
    c.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c.socket == INVALID_SOCKET || !MimitaNet::setNonBlocking(c.socket))
        return false;

    sockaddr_in bindAddress{};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddress.sin_port = htons(0);
    if (bind(c.socket, (sockaddr*)&bindAddress, sizeof(bindAddress)) ==
        SOCKET_ERROR)
    {
        std::printf("[projectile-terminal-smoke] bind failed error=%d\n",
                    WSAGetLastError());
        return false;
    }
    return true;
}

void sendHello(Client& c, const sockaddr_in& server, const char* name)
{
    MimitaNet::HelloPacket p{};
    p.header.type = MimitaNet::PACKET_HELLO;
    copyName(p.name, name);
    sendto(c.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
}

void sendMapReady(Client& c, const sockaddr_in& server)
{
    MimitaNet::ClientMapReadyPacket p{};
    p.header.type = MimitaNet::PACKET_CLIENT_MAP_READY;
    p.header.playerId = c.id;
    p.assignedPlayerId = c.id;
    copyName(p.mapId, c.mapId.empty() ? "funworld3" : c.mapId.c_str());
    sendto(c.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
}

void sendSpawnAck(Client& c, const MimitaNet::PlayerRespawnedPacket& spawn, const sockaddr_in& server)
{
    c.spawnGeneration = spawn.spawnGeneration;
    c.transformEpoch = spawn.transformEpoch;
    c.movementSequence = 1;
    MimitaNet::SpawnAckPacket p{};
    p.header.type = MimitaNet::PACKET_SPAWN_ACK;
    p.header.playerId = c.id;
    p.header.transformEpoch = c.transformEpoch;
    p.spawnGeneration = c.spawnGeneration;
    p.transformEpoch = c.transformEpoch;
    sendto(c.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
}

void syncMovementSequenceToSnapshot(Client& c, uint32_t serverTick)
{
    if (serverTick != 0)
        c.movementSequence = std::max(c.movementSequence, serverTick + 1u);
}

bool pumpJoin(Client& c, const sockaddr_in& server, uint64_t deadline)
{
    char buffer[16384];
    while (MimitaNet::nowMs() < deadline)
    {
        sockaddr_in from{};
        int fromLength = sizeof(from);
        int bytes = recvfrom(c.socket, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLength);
        if (bytes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        auto* h = reinterpret_cast<MimitaNet::PacketHeader*>(buffer);
        if (bytes < (int)sizeof(*h) || h->magic != MimitaNet::PROTOCOL_MAGIC)
            continue;
        if (h->type == MimitaNet::PACKET_WELCOME && bytes >= (int)sizeof(MimitaNet::WelcomePacket))
        {
            auto* w = reinterpret_cast<MimitaNet::WelcomePacket*>(buffer);
            c.id = w->assignedPlayerId;
            c.mapId = w->mapId[0] ? w->mapId : "funworld3";
            sendMapReady(c, server);
        }
        else if (h->type == MimitaNet::PACKET_PLAYER_RESPAWNED && bytes >= (int)sizeof(MimitaNet::PlayerRespawnedPacket))
        {
            sendSpawnAck(c, *reinterpret_cast<MimitaNet::PlayerRespawnedPacket*>(buffer), server);
        }
        else if (h->type == MimitaNet::PACKET_SNAPSHOT)
        {
            MimitaNet::SnapshotChunkPacket chunk{};
            if (bytes >= (int)sizeof(MimitaNet::SnapshotPacket))
            {
                c.snapshot = *reinterpret_cast<MimitaNet::SnapshotPacket*>(buffer);
                syncMovementSequenceToSnapshot(c, c.snapshot.header.tick);
            }
            else if (MimitaNet::parseSnapshotChunk(buffer, (size_t)bytes, chunk))
            {
                syncMovementSequenceToSnapshot(c, chunk.serverTick);
                MimitaNet::clearSnapshotPacket(c.snapshot, chunk.header.tick);
                MimitaNet::appendSnapshotChunkToPacket(chunk, c.snapshot);
            }
            if (c.id && c.snapshot.playerCount >= 2)
                return true;
        }
    }
    return false;
}

void sendPosition(Client& c, const sockaddr_in& server, float x, float y, float z)
{
    MimitaNet::InputPacket p{};
    p.header.type = MimitaNet::PACKET_INPUT;
    p.header.playerId = c.id;
    p.header.transformEpoch = c.transformEpoch;
    p.header.tick = c.movementSequence;
    p.clientPx = x;
    p.clientPy = y;
    p.clientPz = z;
    p.camForwardX = 1.0f;
    p.transformEpoch = c.transformEpoch;
    p.spawnGeneration = c.spawnGeneration;
    p.movementSequence = c.movementSequence++;
    p.clientSimulationTick = p.movementSequence;
    p.movementFlags =
        MimitaNet::MOVEMENT_REPORT_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_DOWN_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_FREEZE_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE;
    p.sizeScale = 1.0f;
    sendto(c.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
}

void sendEquip(Client& c, const sockaddr_in& server, int slot, uint16_t serial)
{
    MimitaNet::InputPacket p{};
    p.header.type = MimitaNet::PACKET_INPUT;
    p.header.playerId = c.id;
    p.header.transformEpoch = c.transformEpoch;
    p.header.tick = c.movementSequence;
    p.equippedSlot = (int16_t)slot;
    p.equipSerial = serial;
    if (const MimitaNet::SnapshotEntity* entity = findEntity(c, c.id))
    {
        p.clientPx = entity->px;
        p.clientPy = entity->py;
        p.clientPz = entity->pz;
    }
    p.transformEpoch = c.transformEpoch;
    p.spawnGeneration = c.spawnGeneration;
    p.movementSequence = c.movementSequence++;
    p.clientSimulationTick = p.movementSequence;
    p.movementFlags =
        MimitaNet::MOVEMENT_REPORT_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_DOWN_DASH_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_FREEZE_AVAILABLE |
        MimitaNet::MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE;
    p.sizeScale = 1.0f;
    sendto(c.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
}

const MimitaNet::SnapshotEntity* findEntity(const Client& c, uint32_t id)
{
    for (uint32_t i = 0; i < c.snapshot.entityCount && i < MimitaNet::MAX_SNAPSHOT_ENTITIES; ++i)
    {
        if (c.snapshot.entities[i].networkEntityId == id)
            return &c.snapshot.entities[i];
    }
    return nullptr;
}

bool sendRocket(Client& shooter, const Client& target, const sockaddr_in& server)
{
    const MimitaNet::SnapshotEntity* s = findEntity(shooter, shooter.id);
    (void)target;
    if (!s)
        return false;
    float ox = s->px;
    float oy = s->py;
    float oz = s->pz;

    MimitaNet::AttackRequestPacket p{};
    p.header.type = MimitaNet::PACKET_ATTACK_REQUEST;
    p.header.tick = shooter.movementSequence;
    p.header.playerId = shooter.id;
    p.requestId = 77;
    p.spawnGeneration = shooter.spawnGeneration;
    p.clientSimulationTick = shooter.movementSequence;
    p.basedOnInputSequence = (uint16_t)shooter.movementSequence;
    p.equippedSlot = 7;
    p.weaponDefNetworkId = 7;
    p.aimOriginX = ox;
    p.aimOriginY = oy;
    p.aimOriginZ = oz;
    p.aimDirZ = -1.0f;
    p.muzzlePosX = ox;
    p.muzzlePosY = oy;
    p.muzzlePosZ = oz;
    sendto(shooter.socket, (const char*)&p, sizeof(p), 0, (const sockaddr*)&server, sizeof(server));
    return true;
}

bool waitExplosion(Client& c, uint32_t& projectileId, int& targetDamage,
                   int& deliveries, bool dropFirst, uint64_t deadline)
{
    char buffer[16384];
    while (MimitaNet::nowMs() < deadline)
    {
        sockaddr_in from{};
        int fromLength = sizeof(from);
        int bytes = recvfrom(c.socket, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLength);
        if (bytes <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        auto* h = reinterpret_cast<MimitaNet::PacketHeader*>(buffer);
        if (bytes < (int)sizeof(*h) || h->magic != MimitaNet::PROTOCOL_MAGIC)
            continue;
        if (h->type != MimitaNet::PACKET_PROJECTILE_EXPLODE_EVENT ||
            bytes < (int)sizeof(MimitaNet::ProjectileExplodeEventPacket))
            continue;
        auto* e = reinterpret_cast<MimitaNet::ProjectileExplodeEventPacket*>(buffer);
        if (e->weapon != MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER || e->fireSerial != 77)
            continue;
        ++deliveries;
        if (dropFirst && deliveries == 1)
            continue;
        projectileId = e->projectileId;
        for (uint8_t i = 0; i < e->victimCount && i < MimitaNet::MAX_PROJECTILE_DAMAGE_RESULTS; ++i)
            targetDamage += e->victims[i].damage;
        return e->projectileId != 0 && e->victimCount > 0;
    }
    return false;
}

void closeClient(Client& c)
{
    if (c.socket != INVALID_SOCKET)
        closesocket(c.socket);
}

} // namespace

int main()
{
    if (!MimitaNet::netStartup())
        return 2;
    sockaddr_in server{};
    if (!MimitaNet::parseAddress("127.0.0.1:1357", server))
        return 3;

    Client a, b;
    if (!openClient(a) || !openClient(b))
        return 4;
    sendHello(a, server, "term_a");
    sendHello(b, server, "term_b");

    uint64_t joinDeadline = MimitaNet::nowMs() + 5000;
    bool readyA = false, readyB = false;
    while (MimitaNet::nowMs() < joinDeadline && (!readyA || !readyB))
    {
        readyA = pumpJoin(a, server, MimitaNet::nowMs() + 20) || readyA;
        readyB = pumpJoin(b, server, MimitaNet::nowMs() + 20) || readyB;
    }
    const uint64_t equipDeadline = MimitaNet::nowMs() + 500;
    uint16_t equipSerial = 51;
    while (MimitaNet::nowMs() < equipDeadline)
    {
        sendEquip(a, server, 7, equipSerial++);
        pumpJoin(a, server, MimitaNet::nowMs() + 20);
        pumpJoin(b, server, MimitaNet::nowMs() + 20);
    }

    bool rocketSent = sendRocket(a, b, server);
    uint32_t projectileA = 0, projectileB = 0;
    int damageA = 0, damageB = 0;
    int deliveriesA = 0, deliveriesB = 0;
    bool sawA = waitExplosion(a, projectileA, damageA, deliveriesA, true, MimitaNet::nowMs() + 3000);
    bool sawB = waitExplosion(b, projectileB, damageB, deliveriesB, false, MimitaNet::nowMs() + 3000);
    bool sameProjectile = projectileA != 0 && projectileA == projectileB;
    bool damageSeen = damageA > 0 && damageB > 0;

    std::printf("[PROJECTILE TERMINAL SMOKE] ready=%d/%d rocketSent=%d explode=%d/%d projectile=%u/%u damage=%d/%d deliveries=%d/%d sameId=%d\n",
        (int)readyA, (int)readyB, (int)rocketSent, (int)sawA, (int)sawB,
        projectileA, projectileB, damageA, damageB, deliveriesA, deliveriesB, (int)sameProjectile);

    closeClient(a);
    closeClient(b);
    MimitaNet::netShutdown();
    return readyA && readyB && rocketSent && sawA && sawB && deliveriesA >= 2 &&
           sameProjectile && damageSeen ? 0 : 1;
}
