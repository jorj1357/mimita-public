// 09 15 2026
/* purpose
* Headless self-test for the late-join generation-bootstrap state machine: cache
* miss/hit flows, stale-packet safety, generation-change-during-bootstrap,
* verification failure, and world-participation gating.
*/
#include "hot-reload/generation-bootstrap-selftest.h"

#include <string>

#include "hot-reload/generation-bootstrap.h"
#include "hot-reload/game-api.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

GenerationManifestV1 makeManifest(std::uint64_t gen, std::uint64_t hash)
{
    GenerationManifestV1 m{};
    m.logicalGenerationId = gen;
    m.platformArtifactHash = hash;
    m.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
    return m;
}

GenerationLocalFactsV1 makeFacts(std::uint64_t hash)
{
    GenerationLocalFactsV1 f{};
    f.artifactHash = hash;
    f.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
    return f;
}

} // namespace

bool runGenerationBootstrapSelfTest(std::string& report)
{
    bool ok = true;
    const std::uint64_t G = 11;
    const std::uint64_t H = 12;
    const std::uint64_t hash = 0xABCD;
    const GenerationManifestV1 manifest = makeManifest(G, hash);
    const GenerationLocalFactsV1 facts = makeFacts(hash);

    // ── No target ─────────────────────────────────────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(0);
        ok &= check(b.state == BootstrapState::Failed &&
                        b.failure == (std::uint32_t)BootstrapFailure::NoTarget,
                    "bootstrap without an active target fails", report);
    }

    // ── Cache miss: acquire -> verify -> ready ────────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        ok &= check(b.state == BootstrapState::AwaitingMetadata &&
                        b.active() &&
                        !b.worldParticipationAllowed(G, G),
                    "metadata pending: bootstrap active, world blocked", report);
        b.onMetadata(G, hash, /*artifactCached=*/false);
        ok &= check(b.state == BootstrapState::Acquiring,
                    "cache miss -> acquiring", report);
        b.onArtifactAcquired(G);
        ok &= check(b.state == BootstrapState::Verifying,
                    "artifact received -> verifying", report);
        b.complete(G, /*codeLoaded=*/true, manifest, facts);
        ok &= check(b.state == BootstrapState::Ready &&
                        b.worldParticipationAllowed(G, G),
                    "verified + loaded -> ready, world allowed", report);
    }

    // ── Cache hit: zero acquisition, still verified ───────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(G, hash, /*artifactCached=*/true);
        ok &= check(b.state == BootstrapState::Verifying,
                    "cache hit -> verifying without acquiring", report);
        b.complete(G, true, manifest, facts);
        ok &= check(b.state == BootstrapState::Ready,
                    "cache hit still requires verification -> ready", report);
    }

    // ── Stale packet safety ───────────────────────────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(H, hash, false);  // packet for another generation
        ok &= check(b.state == BootstrapState::AwaitingMetadata,
                    "metadata for another generation is ignored", report);
        b.onMetadata(G, hash, false);
        b.onArtifactAcquired(H);  // stale acquisition cannot advance
        ok &= check(b.state == BootstrapState::Acquiring,
                    "artifact for another generation is ignored", report);
        b.onArtifactAcquired(G);
        b.complete(H, true, makeManifest(H, hash), makeFacts(hash));
        ok &= check(b.state == BootstrapState::Verifying,
                    "completion for another generation cannot activate", report);
    }

    // ── Generation changes during bootstrap ───────────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(G, hash, false);
        b.onServerActiveChanged(H);
        ok &= check(b.targetGeneration == H &&
                        b.state == BootstrapState::AwaitingMetadata,
                    "server moves to H -> bootstrap re-targets H", report);
        b.complete(G, true, manifest, facts);  // late G completion
        ok &= check(b.state != BootstrapState::Ready,
                    "late completion for G cannot complete H bootstrap", report);
        b.onMetadata(H, hash, true);
        b.complete(H, true, makeManifest(H, hash), makeFacts(hash));
        ok &= check(b.state == BootstrapState::Ready &&
                        b.worldParticipationAllowed(H, H),
                    "H bootstrap completes for the new active generation", report);
    }

    // ── Verification failure stays out of gameplay ────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(G, hash, true);
        GenerationLocalFactsV1 bad = facts;
        bad.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION + 1u;
        b.complete(G, true, manifest, bad);
        ok &= check(b.state == BootstrapState::Failed &&
                        b.failure == (std::uint32_t)VerifyFailure::AbiMismatch &&
                        !b.worldParticipationAllowed(G, G),
                    "ABI mismatch -> failed, no gameplay", report);
    }

    // ── Code not loaded ───────────────────────────────────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(G, hash, true);
        b.complete(G, /*codeLoaded=*/false, manifest, facts);
        ok &= check(b.state == BootstrapState::Failed &&
                        b.failure == (std::uint32_t)BootstrapFailure::NotLoaded,
                    "verified but not loaded -> failed", report);
    }

    // ── Participation also requires local activation ──────────────────
    {
        GenerationBootstrapV1 b;
        b.begin(G);
        b.onMetadata(G, hash, true);
        b.complete(G, true, manifest, facts);
        ok &= check(!b.worldParticipationAllowed(G, /*localActive=*/0) &&
                        b.worldParticipationAllowed(G, G),
                    "ready but not locally active -> world still blocked", report);
    }

    return ok;
}
