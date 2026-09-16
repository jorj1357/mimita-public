// 09 15 2026
/* purpose
* Headless self-test for the unified logical-resource authority. Immutable bytes
* live in the real ArtifactCache; validation + publication go through
* publishContentArtifact, and the ONE authoritative active mapping / last-good /
* handle live in MimitaRuntime::PresentationResourceProvider (the same provider
* the real render path resolves with handleOf). Proves PNG/GLB/WAV publication,
// cache hit, malformed last-good, and supersede safety with no second registry.
*/
#include "hot-reload/content-resource-selftest.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "hot-reload/artifact-cache.h"
#include "hot-reload/content-artifact.h"
#include "project/presentation-resource.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::uintptr_t g_handleCounter = 0;

bool fakeLoad(void*, void** outHandle)
{
    *outHandle = reinterpret_cast<void*>(++g_handleCounter);
    return true;
}
void fakeRetire(void*, void*) {}

void registerLoader(std::uint64_t id)
{
    PresentationResourceProvider::instance().setLoader(id, &fakeLoad, &fakeRetire,
                                                       nullptr);
}

std::vector<unsigned char> pngBytes(unsigned char fill)
{
    std::vector<unsigned char> v(24, fill);
    const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) v[i] = sig[i];
    v[12] = 'I'; v[13] = 'H'; v[14] = 'D'; v[15] = 'R';
    return v;
}
std::vector<unsigned char> glbBytes(unsigned char fill)
{
    std::vector<unsigned char> v(24, fill);
    v[0] = 'g'; v[1] = 'l'; v[2] = 'T'; v[3] = 'F';
    v[4] = 2; v[5] = 0; v[6] = 0; v[7] = 0;
    return v;
}
std::vector<unsigned char> wavBytes(unsigned char fill)
{
    std::vector<unsigned char> v(24, fill);
    v[0] = 'R'; v[1] = 'I'; v[2] = 'F'; v[3] = 'F';
    v[8] = 'W'; v[9] = 'A'; v[10] = 'V'; v[11] = 'E';
    return v;
}
std::vector<unsigned char> malformed(unsigned char fill)
{
    return std::vector<unsigned char>(24, fill);
}

