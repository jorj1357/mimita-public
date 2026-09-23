// 09 23 2026
/* purpose
* Implements the cold wire framing for the generic hot-codec packet.
* Does NOT own sockets, transport, or gameplay.
*/
#include "network/packet-codec-wire.h"

#include <cstring>

#include "network/packet-codec-dispatch.h"
#include "network/packets.h"

namespace MimitaNet {

bool buildHotCodecDatagram(std::uint64_t schemaId, std::uint32_t schemaVersion,
                           const void* src, std::uint32_t srcSize,
                           const PacketCodecEnvelopeV1& in,
                           std::vector<std::uint8_t>& outDatagram,
                           PacketCompatibilityV1* outReason)
{
    std::vector<std::uint8_t> codecBytes;
    if (!PacketCodecDispatch::instance().encode(schemaId, schemaVersion, src, srcSize,
                                                in, codecBytes, outReason))
        return false;
    outDatagram.resize(sizeof(PacketHeader) + codecBytes.size());
    PacketHeader header{};
    header.type = PACKET_HOT_CODEC;
    header.tick = (std::uint32_t)in.serverTick;
    header.playerId = in.connectionId;
    std::memcpy(outDatagram.data(), &header, sizeof(PacketHeader));
    std::memcpy(outDatagram.data() + sizeof(PacketHeader), codecBytes.data(),
                codecBytes.size());
    if (outReason)
        *outReason = PacketCompatibilityV1::Compatible;
    return true;
}

bool parseHotCodecDatagram(const std::uint8_t* datagram, std::uint32_t size,
                           void* out, std::uint32_t outCapacity,
                           std::uint32_t* outSize,
                           PacketCodecEnvelopeV1* outEnvelope,
                           PacketCompatibilityV1* outReason)
{
    if (!datagram || size < sizeof(PacketHeader) + sizeof(PacketCodecEnvelopeV1)) {
        if (outReason)
            *outReason = PacketCompatibilityV1::Malformed;
        return false;
    }
    PacketHeader header{};
    std::memcpy(&header, datagram, sizeof(header));
    if (header.type != PACKET_HOT_CODEC) {
        if (outReason)
            *outReason = PacketCompatibilityV1::Malformed;
        return false;
    }
    return PacketCodecDispatch::instance().decode(
        datagram + sizeof(PacketHeader), size - (std::uint32_t)sizeof(PacketHeader),
        out, outCapacity, outSize, outEnvelope, outReason);
}

} // namespace MimitaNet
