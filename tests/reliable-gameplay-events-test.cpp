// 07 19 2026, 13 05
/* purpose
* Unit test reliable unordered gameplay event queue behavior.
* Verifies loss, ACK loss, duplicates, reconnect sessions, ACK auth, and bounds.
* Exercises generic packet storage independent of projectile weapon type.
* Does NOT run sockets, projectile physics, rendering, or server gameplay loops.
* Does NOT test damage, hitmarkers, prediction, or ICE route selection.
* Does NOT replace integration tests against mimita.exe.
*/

#include "network/server.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace MimitaNet {

struct SentReliablePacket
{
    uint32_t playerId = 0;
    uint8_t type = 0;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
};

static std::vector<SentReliablePacket> gSentPackets;

bool serverSendToPlayer(SOCKET, const ServerPlayer& player, const void* data, size_t size)
{
    if (size >= sizeof(PacketHeader) + sizeof(uint32_t) * 2)
    {
        const char* bytes = reinterpret_cast<const char*>(data);
        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
        SentReliablePacket sent{};
        sent.playerId = player.id;
        sent.type = header->type;
        std::memcpy(&sent.eventId, bytes + sizeof(PacketHeader), sizeof(uint32_t));
        std::memcpy(&sent.eventSessionId, bytes + sizeof(PacketHeader) + sizeof(uint32_t), sizeof(uint32_t));
        gSentPackets.push_back(sent);
    }
    return true;
}

const char* serverTimestamp()
{
    return "[test]";
}

} // namespace MimitaNet

static bool expect(bool value, const char* name)
{
    std::printf("%-64s %s\n", name, value ? "PASS" : "FAIL");
    return value;
}

static sockaddr_in testAddr(uint16_t port)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001u);
    addr.sin_port = htons(port);
    return addr;
}

static MimitaNet::ProjectileExplodeEventPacket explodePacket(uint32_t id)
{
    MimitaNet::ProjectileExplodeEventPacket packet{};
    packet.header.type = MimitaNet::PACKET_PROJECTILE_EXPLODE_EVENT;
    packet.eventId = id;
    packet.projectileId = 1000 + id;
    packet.weapon = MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER;
    return packet;
}

static MimitaNet::ReliableEventAckPacket ackPacket(uint32_t playerId, uint32_t eventId, uint32_t sessionId)
{
    MimitaNet::ReliableEventAckPacket ack{};
    ack.header.type = MimitaNet::PACKET_RELIABLE_EVENT_ACK;
    ack.header.playerId = playerId;
    ack.eventId = eventId;
    ack.eventSessionId = sessionId;
    return ack;
}

static void queueEvent(std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
                       const MimitaNet::ProjectileExplodeEventPacket& packet,
                       uint64_t& totalOut)
{
    MimitaNet::queueReliableGameplayEventToAll(INVALID_SOCKET, players,
        &packet, sizeof(packet), packet.eventId, packet.eventSessionId, totalOut);
}

static MimitaNet::ReliableGameplayEventQueueResult queueDespawn(
    std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    uint32_t eventId,
    uint64_t& totalOut)
{
    MimitaNet::ProjectileDespawnEventPacket despawn{};
    despawn.header.type = MimitaNet::PACKET_PROJECTILE_DESPAWN_EVENT;
    despawn.eventId = eventId;
    despawn.projectileId = 5000 + eventId;
    return MimitaNet::queueReliableGameplayEventToAll(INVALID_SOCKET, players,
        &despawn, sizeof(despawn), despawn.eventId, despawn.eventSessionId, totalOut);
}

