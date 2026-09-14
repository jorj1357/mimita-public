// 09 12 2026
/* purpose
* Remote ragdoll presentation: buffer authoritative limb frames received over
* the network, interpolate them to a delayed presentation time, and write the
* rendered skeleton. Derived-only: never writes authoritative components and
* never feeds the solver.
* Does NOT own the solver, the network transport, or authoritative ragdoll state.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ragdoll/ragdoll-entities.h"
#include "ragdoll/ragdoll-mode.h"

struct Player;

namespace Ragdoll {

struct PresentationFrame {
    std::uint32_t sourceTick = 0;
    std::uint64_t receivedMs = 0;
    std::uint32_t limbCount = 0;
    glm::vec3 position[kMaxSnapshotLimbs];
    glm::quat orientation[kMaxSnapshotLimbs];
};

class RagdollPresentation {
public:
    static RagdollPresentation& instance();

    // Record one received authoritative frame for a remote owner. Ordered by
    // sourceTick; same-tick frames replace, older-than-window frames drop.
    void pushFrame(std::uint32_t ownerActorId, const Snapshot& snapshot);

    // Interpolate the buffered frames to the delayed presentation time and
    // write the player's skeleton. Returns false when the owner is not an
    // active remote ragdoll (no frames, stale, or template not ready).
    bool present(std::uint32_t ownerActorId, Player& player, double delaySeconds);

    bool active(std::uint32_t ownerActorId) const;
    void clear(std::uint32_t ownerActorId);
    void clearAll();

    // Diagnostics (no GUI).
    std::size_t bufferDepth(std::uint32_t ownerActorId) const;
    double lastAlpha(std::uint32_t ownerActorId) const;
    std::uint64_t extrapolationCount(std::uint32_t ownerActorId) const;
    std::uint64_t staleCount(std::uint32_t ownerActorId) const;

private:
    RagdollPresentation() = default;

    struct Owner {
        RagdollBody body;               // template + presentation transforms
        bool templateReady = false;
        std::uint32_t templateLimbCount = 0;
        std::deque<PresentationFrame> buffer;
        std::uint64_t firstReceivedMs = 0;
        std::uint64_t lastReceivedMs = 0;
        std::uint32_t firstTick = 0;
        bool hasRenderTick = false;
        double renderTick = 0.0;
        double lastAlpha = 0.0;
        std::uint64_t extrapolations = 0;
        std::uint64_t stale = 0;
    };

    std::unordered_map<std::uint32_t, Owner> owners_;
};

} // namespace Ragdoll
