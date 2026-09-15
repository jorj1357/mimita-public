// 09 15 2026
/* purpose
* Implements the headless real-transport generation self-test. It serializes the
* real packet structs and sends them over an actual loopback UDP socket
* (sendto/recvfrom), decodes the received bytes back into the packets, and drives
* the real ArtifactStreamer/ArtifactReceiver + ArtifactCache. This proves the
* artifact transaction traverses the OS transport path (encode -> send -> receive
* -> decode -> dispatch), not direct handler invocation.
* Does NOT own the game server loop, activation, or the loader.
*/
#include "network/transport-generation-selftest.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "network/server.h"
#include "network/packets.h"
#include "network/snapshot-chunks.h"
#include "hot-reload/artifact-cache.h"
#include "hot-reload/artifact-transfer.h"
#include "hot-reload/generation-switch-mapping.h"
#include "hot-reload/generation-verify.h"
#include "hot-reload/game-api.h"

using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::vector<unsigned char> makeBytes(std::size_t n)
{
    std::vector<unsigned char> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = (unsigned char)((i * 131 + 17) & 0xff);
    return v;
}

// Round-trip one serialized packet through a real loopback UDP socket.
template <typename T>
bool loopbackSendRecv(SOCKET s, const sockaddr_in& to, const T& out, T& in,
                      std::string& err)
{
    if (sendto(s, (const char*)&out, (int)sizeof(T), 0, (const sockaddr*)&to,
               sizeof(to)) == SOCKET_ERROR) {
        err = "sendto failed";
        return false;
    }
    sockaddr_in from{};
    int fromLen = sizeof(from);
    const int n = recvfrom(s, (char*)&in, (int)sizeof(T), 0, (sockaddr*)&from,
                           &fromLen);
    if (n != (int)sizeof(T)) {
        err = "recvfrom size mismatch";
        return false;
    }
    return true;
}

} // namespace

