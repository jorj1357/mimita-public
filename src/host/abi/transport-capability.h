// 2026-11-12
// Purpose: opaque transport capability for a replaceable game.dll.
//
// Boundary rule:
//   - The resident host owns sockets, ICE, framing, encryption, channels,
//     reliability/ordering, peer/session state, timeouts, and overflow limits.
//   - game.dll owns packet ids, field schemas, encode/decode, handlers, map-sync,
//     reconnect/error policy, and gameplay authority.
//   - The host routes opaque {channel, typeId, bytes} and never parses payloads.
//
// All values are plain/fixed-width. No SOCKET, sockaddr, std::string, STL
// container, exception, or game-DLL-owned pointer is retained across the call.

#pragma once

#include <cstddef>
#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kTransportCapabilityVersion = 1u;
inline constexpr std::uint32_t kEventTransportMessage = 721u;
inline constexpr std::uint32_t kEventTransportDispatch = 722u;
inline constexpr std::uint32_t kTransportMaxPayloadBytes = 64u * 1024u;
inline constexpr std::uint32_t kTransportMaxResponseBytes = 4096u;

using PeerHandle = std::uint64_t;
using SessionHandle = std::uint64_t;
inline constexpr PeerHandle kInvalidPeer = 0u;

enum class TransportResult : std::int32_t {
    Ok = 0,
    Busy = 1,
    InvalidArgument = 2,
    NotFound = 3,
    Backpressure = 4,
    Unsupported = 5,
    TransportError = 6,
};

enum class TransportChannel : std::uint8_t {
    Control = 0,
    Input = 1,
    Snapshot = 2,
    ReliableEvent = 3,
    Avatar = 4,
    Media = 5,
};

enum class TransportFlags : std::uint32_t {
    None = 0,
    Reliable = 1u << 0,
    Ordered = 1u << 1,
    Sequenced = 1u << 2,
};

inline constexpr std::uint32_t to_bits(TransportFlags value) {
    return static_cast<std::uint32_t>(value);
}

// Inbound message delivered to game.dll through GameModuleV1::on_event.
// `payload` is valid only for the duration of the call; copy it to retain.
struct TransportMessageV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    SessionHandle session;
    PeerHandle peer;       // sender
    std::uint32_t typeId;  // game-owned packet id
    std::uint32_t flags;
    std::uint32_t sequence;
    std::uint32_t payloadSize;
    const std::uint8_t* payload;
    std::uint64_t receivedAtMs;
};

// Host-owned response envelope. The game writes only the bytes and type id;
// the resident server sends them to the exact endpoint that delivered the
// inbound message. This keeps SOCKET/sockaddr_in out of game.dll.
struct TransportResponseV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t typeId;
    std::uint32_t flags;
    std::uint32_t payloadSize;
    std::uint8_t payload[kTransportMaxResponseBytes];
};

struct TransportDispatchV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t serverTick;
    TransportMessageV1 message;
    TransportResponseV1 response;
};

struct SessionStateV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    SessionHandle session;
    std::uint32_t role;        // 0 = client, 1 = server, 2 = listen server
    std::uint32_t connected;   // 0/1
    std::uint32_t peerCount;
    std::uint32_t localPlayerId;
    std::uint32_t tick;
    std::uint32_t protocolVersion;
    char mapIdHash[65];
    char schemaHash[65];
    char generationHash[65];
    std::uint64_t sessionAgeMs;
};

struct PeerStateV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    PeerHandle peer;
    std::uint32_t playerId;
    std::uint32_t connected;
    std::uint32_t transportKind;  // 0 = loopback, 1 = UDP, 2 = ICE
    std::uint32_t rttMs;
    std::uint64_t lastReceiveMs;
};

// Transport event kinds delivered separately from message payloads.
enum class TransportEventKind : std::int32_t {
    PeerJoined = 1,
    PeerLeft = 2,
    Timeout = 3,
    ReconnectRequired = 4,
    Overflow = 5,
    TransportError = 6,
};

struct TransportEventV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    SessionHandle session;
    PeerHandle peer;
    TransportEventKind kind;
    std::int32_t reason;  // host-defined bounded reason
};

struct TransportCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    TransportResult (*send)(void* hostUserData, PeerHandle peer,
                            TransportChannel channel, std::uint32_t typeId,
                            std::uint32_t flags, const std::uint8_t* payload,
                            std::uint32_t payloadSize);
    TransportResult (*broadcast)(void* hostUserData, TransportChannel channel,
                                 std::uint32_t typeId, std::uint32_t flags,
                                 const std::uint8_t* payload,
                                 std::uint32_t payloadSize);
    TransportResult (*send_to_player)(void* hostUserData, std::uint32_t playerId,
                                      std::uint32_t typeId, std::uint32_t flags,
                                      const std::uint8_t* payload,
                                      std::uint32_t payloadSize);
    TransportResult (*session_state)(void* hostUserData, SessionStateV1* out);
    TransportResult (*peer_state)(void* hostUserData, PeerHandle peer,
                                  PeerStateV1* out);
    std::uint32_t (*peer_count)(void* hostUserData);
};

}  // namespace mimita::host
