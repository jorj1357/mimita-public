// 09 23 2026
/* purpose
* Headless falsification selftest for the generic packet-codec dispatch:
* round trip, backward-compatible version range, explicit incompatibility
* reasons, malformed/checksum rejection, generation retention across a swap,
* and a schema registered after startup with no cold call site.
* Does NOT own sockets or transport.
*/
#include "hot-reload/packet-codec-selftest.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-packet-codec.h"
#include "network/packet-codec-dispatch.h"
#include "network/packet-codec-wire.h"
#include "network/packets.h"

namespace {

bool gPass = true;

void check(bool condition, const char* what, std::string& report)
{
    if (!condition)
        gPass = false;
    report += condition ? "  [ok] " : "  [FAIL] ";
    report += what;
    report += "\n";
}

using MimitaNet::GamePacketCodecDescriptorV1;
using MimitaNet::PacketCodecEnvelopeV1;
using MimitaNet::PacketCompatibilityV1;

struct TestEchoV1 { std::uint32_t a; float b; };
struct TestEchoV2 { std::uint32_t a; float b; std::uint32_t c; };

// Local codec registry standing in for the DLL's HotPackageBuilder table.
struct TestRegistry {
    std::vector<GamePacketCodecDescriptorV1> codecs;

    // Prefer a codec whose range contains `version`; otherwise return the
    // newest codec for the schema so the dispatcher can classify too-old/too-new.
    const GamePacketCodecDescriptorV1* find(std::uint64_t schemaId,
                                            std::uint32_t version) const
    {
        const GamePacketCodecDescriptorV1* newest = nullptr;
        const GamePacketCodecDescriptorV1* inRange = nullptr;
        for (const auto& c : codecs) {
            if (c.schemaId != schemaId)
                continue;
            if (!newest || c.schemaVersion > newest->schemaVersion)
                newest = &c;
            if (version >= c.minSupportedVersion && version <= c.schemaVersion) {
                if (!inRange || c.schemaVersion > inRange->schemaVersion)
                    inRange = &c;
            }
        }
        return inRange ? inRange : newest;
    }
};

TestRegistry* gRegistry = nullptr;

const GamePacketCodecDescriptorV1* MIMITA_GAME_CALL testLookup(
    void* /*host*/, std::uint64_t schemaId, std::uint32_t version)
{
    return gRegistry ? gRegistry->find(schemaId, version) : nullptr;
}

bool MIMITA_GAME_CALL encodeEcho(void*, const PacketCodecEnvelopeV1*,
                                 const void* src, std::uint32_t srcSize,
                                 std::uint8_t* out, std::uint32_t outCapacity,
                                 std::uint32_t* outSize)
{
    if (!src || !out || !outSize || srcSize < sizeof(TestEchoV1) ||
        outCapacity < sizeof(TestEchoV2))
        return false;
    TestEchoV2 value{};
    std::memcpy(&value, src, sizeof(TestEchoV1));
    std::memcpy(out, &value, sizeof(TestEchoV2));
    *outSize = (std::uint32_t)sizeof(TestEchoV2);
    return true;
}

bool MIMITA_GAME_CALL decodeEcho(void*, const PacketCodecEnvelopeV1*,
                                 const std::uint8_t* payload, std::uint32_t payloadSize,
                                 void* out, std::uint32_t outCapacity,
                                 std::uint32_t* outSize)
{
    if (!payload || !out || !outSize || payloadSize < sizeof(TestEchoV1) ||
        outCapacity < sizeof(TestEchoV2))
        return false;
    TestEchoV2 value{};
    std::memcpy(&value, payload, sizeof(TestEchoV1));
    if (payloadSize >= sizeof(TestEchoV2))
        std::memcpy(&value, payload, sizeof(TestEchoV2));
    std::memcpy(out, &value, sizeof(TestEchoV2));
    *outSize = (std::uint32_t)sizeof(TestEchoV2);
    return true;
}

std::uint32_t MIMITA_GAME_CALL validateEcho(void*, const PacketCodecEnvelopeV1*,
                                            const std::uint8_t*, std::uint32_t payloadSize)
{
    if (payloadSize == sizeof(TestEchoV1) || payloadSize == sizeof(TestEchoV2))
        return (std::uint32_t)PacketCompatibilityV1::Compatible;
    return (std::uint32_t)PacketCompatibilityV1::Malformed;
}

std::uint32_t MIMITA_GAME_CALL validateReject(void*, const PacketCodecEnvelopeV1*,
                                              const std::uint8_t*, std::uint32_t)
{
    return (std::uint32_t)PacketCompatibilityV1::Rejected;
}

const std::uint64_t kEchoSchema = gameHash("packet.test.echo");
const std::uint64_t kNewSchema = gameHash("packet.test.runtime-new");
const std::uint64_t kRejectSchema = gameHash("packet.test.reject");

std::vector<std::uint8_t> craft(const PacketCodecEnvelopeV1& in,
                                const std::vector<std::uint8_t>& payload)
{
    PacketCodecEnvelopeV1 env = in;
    env.payloadSize = (std::uint32_t)payload.size();
    env.payloadChecksum =
        MimitaNet::packetCodecChecksum(payload.data(), (std::uint32_t)payload.size());
    std::vector<std::uint8_t> bytes(sizeof(env) + payload.size());
    std::memcpy(bytes.data(), &env, sizeof(env));
    if (!payload.empty())
        std::memcpy(bytes.data() + sizeof(env), payload.data(), payload.size());
    return bytes;
}

} // namespace