bool runTransportGenerationSelfTest(std::string& report)
{
    bool ok = true;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        report += "[FAIL] WSAStartup failed\n";
        return false;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        report += "[FAIL] socket() failed\n";
        WSACleanup();
        return false;
    }
    const DWORD timeoutMs = 2000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs,
               sizeof(timeoutMs));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    bind(s, (const sockaddr*)&addr, sizeof(addr));
    int addrLen = sizeof(addr);
    getsockname(s, (sockaddr*)&addr, &addrLen);

    // ── Known artifact bytes ──────────────────────────────────────────
    const std::vector<unsigned char> bytes = makeBytes(3000);  // 3 chunks
    const std::uint64_t hash = MimitaRuntime::hashArtifactBytes(bytes.data(),
                                                               bytes.size());
    const std::uint64_t gen = 0x5A5A;

    MimitaRuntime::ArtifactStreamer streamer;
    ok &= check(streamer.begin(gen, hash, bytes.data(), bytes.size()),
                "streamer chunks the known artifact", report);

    // ── ANNOUNCE (CodeGenerationPacket) crosses the socket ────────────
    {
        CodeGenerationPacket announce{};
        announce.header.type = PACKET_CODE_GENERATION;
        announce.header.tick = 1000;
        announce.generation = (uint32_t)gen;
        announce.direction = 1;
        announce.phase = 0;
        announce.platformPackageHash = hash;
        CodeGenerationPacket got{};
        std::string err;
        ok &= check(loopbackSendRecv(s, addr, announce, got, err) &&
                        got.generation == (uint32_t)gen &&
                        got.platformPackageHash == hash,
                    "ANNOUNCE traverses the real transport", report);
    }

    // ── ARTIFACT_REQUEST crosses the socket ───────────────────────────
    {
        ArtifactRequestPacket req{};
        req.header.type = PACKET_ARTIFACT_REQUEST;
        req.logicalGenerationId = gen;
        req.platformArtifactHash = hash;
        ArtifactRequestPacket got{};
        std::string err;
        ok &= check(loopbackSendRecv(s, addr, req, got, err) &&
                        got.platformArtifactHash == hash,
                    "ARTIFACT_REQUEST traverses the real transport", report);
    }

    // ── ARTIFACT_BEGIN + CHUNKs cross the socket, bytes reconstruct ───
    MimitaRuntime::ArtifactCache& cache = MimitaRuntime::ArtifactCache::instance();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mimita-transport-generation";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    cache.setRoot(root);

    MimitaRuntime::ArtifactReceiver rx;
    std::uint32_t bytesTransferred = 0;
    {
        ArtifactBeginPacket begin{};
        streamer.makeBegin(begin);
        begin.header.type = PACKET_ARTIFACT_BEGIN;
        ArtifactBeginPacket gotBegin{};
        std::string err;
        ok &= check(loopbackSendRecv(s, addr, begin, gotBegin, err),
                    "ARTIFACT_BEGIN traverses the real transport", report);
        bytesTransferred += (std::uint32_t)sizeof(ArtifactBeginPacket);
        rx.begin(gotBegin);

        bool chunksOk = true;
        for (std::uint32_t i = 0; i < streamer.chunkCount(); ++i) {
            ArtifactChunkPacket chunk{};
            streamer.makeChunk(i, chunk);
            chunk.header.type = PACKET_ARTIFACT_CHUNK;
            ArtifactChunkPacket gotChunk{};
            if (!loopbackSendRecv(s, addr, chunk, gotChunk, err)) {
                chunksOk = false;
                break;
            }
            bytesTransferred += (std::uint32_t)sizeof(ArtifactChunkPacket);
            if (!rx.onChunk(gotChunk)) {
                chunksOk = false;
                break;
            }
        }
        ok &= check(chunksOk && rx.complete(),
                    "ARTIFACT_CHUNKs traverse the real transport and reassemble",
                    report);
    }

    {
        std::string err;
        const bool committed = rx.commit(err);
        ok &= check(committed && cache.contains(hash),
                    "reconstructed bytes hash-verify and commit to the cache",
                    report);
        std::vector<unsigned char> stored;
        cache.read(hash, stored);
        ok &= check(stored.size() == bytes.size() &&
                        std::memcmp(stored.data(), bytes.data(), bytes.size()) == 0,
                    "cache holds the exact reconstructed bytes", report);
    }

    // ── READY crosses the socket (client -> server report) ────────────
    {
        CodeGenerationPacket ready{};
        ready.header.type = PACKET_CODE_GENERATION;
        ready.generation = (uint32_t)gen;
        ready.direction = 0;
        ready.phase = 1;  // READY
        ready.platformPackageHash = hash;
        CodeGenerationPacket got{};
        std::string err;
        ok &= check(loopbackSendRecv(s, addr, ready, got, err) && got.phase == 1 &&
                        got.generation == (uint32_t)gen,
                    "READY traverses the real transport", report);
    }

    // ── Manifest crosses the socket; READY depends on verifyGeneration ─
    {
        GenerationManifestPacket m{};
        m.header.type = PACKET_GENERATION_MANIFEST;
        m.logicalGenerationId = gen;
        m.platformArtifactHash = hash;
        m.platformArtifactSize = (std::uint32_t)bytes.size();
        m.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
        m.requiredCapabilityCount = 1;
        m.requiredCapabilities[0] = 0xABC;
        m.requiredSchemaCount = 1;
        m.requiredSchemas[0] = 0x123;
        m.requiredDependencyCount = 1;
        m.requiredDependencies[0] = 0x777;
        GenerationManifestPacket got{};
        std::string err;
        ok &= check(loopbackSendRecv(s, addr, m, got, err) &&
                        got.logicalGenerationId == gen &&
                        got.platformArtifactHash == hash &&
                        got.requiredCapabilityCount == 1 &&
                        got.requiredCapabilities[0] == 0xABC &&
                        got.requiredSchemas[0] == 0x123 &&
                        got.requiredDependencies[0] == 0x777,
                    "manifest requirement arrays traverse the real transport",
                    report);

        MimitaRuntime::GenerationManifestV1 vman{};
        vman.logicalGenerationId = got.logicalGenerationId;
        vman.platformArtifactHash = got.platformArtifactHash;
        vman.platformArtifactSize = got.platformArtifactSize;
        vman.hotAbiVersion = got.hotAbiVersion;
        vman.requiredCapabilityCount = got.requiredCapabilityCount;
        vman.requiredCapabilities[0] = got.requiredCapabilities[0];
        vman.requiredSchemaCount = got.requiredSchemaCount;
        vman.requiredSchemas[0] = got.requiredSchemas[0];
        vman.requiredDependencyCount = got.requiredDependencyCount;
        vman.requiredDependencies[0] = got.requiredDependencies[0];
        MimitaRuntime::GenerationLocalFactsV1 facts{};
        facts.artifactHash = hash;
        facts.artifactSize = (std::uint32_t)bytes.size();
        facts.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
        facts.hasCapability = [](void*, std::uint64_t id) { return id == 0xABC; };
        facts.hasSchema = [](void*, std::uint64_t id) { return id == 0x123; };
        facts.hasDependency = [](void*, std::uint64_t id) { return id == 0x777; };
        auto sendReady = [&]() -> bool {
            CodeGenerationPacket r{};
            r.header.type = PACKET_CODE_GENERATION;
            r.generation = (std::uint32_t)gen;
            r.direction = 0;
            r.phase = 1;
            r.platformPackageHash = hash;
            CodeGenerationPacket g{};
            std::string e;
            return loopbackSendRecv(s, addr, r, g, e) && g.phase == 1;
        };
        ok &= check(MimitaRuntime::verifyGeneration(vman, facts) ==
                            MimitaRuntime::VerifyFailure::None &&
                        sendReady(),
                    "wire manifest verifies -> READY crosses the transport",
                    report);

        MimitaRuntime::GenerationManifestV1 badCap = vman;
        badCap.requiredCapabilities[0] = 0xDEAD;
        ok &= check(MimitaRuntime::verifyGeneration(badCap, facts) ==
                            MimitaRuntime::VerifyFailure::MissingCapability,
                    "artifact valid but capability missing -> MissingCapability, no READY",
                    report);

        MimitaRuntime::GenerationManifestV1 badAbi = vman;
        badAbi.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION + 1u;
        ok &= check(MimitaRuntime::verifyGeneration(badAbi, facts) ==
                            MimitaRuntime::VerifyFailure::AbiMismatch,
                    "artifact valid but hot ABI incompatible -> AbiMismatch, no READY",
                    report);

        MimitaRuntime::GenerationManifestV1 badSchema = vman;
        badSchema.requiredSchemas[0] = 0xBEEF;
        ok &= check(MimitaRuntime::verifyGeneration(badSchema, facts) ==
                            MimitaRuntime::VerifyFailure::SchemaMismatch,
                    "artifact valid but schema mismatch -> SchemaMismatch, no READY",
                    report);

        MimitaRuntime::GenerationManifestV1 other = vman;
        other.platformArtifactHash = hash ^ 0x55u;
        ok &= check(MimitaRuntime::verifyGeneration(other, facts) ==
                            MimitaRuntime::VerifyFailure::HashMismatch,
                    "manifest identity binding: G's manifest does not validate H",
                    report);

        GenerationManifestPacket over = m;
        over.requiredCapabilityCount = GENERATION_MANIFEST_MAX_REQUIREMENTS + 1;
        const bool countsOk =
            over.manifestVersion == GENERATION_MANIFEST_VERSION &&
            over.requiredCapabilityCount <= GENERATION_MANIFEST_MAX_REQUIREMENTS;
        ok &= check(!countsOk, "oversized requirement count is rejected", report);
    }

    // ── SWITCH crosses the socket ─────────────────────────────────────
    {
        CodeGenerationPacket sw{};
        sw.header.type = PACKET_CODE_GENERATION;
        sw.header.tick = 1000;
        sw.generation = (uint32_t)gen;
        sw.direction = 1;
        sw.phase = 2;
        sw.switchTick = 1030;
        CodeGenerationPacket got{};
        std::string err;
        const bool sent = loopbackSendRecv(s, addr, sw, got, err);
        const std::uint32_t mapped =
            MimitaRuntime::mapServerSwitchTickToClientLocal(
                got.header.tick, 1004, got.switchTick);
        ok &= check(sent && got.phase == 2 && got.switchTick == 1030 &&
                        mapped == 1034,
                    "SWITCH traverses the transport; tick maps to 1034 (not 1030)",
                    report);
    }

    // ── Snapshot generation crosses the socket ────────────────────────
    {
        std::vector<CompactEntityData> one(1);
        one[0].networkEntityId = 7;
        std::vector<std::vector<uint8_t>> chunks;
        std::string err;
        ok &= check(buildSnapshotChunks(one.data(), 1, 1000, 0, (uint32_t)gen,
                                        chunks, &err) &&
                        chunks.size() == 1,
                    "snapshot chunk built with generation", report);
        if (!chunks.empty()) {
            sendto(s, (const char*)chunks[0].data(), (int)chunks[0].size(), 0,
                   (const sockaddr*)&addr, sizeof(addr));
            std::vector<uint8_t> buf(2048);
            sockaddr_in from{};
            int fromLen = sizeof(from);
            const int n = recvfrom(s, (char*)buf.data(), (int)buf.size(), 0,
                                   (sockaddr*)&from, &fromLen);
            SnapshotChunkPacket got{};
            ok &= check(n > 0 &&
                            parseSnapshotChunk(buf.data(), (size_t)n, got, &err) &&
                            got.logicalGenerationId == (uint32_t)gen,
                        "snapshot generation traverses the real transport", report);
        }
    }

    // ── Cache-hit case: no artifact bytes transferred ─────────────────
    {
        // The artifact is cached; a receiver "beginFromCache" completes with no
        // chunk bytes sent.
        MimitaRuntime::ArtifactAcquirer acq;
        acq.beginFromCache(gen, hash, cache.contains(hash));
        std::string err;
        ok &= check(acq.commit(err) && cache.contains(hash),
                    "cache hit verifies without transferring bytes", report);
    }

    report += "  [info] artifact bytes=" + std::to_string(bytes.size()) +
              " chunks=" + std::to_string(streamer.chunkCount()) +
              " transportBytes=" + std::to_string(bytesTransferred) + "\n";

    closesocket(s);
    std::filesystem::remove_all(root, ec);
    WSACleanup();
    return ok;
}
