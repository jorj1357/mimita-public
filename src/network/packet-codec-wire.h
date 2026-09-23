// 09 23 2026
/* purpose
* Cold wire framing for the generic hot-codec packet. Builds and parses
* [PacketHeader{type=PACKET_HOT_CODEC}][PacketCodecEnvelopeV1][inner payload] so
* the server and client dispatch share one path and no per-schema packet type is
* needed.
* Does NOT own sockets or transport; the caller sends/receives the bytes.
*/
#pragma once

#include <cstdint>
#include <vector>

#include "hot-reload/hot-packet-codec.h"

namespace MimitaNet {

// Build a full datagram. `in` carries ticks/connection/sequence/ack identity.
bool buildHotCodecDatagram(std::uint64_t schemaId, std::uint32_t schemaVersion,
                           const void* src, std::uint32_t srcSize,
                           const PacketCodecEnvelopeV1& in,
                           std::vector<std::uint8_t>& outDatagram,
                           PacketCompatibilityV1* outReason = nullptr);

// Parse a full datagram. Returns false when it is not a valid hot-codec packet;
// `outReason` carries the explicit codec reason. `outEnvelope` receives the
// decoded envelope when non-null.
bool parseHotCodecDatagram(const std::uint8_t* datagram, std::uint32_t size,
                           void* out, std::uint32_t outCapacity,
                           std::uint32_t* outSize,
                           PacketCodecEnvelopeV1* outEnvelope = nullptr,
                           PacketCompatibilityV1* outReason = nullptr);

} // namespace MimitaNet