bool runPacketCodecSelfTest(std::string& report)
{
    gPass = true;
    using MimitaRuntime::GenericRuntime;
    GenericRuntime& rt = GenericRuntime::instance();
    auto& dispatch = MimitaNet::PacketCodecDispatch::instance();

    TestRegistry registry;
    registry.codecs.push_back(GamePacketCodecDescriptorV1{
        kEchoSchema, 2, 1, 0, 0, &encodeEcho, &decodeEcho, &validateEcho,
        "packet.test.echo"});
    gRegistry = &registry;

    rt.registerKernelCapability(MimitaNet::GAME_CAP_PACKET_CODECS,
                                MimitaNet::GAME_SIG_PACKET_CODECS, 0,
                                reinterpret_cast<void*>(&testLookup),
                                "selftest.packet-codecs");
    check(rt.hasCapability(MimitaNet::GAME_CAP_PACKET_CODECS),
          "packet codec lookup capability registered", report);

    // ── Round trip ────────────────────────────────────────────────────
    TestEchoV1 source{42u, 1.5f};
    PacketCodecEnvelopeV1 in{};
    in.protocolFamily = gameHash("family.gameplay");
    in.connectionId = 7;
    in.packetSequence = 100;
    in.serverTick = 5000;
    std::vector<std::uint8_t> encoded;
    PacketCompatibilityV1 reason = PacketCompatibilityV1::Malformed;
    check(dispatch.encode(kEchoSchema, 2, &source, sizeof(source), in, encoded, &reason),
          "encode round trip succeeds", report);
    check(encoded.size() == sizeof(PacketCodecEnvelopeV1) + sizeof(TestEchoV2),
          "encoded size is envelope + payload", report);
    TestEchoV2 decoded{};
    std::uint32_t decodedSize = 0;
    PacketCodecEnvelopeV1 outEnv{};
    check(dispatch.decode(encoded.data(), (std::uint32_t)encoded.size(), &decoded,
                          sizeof(decoded), &decodedSize, &outEnv, &reason),
          "decode round trip succeeds", report);
    check(decoded.a == 42u && decoded.b == 1.5f && decoded.c == 0u,
          "decoded payload matches source", report);
    check(outEnv.schemaId == kEchoSchema && outEnv.schemaVersion == 2 &&
              outEnv.connectionId == 7 && outEnv.packetSequence == 100,
          "decoded envelope preserves identity fields", report);

    // ── Backward-compatible older version (v1 within [1,2]) ───────────
    TestEchoV1 oldValue{9u, 2.5f};
    std::vector<std::uint8_t> oldPayload(sizeof(TestEchoV1));
    std::memcpy(oldPayload.data(), &oldValue, sizeof(TestEchoV1));
    PacketCodecEnvelopeV1 v1Env{};
    v1Env.schemaId = kEchoSchema;
    v1Env.schemaVersion = 1;
    std::vector<std::uint8_t> v1Bytes = craft(v1Env, oldPayload);
    check(dispatch.decode(v1Bytes.data(), (std::uint32_t)v1Bytes.size(), &decoded,
                          sizeof(decoded), &decodedSize, nullptr, &reason) &&
              decoded.a == 9u && decoded.c == 0u,
          "older version decodes through the same codec", report);

    // ── Explicit incompatibility reasons ──────────────────────────────
    {
        std::vector<std::uint8_t> unknown = craft(
            PacketCodecEnvelopeV1{0, 1, gameHash("packet.test.unknown"), 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0},
            oldPayload);
        check(!dispatch.decode(unknown.data(), (std::uint32_t)unknown.size(), &decoded,
                               sizeof(decoded), &decodedSize, nullptr, &reason) &&
                  reason == PacketCompatibilityV1::UnknownSchema,
              "unknown schema is rejected explicitly", report);
    }
    {
        std::vector<std::uint8_t> tooNew = craft(
            PacketCodecEnvelopeV1{0, 3, kEchoSchema, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            oldPayload);
        check(!dispatch.decode(tooNew.data(), (std::uint32_t)tooNew.size(), &decoded,
                               sizeof(decoded), &decodedSize, nullptr, &reason) &&
                  reason == PacketCompatibilityV1::VersionTooNew,
              "version too new is rejected explicitly", report);
    }
    {
        std::vector<std::uint8_t> tooOld = craft(
            PacketCodecEnvelopeV1{0, 0, kEchoSchema, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            oldPayload);
        check(!dispatch.decode(tooOld.data(), (std::uint32_t)tooOld.size(), &decoded,
                               sizeof(decoded), &decodedSize, nullptr, &reason) &&
                  reason == PacketCompatibilityV1::VersionTooOld,
              "version too old is rejected explicitly", report);
    }

    // ── Corruption and malformed payload ──────────────────────────────
    {
        std::vector<std::uint8_t> corrupted = encoded;
        corrupted.back() ^= 0xFFu;
        check(!dispatch.decode(corrupted.data(), (std::uint32_t)corrupted.size(),
                               &decoded, sizeof(decoded), &decodedSize, nullptr, &reason) &&
                  reason == PacketCompatibilityV1::ChecksumMismatch,
              "corrupted payload is rejected by checksum", report);
    }
    {
        std::vector<std::uint8_t> wrongSize = craft(
            PacketCodecEnvelopeV1{0, 2, kEchoSchema, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            std::vector<std::uint8_t>(10, 0));  // neither 8 (v1) nor 12 (v2)
        check(!dispatch.decode(wrongSize.data(), (std::uint32_t)wrongSize.size(),
                               &decoded, sizeof(decoded), &decodedSize, nullptr, &reason) &&
                  reason == PacketCompatibilityV1::Malformed,
              "schema-invalid payload is rejected as malformed", report);
    }

    // ── Generation retention across an activation ─────────────────────
    dispatch.clearRetained();
    dispatch.retainEncoded(100, encoded);
    check(dispatch.retainedCount() == 1, "encoded packet is retained by sequence", report);
    check(dispatch.decodeRetained(100, &decoded, sizeof(decoded), &decodedSize, &outEnv,
                                  &reason) &&
              decoded.a == 42u,
          "retained packet decodes after a generation swap", report);
    dispatch.acknowledge(100);
    check(dispatch.retainedCount() == 0 &&
              !dispatch.decodeRetained(100, &decoded, sizeof(decoded), &decodedSize,
                                       nullptr, &reason),
          "acknowledged packet is retired from retention", report);

    // ── Reliable-event retention bridge ───────────────────────────────
    // Bytes retained by reliable event id stay decodable until the reliable
    // queue acknowledges/expires that event.
    dispatch.retainForEvent(9001, encoded);
    check(dispatch.retainedEventCount() == 1,
          "reliable event bytes are retained by event id", report);
    check(dispatch.decodeRetainedEvent(9001, &decoded, sizeof(decoded), &decodedSize,
                                       nullptr, &reason) &&
              decoded.a == 42u,
          "retained reliable event decodes after a swap", report);
    dispatch.retireEvent(9001);
    check(dispatch.retainedEventCount() == 0 &&
              !dispatch.decodeRetainedEvent(9001, &decoded, sizeof(decoded),
                                            &decodedSize, nullptr, &reason),
          "acknowledged reliable event retires its retained bytes", report);

    // ── A schema registered after startup (no cold call site) ─────────
    registry.codecs.push_back(GamePacketCodecDescriptorV1{
        kNewSchema, 1, 1, 0, 0, &encodeEcho, &decodeEcho, &validateEcho,
        "packet.test.runtime-new"});
    TestEchoV1 fresh{123u, 4.0f};
    std::vector<std::uint8_t> freshBytes;
    check(dispatch.encode(kNewSchema, 1, &fresh, sizeof(fresh), in, freshBytes, &reason),
          "runtime-registered new schema encodes", report);
    TestEchoV2 freshOut{};
    check(dispatch.decode(freshBytes.data(), (std::uint32_t)freshBytes.size(), &freshOut,
                          sizeof(freshOut), &decodedSize, nullptr, &reason) &&
              freshOut.a == 123u,
          "runtime-registered new schema decodes", report);

    // ── Policy rejection reason propagates ────────────────────────────
    registry.codecs.push_back(GamePacketCodecDescriptorV1{
        kRejectSchema, 1, 1, 0, 0, &encodeEcho, &decodeEcho, &validateReject,
        "packet.test.reject"});
    std::vector<std::uint8_t> rejectBytes;
    check(dispatch.encode(kRejectSchema, 1, &fresh, sizeof(fresh), in, rejectBytes, &reason),
          "reject schema encodes", report);
    check(!dispatch.decode(rejectBytes.data(), (std::uint32_t)rejectBytes.size(), &decoded,
                           sizeof(decoded), &decodedSize, nullptr, &reason) &&
              reason == PacketCompatibilityV1::Rejected,
          "codec policy rejection propagates", report);

    // ── Real datagram over a real loopback UDP socket ─────────────────
    // Proves the wire framing [PacketHeader][envelope][payload] survives an
    // actual OS socket, not just an in-process buffer.
    {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        bind(s, (const sockaddr*)&addr, sizeof(addr));
        int addrLen = sizeof(addr);
        getsockname(s, (sockaddr*)&addr, &addrLen);
        const DWORD timeoutMs = 2000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs,
                   sizeof(timeoutMs));

        PacketCodecEnvelopeV1 wireIn{};
        wireIn.protocolFamily = gameHash("family.gameplay");
        wireIn.connectionId = 3;
        wireIn.packetSequence = 77;
        std::vector<std::uint8_t> datagram;
        const bool built = MimitaNet::buildHotCodecDatagram(
            kEchoSchema, 2, &source, sizeof(source), wireIn, datagram, &reason);
        check(built, "wire: hot-codec datagram builds", report);
        if (built)
        {
            sendto(s, (const char*)datagram.data(), (int)datagram.size(), 0,
                   (const sockaddr*)&addr, sizeof(addr));
            std::vector<std::uint8_t> received(2048);
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int n = recvfrom(s, (char*)received.data(), (int)received.size(), 0,
                             (sockaddr*)&from, &fromLen);
            TestEchoV2 wireDecoded{};
            std::uint32_t wireSize = 0;
            PacketCodecEnvelopeV1 wireEnv{};
            const bool parsed = n > 0 &&
                MimitaNet::parseHotCodecDatagram(received.data(), (std::uint32_t)n,
                                                 &wireDecoded, sizeof(wireDecoded),
                                                 &wireSize, &wireEnv, &reason);
            check(parsed && wireDecoded.a == 42u && wireEnv.packetSequence == 77,
                  "wire: hot-codec packet round-trips over loopback UDP", report);
            std::vector<std::uint8_t> tampered(received.begin(),
                                               received.begin() + (n > 0 ? n : 0));
            if (!tampered.empty())
            {
                tampered.back() ^= 0xFFu;
                const bool bad = MimitaNet::parseHotCodecDatagram(
                    tampered.data(), (std::uint32_t)tampered.size(), &wireDecoded,
                    sizeof(wireDecoded), &wireSize, nullptr, &reason);
                check(!bad && reason == PacketCompatibilityV1::ChecksumMismatch,
                      "wire: corrupted datagram is rejected", report);
            }
        }
        closesocket(s);
        WSACleanup();
    }

    gRegistry = nullptr;
    report += gPass ? "[PACKET CODEC SELFTEST] PASS\n" : "[PACKET CODEC SELFTEST] FAIL\n";
    return gPass;
}
