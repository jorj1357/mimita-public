// 09 12 2026
/* purpose
* Own creation/inspection mode and the local authoring fork: pick a world
* object, inspect it, and record non-destructive patch operations against the
* base map. Base assets are never rewritten; the fork is a versioned patch.
* Does NOT render, own the world, or mutate authoritative state.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "project/project-types.h"

struct World;

namespace Editor {

struct WorldObjectRef {
    std::uint64_t entity = 0;
    std::string label;
    std::string kind;        // "triangle", "block", "sphere", "mesh_batch"
    std::uint32_t sourceIndex = 0;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    std::string meshHash;
    std::string materialHash;
};

struct PatchOp {
    enum class Kind { Duplicate, Transform, Delete, Material, Hide };
    Kind kind = Kind::Transform;
    std::uint64_t sourceEntity = 0;
    std::uint64_t newEntity = 0;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    std::string material;
};

class CreationMode {
public:
    static CreationMode& instance();

    bool enabled() const { return enabled_; }
    void setEnabled(bool on);

    // Rays into the world using the existing collision-triangle pick.
    bool pick(const World& world, const glm::vec3& origin, const glm::vec3& direction,
              WorldObjectRef& out) const;

    // Per-tick continuous look-at selection while creation mode is enabled.
    // Runs from the fixed simulation tick (not the render frame). Updates the
    // selected entity and the overlay text, and emits change-only diagnostics.
    void updateTick(const World& world, const glm::vec3& origin, const glm::vec3& direction);

    bool hasPick() const { return hasPick_; }
    const WorldObjectRef& lastPick() const { return lastPick_; }
    float lastPickDistance() const { return lastPickDistance_; }
    const std::string& overlayText() const { return overlayText_; }

    // Apply a selection computed by the hot editor module. The kernel keeps the
    // cache so edit commands and non-hot consumers still see the selection.
    void setExternalResult(std::uint64_t entity, std::uint32_t hitKind, float distance);

    // Human/agent text for the currently selected object. Same data a GUI reads.
    std::string describe(const WorldObjectRef& ref) const;
    std::string describeSelection() const;

    // Authoring operations. Each records one patch op and returns the new id.
    std::uint64_t duplicate(const WorldObjectRef& ref, const glm::vec3& offset);
    bool transform(std::uint64_t entity, const glm::vec3& position,
                   const glm::vec3& rotation, const glm::vec3& scale);
    bool remove(std::uint64_t entity);

    const std::vector<PatchOp>& patch() const { return patch_; }
    std::string baseMapHash() const { return baseMapHash_; }
    std::string forkHash() const;
    void clear();

    // Versioned authoring history, reusing the project vocabulary. Each edit
    // records a ChangeEntry; commit() appends a ProjectVersion whose tree hash
    // is the fork hash. The original asset is never rewritten.
    const Project::ChangeSet& changeSet() const { return changeSet_; }
    const std::vector<Project::ProjectVersion>& versionChain() const { return versionChain_; }
    Project::ProjectVersion commit(const std::string& label = "");

    std::uint64_t selected() const { return selected_; }
    void setSelected(std::uint64_t entity) { selected_ = entity; }

    // Testable, side-effect-free fork hash for a patch list.
    static std::string forkHashFor(const std::string& baseHash,
                                   const std::vector<PatchOp>& patch);

private:
    CreationMode() = default;
    CreationMode(const CreationMode&) = delete;
    CreationMode& operator=(const CreationMode&) = delete;

    void recordChange(Project::ChangeOp op, std::uint64_t target,
                      const std::string& before, const std::string& after);

    bool enabled_ = false;
    std::uint64_t selected_ = 0;
    std::uint64_t nextEntitySerial_ = 1;
    // Last continuous look-at result (per-tick).
    WorldObjectRef lastPick_;
    bool hasPick_ = false;
    float lastPickDistance_ = 0.0f;
    std::string overlayText_;
    // Change-only diagnostic keys.
    bool lastLoggedEnabled_ = false;
    bool lastLoggedHit_ = false;
    std::uint64_t lastLoggedEntity_ = 0;
    int lastLoggedTriangle_ = -1;
    std::vector<PatchOp> patch_;
    std::string baseMapHash_;
    Project::ChangeSet changeSet_;
    std::vector<Project::ProjectVersion> versionChain_;
    std::uint32_t nextChangeSerial_ = 1;
};

} // namespace Editor
