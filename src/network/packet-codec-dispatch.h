// 09 23 2026
/* purpose
* Cold, policy-free mechanism that dispatches packet codecs through the generic
* capability registry. The EXE owns framing, buffers, and retention; the hot
* module owns the schema layout and encode/decode/validate policy.
* Does NOT own sockets, transport, or gameplay; it only turns opaque bytes into
* a schema call and back.
* Does NOT add a new EXE call site per packet schema; a new schema registers a
* hot codec and this dispatcher finds it by (schemaId, schemaVersion).
*/
#pragma once

#include <cstdint>
#include <vector>

#include "hot-reload/hot-packet-codec.h"

namespace MimitaNet {

class PacketCodecDispatch {
public:
    static PacketCodecDispatch& instance();

    // Kernel host pointer passed to codecs (may be null in headless selftests).
    void setHost(void* host) { host_ = host; }
    void* host() const { return host_; }

    // Resolve the active generation's codec for a schema/version. Returns null
    // when no provider handles it.
    const GamePacketCodecDescriptorV1* find(std::uint64_t schemaId,
                                            std::uint32_t schemaVersion) const;

    // Encode `src` into outBytes = [envelope][payload]. Identity fields
    // (ticks, connection, sequence, ack) come from `in`; payloadSize, checksum,
    // generation and codeHash are filled here.
    bool encode(std::uint64_t schemaId, std::uint32_t schemaVersion,
                const void* src, std::uint32_t srcSize,
                const PacketCodecEnvelopeV1& in,
                std::vector<std::uint8_t>& outBytes,
                PacketCompatibilityV1* outReason = nullptr) const;

    // Decode [envelope][payload]. Verifies envelope size, payload size, checksum,
    // and codec version range, then calls the codec decode.
    bool decode(const std::uint8_t* bytes, std::uint32_t size,
                void* out, std::uint32_t outCapacity, std::uint32_t* outSize,
                PacketCodecEnvelopeV1* outEnvelope = nullptr,
                PacketCompatibilityV1* outReason = nullptr) const;

    // Validate only (no decode output).
    PacketCompatibilityV1 validate(const std::uint8_t* bytes,
                                   std::uint32_t size) const;

    // ── Generation retention ──────────────────────────────────────────
    // A reliable packet already in flight was encoded by the generation that
    // created it. Retain its exact bytes keyed by send sequence so it can still
    // be decoded after an activation, until acknowledged or expired.
    void retainEncoded(std::uint32_t sequence, const std::vector<std::uint8_t>& bytes);
    bool decodeRetained(std::uint32_t sequence, void* out, std::uint32_t outCapacity,
                        std::uint32_t* outSize,
                        PacketCodecEnvelopeV1* outEnvelope = nullptr,
                        PacketCompatibilityV1* outReason = nullptr) const;
    void acknowledge(std::uint32_t upToSequence);
    std::size_t retainedCount() const { return retained_.size(); }
    void clearRetained() { retained_.clear(); }

    // Reliable-event bridge: retain the exact bytes keyed by reliable event id,
    // retired by the reliable queue when that event is acknowledged or expires.
    // This is what keeps in-flight reliable hot-coded packets decodable with the
    // generation that encoded them.
    void retainForEvent(std::uint32_t eventId, const std::vector<std::uint8_t>& bytes);
    bool decodeRetainedEvent(std::uint32_t eventId, void* out, std::uint32_t outCapacity,
                             std::uint32_t* outSize,
                             PacketCodecEnvelopeV1* outEnvelope = nullptr,
                             PacketCompatibilityV1* outReason = nullptr) const;
    void retireEvent(std::uint32_t eventId);
    std::size_t retainedEventCount() const { return retainedEvents_.size(); }

private:
    PacketCodecDispatch() = default;
    PacketCodecDispatch(const PacketCodecDispatch&) = delete;
    PacketCodecDispatch& operator=(const PacketCodecDispatch&) = delete;

    struct RetainedPacket {
        std::uint32_t key = 0;
        std::vector<std::uint8_t> bytes;
    };

    static void retainInto(std::vector<RetainedPacket>& list, std::uint32_t key,
                           const std::vector<std::uint8_t>& bytes,
                           std::size_t maxEntries);

    void* host_ = nullptr;
    std::vector<RetainedPacket> retained_;
    std::vector<RetainedPacket> retainedEvents_;
    static constexpr std::size_t kMaxRetained = 256;
};

} // namespace MimitaNet
