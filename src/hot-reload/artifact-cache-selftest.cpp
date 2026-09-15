// 09 15 2026
/* purpose
* Implements the headless content-addressed artifact-cache/acquisition self-test:
* hash verify, immutable dedupe, no-overwrite, acquisition phase machine, and
* failure on hash mismatch. Mechanism only (no network).
* Does NOT own rendering/presentation or the transport.
*/
#include "hot-reload/artifact-cache-selftest.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "hot-reload/artifact-cache.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runArtifactCacheSelfTest(std::string& report)
{
    bool ok = true;
    ArtifactCache& cache = ArtifactCache::instance();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mimita-hotcache-selftest";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    cache.setRoot(root);

    const char* a = "artifact-bytes-A-payload";
    const char* b = "artifact-bytes-B-payload-different";
    const std::size_t aLen = std::strlen(a);
    const std::size_t bLen = std::strlen(b);
    const std::uint64_t hashA = hashArtifactBytes(a, aLen);
    const std::uint64_t hashB = hashArtifactBytes(b, bLen);

    ok &= check(hashA != hashB && ArtifactCache::verify(hashA, a, aLen),
                "artifact hash verifies matching bytes", report);
    ok &= check(!ArtifactCache::verify(hashA + 1, a, aLen),
                "artifact hash rejects mismatched bytes", report);

    ok &= check(cache.store(hashA, a, aLen) && cache.contains(hashA),
                "store writes an immutable content-addressed artifact", report);
    ok &= check(cache.store(hashA, a, aLen),
                "re-store of identical bytes is idempotent (dedupe)", report);
    ok &= check(!cache.store(hashA, b, bLen),
                "store never overwrites an existing hash with different bytes",
                report);

    {
        std::vector<unsigned char> out;
        ok &= check(cache.read(hashA, out) && out.size() == aLen &&
                        std::memcmp(out.data(), a, aLen) == 0,
                    "stored artifact reads back exactly", report);
    }

    ok &= check(!cache.store(hashB + 7, b, bLen),
                "store rejects bytes that do not match the expected hash", report);

    // Acquisition machine: receive -> verify -> store.
    {
        ArtifactAcquirer acq;
        acq.begin(0x1234, hashB);
        ok &= check(acq.phase() == AcquirePhase::Requesting,
                    "acquirer begins in Requesting", report);
        acq.appendChunk(b, 10);
        acq.appendChunk(b + 10, bLen - 10);
        ok &= check(acq.phase() == AcquirePhase::Receiving,
                    "acquirer reports Receiving while chunks arrive", report);
        std::string err;
        ok &= check(acq.commit(err) && acq.phase() == AcquirePhase::Complete &&
                        cache.contains(hashB),
                    "acquirer verifies + stores received artifact", report);
    }

    // Mismatch: wrong bytes for the expected hash fails.
    {
        ArtifactAcquirer acq;
        acq.begin(0x1234, hashB);
        acq.appendChunk(a, aLen);
        std::string err;
        ok &= check(!acq.commit(err) && acq.phase() == AcquirePhase::Failed &&
                        !err.empty(),
                    "acquirer fails on artifact hash mismatch", report);
    }

    // Cache-hit path short-circuits to Verifying and completes without transfer.
    {
        ArtifactAcquirer acq;
        acq.beginFromCache(0x1234, hashA, true);
        ok &= check(acq.phase() == AcquirePhase::Verifying,
                    "cache hit short-circuits acquisition to Verifying", report);
        std::string err;
        ok &= check(acq.commit(err) && acq.phase() == AcquirePhase::Complete,
                    "cache-hit acquisition completes via verification", report);
    }

    std::filesystem::remove_all(root, ec);
    return ok;
}
