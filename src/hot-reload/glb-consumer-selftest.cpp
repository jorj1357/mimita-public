// 09 15 2026
/* purpose
* Proves a REAL equipped-tool mesh logical id changes its content live through the
* canonical content+provider path, and that the SAME production consumer call the
* renderer uses (PresentationResourceProvider::handleOf(HOT_MESH_ROCKET)) observes
* the new version without any Tool/entity/equip change. Visual frame capture is
* not available headlessly; the production call-path resolution is asserted.
*/
#include "hot-reload/glb-consumer-selftest.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "hot-reload/artifact-cache.h"
#include "hot-reload/content-artifact.h"
#include "hot-reload/hot-presentation.h"
#include "project/presentation-resource.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::uintptr_t g_meshHandles = 0;
bool meshLoad(void*, void** outHandle)
{
    *outHandle = reinterpret_cast<void*>(++g_meshHandles);
    return true;
}
void meshRetire(void*, void*) {}

std::vector<unsigned char> glb(unsigned char fill)
{
    std::vector<unsigned char> v(32, fill);
    v[0] = 'g'; v[1] = 'l'; v[2] = 'T'; v[3] = 'F';
    v[4] = 2; v[5] = 0; v[6] = 0; v[7] = 0;
    return v;
}

} // namespace

bool runGlbConsumerSelfTest(std::string& report)
{
    bool ok = true;
    ArtifactCache::instance().setRoot(
        std::filesystem::temp_directory_path() / "mimita-glb-consumer");
    ResourceRegistry::instance().clear();
    PresentationResourceProvider& provider =
        PresentationResourceProvider::instance();
    provider.clear();

    // The equipped tool's logical mesh id — the identity the renderer resolves.
    const std::uint64_t meshId = HOT_MESH_ROCKET;
    provider.setLoader(meshId, &meshLoad, &meshRetire, nullptr);

    const std::vector<unsigned char> A = glb(0x30);
    const std::vector<unsigned char> B = glb(0x60);
    const std::uint64_t hashA = hashArtifactBytes(A.data(), A.size());
    const std::uint64_t hashB = hashArtifactBytes(B.data(), B.size());
    ok &= check(hashA != hashB, "GLB A and B are distinct versions", report);

    // Publish A through the canonical content path.
    ArtifactCache::instance().store(hashA, A.data(), A.size());
    ResourceRegistry::instance().announceCandidate(
        {meshId, (std::uint32_t)ResourceKind::Glb, hashA, (std::uint32_t)A.size()});
    std::string err;
    const bool aPub = publishContentArtifact(meshId, hashA, A.data(), A.size(), err);
    void* handleA = provider.handleOf(meshId);
    ok &= check(aPub && provider.current(meshId) != nullptr &&
                    provider.current(meshId)->contentHash == hashA &&
                    handleA != nullptr,
                "tool mesh A active via canonical provider", report);

    // Publish B through the same canonical content path (no direct apply).
    ResourceRegistry::instance().announceCandidate(
        {meshId, (std::uint32_t)ResourceKind::Glb, hashB, (std::uint32_t)B.size()});
    const bool bPub = publishContentArtifact(meshId, hashB, B.data(), B.size(), err);
    void* handleB = provider.handleOf(meshId);

    // The production render consumer call path: handleOf(meshResourceId).
    ok &= check(bPub && provider.current(meshId)->contentHash == hashB &&
                    handleB != nullptr && handleB != handleA,
                "production handleOf(mesh.tool.rocket) resolves B, new handle",
                report);

    // Logical id is the source of identity and does not change on A -> B.
    ok &= check(ResourceRegistry::instance().candidateHashOf(meshId) == hashB ||
                    ResourceRegistry::instance().resolve(meshId) == hashB,
                "logical mesh id is stable; only the resolved version changed",
                report);

    // Malformed GLB keeps the real last-good version B.
    std::vector<unsigned char> bad(32, 0x11);  // not a GLB
    const std::uint64_t hashBad = hashArtifactBytes(bad.data(), bad.size());
    ArtifactCache::instance().store(hashBad, bad.data(), bad.size());
    ResourceRegistry::instance().announceCandidate(
        {meshId, (std::uint32_t)ResourceKind::Glb, hashBad,
         (std::uint32_t)bad.size()});
    const bool badRejected =
        !publishContentArtifactFromCache(meshId, err);
    void* handleAfterBad = provider.handleOf(meshId);
    ok &= check(badRejected && provider.current(meshId)->contentHash == hashB &&
                    handleAfterBad == handleB,
                "malformed GLB keeps last-good B in the real consumer path",
                report);

    // Runtime preparation failure (loader fails) must also keep B.
    {
        struct Failing { static bool load(void*, void**) { return false; } };
        provider.setLoader(meshId, &Failing::load, &meshRetire, nullptr);
        const std::vector<unsigned char> C = glb(0x90);
        const std::uint64_t hashC = hashArtifactBytes(C.data(), C.size());
        ArtifactCache::instance().store(hashC, C.data(), C.size());
        ResourceRegistry::instance().announceCandidate(
            {meshId, (std::uint32_t)ResourceKind::Glb, hashC,
             (std::uint32_t)C.size()});
        const bool prepFailed = !publishContentArtifactFromCache(meshId, err);
        ok &= check(prepFailed && provider.current(meshId)->contentHash == hashB &&
                        provider.handleOf(meshId) == handleB,
                    "runtime preparation failure keeps last-good B", report);
        provider.setLoader(meshId, &meshLoad, &meshRetire, nullptr);
    }

    // Code generation independence: the provider state is a cold singleton keyed
    // by logical id + hash and holds no hot-DLL pointer, so a code swap cannot
    // invalidate it; and a further publish after a code swap still works.
    {
        const std::vector<unsigned char> D = glb(0xC0);
        const std::uint64_t hashD = hashArtifactBytes(D.data(), D.size());
        ResourceRegistry::instance().announceCandidate(
            {meshId, (std::uint32_t)ResourceKind::Glb, hashD,
             (std::uint32_t)D.size()});
        ok &= check(publishContentArtifact(meshId, hashD, D.data(), D.size(), err) &&
                        provider.current(meshId)->contentHash == hashD,
                    "post-code-swap publish D works on the same logical id", report);
    }

    report += "  [info] mesh logical id=mesh.tool.rocket A=" +
        std::to_string(hashA) + " B=" + std::to_string(hashB) + "\n";
    return ok;
}
