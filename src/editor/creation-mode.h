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

    std::uint64_t selected() const { return selected_; }
    void setSelected(std::uint64_t entity) { selected_ = entity; }

    // Testable, side-effect-free fork hash for a patch list.
    static std::string forkHashFor(const std::string& baseHash,
                                   const std::vector<PatchOp>& patch);

private:
    CreationMode() = default;
    CreationMode(const CreationMode&) = delete;
    CreationMode& operator=(const CreationMode&) = delete;

    bool enabled_ = false;
    std::uint64_t selected_ = 0;
    std::uint64_t nextEntitySerial_ = 1;
    std::vector<PatchOp> patch_;
    std::string baseMapHash_;
};

} // namespace Editor
