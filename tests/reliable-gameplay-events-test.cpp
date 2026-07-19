// 07 19 2026, 13 05
/* purpose
* Unit test reliable unordered gameplay event queue behavior.
* Verifies event IDs, retransmission after simulated loss, ACK removal, and bounds.
* Exercises generic packet storage independent of projectile weapon type.
* Does NOT run sockets, projectile physics, rendering, or server gameplay loops.
* Does NOT test damage, hitmarkers, prediction, or ICE route selection.
* Does NOT replace integration tests against mimita.exe.
*/

#include "network/server.h"

#include <cstdio>

namespace MimitaNet {

static int gSentPackets = 0;

bool serverSendToPlayer(SOCKET, const ServerPlayer&, const void*, size_t)
{
    ++gSentPackets;
    return true;
}

const char* serverTimestamp()
{
    return "[test]";
}

} // namespace MimitaNet

static bool expect(bool value, const char* name)
{
    std::printf("%-56s %s\n", name, value ? "PASS" : "FAIL");
    return value;
}

int main()
{
    using namespace MimitaNet;

    bool ok = true;
    std::unordered_map<uint32_t, ServerPlayer> players;
    players[1].id = 1;
    players[2].id = 2;

    ProjectileExplodeEventPacket first{};
    first.header.type = PACKET_PROJECTILE_EXPLODE_EVENT;
    first.eventId = nextReliableGameplayEventId();
    first.eventSessionId = serverReliableEventSessionId();
    first.projectileId = 10;
    first.weapon = NETWORK_WEAPON_ROCKET_LAUNCHER;

    ProjectileExplodeEventPacket second = first;
    second.eventId = nextReliableGameplayEventId();
    second.projectileId = 11;
    second.weapon = NETWORK_WEAPON_GRENADE_LAUNCHER;

    ok &= expect(first.eventId != 0 && second.eventId != 0 && first.eventId != second.eventId,
                 "multiple terminal events have distinct event IDs");

    gSentPackets = 0;
    uint64_t totalOut = 0;
    queueReliableGameplayEventToAll(INVALID_SOCKET, players, &first, sizeof(first),
                                    first.eventId, first.eventSessionId, totalOut);
    ok &= expect(players[1].pendingReliableEvents.size() == 1 &&
                 players[2].pendingReliableEvents.size() == 1,
                 "first terminal queued per connection");
    ok &= expect(gSentPackets == 2 && totalOut == 2,
                 "initial delivery attempted for every connection");

    for (auto& kv : players)
        kv.second.pendingReliableEvents.front().lastSendMs = 0;
    tickReliableGameplayEvents(INVALID_SOCKET, players, totalOut);
    ok &= expect(gSentPackets >= 4,
                 "dropped first terminal retransmits");

    ReliableEventAckPacket ack{};
    ack.header.type = PACKET_RELIABLE_EVENT_ACK;
    ack.header.playerId = 1;
    ack.eventId = first.eventId;
    ack.eventSessionId = first.eventSessionId;
    handleReliableEventAck(reinterpret_cast<const char*>(&ack), sizeof(ack), players);
    ok &= expect(players[1].pendingReliableEvents.empty() &&
                 players[2].pendingReliableEvents.size() == 1,
                 "ACK removes only that connection's pending event");

    queueReliableGameplayEventToAll(INVALID_SOCKET, players, &second, sizeof(second),
                                    second.eventId, second.eventSessionId, totalOut);
    ok &= expect(players[1].pendingReliableEvents.size() == 1 &&
                 players[2].pendingReliableEvents.size() == 2,
                 "simultaneous terminal events remain distinct");

    for (uint32_t i = 0; i < 80; ++i)
    {
        ProjectileDespawnEventPacket despawn{};
        despawn.header.type = PACKET_PROJECTILE_DESPAWN_EVENT;
        despawn.eventId = nextReliableGameplayEventId();
        despawn.eventSessionId = serverReliableEventSessionId();
        despawn.projectileId = 1000 + i;
        queueReliableGameplayEventToAll(INVALID_SOCKET, players, &despawn, sizeof(despawn),
                                        despawn.eventId, despawn.eventSessionId, totalOut);
    }
    ok &= expect(players[1].pendingReliableEvents.size() <= 64 &&
                 players[2].pendingReliableEvents.size() <= 64,
                 "pending reliable storage is bounded");

    std::printf("\n=== Reliable Gameplay Events: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