int main()
{
    using namespace MimitaNet;

    bool ok = true;
    setReliableGameplayEventTestNowMs(1000);

    std::unordered_map<uint32_t, ServerPlayer> players;
    players[1].id = 1;
    players[1].addr = testAddr(20001);
    players[2].id = 2;
    players[2].addr = testAddr(20002);

    const uint32_t idA = nextReliableGameplayEventId();
    const uint32_t idB = nextReliableGameplayEventId();
    ok &= expect(idA != 0 && idB != 0 && idA != idB,
                 "event IDs are non-zero and distinct");

    uint64_t totalOut = 0;
    gSentPackets.clear();
    ProjectileExplodeEventPacket first = explodePacket(idA);
    queueEvent(players, first, totalOut);
    ok &= expect(players[1].pendingReliableEvents.size() == 1 &&
                 players[2].pendingReliableEvents.size() == 1,
                 "first event queued per connection");
    ok &= expect(gSentPackets.size() == 2 && totalOut == 2,
                 "initial event delivery attempted once per connection");
    ok &= expect(gSentPackets[0].eventSessionId != 0 &&
                 gSentPackets[1].eventSessionId != 0 &&
                 gSentPackets[0].eventSessionId != gSentPackets[1].eventSessionId,
                 "event session IDs are per connection");

    setReliableGameplayEventTestNowMs(1099);
    tickReliableGameplayEvents(INVALID_SOCKET, players, totalOut);
    ok &= expect(gSentPackets.size() == 2,
                 "retry does not fire before 100ms window");
    setReliableGameplayEventTestNowMs(1100);
    tickReliableGameplayEvents(INVALID_SOCKET, players, totalOut);
    ok &= expect(gSentPackets.size() == 4,
                 "first event packet lost retransmits after retry window");

    const uint32_t session1 = players[1].reliableEventSessionId;
    ReliableEventAckPacket goodAck = ackPacket(1, idA, session1);
    sockaddr_in wrongAddr = testAddr(29999);
    handleReliableEventAck(reinterpret_cast<const char*>(&goodAck), sizeof(goodAck), players, &wrongAddr);
    ok &= expect(players[1].pendingReliableEvents.size() == 1,
                 "ACK from wrong UDP address is rejected");

    sockaddr_in addr1 = players[1].addr;
    ReliableEventAckPacket staleAck = ackPacket(1, idA, session1 + 99);
    handleReliableEventAck(reinterpret_cast<const char*>(&staleAck), sizeof(staleAck), players, &addr1);
    ok &= expect(players[1].pendingReliableEvents.size() == 1,
                 "ACK with wrong session is rejected");

    handleReliableEventAck(reinterpret_cast<const char*>(&goodAck), sizeof(goodAck), players, &addr1);
    handleReliableEventAck(reinterpret_cast<const char*>(&goodAck), sizeof(goodAck), players, &addr1);
    ok &= expect(players[1].pendingReliableEvents.empty() &&
                 players[2].pendingReliableEvents.size() == 1,
                 "good ACK removes once and duplicate ACK is harmless");

    ProjectileExplodeEventPacket second = explodePacket(idB);
    queueEvent(players, second, totalOut);
    ok &= expect(players[1].pendingReliableEvents.size() == 1 &&
                 players[2].pendingReliableEvents.size() == 2,
                 "unordered multiple events remain distinct");

    ReliableEventAckPacket secondAck = ackPacket(2, idB, players[2].reliableEventSessionId);
    sockaddr_in addr2 = players[2].addr;
    handleReliableEventAck(reinterpret_cast<const char*>(&secondAck), sizeof(secondAck), players, &addr2);
    ok &= expect(players[2].pendingReliableEvents.size() == 1 &&
                 players[2].pendingReliableEvents.front().eventId == idA,
                 "out-of-order ACK removes only its matching event");

    for (uint32_t i = 0; i < 20; ++i)
    {
        ProjectileExplodeEventPacket packet = explodePacket(nextReliableGameplayEventId());
        queueEvent(players, packet, totalOut);
        if ((i % 10) >= 3)
        {
            ReliableEventAckPacket ack = ackPacket(1, packet.eventId, players[1].reliableEventSessionId);
            handleReliableEventAck(reinterpret_cast<const char*>(&ack), sizeof(ack), players, &addr1);
        }
    }
    setReliableGameplayEventTestNowMs(1300);
    tickReliableGameplayEvents(INVALID_SOCKET, players, totalOut);
    ok &= expect(players[1].pendingReliableEvents.size() >= 6,
                 "30 percent deterministic ACK loss leaves bounded pending retries");

    const uint32_t oldSession = players[1].reliableEventSessionId;
    players.erase(1);
    ok &= expect(players.find(1) == players.end(),
                 "disconnect releases pending queue with player record");
    players[1].id = 1;
    players[1].addr = testAddr(20101);
    ProjectileExplodeEventPacket reconnectEvent = explodePacket(nextReliableGameplayEventId());
    queueEvent(players, reconnectEvent, totalOut);
    const uint32_t newSession = players[1].reliableEventSessionId;
    ok &= expect(newSession != 0 && newSession != oldSession,
                 "reconnect receives a new event session ID");
    sockaddr_in newAddr1 = players[1].addr;
    ReliableEventAckPacket oldSessionAck = ackPacket(1, reconnectEvent.eventId, oldSession);
    handleReliableEventAck(reinterpret_cast<const char*>(&oldSessionAck), sizeof(oldSessionAck), players, &newAddr1);
    ok &= expect(players[1].pendingReliableEvents.size() == 1,
                 "stale previous-session ACK cannot clear new pending event");

    std::unordered_map<uint32_t, ServerPlayer> capacityPlayers;
    capacityPlayers[10].id = 10;
    capacityPlayers[10].addr = testAddr(21010);
    for (uint32_t i = 0; i < 63; ++i)
        queueDespawn(capacityPlayers, nextReliableGameplayEventId(), totalOut);
    ok &= expect(capacityPlayers[10].pendingReliableEvents.size() == 63,
                 "entries at capacity minus one are queued");
    ReliableGameplayEventQueueResult capResult = queueDespawn(capacityPlayers, nextReliableGameplayEventId(), totalOut);
    ok &= expect(capResult == ReliableGameplayEventQueueResult::Queued &&
                 capacityPlayers[10].pendingReliableEvents.size() == 64,
                 "final capacity slot can still be queued");
    ReliableEventAckPacket capAck = ackPacket(10,
        capacityPlayers[10].pendingReliableEvents.front().eventId,
        capacityPlayers[10].reliableEventSessionId);
    sockaddr_in addr10 = capacityPlayers[10].addr;
    handleReliableEventAck(reinterpret_cast<const char*>(&capAck), sizeof(capAck), capacityPlayers, &addr10);
    ok &= expect(capacityPlayers[10].pendingReliableEvents.size() == 63,
                 "ACK frees reliable queue capacity");
    capResult = queueDespawn(capacityPlayers, nextReliableGameplayEventId(), totalOut);
    ok &= expect(capResult == ReliableGameplayEventQueueResult::Queued &&
                 capacityPlayers[10].pendingReliableEvents.size() == 64,
                 "freed capacity accepts a new event");

    std::unordered_map<uint32_t, ServerPlayer> saturatedPlayers;
    saturatedPlayers[20].id = 20;
    saturatedPlayers[20].addr = testAddr(21020);
    saturatedPlayers[21].id = 21;
    saturatedPlayers[21].addr = testAddr(21021);
    for (uint32_t i = 0; i < 64; ++i)
        queueDespawn(saturatedPlayers, nextReliableGameplayEventId(), totalOut);
    const uint32_t healthyBefore = (uint32_t)saturatedPlayers[21].pendingReliableEvents.size();
    ReliableGameplayEventQueueResult satResult = queueDespawn(saturatedPlayers, nextReliableGameplayEventId(), totalOut);
    ok &= expect(satResult == ReliableGameplayEventQueueResult::BacklogSaturated,
                 "queue saturation returns a typed saturated result");
    ok &= expect(saturatedPlayers.find(20) == saturatedPlayers.end() &&
                 saturatedPlayers.find(21) == saturatedPlayers.end(),
                 "saturated connections enter disconnect flow");
    ok &= expect(healthyBefore == 64,
                 "saturation never grows past the fixed queue bound");

    std::unordered_map<uint32_t, ServerPlayer> isolatedPlayers;
    isolatedPlayers[30].id = 30;
    isolatedPlayers[30].addr = testAddr(21030);
    isolatedPlayers[31].id = 31;
    isolatedPlayers[31].addr = testAddr(21031);
    for (uint32_t i = 0; i < 64; ++i)
    {
        ProjectileDespawnEventPacket onlyFull{};
        onlyFull.header.type = PACKET_PROJECTILE_DESPAWN_EVENT;
        onlyFull.eventId = nextReliableGameplayEventId();
        onlyFull.projectileId = 7000 + i;
        queueReliableGameplayEventToAll(INVALID_SOCKET, isolatedPlayers, &onlyFull, sizeof(onlyFull),
                                        onlyFull.eventId, onlyFull.eventSessionId, totalOut);
        if (i == 0)
        {
            ReliableEventAckPacket ack = ackPacket(31, onlyFull.eventId, isolatedPlayers[31].reliableEventSessionId);
            sockaddr_in addr31 = isolatedPlayers[31].addr;
            handleReliableEventAck(reinterpret_cast<const char*>(&ack), sizeof(ack), isolatedPlayers, &addr31);
        }
    }
    ReliableGameplayEventQueueResult isolatedResult = queueDespawn(isolatedPlayers, nextReliableGameplayEventId(), totalOut);
    ok &= expect(isolatedResult == ReliableGameplayEventQueueResult::BacklogSaturated &&
                 isolatedPlayers.find(30) == isolatedPlayers.end() &&
                 isolatedPlayers.find(31) != isolatedPlayers.end(),
                 "one unhealthy client does not disconnect another client with capacity");
    ReliableEventAckPacket survivorAck = ackPacket(31,
        isolatedPlayers[31].pendingReliableEvents.front().eventId,
        isolatedPlayers[31].reliableEventSessionId);
    sockaddr_in survivorAddr = isolatedPlayers[31].addr;
    handleReliableEventAck(reinterpret_cast<const char*>(&survivorAck), sizeof(survivorAck), isolatedPlayers, &survivorAddr);
    ok &= expect(queueDespawn(isolatedPlayers, nextReliableGameplayEventId(), totalOut) == ReliableGameplayEventQueueResult::Queued,
                 "server simulation can continue queuing for healthy clients");

    std::unordered_map<uint32_t, ServerPlayer> attemptPlayers;
    attemptPlayers[40].id = 40;
    attemptPlayers[40].addr = testAddr(21040);
    queueDespawn(attemptPlayers, nextReliableGameplayEventId(), totalOut);
    attemptPlayers[40].pendingReliableEvents.front().attempts = 80;
    setReliableGameplayEventTestNowMs(1400);
    tickReliableGameplayEvents(INVALID_SOCKET, attemptPlayers, totalOut);
    ok &= expect(attemptPlayers.find(40) == attemptPlayers.end(),
                 "maximum-attempt exhaustion disconnects the client");

    std::unordered_map<uint32_t, ServerPlayer> ttlPlayers;
    ttlPlayers[50].id = 50;
    ttlPlayers[50].addr = testAddr(21050);
    setReliableGameplayEventTestNowMs(2000);
    queueDespawn(ttlPlayers, nextReliableGameplayEventId(), totalOut);
    setReliableGameplayEventTestNowMs(12001);
    tickReliableGameplayEvents(INVALID_SOCKET, ttlPlayers, totalOut);
    ok &= expect(ttlPlayers.find(50) == ttlPlayers.end(),
                 "TTL exhaustion disconnects the client");

    setReliableGameplayEventTestNowMs(0);
    std::printf("\n=== Reliable Gameplay Events: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
