// 09 15 2026
/* purpose
* Implements the distributed hot-generation per-peer state machine, quorum, and
* host switch scheduling. Mechanism only: no building, loading, transport, or
* artifact transfer. See generation-distribution.h for the contract.
*/
#include "hot-reload/generation-distribution.h"

namespace MimitaRuntime {

GenerationDistribution& GenerationDistribution::instance()
{
    static GenerationDistribution instance;
    return instance;
}

void GenerationDistribution::reset()
{
    peers_.clear();
    scheduledGeneration_ = 0;
    scheduledTick_ = 0;
}

void GenerationDistribution::announce(std::uint64_t peerId,
                                      const GenerationIdentityV1& identity)
{
    GenerationPeerStateV1& s = peers_[peerId];
    s.peerId = peerId;
    // A new announce replaces any prior candidate for this peer.
    s.identity = identity;
    s.phase = GenerationPhase::Announced;
}

void GenerationDistribution::setPhase(std::uint64_t peerId, GenerationPhase phase)
{
    auto it = peers_.find(peerId);
    if (it == peers_.end())
        return;
    it->second.phase = phase;
}

void GenerationDistribution::setActive(std::uint64_t peerId,
                                       std::uint64_t logicalGenerationId)
{
    auto it = peers_.find(peerId);
    if (it == peers_.end())
        return;
    it->second.identity.logicalGenerationId = logicalGenerationId;
    it->second.phase = GenerationPhase::Active;
}

GenerationPhase GenerationDistribution::phaseOf(std::uint64_t peerId) const
{
    auto it = peers_.find(peerId);
    return it == peers_.end() ? GenerationPhase::Unknown : it->second.phase;
}

std::uint64_t GenerationDistribution::candidateGenerationOf(std::uint64_t peerId) const
{
    auto it = peers_.find(peerId);
    return it == peers_.end() ? 0 : it->second.identity.logicalGenerationId;
}

bool GenerationDistribution::hasPeer(std::uint64_t peerId) const
{
    return peers_.find(peerId) != peers_.end();
}

void GenerationDistribution::removePeer(std::uint64_t peerId)
{
    peers_.erase(peerId);
}

bool GenerationDistribution::quorumReady(
    const std::vector<std::uint64_t>& requiredPeers,
    std::uint64_t logicalGenerationId) const
{
    if (requiredPeers.empty() || logicalGenerationId == 0)
        return false;
    for (std::uint64_t peer : requiredPeers) {
        auto it = peers_.find(peer);
        if (it == peers_.end())
            return false;
        const GenerationPeerStateV1& s = it->second;
        // A peer that is already Active on the generation counts as ready.
        if (s.phase == GenerationPhase::Active &&
            s.identity.logicalGenerationId == logicalGenerationId)
            continue;
        if (s.phase != GenerationPhase::Ready ||
            s.identity.logicalGenerationId != logicalGenerationId)
            return false;
    }
    return true;
}

bool GenerationDistribution::scheduleSwitch(std::uint64_t logicalGenerationId,
                                            std::uint32_t tick)
{
    if (logicalGenerationId == 0 || tick == 0)
        return false;
    // A scheduled generation may be superseded by a newer one before activation.
    if (scheduledGeneration_ != 0 && logicalGenerationId < scheduledGeneration_)
        return false;
    scheduledGeneration_ = logicalGenerationId;
    scheduledTick_ = tick;
    return true;
}

void GenerationDistribution::cancelSwitch()
{
    scheduledGeneration_ = 0;
    scheduledTick_ = 0;
}

bool GenerationDistribution::supersedes(std::uint64_t logicalGenerationId) const
{
    return scheduledGeneration_ != 0 && logicalGenerationId > scheduledGeneration_;
}

} // namespace MimitaRuntime
