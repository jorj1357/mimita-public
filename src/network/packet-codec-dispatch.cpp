// 09 23 2026
/* purpose
* Implements the cold packet-codec dispatch and generation retention.
* Resolves the hot lookup provider per call (never caches across a swap).
* Does NOT compile code, own sockets, or decide gameplay.
*/
#include "network/packet-codec-dispatch.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"

namespace MimitaNet {

namespace {

// Upper bound for one encoded payload. Matches the safe game datagram budget;
// kept local so this mechanism does not depend on the legacy packet header.
constexpr std::uint32_t kMaxEncodedPayload = 1200;

std::uint64_t hashLow64(const std::string& hex)
{
    std::uint64_t value = 0;
    std::size_t taken = 0;
    for (char c : hex) {
        std::uint64_t nibble = 0;
        if (c >= '0' && c <= '9') nibble = (std::uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = (std::uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') nibble = (std::uint64_t)(c - 'A' + 10);
        else continue;
        value = (value << 4) | nibble;
        if (++taken >= 16)
            break;
    }
    return value;
}

void setReason(PacketCompatibilityV1* out, PacketCompatibilityV1 value)
{
    if (out)
        *out = value;
}

} // namespace

PacketCodecDispatch& PacketCodecDispatch::instance()
{
    static PacketCodecDispatch dispatch;
    return dispatch;
}

const GamePacketCodecDescriptorV1* PacketCodecDispatch::find(
    std::uint64_t schemaId, std::uint32_t schemaVersion) const
{
    // Re-resolve every call: the provider pointer belongs to the active
    // generation and must not be cached across an activation.
    auto lookup = reinterpret_cast<GamePacketCodecLookupFn>(
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_PACKET_CODECS));
    if (!lookup || schemaId == 0)
        return nullptr;
    return lookup(host_, schemaId, schemaVersion);
}

bool PacketCodecDispatch::encode(std::uint64_t schemaId, std::uint32_t schemaVersion,
                                 const void* src, std::uint32_t srcSize,
                                 const PacketCodecEnvelopeV1& in,
                                 std::vector<std::uint8_t>& outBytes,
                                 PacketCompatibilityV1* outReason) const
{
    outBytes.clear();
    const GamePacketCodecDescriptorV1* codec = find(schemaId, schemaVersion);
    if (!codec || !codec->encode) {
        setReason(outReason, PacketCompatibilityV1::UnknownSchema);
        return false;
    }
    if (schemaVersion > codec->schemaVersion) {
        setReason(outReason, PacketCompatibilityV1::VersionTooNew);
        return false;
    }
    if (schemaVersion < codec->minSupportedVersion) {
        setReason(outReason, PacketCompatibilityV1::VersionTooOld);
        return false;
    }

    PacketCodecEnvelopeV1 env = in;
    env.schemaId = schemaId;
    env.schemaVersion = schemaVersion;

    std::vector<std::uint8_t> payload(kMaxEncodedPayload);
    std::uint32_t outSize = 0;
    if (!codec->encode(host_, &env, src, srcSize, payload.data(),
                       (std::uint32_t)payload.size(), &outSize) ||
        outSize > payload.size()) {
        setReason(outReason, PacketCompatibilityV1::Malformed);
        return false;
    }

    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    env.generation = status.activeGeneration;
    env.codeHash = hashLow64(status.activeHash);
    env.payloadSize = outSize;
    env.payloadChecksum = packetCodecChecksum(payload.data(), outSize);

    outBytes.resize(sizeof(PacketCodecEnvelopeV1) + outSize);
    std::memcpy(outBytes.data(), &env, sizeof(PacketCodecEnvelopeV1));
    std::memcpy(outBytes.data() + sizeof(PacketCodecEnvelopeV1), payload.data(), outSize);
    setReason(outReason, PacketCompatibilityV1::Compatible);
    return true;
}

bool PacketCodecDispatch::decode(const std::uint8_t* bytes, std::uint32_t size,
                                 void* out, std::uint32_t outCapacity,
                                 std::uint32_t* outSize,
                                 PacketCodecEnvelopeV1* outEnvelope,
                                 PacketCompatibilityV1* outReason) const
{
    if (!bytes || size < sizeof(PacketCodecEnvelopeV1)) {
        setReason(outReason, PacketCompatibilityV1::Malformed);
        return false;
    }
    PacketCodecEnvelopeV1 env{};
    std::memcpy(&env, bytes, sizeof(env));
    const std::uint32_t payloadOffset = (std::uint32_t)sizeof(PacketCodecEnvelopeV1);
    if (env.payloadSize != size - payloadOffset) {
        setReason(outReason, PacketCompatibilityV1::PayloadSizeMismatch);
        return false;
    }
    const std::uint8_t* payload = bytes + payloadOffset;
    if (packetCodecChecksum(payload, env.payloadSize) != env.payloadChecksum) {
        setReason(outReason, PacketCompatibilityV1::ChecksumMismatch);
        return false;
    }

    const GamePacketCodecDescriptorV1* codec = find(env.schemaId, env.schemaVersion);
    if (!codec || !codec->decode) {
        setReason(outReason, PacketCompatibilityV1::UnknownSchema);
        return false;
    }
    if (env.schemaVersion > codec->schemaVersion) {
        setReason(outReason, PacketCompatibilityV1::VersionTooNew);
        return false;
    }
    if (env.schemaVersion < codec->minSupportedVersion) {
        setReason(outReason, PacketCompatibilityV1::VersionTooOld);
        return false;
    }
    if (codec->validate) {
        const auto reason = (PacketCompatibilityV1)codec->validate(
            host_, &env, payload, env.payloadSize);
        if (reason != PacketCompatibilityV1::Compatible) {
            setReason(outReason, reason);
            return false;
        }
    }
    std::uint32_t written = 0;
    if (!codec->decode(host_, &env, payload, env.payloadSize, out, outCapacity, &written)) {
        setReason(outReason, PacketCompatibilityV1::Malformed);
        return false;
    }
    if (outSize)
        *outSize = written;
    if (outEnvelope)
        *outEnvelope = env;
    setReason(outReason, PacketCompatibilityV1::Compatible);
    return true;
}

PacketCompatibilityV1 PacketCodecDispatch::validate(const std::uint8_t* bytes,
                                                    std::uint32_t size) const
{
    PacketCompatibilityV1 reason = PacketCompatibilityV1::Malformed;
    // Decode into a scratch buffer; validation is the reason, not the output.
    std::vector<std::uint8_t> scratch(1024);
    std::uint32_t written = 0;
    decode(bytes, size, scratch.data(), (std::uint32_t)scratch.size(), &written,
           nullptr, &reason);
    return reason;
}

void PacketCodecDispatch::retainInto(std::vector<RetainedPacket>& list,
                                     std::uint32_t key,
                                     const std::vector<std::uint8_t>& bytes,
                                     std::size_t maxEntries)
{
    for (auto& entry : list) {
        if (entry.key == key) {
            entry.bytes = bytes;
            return;
        }
    }
    if (list.size() >= maxEntries)
        list.erase(list.begin());
    RetainedPacket entry;
    entry.key = key;
    entry.bytes = bytes;
    list.push_back(std::move(entry));
}

void PacketCodecDispatch::retainEncoded(std::uint32_t sequence,
                                        const std::vector<std::uint8_t>& bytes)
{
    retainInto(retained_, sequence, bytes, kMaxRetained);
}

bool PacketCodecDispatch::decodeRetained(std::uint32_t sequence, void* out,
                                         std::uint32_t outCapacity,
                                         std::uint32_t* outSize,
                                         PacketCodecEnvelopeV1* outEnvelope,
                                         PacketCompatibilityV1* outReason) const
{
    for (const RetainedPacket& entry : retained_) {
        if (entry.key == sequence) {
            return decode(entry.bytes.data(), (std::uint32_t)entry.bytes.size(),
                          out, outCapacity, outSize, outEnvelope, outReason);
        }
    }
    setReason(outReason, PacketCompatibilityV1::UnknownSchema);
    return false;
}

void PacketCodecDispatch::acknowledge(std::uint32_t upToSequence)
{
    retained_.erase(
        std::remove_if(retained_.begin(), retained_.end(),
                       [upToSequence](const RetainedPacket& entry) {
                           return entry.key <= upToSequence;
                       }),
        retained_.end());
}

void PacketCodecDispatch::retainForEvent(std::uint32_t eventId,
                                         const std::vector<std::uint8_t>& bytes)
{
    retainInto(retainedEvents_, eventId, bytes, kMaxRetained);
}

bool PacketCodecDispatch::decodeRetainedEvent(std::uint32_t eventId, void* out,
                                              std::uint32_t outCapacity,
                                              std::uint32_t* outSize,
                                              PacketCodecEnvelopeV1* outEnvelope,
                                              PacketCompatibilityV1* outReason) const
{
    for (const RetainedPacket& entry : retainedEvents_) {
        if (entry.key == eventId) {
            return decode(entry.bytes.data(), (std::uint32_t)entry.bytes.size(),
                          out, outCapacity, outSize, outEnvelope, outReason);
        }
    }
    setReason(outReason, PacketCompatibilityV1::UnknownSchema);
    return false;
}

void PacketCodecDispatch::retireEvent(std::uint32_t eventId)
{
    retainedEvents_.erase(
        std::remove_if(retainedEvents_.begin(), retainedEvents_.end(),
                       [eventId](const RetainedPacket& entry) {
                           return entry.key == eventId;
                       }),
        retainedEvents_.end());
}

} // namespace MimitaNet
