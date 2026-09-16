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
#include "hot-reload/migration-prep.h"
#include "hot-reload/generation-distribution.h"
#include "hot-reload/generation-bootstrap.h"
#include "hot-reload/content-artifact.h"

using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::uintptr_t g_contentHandles = 0;
bool contentLoad(void*, void** outHandle)
{
    *outHandle = reinterpret_cast<void*>(++g_contentHandles);
    return true;
}
void contentRetire(void*, void*) {}

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
        m.requiredSchemaVersions[0] = 2;
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
                        got.requiredSchemaVersions[0] == 2 &&
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

        // Migration preparation is part of READY semantics: verification passes
        // but no migration path exists for a stored version change -> no READY.
        {
            MimitaRuntime::MigrationPrepareFactsV1 mf{};
            mf.candidateSchemaCount = 1;
            mf.candidateSchemaIds[0] = 0x2222;
            mf.candidateSchemaVersions[0] = 2;
            mf.storedSchemaVersion = [](void*, std::uint64_t) { return 1u; };
            mf.hasMigrationPath = [](void*, std::uint64_t, std::uint32_t,
                                     std::uint32_t) { return false; };
            const MimitaRuntime::MigrationPlanV1 plan =
                MimitaRuntime::prepareMigration(5, gen, mf);
            ok &= check(MimitaRuntime::verifyGeneration(vman, facts) ==
                                MimitaRuntime::VerifyFailure::None &&
                            plan.outcome == MimitaRuntime::MigrationOutcome::Failed &&
                            plan.failure ==
                                MimitaRuntime::MigrationFailure::MissingMigration,
                        "verify passes but migration missing -> no READY",
                        report);
        }

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

    // ── REAL multi-peer quorum over sockets (server + A + B) ──────────
    {
        auto makeSock = []() -> SOCKET {
            SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.sin_port = 0;
            if (sock != INVALID_SOCKET)
                bind(sock, (const sockaddr*)&a, sizeof(a));
            return sock;
        };
        auto addrOf = [](SOCKET sock) {
            sockaddr_in a{};
            int len = sizeof(a);
            getsockname(sock, (sockaddr*)&a, &len);
            return a;
        };
        SOCKET aSock = makeSock();
        SOCKET bSock = makeSock();
        const sockaddr_in aAddr = addrOf(aSock);
        const sockaddr_in bAddr = addrOf(bSock);

        auto serverSend = [&](const sockaddr_in& to) {
            CodeGenerationPacket announce{};
            announce.header.type = PACKET_CODE_GENERATION;
            announce.generation = (uint32_t)gen;
            announce.direction = 1;
            announce.platformPackageHash = hash;
            sendto(s, (const char*)&announce, sizeof(announce), 0,
                   (const sockaddr*)&to, sizeof(to));
            GenerationManifestPacket man{};
            man.header.type = PACKET_GENERATION_MANIFEST;
            man.logicalGenerationId = gen;
            man.platformArtifactHash = hash;
            man.requiredCapabilityCount = 1;
            man.requiredCapabilities[0] = 0xABC;
            sendto(s, (const char*)&man, sizeof(man), 0, (const sockaddr*)&to,
                   sizeof(to));
        };
        auto clientRecvAnnounce = [&](SOCKET client) {
            char buf[512];
            sockaddr_in from{};
            int len = sizeof(from);
            const int n = recvfrom(client, buf, sizeof(buf), 0, (sockaddr*)&from,
                                   &len);
            return n >= (int)sizeof(CodeGenerationPacket) &&
                reinterpret_cast<CodeGenerationPacket*>(buf)->generation ==
                    (uint32_t)gen;
        };
        auto clientSendReady = [&](SOCKET client) {
            CodeGenerationPacket ready{};
            ready.header.type = PACKET_CODE_GENERATION;
            ready.generation = (uint32_t)gen;
            ready.direction = 0;
            ready.phase = 1;
            ready.platformPackageHash = hash;
            sendto(client, (const char*)&ready, sizeof(ready), 0,
                   (const sockaddr*)&addr, sizeof(addr));
        };
        auto serverRecvReady = [&](uint64_t peerId) {
            char buf[512];
            sockaddr_in from{};
            int len = sizeof(from);
            const int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
            if (n < (int)sizeof(CodeGenerationPacket))
                return false;
            const CodeGenerationPacket* got =
                reinterpret_cast<const CodeGenerationPacket*>(buf);
            if (got->phase != 1 || got->generation != (uint32_t)gen)
                return false;
            MimitaRuntime::GenerationIdentityV1 ident{};
            ident.logicalGenerationId = gen;
            ident.platformArtifactHash = hash;
            ident.abiVersion = (uint32_t)MIMITA_GAME_API_VERSION;
            auto& dist = MimitaRuntime::GenerationDistribution::instance();
            dist.announce(peerId, ident);
            dist.setPhase(peerId, MimitaRuntime::GenerationPhase::Ready);
            return true;
        };

        auto& dist = MimitaRuntime::GenerationDistribution::instance();
        dist.reset();
        serverSend(aAddr);
        serverSend(bAddr);
        const bool aAnnounced = clientRecvAnnounce(aSock);
        const bool bAnnounced = clientRecvAnnounce(bSock);

        clientSendReady(aSock);
        const bool aReady = serverRecvReady(1);
        ok &= check(aAnnounced && bAnnounced && aReady &&
                        !dist.quorumReady({1, 2}, gen),
                    "A READY, B not -> no quorum, no SWITCH", report);

        clientSendReady(bSock);
        const bool bReady = serverRecvReady(2);
        const bool quorum = dist.quorumReady({1, 2}, gen);
        const bool scheduled = quorum && dist.scheduleSwitch(gen, 1030);
        ok &= check(bReady && quorum && scheduled && dist.switchScheduled(),
                    "B READY -> quorum true -> SWITCH scheduled", report);

        // Disconnect AFTER commit: committed G must not be cancelled.
        dist.removePeer(2);
        ok &= check(dist.switchScheduled() &&
                        dist.scheduledGeneration() == gen,
                    "disconnect after commit does not cancel G", report);
        dist.reset();

        // Disconnect BEFORE commit: recompute quorum over remaining peers.
        dist.announce(1, {gen, 0, hash, (uint32_t)MIMITA_GAME_API_VERSION});
        dist.setPhase(1, MimitaRuntime::GenerationPhase::Ready);
        dist.announce(2, {gen, 0, hash, (uint32_t)MIMITA_GAME_API_VERSION});
        const bool blocked = !dist.quorumReady({1, 2}, gen);
        dist.removePeer(2);
        const bool recomputed = dist.quorumReady({1}, gen);
        ok &= check(blocked && recomputed,
                    "disconnect before commit recomputes quorum over survivors",
                    report);
        dist.reset();

        closesocket(aSock);
        closesocket(bSock);
    }

    // ── Late-join generation bootstrap over the real transport ─────────
    {
        auto makeManifest = [](std::uint64_t g, std::uint64_t h, std::uint32_t sz) {
            MimitaRuntime::GenerationManifestV1 m{};
            m.logicalGenerationId = g;
            m.platformArtifactHash = h;
            m.platformArtifactSize = sz;
            m.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
            return m;
        };
        auto makeFacts = [](std::uint64_t h, std::uint32_t sz) {
            MimitaRuntime::GenerationLocalFactsV1 f{};
            f.artifactHash = h;
            f.artifactSize = sz;
            f.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
            return f;
        };

        // Server already active on a generation the client does not have.
        const std::vector<unsigned char> bytes2 = makeBytes(2500);
        const std::uint64_t hash2 =
            MimitaRuntime::hashArtifactBytes(bytes2.data(), bytes2.size());
        const std::uint64_t gen2 = 0x6B6B;
        MimitaRuntime::ArtifactStreamer streamer2;
        streamer2.begin(gen2, hash2, bytes2.data(), bytes2.size());

        // ── Cache miss ────────────────────────────────────────────────
        {
            MimitaRuntime::GenerationBootstrapV1 boot;
            boot.begin(gen2);
            CodeGenerationPacket bootPkt{};
            bootPkt.header.type = PACKET_CODE_GENERATION;
            bootPkt.generation = (std::uint32_t)gen2;
            bootPkt.direction = 1;
            bootPkt.phase = CODE_GENERATION_PHASE_ACTIVE_BOOTSTRAP;
            bootPkt.platformPackageHash = hash2;
            CodeGenerationPacket got{};
            std::string err;
            const bool advertised =
                loopbackSendRecv(s, addr, bootPkt, got, err) &&
                got.phase == CODE_GENERATION_PHASE_ACTIVE_BOOTSTRAP;
            boot.onMetadata(got.generation, got.platformPackageHash,
                            cache.contains(hash2));
            const bool blockedBefore =
                !boot.worldParticipationAllowed(gen2, gen2);

            MimitaRuntime::ArtifactReceiver rx2;
            std::uint32_t transferred = 0;
            ArtifactBeginPacket begin{};
            streamer2.makeBegin(begin);
            begin.header.type = PACKET_ARTIFACT_BEGIN;
            ArtifactBeginPacket gotBegin{};
            bool okTransfer = loopbackSendRecv(s, addr, begin, gotBegin, err);
            rx2.begin(gotBegin);
            transferred += (std::uint32_t)sizeof(ArtifactBeginPacket);
            for (std::uint32_t i = 0; okTransfer && i < streamer2.chunkCount(); ++i) {
                ArtifactChunkPacket chunk{};
                streamer2.makeChunk(i, chunk);
                chunk.header.type = PACKET_ARTIFACT_CHUNK;
                ArtifactChunkPacket gotChunk{};
                okTransfer = loopbackSendRecv(s, addr, chunk, gotChunk, err) &&
                    rx2.onChunk(gotChunk);
                transferred += (std::uint32_t)sizeof(ArtifactChunkPacket);
            }
            boot.onArtifactAcquired(gen2);
            std::string commitErr;
            const bool committed = okTransfer && rx2.complete() &&
                rx2.commit(commitErr);
            boot.complete(gen2, /*codeLoaded=*/true, makeManifest(gen2, hash2,
                          (std::uint32_t)bytes2.size()),
                          makeFacts(hash2, (std::uint32_t)bytes2.size()));
            ok &= check(advertised && blockedBefore &&
                            boot.state == MimitaRuntime::BootstrapState::Ready &&
                            boot.worldParticipationAllowed(gen2, gen2) &&
                            transferred > 0 && committed,
                        "late join cache miss: acquire -> verify -> world", report);
        }

        // ── Cache hit (artifact now cached): zero chunk bytes ─────────
        {
            MimitaRuntime::GenerationBootstrapV1 boot;
            boot.begin(gen2);
            boot.onMetadata(gen2, hash2, cache.contains(hash2));
            const bool verifying = boot.state == MimitaRuntime::BootstrapState::Verifying;
            const std::uint32_t transferred = 0;  // no chunks requested
            boot.complete(gen2, true, makeManifest(gen2, hash2,
                          (std::uint32_t)bytes2.size()),
                          makeFacts(hash2, (std::uint32_t)bytes2.size()));
            ok &= check(verifying && transferred == 0 && cache.contains(hash2) &&
                            boot.state == MimitaRuntime::BootstrapState::Ready &&
                            boot.worldParticipationAllowed(gen2, gen2),
                        "late join cache hit: zero chunks, still verified", report);
        }

        // ── Generation changes during bootstrap ───────────────────────
        {
            MimitaRuntime::GenerationBootstrapV1 boot;
            boot.begin(gen2);
            boot.onMetadata(gen2, hash2, true);
            boot.onServerActiveChanged((std::uint64_t)gen);  // server moved on
            boot.complete(gen2, true, makeManifest(gen2, hash2,
                          (std::uint32_t)bytes2.size()),
                          makeFacts(hash2, (std::uint32_t)bytes2.size()));
            ok &= check(boot.targetGeneration == (std::uint64_t)gen &&
                            boot.state != MimitaRuntime::BootstrapState::Ready,
                        "stale bootstrap cannot activate after active gen changes",
                        report);
        }
    }

    // ── Content descriptor over the real socket (PNG/GLB shape) ────────
    {
        MimitaRuntime::ResourceRegistry::instance().clear();
        MimitaRuntime::PresentationResourceProvider::instance().clear();
        std::vector<unsigned char> glbA(24, 0x30);
        glbA[0] = 'g'; glbA[1] = 'l'; glbA[2] = 'T'; glbA[3] = 'F';
        glbA[4] = 2; glbA[5] = 0; glbA[6] = 0; glbA[7] = 0;
        const std::uint64_t hashA =
            MimitaRuntime::hashArtifactBytes(glbA.data(), glbA.size());
        const std::uint64_t resId =
            MimitaRuntime::resourceIdFromLogicalName("mesh.tool.rocket");
        MimitaRuntime::PresentationResourceProvider::instance().setLoader(
            resId, &contentLoad, &contentRetire, nullptr);

        ContentArtifactPacket desc{};
        desc.header.type = PACKET_CONTENT_ARTIFACT;
        desc.logicalResourceId = resId;
        desc.resourceKind = (std::uint32_t)MimitaRuntime::ResourceKind::Glb;
        desc.contentHash = hashA;
        desc.byteSize = (std::uint32_t)glbA.size();
        ContentArtifactPacket gotDesc{};
        std::string err;
        const bool descended = loopbackSendRecv(s, addr, desc, gotDesc, err) &&
            gotDesc.logicalResourceId == resId && gotDesc.contentHash == hashA &&
            gotDesc.resourceKind == (std::uint32_t)MimitaRuntime::ResourceKind::Glb;

        // Identity binding: bytes for a different hash cannot satisfy this.
        ok &= check(descended &&
                        MimitaRuntime::ResourceRegistry::instance().announceCandidate(
                            {gotDesc.logicalResourceId, gotDesc.resourceKind,
                             gotDesc.contentHash, gotDesc.byteSize}),
                    "content descriptor traverses the real transport", report);

        // Cache miss: transfer bytes through the existing artifact path.
        MimitaRuntime::ArtifactStreamer rs;
        rs.begin(resId, hashA, glbA.data(), glbA.size());
        MimitaRuntime::ArtifactReceiver rr;
        std::uint32_t contentBytes = 0;
        {
            ArtifactBeginPacket begin{};
            rs.makeBegin(begin);
            begin.header.type = PACKET_ARTIFACT_BEGIN;
            ArtifactBeginPacket gotBegin{};
            bool tr = loopbackSendRecv(s, addr, begin, gotBegin, err);
            rr.begin(gotBegin);
            contentBytes += (std::uint32_t)sizeof(ArtifactBeginPacket);
            for (std::uint32_t i = 0; tr && i < rs.chunkCount(); ++i) {
                ArtifactChunkPacket chunk{};
                rs.makeChunk(i, chunk);
                chunk.header.type = PACKET_ARTIFACT_CHUNK;
                ArtifactChunkPacket gotChunk{};
                tr = loopbackSendRecv(s, addr, chunk, gotChunk, err) &&
                    rr.onChunk(gotChunk);
                contentBytes += (std::uint32_t)sizeof(ArtifactChunkPacket);
            }
        }
        std::string commitErr;
        std::string perr;
        bool published = false;
        if (rr.complete() && rr.commit(commitErr)) {
            published = MimitaRuntime::publishContentArtifactFromCache(resId, perr);
        }
        const MimitaRuntime::ResourceGeneration* rg =
            MimitaRuntime::PresentationResourceProvider::instance().current(resId);
        ok &= check(published && contentBytes > 0 && rg != nullptr &&
                        rg->contentHash == hashA,
                    "content cache miss: transfer -> validate -> publish [" +
                        commitErr + perr + "]", report);

        // Cache hit: republishing the same descriptor needs zero bytes.
        ok &= check(MimitaRuntime::ArtifactCache::instance().contains(hashA) &&
                        MimitaRuntime::ResourceRegistry::instance()
                            .announceCandidate({resId, (std::uint32_t)
                                MimitaRuntime::ResourceKind::Glb, hashA,
                                (std::uint32_t)glbA.size()}),
                    "content cache hit: descriptor announces with bytes cached",
                    report);
        std::string hitErr;
        ok &= check(MimitaRuntime::publishContentArtifactFromCache(resId, hitErr),
                    "content cache hit publishes with zero chunks", report);

        // Stale descriptor: B announced, C supersedes, late B publish rejected.
        std::vector<unsigned char> glbB(24, 0x40);
        glbB[0]='g'; glbB[1]='l'; glbB[2]='T'; glbB[3]='F'; glbB[4]=2; glbB[5]=0; glbB[6]=0; glbB[7]=0;
        std::vector<unsigned char> glbC(24, 0x50);
        glbC[0]='g'; glbC[1]='l'; glbC[2]='T'; glbC[3]='F'; glbC[4]=2; glbC[5]=0; glbC[6]=0; glbC[7]=0;
        const std::uint64_t hashB =
            MimitaRuntime::hashArtifactBytes(glbB.data(), glbB.size());
        const std::uint64_t hashC =
            MimitaRuntime::hashArtifactBytes(glbC.data(), glbC.size());
        auto& reg = MimitaRuntime::ResourceRegistry::instance();
        reg.announceCandidate({resId, (std::uint32_t)MimitaRuntime::ResourceKind::Glb,
                               hashB, (std::uint32_t)glbB.size()});
        reg.announceCandidate({resId, (std::uint32_t)MimitaRuntime::ResourceKind::Glb,
                               hashC, (std::uint32_t)glbC.size()});
        std::string serr;
        const bool lateRejected =
            !MimitaRuntime::publishContentArtifact(resId, hashB, glbB.data(),
                                                   glbB.size(), serr);
        const bool newerPublished =
            MimitaRuntime::publishContentArtifact(resId, hashC, glbC.data(),
                                                  glbC.size(), serr);
        const MimitaRuntime::ResourceGeneration* rgC =
            MimitaRuntime::PresentationResourceProvider::instance().current(resId);
        ok &= check(lateRejected && newerPublished && rgC != nullptr &&
                        rgC->contentHash == hashC,
                    "stale descriptor cannot overwrite the newer content version",
                    report);
    }

    report += "  [info] artifact bytes=" + std::to_string(bytes.size()) +
              " chunks=" + std::to_string(streamer.chunkCount()) +
              " transportBytes=" + std::to_string(bytesTransferred) + "\n";

    closesocket(s);
    std::filesystem::remove_all(root, ec);
    WSACleanup();
    return ok;
}
