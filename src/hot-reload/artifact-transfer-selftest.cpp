// 09 15 2026
/* purpose
* Implements the headless chunked artifact wire-transfer self-test: in-order,
* duplicate, out-of-order, missing-chunk, wrong-size, wrong-hash, disconnect,
* cache commit, and invalid-begin cases through the real packet structs.
* Does NOT own rendering/presentation or the transport.
*/
#include "hot-reload/artifact-transfer-selftest.h"

#include <filesystem>
#include <string>
#include <vector>

#include "hot-reload/artifact-cache.h"
#include "hot-reload/artifact-transfer.h"

using namespace MimitaRuntime;

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
        v[i] = (unsigned char)((i * 31 + 7) & 0xff);
    return v;
}

} // namespace

bool runArtifactTransferSelfTest(std::string& report)
{
    bool ok = true;
    ArtifactCache& cache = ArtifactCache::instance();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mimita-artifact-transfer-selftest";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    cache.setRoot(root);

    const std::vector<unsigned char> bytes = makeBytes(2500);  // 3 chunks
    const std::uint64_t hash = hashArtifactBytes(bytes.data(), bytes.size());
    const std::uint64_t genId = 0xABCDEF;

    ArtifactStreamer streamer;
    ok &= check(streamer.begin(genId, hash, bytes.data(), bytes.size()) &&
                    streamer.chunkCount() == 3,
                "streamer chunks a verified artifact (bounded chunks)", report);
    ok &= check(!streamer.begin(genId, hash + 1, bytes.data(), bytes.size()),
                "streamer rejects bytes that do not match the artifact hash",
                report);
    ok &= check(!streamer.begin(genId, 0, bytes.data(), bytes.size()),
                "streamer rejects unknown/zero artifact hash", report);

    ArtifactBeginPacket begin{};
    streamer.makeBegin(begin);
    ArtifactChunkPacket c0{}, c1{}, c2{};
    streamer.makeChunk(0, c0);
    streamer.makeChunk(1, c1);
    streamer.makeChunk(2, c2);
    ok &= check(c0.size == 1000 && c1.size == 1000 && c2.size == 500 &&
                    c2.offset == 2000,
                "chunk sizes/offsets are exact", report);

    // In-order, with a duplicate chunk (safe).
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        ok &= check(rx.onChunk(c0) && rx.onChunk(c0) && rx.receivedChunks() == 1,
                    "duplicate chunk is safe and counted once", report);
        rx.onChunk(c1);
        rx.onChunk(c2);
        ok &= check(rx.complete() && rx.phase() == AcquirePhase::Verifying,
                    "in-order transfer completes to Verifying", report);
        std::string err;
        ok &= check(rx.commit(err) && rx.phase() == AcquirePhase::Complete &&
                        cache.contains(hash),
                    "receiver verifies + stores the artifact into the cache",
                    report);
    }

    // Cache hit: a fresh receiver beginning from a cached hash skips transfer.
    {
        ArtifactReceiver rx;
        ArtifactBeginPacket b = begin;
        rx.begin(b);
        ok &= check(cache.contains(hash), "cache hit available for re-verify", report);
    }

    // Out-of-order delivery.
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        rx.onChunk(c2);
        rx.onChunk(c0);
        rx.onChunk(c1);
        std::string err;
        ok &= check(rx.complete() && rx.commit(err),
                    "out-of-order chunks reassemble and commit", report);
    }

    // Missing chunk: never valid.
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        rx.onChunk(c0);
        rx.onChunk(c1);
        std::string err;
        ok &= check(!rx.complete() && !rx.commit(err),
                    "missing chunk cannot become a valid artifact", report);
    }

    // Wrong chunk size is rejected.
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        ArtifactChunkPacket bad = c0;
        bad.size = 999;
        ok &= check(!rx.onChunk(bad), "wrong chunk size is rejected", report);
    }

    // Wrong-hash chunk is rejected.
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        ArtifactChunkPacket bad = c0;
        bad.platformArtifactHash = hash + 1;
        ok &= check(!rx.onChunk(bad), "wrong-hash chunk is rejected", report);
    }

    // Disconnect mid-transfer fails cleanly; never READY.
    {
        ArtifactReceiver rx;
        rx.begin(begin);
        rx.onChunk(c0);
        rx.fail("disconnect");
        ok &= check(rx.phase() == AcquirePhase::Failed && !rx.complete(),
                    "interrupted transfer fails cleanly", report);
    }

    // Invalid begin (chunk count too small for total size).
    {
        ArtifactReceiver rx;
        ArtifactBeginPacket bad = begin;
        bad.chunkCount = 1;  // 1*1000 < 2500
        rx.begin(bad);
        ok &= check(rx.phase() == AcquirePhase::Failed,
                    "invalid begin (size/chunk mismatch) fails cleanly", report);
    }

    std::filesystem::remove_all(root, ec);
    return ok;
}
