// 09 15 2026
/* purpose
* Kernel-side distributed hot-generation bookkeeping: the per-peer READY state
* machine, quorum, and host switch scheduling on top of the EXISTING generation
* pipeline (HotReloadSystem) and the EXISTING CodeGenerationPacket. It does not
* build, load, or activate anything itself; it tracks who is ready for which
* logical generation and whether the host may schedule a shared switch tick.
* Logical behavior identity (platform-independent) is kept separate from the
* platform artifact hash. Does NOT own source watching, compilation, loading, the
* network transport, or artifact transfer.
*/
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace MimitaRuntime {

enum class GenerationPhase : std::uint32_t {
    Unknown = 0,
    Announced = 1,
    Acquiring = 2,
    Validating = 3,
    Ready = 4,
    Active = 5,
    Failed = 6,
};

// Platform-independent logical generation identity vs the exact platform
// artifact. A logical generation may map to several platform artifacts
// (Windows x64, Linux x64, macOS arm64, ...); peers validate the logical
// identity AND their own platform artifact hash.
struct GenerationIdentityV1 {
    std::uint64_t logicalGenerationId = 0;   // canonical logical generation id
    std::uint64_t logicalBehaviorHash = 0;   // deterministic behavior hash
    std::uint64_t platformArtifactHash = 0;  // exact artifact bytes for this peer
    std::uint32_t abiVersion = 0;
};

struct GenerationPeerStateV1 {
    std::uint64_t peerId = 0;
    GenerationIdentityV1 identity{};
    GenerationPhase phase = GenerationPhase::Unknown;
};

class GenerationDistribution {
public:
    static GenerationDistribution& instance();

    void reset();

    // Host: announce a generation to a peer (clears any prior candidate).
    void announce(std::uint64_t peerId, const GenerationIdentityV1& identity);
    // Peer: report phase progress for its current candidate.
    void setPhase(std::uint64_t peerId, GenerationPhase phase);
    // Peer: record that it is now running this logical generation.
    void setActive(std::uint64_t peerId, std::uint64_t logicalGenerationId);

    GenerationPhase phaseOf(std::uint64_t peerId) const;
    std::uint64_t candidateGenerationOf(std::uint64_t peerId) const;
    bool hasPeer(std::uint64_t peerId) const;
    void removePeer(std::uint64_t peerId);

    // Host: true when every required peer is Ready (or already Active) for
    // `logicalGenerationId`. Empty required set returns false (nothing to gate).
    bool quorumReady(const std::vector<std::uint64_t>& requiredPeers,
                     std::uint64_t logicalGenerationId) const;

    // Host: schedule the shared switch tick (only meaningful after quorum).
    bool scheduleSwitch(std::uint64_t logicalGenerationId, std::uint32_t tick);
    void cancelSwitch();
    bool switchScheduled() const { return scheduledGeneration_ != 0; }
    std::uint64_t scheduledGeneration() const { return scheduledGeneration_; }
    std::uint32_t scheduledTick() const { return scheduledTick_; }

    // Multiple rapid edits: a newer candidate supersedes a not-yet-active one.
    bool supersedes(std::uint64_t logicalGenerationId) const;

    std::size_t peerCount() const { return peers_.size(); }

private:
    GenerationDistribution() = default;

    std::unordered_map<std::uint64_t, GenerationPeerStateV1> peers_;
    std::uint64_t scheduledGeneration_ = 0;
    std::uint32_t scheduledTick_ = 0;
};

} // namespace MimitaRuntime
