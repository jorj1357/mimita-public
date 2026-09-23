// 09 23 2026
/* purpose
* Hot packet-codec module. Registers the generic `net.packet-codecs` lookup
* capability and one real codec (a bounded ping/pong schema). This is the proof
* that a packet schema is a hot source edit: adding a codec here (or in a new
* module file under this glob) requires no EXE call site or packet type.
* Does NOT own sockets, framing, or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include <cstring>
#include <vector>

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-packet-codec.h"
#include "hot-reload/hot-packet-schemas.h"

namespace {

using MimitaNet::GamePacketCodecDescriptorV1;
using MimitaNet::PacketCodecEnvelopeV1;
using MimitaNet::PacketCompatibilityV1;

// Hot-owned inner schema. The EXE never sees this layout; only the codec does.
struct HotPingV1 {
    std::uint32_t nonce;
    std::uint32_t value;
};

// ── packet.ping: a core gameplay schema declared in the hot schema layer ─────
// The canonical PacketHeader stays cold framing; this codec carries only the
// schema-owned inner payload (clientTimeMs) so a layout change is a version
// bump, not an in-place cast.
struct PingInnerV1 {
    std::uint64_t clientTimeMs;
};

bool MIMITA_GAME_CALL encodePing(void* /*host*/, const PacketCodecEnvelopeV1* /*env*/,
                                 const void* src, std::uint32_t srcSize,
                                 std::uint8_t* out, std::uint32_t outCapacity,
                                 std::uint32_t* outSize)
{
    if (!src || !out || !outSize || srcSize < sizeof(PingInnerV1) ||
        outCapacity < sizeof(PingInnerV1))
        return false;
    std::memcpy(out, src, sizeof(PingInnerV1));
    *outSize = (std::uint32_t)sizeof(PingInnerV1);
    return true;
}

bool MIMITA_GAME_CALL decodePing(void* /*host*/, const PacketCodecEnvelopeV1* /*env*/,
                                 const std::uint8_t* payload, std::uint32_t payloadSize,
                                 void* out, std::uint32_t outCapacity,
                                 std::uint32_t* outSize)
{
    if (!payload || !out || !outSize || payloadSize < sizeof(PingInnerV1) ||
        outCapacity < sizeof(PingInnerV1))
        return false;
    std::memcpy(out, payload, sizeof(PingInnerV1));
    *outSize = (std::uint32_t)sizeof(PingInnerV1);
    return true;
}

std::uint32_t MIMITA_GAME_CALL validatePing(void* /*host*/,
                                            const PacketCodecEnvelopeV1* /*env*/,
                                            const std::uint8_t* /*payload*/,
                                            std::uint32_t payloadSize)
{
    return payloadSize == sizeof(PingInnerV1)
        ? (std::uint32_t)PacketCompatibilityV1::Compatible
        : (std::uint32_t)PacketCompatibilityV1::Malformed;
}

const GamePacketCodecDescriptorV1 kPingCodec{
    MimitaNet::Schemas::kPing, MimitaNet::Schemas::kPingVersion,
    MimitaNet::Schemas::kPingVersion, 0, 0,
    &encodePing, &decodePing, &validatePing, "packet.ping"};

// One valid inner payload size for the ping schema (used by the length guard).
static constexpr std::uint32_t kHotPingBytes = 8;

// Mirror of the outer wire header (20 bytes: magic, version, type, reserved,
// tick, playerId, transformEpoch). Hot code builds the framing header itself so
// a reply can be sent directly; the schema semantics stay in the envelope.
#pragma pack(push, 1)
struct HotCodecWireHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint8_t type;
    std::uint8_t reserved;
    std::uint32_t tick;
    std::uint32_t playerId;
    std::uint32_t transformEpoch;
};
#pragma pack(pop)
static_assert(sizeof(HotCodecWireHeader) == 20, "wire header size changed");
static constexpr std::uint32_t kHotCodecPacketType = 90;  // PACKET_HOT_CODEC

bool MIMITA_GAME_CALL encodeHotPing(void* /*host*/,
                                    const PacketCodecEnvelopeV1* /*envelope*/,
                                    const void* src, std::uint32_t srcSize,
                                    std::uint8_t* out, std::uint32_t outCapacity,
                                    std::uint32_t* outSize)
{
    if (!src || !out || !outSize || srcSize < sizeof(HotPingV1) ||
        outCapacity < sizeof(HotPingV1))
        return false;
    std::memcpy(out, src, sizeof(HotPingV1));
    *outSize = (std::uint32_t)sizeof(HotPingV1);
    return true;
}

bool MIMITA_GAME_CALL decodeHotPing(void* /*host*/,
                                    const PacketCodecEnvelopeV1* /*envelope*/,
                                    const std::uint8_t* payload,
                                    std::uint32_t payloadSize, void* out,
                                    std::uint32_t outCapacity,
                                    std::uint32_t* outSize)
{
    if (!payload || !out || !outSize || payloadSize < sizeof(HotPingV1) ||
        outCapacity < sizeof(HotPingV1))
        return false;
    std::memcpy(out, payload, sizeof(HotPingV1));
    *outSize = (std::uint32_t)sizeof(HotPingV1);
    return true;
}

