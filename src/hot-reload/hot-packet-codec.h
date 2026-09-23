// 09 23 2026
/* purpose
* Define the generic, hot-replaceable packet codec boundary.
* The EXE owns the socket, framing, buffers, and this envelope layout. A hot
* module owns the inner schema layout and its encode/decode/validate policy.
* Adding a NEW packet schema must not require a new EXE call site: the cold
* receive/send path passes an opaque {schemaId, schemaVersion, bytes} to the
* hot codec registry and accepts opaque bytes back.
* Does NOT own sockets, queues, or transport; the envelope is the only shared
* wire contract and it never changes with a gameplay schema.
* Does NOT permit STL containers, owning pointers, or engine objects to cross
* the boundary; codecs operate on caller-owned POD buffers.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Outer envelope. Stable across every inner schema change. `schemaId` +
// `schemaVersion` select the codec; the payload bytes are otherwise opaque to
// the EXE. Never versioned by gameplay.
#pragma pack(push, 1)
struct PacketCodecEnvelopeV1 {
    std::uint32_t protocolFamily;    // gameHash("family.gameplay") etc.; 0 = any
    std::uint32_t schemaVersion;     // monotonically increasing per schema
    std::uint64_t schemaId;          // gameHash("packet.input") etc.
    std::uint32_t payloadSize;       // bytes of inner payload following the envelope
    std::uint32_t connectionId;      // connection/player id where relevant (0 = none)
    std::uint32_t packetSequence;    // per-connection send sequence
    std::uint32_t ackSequence;       // last acknowledged sequence (0 = none)
    std::uint32_t ackBits;           // 32-bit acknowledgement bitfield
    std::uint32_t reserved0;
    std::uint64_t serverTick;        // authoritative tick where known (0 = none)
    std::uint64_t clientTick;        // client simulation tick where known (0 = none)
    std::uint32_t generation;        // logical hot generation that encoded this
    std::uint32_t reserved1;
    std::uint64_t codeHash;          // low 64 bits of the encoding generation hash
    std::uint32_t payloadChecksum;   // FNV-1a over the payload (see packetCodecChecksum)
    std::uint32_t reserved2;
};
#pragma pack(pop)

static_assert(sizeof(PacketCodecEnvelopeV1) == 80,
              "PacketCodecEnvelopeV1 wire size changed");

// FNV-1a over the payload. One implementation shared by EXE and hot codecs so a
// checksum mismatch is a real corruption signal, not a policy difference.
inline std::uint32_t packetCodecChecksum(const void* data, std::uint32_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::uint32_t hash = 2166136261u;
    for (std::uint32_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

// Result of validating an envelope + payload against a schema. Explicit reasons
// so a peer never silently decodes an unknown or incompatible schema.
enum class PacketCompatibilityV1 : std::uint32_t {
    Compatible = 0,
    UnknownSchema = 1,       // no codec for this schemaId at all
    VersionTooOld = 2,       // codec exists but cannot read this older version
    VersionTooNew = 3,       // codec exists but is older than the sender
    PayloadSizeMismatch = 4, // envelope payloadSize disagrees with the buffer
    ChecksumMismatch = 5,    // payload corrupted in transit
    Malformed = 6,           // schema-specific validation failed
    Rejected = 7,            // policy rejection (caller reason)
};

// Codec calls. `host` is the kernel host pointer (may be null). All buffers are
// caller-owned POD; a codec must not retain a pointer past the call.
using GamePacketEncodeFn = bool (MIMITA_GAME_CALL *)(
    void* host, const PacketCodecEnvelopeV1* envelope, const void* src,
    std::uint32_t srcSize, std::uint8_t* out, std::uint32_t outCapacity,
    std::uint32_t* outSize);

using GamePacketDecodeFn = bool (MIMITA_GAME_CALL *)(
    void* host, const PacketCodecEnvelopeV1* envelope, const std::uint8_t* payload,
    std::uint32_t payloadSize, void* out, std::uint32_t outCapacity,
    std::uint32_t* outSize);

// Optional schema-specific validation (ranges, enums, NaNs). Return
// PacketCompatibilityV1::Compatible to accept. A null validate means the generic
// envelope/checksum checks are the whole validation.
using GamePacketValidateFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, const PacketCodecEnvelopeV1* envelope, const std::uint8_t* payload,
    std::uint32_t payloadSize);

struct GamePacketCodecDescriptorV1 {
    std::uint64_t schemaId;
    std::uint32_t schemaVersion;
    std::uint32_t minSupportedVersion;  // oldest version this codec can decode
    std::uint32_t flags;
    std::uint32_t reserved;
    GamePacketEncodeFn encode;
    GamePacketDecodeFn decode;
    GamePacketValidateFn validate;      // optional (may be null)
    const char* name;
};

// The one generic doorway. Resolve the provider by capability id and call it to
// find the codec for (schemaId, schemaVersion). Returns null when the active
// generation has no codec for that schema/version.
using GamePacketCodecLookupFn = const GamePacketCodecDescriptorV1* (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t schemaId, std::uint32_t schemaVersion);

// Capability ids. The lookup function is registered by a hot provider; the EXE
// resolves it per call and never caches it across a generation swap.
static constexpr std::uint64_t GAME_CAP_PACKET_CODECS = gameHash("net.packet-codecs");
static constexpr std::uint64_t GAME_SIG_PACKET_CODECS =
    gameHash("sig.net.packet-codecs.v1");

// Generic receive event. The payload is the raw [envelope][inner payload] bytes:
// hot code casts the first sizeof(PacketCodecEnvelopeV1) bytes to the envelope
// and reads the remainder as its schema payload. The kernel never knows the
// schema, so a new one needs no new event type or call site.
static constexpr std::uint64_t GAME_EVENT_HOT_PACKET = gameHash("net.packet");

// Generic reply capability: `net.packet-reply`. Hot code answers a received
// packet with bytes the kernel sends back to the originating connection (used
// for handshakes). The kernel owns the socket; the reply bytes may themselves
// be a hot-coded datagram. Registered by the EXE.
using GameHotPacketReplyFn = void (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t connectionId, const void* bytes, std::uint32_t size);
static constexpr std::uint64_t GAME_CAP_HOT_PACKET_REPLY =
    gameHash("net.packet-reply");

} // namespace MimitaNet
