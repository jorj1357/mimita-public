// 09 12 2026
/* purpose
* Implements creation mode, world-object picking, inspection text, and the
* local authoring fork.
* Does NOT render or mutate authoritative state.
*/
#include "editor/creation-mode.h"

#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/code-hash.h"
#include "live-code/live-journal.h"
#include "physics/physics-types.h"
#include "physics/ray-utils.h"
#include "project/project-types.h"
#include "ragdoll/ragdoll-components.h"
#include "telemetry/telemetry.h"
#include "terminal/terminal-state.h"
#include "world/world.h"

#include <cstdio>
#include <sstream>

namespace Editor {

namespace {

std::uint64_t ensureWorldObjectEntity(std::uint32_t sourceIndex)
{
    const EntityId id =
        Ecs::ensure(EntityRealm::Local, EntityDomain::WorldObject, sourceIndex);
    return Ecs::raw(id);
}

std::string vec3Text(const glm::vec3& v)
{
    char buffer[80];
    std::snprintf(buffer, sizeof(buffer), "(%.3f, %.3f, %.3f)", v.x, v.y, v.z);
    return buffer;
}

const char* patchKindName(PatchOp::Kind kind)
{
    switch (kind) {
    case PatchOp::Kind::Duplicate: return "duplicate";
    case PatchOp::Kind::Transform: return "transform";
    case PatchOp::Kind::Delete: return "delete";
    case PatchOp::Kind::Material: return "material";
    case PatchOp::Kind::Hide: return "hide";
    }
    return "unknown";
}

} // namespace

CreationMode& CreationMode::instance()
{
    static CreationMode mode;
    return mode;
}

void CreationMode::setEnabled(bool on)
{
    if (enabled_ == on)
        return;
    enabled_ = on;
    if (on) {
        const std::string mapPath = ACTIVE_MAP_PATH;
        if (!mapPath.empty()) {
            Project::ContentId id = Project::ContentId::fromFile(mapPath);
            baseMapHash_ = id.str();
        }
    }
    LiveEventJournal::Fields fields;
    fields.result = on ? "on" : "off";
    fields.extra = std::string("\"modecreate\":") + (on ? "1" : "0") +
                   ",\"base_map_hash\":\"" + baseMapHash_ + "\"";
    LiveEventJournal::instance().record("creation_mode_changed", fields);
}

bool CreationMode::pick(const World& world, const glm::vec3& origin,
                        const glm::vec3& direction, WorldObjectRef& out) const
{
    const int triangle = selectWorldTriangle(world, origin, direction);
    if (triangle < 0)
        return false;

    out = WorldObjectRef{};
    out.kind = "triangle";
    out.sourceIndex = (std::uint32_t)triangle;
    if (triangle < (int)world.collisionMesh.triangles.size()) {
        const CollisionTriangle& tri = world.collisionMesh.triangles[triangle];
        out.position = (tri.a + tri.b + tri.c) / 3.0f;
    }
    out.entity = ensureWorldObjectEntity(out.sourceIndex);
    out.label = "triangle_" + std::to_string(out.sourceIndex);
    out.meshHash = baseMapHash_;
    return true;
}

std::string CreationMode::describe(const WorldObjectRef& ref) const
{
    std::ostringstream out;
    out << "[CREATE INSPECT]\n";
    out << "entity=" << ref.entity << "\n";
    out << "name=" << ref.label << "\n";
    out << "kind=" << ref.kind << " sourceIndex=" << ref.sourceIndex << "\n";
    out << "position=" << vec3Text(ref.position) << "\n";
    out << "rotation=" << vec3Text(ref.rotation) << "\n";
    out << "scale=" << vec3Text(ref.scale) << "\n";
    out << "baseMapHash=" << baseMapHash_ << "\n";
    out << "meshHash=" << ref.meshHash << "\n";
    out << "materialHash=" << ref.materialHash << "\n";

    if (ref.entity != 0) {
        EntityRegistry& registry = EntityRegistry::instance();
        const EntityId id = (EntityId)ref.entity;
        std::string components;
        if (registry.has<TransformComponent>(id)) components += "Transform,";
        if (registry.has<BodyComponent>(id)) components += "Body,";
        if (registry.has<ColliderComponent>(id)) components += "Collider,";
        if (registry.has<BehaviorBindingsComponent>(id)) components += "Behaviors,";
        if (registry.has<Ragdoll::LimbComponent>(id)) components += "Limb,";
        if (registry.has<Ragdoll::JointComponent>(id)) components += "Joint,";
        if (registry.has<Ragdoll::GrabComponent>(id)) components += "Grab,";
        if (registry.has<Ragdoll::RagdollRootComponent>(id)) components += "RagdollRoot,";
        out << "components=" << (components.empty() ? "(none)" : components) << "\n";

        if (const auto* limb = registry.tryGet<Ragdoll::LimbComponent>(id)) {
            out << "ragdoll.limb=" << limb->limbIndex
                << " parent=" << limb->parentIndex
                << " mass=" << limb->mass
                << " radius=" << limb->radius
                << " pos=" << vec3Text(limb->position) << "\n";
        }
        if (const auto* joint = registry.tryGet<Ragdoll::JointComponent>(id)) {
            out << "ragdoll.joint parent=" << joint->parentLimb
                << " restLength=" << joint->restLength
                << " maxStretch=" << joint->maxStretch
                << " stiffness=" << joint->stiffness
                << " damping=" << joint->damping << "\n";
        }
        if (const auto* grab = registry.tryGet<Ragdoll::GrabComponent>(id)) {
            out << "ragdoll.grab active=" << (grab->active ? 1 : 0)
                << " hand=" << grab->hand
                << " target=" << grab->targetEntity
                << " strength=" << grab->strength << "\n";
        }
        if (const auto* root = registry.tryGet<Ragdoll::RagdollRootComponent>(id)) {
            out << "ragdoll.root owner=" << root->ownerActorId
                << " limbs=" << root->limbCount
                << " iterations=" << root->solverIterations
                << " gravity=" << root->gravityScale
                << " stiffness=" << root->stiffness
                << " damping=" << root->damping
                << " corpse=" << (root->corpse ? 1 : 0)
                << " lastSolveTick=" << root->lastSolveTick << "\n";
        }

        const HotReloadSystem::Status status = HotReloadSystem::instance().status();
        out << "generation=" << status.activeGeneration << " codeHash="
            << status.activeHash << "\n";

        out << "telemetry=" << Telemetry::Registry::instance().entityJson(ref.entity) << "\n";
    }
    out << "forkHash=" << forkHash();
    return out.str();
}

std::string CreationMode::describeSelection() const
{
    return "selected=" + std::to_string(selected_);
}

std::uint64_t CreationMode::duplicate(const WorldObjectRef& ref, const glm::vec3& offset)
{
    const std::uint32_t serial = (std::uint32_t)nextEntitySerial_++;
    const std::uint64_t newId = ensureWorldObjectEntity(0x40000000u | serial);
    PatchOp op;
    op.kind = PatchOp::Kind::Duplicate;
    op.sourceEntity = ref.entity;
    op.newEntity = newId;
    op.position = ref.position + offset;
    op.rotation = ref.rotation;
    op.scale = ref.scale;
    patch_.push_back(op);
    selected_ = newId;

    LiveEventJournal::Fields fields;
    fields.actorId = "creation";
    fields.result = "duplicate";
    fields.extra = std::string("\"source_entity\":") + std::to_string(ref.entity) +
                   ",\"new_entity\":" + std::to_string(newId) +
                   ",\"base_map_hash\":\"" + baseMapHash_ + "\"" +
                   ",\"fork_hash\":\"" + forkHash() + "\"";
    LiveEventJournal::instance().record("entity_duplicated", fields);
    return newId;
}

bool CreationMode::transform(std::uint64_t entity, const glm::vec3& position,
                             const glm::vec3& rotation, const glm::vec3& scale)
{
    if (entity == 0)
        return false;
    PatchOp op;
    op.kind = PatchOp::Kind::Transform;
    op.sourceEntity = entity;
    op.position = position;
    op.rotation = rotation;
    op.scale = scale;
    patch_.push_back(op);

    LiveEventJournal::Fields fields;
    fields.actorId = "creation";
    fields.result = "transform";
    fields.extra = std::string("\"entity\":") + std::to_string(entity) +
                   ",\"position\":\"" + vec3Text(position) + "\"" +
                   ",\"fork_hash\":\"" + forkHash() + "\"";
    LiveEventJournal::instance().record("entity_transform_changed", fields);
    return true;
}

bool CreationMode::remove(std::uint64_t entity)
{
    if (entity == 0)
        return false;
    PatchOp op;
    op.kind = PatchOp::Kind::Delete;
    op.sourceEntity = entity;
    patch_.push_back(op);
    if (selected_ == entity)
        selected_ = 0;

    LiveEventJournal::Fields fields;
    fields.actorId = "creation";
    fields.result = "delete";
    fields.extra = std::string("\"entity\":") + std::to_string(entity) +
                   ",\"fork_hash\":\"" + forkHash() + "\"";
    LiveEventJournal::instance().record("entity_deleted_from_fork", fields);
    return true;
}

std::string CreationMode::forkHash() const
{
    return forkHashFor(baseMapHash_, patch_);
}

std::string CreationMode::forkHashFor(const std::string& baseHash,
                                      const std::vector<PatchOp>& patch)
{
    std::string canonical = "base:" + baseHash + "\n";
    for (const auto& op : patch) {
        canonical += patchKindName(op.kind);
        canonical += ':';
        canonical += std::to_string(op.sourceEntity);
        canonical += ':';
        canonical += std::to_string(op.newEntity);
        canonical += ':';
        canonical += vec3Text(op.position);
        canonical += ':';
        canonical += vec3Text(op.rotation);
        canonical += ':';
        canonical += vec3Text(op.scale);
        canonical += ':';
        canonical += op.material;
        canonical += '\n';
    }
    return LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
}

void CreationMode::clear()
{
    patch_.clear();
    selected_ = 0;
    nextEntitySerial_ = 1;
}

} // namespace Editor
