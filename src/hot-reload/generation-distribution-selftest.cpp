// 09 15 2026
/* purpose
* Implements the headless distributed-generation state-machine self-test:
* per-peer READY phases, quorum, active-counts-as-ready, multiple-candidate
* supersede, peer removal, and switch cancellation. Mechanism only (no network).
* Does NOT own rendering/presentation or the transport.
*/
#include "hot-reload/generation-distribution-selftest.h"

#include <string>

#include "hot-reload/generation-distribution.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

GenerationIdentityV1 identity(std::uint64_t gen, std::uint64_t logicalHash,
                              std::uint64_t artifactHash, std::uint32_t abi)
{
    GenerationIdentityV1 id{};
    id.logicalGenerationId = gen;
    id.logicalBehaviorHash = logicalHash;
    id.platformArtifactHash = artifactHash;
    id.abiVersion = abi;
    return id;
}

} // namespace

bool runGenerationDistributionSelfTest(std::string& report)
{
    bool ok = true;
    GenerationDistribution& dist = GenerationDistribution::instance();
    dist.reset();

    const std::uint64_t peerA = 1;
    const std::uint64_t peerB = 2;
    const std::uint64_t genG = 0x1111;
    const std::vector<std::uint64_t> required{peerA, peerB};

    // Announce keeps the logical identity separate from the platform artifact.
    dist.announce(peerA, identity(genG, 0xABCD, 0x1111AA, 2));
    ok &= check(dist.phaseOf(peerA) == GenerationPhase::Announced &&
                    dist.candidateGenerationOf(peerA) == genG,
                "announce records logical generation + phase", report);

    dist.announce(peerB, identity(genG, 0xABCD, 0x2222BB, 2));
    ok &= check(!dist.quorumReady(required, genG),
                "quorum not ready while peers are announced only", report);

    dist.setPhase(peerA, GenerationPhase::Acquiring);
    dist.setPhase(peerA, GenerationPhase::Validating);
    dist.setPhase(peerA, GenerationPhase::Ready);
    dist.setPhase(peerB, GenerationPhase::Ready);
    ok &= check(dist.quorumReady(required, genG),
                "quorum ready when all required peers are Ready", report);

    // A peer on a different logical generation is not ready for genG.
    dist.announce(peerB, identity(0x2222, 0xBEEF, 0x2222BB, 2));
    ok &= check(!dist.quorumReady(required, genG),
                "peer on a different candidate breaks quorum for genG", report);

    // Active counts as ready.
    dist.setActive(peerB, genG);
    ok &= check(dist.quorumReady(required, genG),
                "peer Active on genG counts toward quorum", report);

    // Host schedules the shared switch tick only after quorum.
    ok &= check(dist.scheduleSwitch(genG, 1234u) &&
                    dist.switchScheduled() &&
                    dist.scheduledGeneration() == genG &&
                    dist.scheduledTick() == 1234u,
                "host schedules shared switch tick for the generation", report);

    // Multiple rapid edits: a newer candidate supersedes the scheduled one; an
    // older candidate cannot.
    ok &= check(dist.supersedes(0x3333) && !dist.supersedes(0x0100) &&
                    !dist.scheduleSwitch(0x0100, 2000u),
                "newer generation supersedes; older candidate rejected", report);

    // Peer disconnect updates quorum deterministically.
    dist.removePeer(peerA);
    ok &= check(!dist.hasPeer(peerA) &&
                    !dist.quorumReady(required, genG),
                "disconnected peer is removed and breaks quorum", report);

    dist.cancelSwitch();
    ok &= check(!dist.switchScheduled(), "switch can be cancelled", report);

    // Deterministic repeat.
    dist.reset();
    dist.announce(peerA, identity(genG, 0xABCD, 0x1111AA, 2));
    dist.setPhase(peerA, GenerationPhase::Ready);
    const bool q1 = dist.quorumReady({peerA}, genG);
    dist.reset();
    dist.announce(peerA, identity(genG, 0xABCD, 0x1111AA, 2));
    dist.setPhase(peerA, GenerationPhase::Ready);
    const bool q2 = dist.quorumReady({peerA}, genG);
    ok &= check(q1 == q2, "distribution state machine is deterministic", report);

    dist.reset();
    return ok;
}
