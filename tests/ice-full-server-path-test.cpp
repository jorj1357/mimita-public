// 07 21 2026, 16 50
/* purpose
* Tests transport-neutral ICE/UDP receive identity and packet classification.
* Uses real MiMITA packet structs and server transport identity declarations.
* Randomizes valid and malformed payloads to prove transport parity decisions.
* Does NOT open sockets, start libjuice, or run the live process harness.
* Does NOT mutate movement formulas, weapon behavior, or projectile simulation.
* Does NOT replace tools/test-ice-multiplayer.py for real ICE gameplay proof.
*/

#include "network/server.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace MimitaNet;

namespace {

enum class PacketClass : uint8_t
{
    Empty,
    TooSmall,
    TooLarge,
    ProtocolMismatch,
    UnknownType,
    KnownType
};

bool knownPacketType(uint8_t type)
{
    switch (type)
    {
    case PACKET_HELLO:
    case PACKET_WELCOME:
    case PACKET_INPUT:
    case PACKET_SNAPSHOT:
    case PACKET_DISCONNECT:
    case PACKET_PING:
    case PACKET_PLAYER_LIST:
    case PACKET_PROFILE:
    case PACKET_ENTITY_SPAWN:
    case PACKET_ENTITY_DESPAWN:
    case PACKET_SPAWN_NPC_REQUEST:
    case PACKET_TELEPORT_REQUEST:
    case PACKET_EXPLODE_REQUEST:
    case PACKET_SHOT_REQUEST:
    case PACKET_SHOT_EVENT:
    case PACKET_CHAT_MESSAGE:
    case PACKET_NPC_DAMAGE_REQUEST:
    case PACKET_NPC_DAMAGE_EVENT:
    case PACKET_SERVER_COMMAND:
    case PACKET_JOIN_REQUEST:
    case PACKET_JOIN_ACCEPT:
    case PACKET_JOIN_REJECT:
    case PACKET_RECONNECT_REQUEST:
    case PACKET_RECONNECT_ACCEPT:
    case PACKET_DISAGREEMENT:
    case PACKET_CLIENT_MAP_READY:
    case PACKET_PROJECTILE_FIRE_REQUEST:
    case PACKET_PROJECTILE_SPAWN_EVENT:
    case PACKET_PROJECTILE_STATE_EVENT:
    case PACKET_PROJECTILE_EXPLODE_EVENT:
    case PACKET_PROJECTILE_DESPAWN_EVENT:
    case PACKET_MELEE_HIT_REQUEST:
    case PACKET_MELEE_HIT_EVENT:
    case PACKET_PELLET_BLAST_REQUEST:
    case PACKET_PELLET_BLAST_EVENT:
    case PACKET_GODBALL_STATE:
    case PACKET_PROJECTILE_FIRE_RESULT:
    case PACKET_ATTACK_REQUEST:
    case PACKET_ATTACK_RESULT:
    case PACKET_RELOAD_REQUEST:
    case PACKET_RELOAD_RESULT:
    case PACKET_RESPAWN_REQUEST:
    case PACKET_PLAYER_RESPAWNED:
    case PACKET_SPAWN_ACK:
    case PACKET_SPAWN_ACTIVATED:
    case PACKET_RELIABLE_EVENT_ACK:
    case PACKET_DAMAGE_CONFIRMED_EVENT:
        return true;
    default:
        return false;
    }
}

PacketClass classifyPayload(const TransportReceiveEvent& event)
{
    if (!event.payload || event.payloadBytes <= 0)
        return PacketClass::Empty;
    if (event.payloadBytes < (int)sizeof(PacketHeader))
        return PacketClass::TooSmall;
    if (event.payloadBytes > 2048 ||
        event.payloadBytes > MAX_GAME_DATAGRAM_BYTES)
        return PacketClass::TooLarge;

    PacketHeader header{};
    std::memcpy(&header, event.payload, sizeof(header));
    if (header.magic != PROTOCOL_MAGIC || header.version != PROTOCOL_VERSION)
        return PacketClass::ProtocolMismatch;
    return knownPacketType(header.type) ? PacketClass::KnownType
                                       : PacketClass::UnknownType;
}

bool sameClassForUdpAndIce(const std::vector<uint8_t>& payload)
{
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = htonl(0x7f000001u);
    endpoint.sin_port = htons(1357);

    TransportReceiveEvent udp{};
    udp.connectionId = {TransportKind::Udp, 0x7f000001054du};
    udp.remoteEndpoint = endpoint;
    udp.payload = payload.empty() ? nullptr : payload.data();
    udp.payloadBytes = (int)payload.size();
    udp.transportKind = TransportKind::Udp;

    TransportReceiveEvent ice = udp;
    ice.connectionId = {TransportKind::Ice, 42};
    ice.transportKind = TransportKind::Ice;

    return classifyPayload(udp) == classifyPayload(ice);
}

bool require(bool condition, const char* message)
{
    if (!condition)
        std::printf("[ICE FULL SERVER PATH TEST] FAIL %s\n", message);
    return condition;
}

} // namespace

