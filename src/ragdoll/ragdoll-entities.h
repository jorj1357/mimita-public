// 09 12 2026
/* purpose
* Own the ragdoll entity/component projection: stable limb EntityIds, joint and
* grab components, solver policy, and a network snapshot codec.
* Transitional: the legacy RagdollModeSystem still runs the solver; this is the
* canonical persistent representation and the migration target.
* Does NOT own rendering or the solver math.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/entity-types.h"
#include "ragdoll/ragdoll-components.h"

struct RagdollBody;
struct RagdollGrabState;

namespace Ragdoll {

struct LimbSnapshot {
    std::uint32_t limbIndex = 0;
    float position[3]{};
    float rotation[4]{1.0f, 0.0f, 0.0f, 0.0f};
};

// One serialized grab constraint. `targetLimb` is kInvalidLimb for a world
// anchor; `anchor` is the world grab point (or the moving target point for a
// cross-actor entity grab, resolved by the sender each frame).
struct GrabSnapshot {
    std::uint8_t active = 0;
    std::uint8_t hand = 0;
    std::uint16_t reserved = 0;
    std::uint32_t targetLimb = kInvalidLimb;
    float anchor[3]{};
    float handLocal[3]{};
    float strength = 1.0f;
};

// One serialized ragdoll frame. Fixed capacity keeps the boundary POD.
inline constexpr int kMaxSnapshotLimbs = 24;
struct Snapshot {
    std::uint32_t ownerActorId = 0;
    std::uint32_t limbCount = 0;
    std::uint64_t tick = 0;
    LimbSnapshot limbs[kMaxSnapshotLimbs];
    GrabSnapshot grabs[2];
};

class RagdollEntities {
public:
    static RagdollEntities& instance();

    // Create the limb/root/grab entities for one ragdoll owner (idempotent).
    void bind(std::uint32_t ownerActorId, const RagdollBody& body);
    // Create a minimal limb set for a remote owner reconstructed from a
    // snapshot (no local RagdollBody template available).
    void bindLimbCount(std::uint32_t ownerActorId, std::uint32_t limbCount);
    void unbind(std::uint32_t ownerActorId);
    bool bound(std::uint32_t ownerActorId) const;

    EntityId limbEntity(std::uint32_t ownerActorId, std::uint32_t limbIndex) const;
    std::uint32_t limbCount(std::uint32_t ownerActorId) const;

    // Copy the live body transforms into the components.
    void syncFromBody(std::uint32_t ownerActorId, const RagdollBody& body);
    // Inverse: load the canonical component transforms into a solve workspace.
    void syncToBody(std::uint32_t ownerActorId, RagdollBody& body) const;
    void setGrab(std::uint32_t ownerActorId, bool left, const RagdollGrabState& grab);
    // Entity-to-entity grab target within the same owner's limb set.
    void setGrabTarget(std::uint32_t ownerActorId, bool left,
                       std::int32_t targetLimbIndex, float strength = 1.0f);

    // Alias the shared policy type; one owner in ragdoll-components.h.
    using SolveParams = Ragdoll::SolveParams;
    // Asks the hot gameplay module for solver policy; falls back to stored root.
    SolveParams solveParams(std::uint32_t ownerActorId) const;
    void setSolveParams(std::uint32_t ownerActorId, const SolveParams& params);

    std::size_t entityCount() const;

    // Network codec (POD snapshot).
    bool writeSnapshot(std::uint32_t ownerActorId, Snapshot& out) const;
    // Applies a snapshot; creates the limb set when the owner is unknown.
    bool applySnapshot(const Snapshot& snapshot);
    // Explicit remote reconstruction entry point (same as apply, named for the
    // first-snapshot case).
    bool reconstructFromSnapshot(const Snapshot& snapshot) { return applySnapshot(snapshot); }

private:
    struct BodyBinding {
        std::vector<EntityId> limbs;
        EntityId root = kInvalidEntityId;
        EntityId leftGrab = kInvalidEntityId;
        EntityId rightGrab = kInvalidEntityId;
    };

    std::unordered_map<std::uint32_t, BodyBinding> bindings_;
};

} // namespace Ragdoll