std::uint32_t MIMITA_GAME_CALL validateHotPing(
    void* /*host*/, const PacketCodecEnvelopeV1* /*envelope*/,
    const std::uint8_t* /*payload*/, std::uint32_t payloadSize)
{
    return payloadSize == sizeof(HotPingV1)
        ? (std::uint32_t)PacketCompatibilityV1::Compatible
        : (std::uint32_t)PacketCompatibilityV1::Malformed;
}

const GamePacketCodecDescriptorV1 kHotPingCodec{
    gameHash("packet.hot.ping"), 1, 1, 0, 0,
    &encodeHotPing, &decodeHotPing, &validateHotPing, "packet.hot.ping"};

// The one generic lookup doorway the cold dispatcher resolves by capability id.
const GamePacketCodecDescriptorV1* MIMITA_GAME_CALL lookupPacketCodec(
    void* /*host*/, std::uint64_t schemaId, std::uint32_t schemaVersion)
{
    return HotPackageBuilder::instance().findPacketCodec(schemaId, schemaVersion);
}

// Generic net.packet consumer: a real hot handler for the ping schema. On
// receive it replies through the kernel's net.packet-reply capability with a
// re-encoded ping whose value is incremented. This is the live round trip
// (received -> decoded event -> hot handler -> reply), with no EXE call site.
// Runtime event dispatch signature: `host` is the GameplayContextV1*.
void MIMITA_GAME_CALL onHotPacket(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!event || !ctx || !ctx->resolveCapability || !event->payload)
        return;
    if (event->payloadSize < sizeof(PacketCodecEnvelopeV1) + kHotPingBytes)
        return;
    const auto* envelope =
        static_cast<const PacketCodecEnvelopeV1*>(event->payload);
    // Only the connection the packet came from may receive the reply.
    if (envelope->schemaId != gameHash("packet.hot.ping"))
        return;
    auto reply = reinterpret_cast<MimitaNet::GameHotPacketReplyFn>(
        ctx->resolveCapability(ctx->host, MimitaNet::GAME_CAP_HOT_PACKET_REPLY));
    if (!reply)
        return;

    HotPingV1 ping{};
    std::memcpy(&ping, static_cast<const std::uint8_t*>(event->payload) +
                          sizeof(PacketCodecEnvelopeV1),
                sizeof(ping));
    ping.value += 1u;  // hot response policy: acknowledge and advance

    PacketCodecEnvelopeV1 outEnv{};
    outEnv.connectionId = envelope->connectionId;
    outEnv.serverTick = envelope->serverTick;
    outEnv.clientTick = envelope->clientTick;
    std::vector<std::uint8_t> payload(sizeof(ping));
    std::memcpy(payload.data(), &ping, sizeof(ping));
    PacketCodecEnvelopeV1 env = outEnv;
    env.schemaId = envelope->schemaId;
    env.schemaVersion = envelope->schemaVersion;
    env.payloadSize = (std::uint32_t)payload.size();
    env.payloadChecksum =
        MimitaNet::packetCodecChecksum(payload.data(), (std::uint32_t)payload.size());
    // Build the full wire datagram [PacketHeader][envelope][payload] so the
    // kernel can send it directly. The header layout is the stable outer
    // contract; schema id/version live in the envelope.
    std::vector<std::uint8_t> datagram(sizeof(HotCodecWireHeader) +
                                       sizeof(env) + payload.size());
    HotCodecWireHeader header{};
    header.magic = 0x4d494d38u;     // PROTOCOL_MAGIC "MIM8"
    header.version = 34;            // PROTOCOL_VERSION
    header.type = (std::uint8_t)kHotCodecPacketType;
    header.tick = (std::uint32_t)envelope->serverTick;
    header.playerId = envelope->connectionId;
    std::memcpy(datagram.data(), &header, sizeof(header));
    std::memcpy(datagram.data() + sizeof(header), &env, sizeof(env));
    std::memcpy(datagram.data() + sizeof(header) + sizeof(env), payload.data(),
                payload.size());
    reply(ctx->host, envelope->connectionId, datagram.data(),
          (std::uint32_t)datagram.size());
}

const GameCapabilityDescriptorV1 kPacketCodecProvider{
    MimitaNet::GAME_CAP_PACKET_CODECS,
    MimitaNet::GAME_SIG_PACKET_CODECS,
    0,
    reinterpret_cast<void*>(&lookupPacketCodec),
    "net.packet-codecs"};

} // namespace

const MimitaHotPackage::PacketCodecRegistrar s_hotPingCodecRegistrar{kHotPingCodec};
const MimitaHotPackage::PacketCodecRegistrar s_pingCodecRegistrar{kPingCodec};
const MimitaHotPackage::CapabilityRegistrar s_packetCodecProviderRegistrar{
    kPacketCodecProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_packetCodecRequirement{
    MimitaNet::GAME_CAP_PACKET_CODECS, MimitaNet::GAME_SIG_PACKET_CODECS, 0};

// The generic net.packet consumer. Registered by the runtime event id so a hot
// handler owns packet consequence with no kernel schema knowledge.
const MimitaHotPackage::EventRegistrar s_hotPacketConsumer{
    {MimitaNet::GAME_EVENT_HOT_PACKET, 0, 0, onHotPacket, "net.packet-consumer"}};
// Requires the kernel reply capability so activation validates the signature.
const MimitaHotPackage::CapabilityRequirementRegistrar s_hotPacketReplyRequirement{
    MimitaNet::GAME_CAP_HOT_PACKET_REPLY, 0, 0};

#endif // MIMITA_GAME_DLL