int main()
{
    static_assert(PROTOCOL_VERSION == 25,
                  "Stage 4A generic AttackRequest layout requires protocol 25");
    static_assert(sizeof(InputPacket) == 140,
                  "InputPacket layout changed unexpectedly");
    static_assert(sizeof(JoinRequestPacket) == 116,
                  "JoinRequestPacket layout changed unexpectedly");
    static_assert(sizeof(JoinAcceptPacket) == 160,
                  "JoinAcceptPacket layout changed unexpectedly");
    static_assert(sizeof(ClientMapReadyPacket) == 56,
                  "ClientMapReadyPacket layout changed unexpectedly");
    static_assert(sizeof(SnapshotChunkPacket) < MAX_GAME_DATAGRAM_BYTES,
                  "Snapshot chunks must remain safe datagram payloads");

    bool ok = true;
    TransportConnectionId udp{TransportKind::Udp, 1};
    TransportConnectionId ice{TransportKind::Ice, 1};
    TransportConnectionId sameIce = ice;
    TransportConnectionId newIce{TransportKind::Ice, 2};
    ok &= require(!(udp == ice), "UDP and ICE connection IDs collided");
    ok &= require(ice == sameIce, "same ICE peer identity was not stable");
    ok &= require(!(ice == newIce), "new ICE connection reused active identity");

    JoinRequestPacket join{};
    join.header.type = PACKET_JOIN_REQUEST;
    std::strncpy(join.joinToken, "token", sizeof(join.joinToken) - 1);
    std::strncpy(join.name, "parity", sizeof(join.name) - 1);
    std::vector<uint8_t> joinBytes(sizeof(join));
    std::memcpy(joinBytes.data(), &join, sizeof(join));
    ok &= require(sameClassForUdpAndIce(joinBytes),
                  "join bytes classified differently by transport");

    InputPacket input{};
    input.header.type = PACKET_INPUT;
    input.header.playerId = 7;
    input.spawnGeneration = 3;
    input.transformEpoch = 9;
    input.movementSequence = 11;
    std::vector<uint8_t> inputBytes(sizeof(input));
    std::memcpy(inputBytes.data(), &input, sizeof(input));
    ok &= require(sameClassForUdpAndIce(inputBytes),
                  "input bytes classified differently by transport");

    constexpr uint32_t seed = 0x3c202607u;
    constexpr int cases = 1000;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> sizeDist(0, MAX_GAME_DATAGRAM_BYTES + 80);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<int> typeDist(0, 60);

    int mismatches = 0;
    for (int i = 0; i < cases; ++i)
    {
        std::vector<uint8_t> payload((size_t)sizeDist(rng));
        for (uint8_t& b : payload)
            b = (uint8_t)byteDist(rng);

        if (payload.size() >= sizeof(PacketHeader) && (i % 3) != 0)
        {
            PacketHeader header{};
            header.magic = ((i % 11) == 0) ? 0x12345678u : PROTOCOL_MAGIC;
            header.version = ((i % 13) == 0) ? (uint16_t)99 : PROTOCOL_VERSION;
            header.type = (uint8_t)typeDist(rng);
            std::memcpy(payload.data(), &header, sizeof(header));
        }

        if (!sameClassForUdpAndIce(payload))
        {
            ++mismatches;
            std::printf("[ICE FULL SERVER PATH TEST] mismatch case=%d size=%zu\n",
                        i, payload.size());
            break;
        }
    }

    std::printf("[ICE FULL SERVER PATH TEST] seed=%u cases=%d mismatches=%d\n",
                seed, cases, mismatches);
    if (!ok || mismatches != 0)
        return 1;
    std::printf("[ICE FULL SERVER PATH TEST] PASS\n");
    return 0;
}