bool exerciseKind(const char* logicalName, ResourceKind kind, unsigned char fillA,
                  unsigned char fillB, std::string& report)
{
    bool ok = true;
    const std::uint64_t id = resourceIdFromLogicalName(logicalName);
    ResourceRegistry& reg = ResourceRegistry::instance();
    ArtifactCache& cache = ArtifactCache::instance();
    PresentationResourceProvider& provider =
        PresentationResourceProvider::instance();
    registerLoader(id);

    auto bytesForKind = [&](unsigned char fill) {
        if (kind == ResourceKind::Png) return pngBytes(fill);
        if (kind == ResourceKind::Glb) return glbBytes(fill);
        return wavBytes(fill);
    };
    const std::vector<unsigned char> A = bytesForKind(fillA);
    const std::vector<unsigned char> B = bytesForKind(fillB);
    const std::uint64_t hashA = hashArtifactBytes(A.data(), A.size());
    const std::uint64_t hashB = hashArtifactBytes(B.data(), B.size());

    // A published: the canonical provider becomes the active authority.
    cache.store(hashA, A.data(), A.size());
    reg.announceCandidate({id, (std::uint32_t)kind, hashA, (std::uint32_t)A.size()});
    std::string err;
    ok &= check(publishContentArtifact(id, hashA, A.data(), A.size(), err) &&
                    provider.current(id) != nullptr &&
                    provider.current(id)->contentHash == hashA &&
                    provider.handleOf(id) != nullptr,
                std::string(logicalName) + ": publish A -> provider authority",
                report);

    // B published; provider is the single active mapping (no second registry).
    reg.announceCandidate({id, (std::uint32_t)kind, hashB, (std::uint32_t)B.size()});
    const bool bPub = publishContentArtifact(id, hashB, B.data(), B.size(), err);
    ok &= check(bPub && provider.current(id)->contentHash == hashB &&
                    ResourceRegistry::instance().resolve(id) == hashB,
                std::string(logicalName) + ": publish B, resolve reads provider",
                report);

    // Cache hit publishes with zero transferred bytes.
    cache.store(hashB, B.data(), B.size());
    reg.announceCandidate({id, (std::uint32_t)kind, hashB, (std::uint32_t)B.size()});
    ok &= check(publishContentArtifactFromCache(id, err) &&
                    provider.current(id)->contentHash == hashB,
                std::string(logicalName) + ": cache hit publishes via provider",
                report);

    // Malformed candidate: validation fails, provider keeps last-good B.
    const std::vector<unsigned char> bad = malformed(fillA ^ 0x5A);
    const std::uint64_t hashBad = hashArtifactBytes(bad.data(), bad.size());
    cache.store(hashBad, bad.data(), bad.size());
    reg.announceCandidate({id, (std::uint32_t)kind, hashBad,
                           (std::uint32_t)bad.size()});
    const bool rejected = !publishContentArtifactFromCache(id, err);
    ok &= check(rejected && provider.current(id)->contentHash == hashB &&
                    provider.handleOf(id) != nullptr,
                std::string(logicalName) + ": malformed keeps provider last-good B",
                report);

    // Stale/superseded: B2 announced, C2 supersedes; late B2 publish rejected.
    const std::vector<unsigned char> B2 = bytesForKind(fillB ^ 0x3C);
    const std::vector<unsigned char> C2 = bytesForKind(fillA ^ 0x77);
    const std::uint64_t hashB2 = hashArtifactBytes(B2.data(), B2.size());
    const std::uint64_t hashC2 = hashArtifactBytes(C2.data(), C2.size());
    reg.announceCandidate({id, (std::uint32_t)kind, hashB2, (std::uint32_t)B2.size()});
    reg.announceCandidate({id, (std::uint32_t)kind, hashC2, (std::uint32_t)C2.size()});
    const bool staleRejected = !publishContentArtifact(id, hashB2, B2.data(),
                                                       B2.size(), err);
    ok &= check(staleRejected && provider.current(id)->contentHash == hashB,
                std::string(logicalName) + ": superseded candidate cannot overwrite",
                report);
    ok &= check(publishContentArtifact(id, hashC2, C2.data(), C2.size(), err) &&
                    provider.current(id)->contentHash == hashC2,
                std::string(logicalName) + ": newer candidate publishes", report);

    return ok;
}

} // namespace

bool runContentResourceSelfTest(std::string& report)
{
    bool ok = true;
    ArtifactCache::instance().setRoot(
        std::filesystem::temp_directory_path() / "mimita-content-resource");
    ResourceRegistry::instance().clear();
    PresentationResourceProvider::instance().clear();

    ok &= exerciseKind("ui.menu.logo", ResourceKind::Png, 0x10, 0x20, report);
    ok &= exerciseKind("mesh.tool.rocket", ResourceKind::Glb, 0x30, 0x40, report);
    ok &= exerciseKind("audio.weapon.rocket.fire", ResourceKind::Wav, 0x50, 0x60,
                       report);

    // Single source of truth: the provider holds the active mapping and the
    // content registry has no independent active state.
    {
        const std::uint64_t id = resourceIdFromLogicalName("mesh.tool.rocket");
        const ResourceGeneration* g = PresentationResourceProvider::instance()
            .current(id);
        ok &= check(g != nullptr && g->contentHash != 0 &&
                        ResourceRegistry::instance().resolve(id) == g->contentHash,
                    "one canonical authority (provider) for the active mapping",
                    report);
    }

    ok &= check(validatorFor((std::uint32_t)ResourceKind::Png) == &validatePng &&
                    validatorFor((std::uint32_t)ResourceKind::Glb) == &validateGlb &&
                    validatorFor((std::uint32_t)ResourceKind::Wav) == &validateWav,
                "kind selects the validator generically", report);

    return ok;
}
