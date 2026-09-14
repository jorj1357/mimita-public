// 09 12 2026
/* purpose
* Implements creation mode, world-object picking, inspection text, and the
* local authoring fork.
* Does NOT render or mutate authoritative state.
*/
#include "editor/creation-mode.h"

#include "debug/debug-log.h"
#include "editor/entity-inspector.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/code-hash.h"
#include "live-code/live-journal.h"
#include "physics/physics-types.h"
#include "physics/ray-utils.h"
#include "project/project-types.h"
#include "physics/constraints/constraint-components.h"
#include "ragdoll/ragdoll-components.h"
#include "telemetry/telemetry.h"
#include "terminal/terminal-state.h"
#include "world/world.h"

#include <cstdio>
#include <chrono>
#include <cmath>
#include <sstream>

namespace Editor {

namespace {

std::uint64_t ensureWorldObjectEntity(std::uint32_t sourceIndex,
                                      const std::string& meshHash,
                                      const std::string& materialHash)
{
    const EntityId id =
        Ecs::ensure(EntityRealm::Local, EntityDomain::WorldObject, sourceIndex);
    WorldObjectComponent wo;
    wo.sourceIndex = sourceIndex;
    wo.nodeIndex = sourceIndex;
    wo.meshHash = meshHash;
    wo.materialHash = materialHash;
    EntityRegistry::instance().add<WorldObjectComponent>(id, wo);
    return Ecs::raw(id);
}

// Ray vs sphere. Returns the nearest positive hit distance in `t`.
bool raySphere(const glm::vec3& origin, const glm::vec3& dir,
               const glm::vec3& center, float radius, float& t)
{
    const glm::vec3 oc = origin - center;
    const float b = glm::dot(oc, dir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f)
        return false;
    const float s = std::sqrt(disc);
    float hit = -b - s;
    if (hit < 0.0f)
        hit = -b + s;
    if (hit < 0.0f)
        return false;
    t = hit;
    return true;
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
    case PatchOp::Kind::Label: return "label";
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
    if (on && gpActiveMapPath) {
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
    out = WorldObjectRef{};
    const glm::vec3 dir = glm::normalize(direction);

    float bestT = 1e30f;
    int bestTriangle = -1;
    // 1) World geometry (collision triangles).
    const int triangle = selectWorldTriangle(world, origin, direction);
    if (triangle >= 0) {
        bestTriangle = triangle;
        if (triangle < (int)world.collisionMesh.triangles.size()) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triangle];
            bestT = glm::length((tri.a + tri.b + tri.c) / 3.0f - origin);
        }
    }

    // 2) Entities with a body bound (players, NPCs, projectiles, limbs).
    EntityId bestEntity = kInvalidEntityId;
    glm::vec3 bestEntityPos(0.0f);
    EntityDomain bestDomain = EntityDomain::None;
    EntityRegistry& registry = EntityRegistry::instance();
    for (EntityId id : registry.all()) {
        const EntityIdentity* ident = registry.identity(id);
        if (!ident)
            continue;
        if (ident->domain == EntityDomain::None ||
            ident->domain == EntityDomain::WorldObject ||
            ident->domain == EntityDomain::Constraint)
            continue;
        glm::vec3 center(0.0f);
        float radius = 0.0f;
        if (const auto* limb = registry.tryGet<Ragdoll::LimbComponent>(id)) {
            center = limb->position;
            radius = limb->radius + limb->halfHeight;
        } else if (const auto* t = registry.tryGet<TransformComponent>(id)) {
            center = t->position;
            if (const auto* body = registry.tryGet<BodyComponent>(id))
                radius = std::max(body->radius, body->height * 0.5f);
            else if (const auto* col = registry.tryGet<ColliderComponent>(id))
                radius = std::max(col->radius, col->height * 0.5f);
            else
                radius = 0.5f;
        } else {
            continue;
        }
        if (radius <= 0.0f)
            radius = 0.5f;
        float t = 0.0f;
        if (raySphere(origin, dir, center, radius, t) && t < bestT) {
            bestT = t;
            bestEntity = id;
            bestEntityPos = center;
            bestDomain = ident->domain;
            bestTriangle = -1;
        }
    }

    if (bestEntity != kInvalidEntityId) {
        out.entity = Ecs::raw(bestEntity);
        out.kind = entityDomainName(bestDomain);
        out.position = bestEntityPos;
        out.label = out.kind + ":" + std::to_string(bestEntity);
        return true;
    }

    if (bestTriangle >= 0) {
        out.kind = "triangle";
        out.sourceIndex = (std::uint32_t)bestTriangle;
        if (bestTriangle < (int)world.collisionMesh.triangles.size()) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[bestTriangle];
            out.position = (tri.a + tri.b + tri.c) / 3.0f;
        }
        out.entity = ensureWorldObjectEntity(out.sourceIndex, baseMapHash_, "");
        out.label = "triangle_" + std::to_string(out.sourceIndex);
        out.meshHash = baseMapHash_;
        return true;
    }
    return false;
}

void CreationMode::setExternalResult(std::uint64_t entity, std::uint32_t hitKind,
                                     float distance)
{
    hasPick_ = entity != 0 || hitKind != 0;
    selected_ = entity;
    lastPick_ = WorldObjectRef{};
    lastPick_.entity = entity;
    lastPick_.kind = hitKind == 2 ? "entity" : (hitKind == 1 ? "triangle" : "");
    lastPickDistance_ = distance;
}

void CreationMode::updateTick(const World& world, const glm::vec3& origin,
                              const glm::vec3& direction)
{
    if (!enabled_) {
        if (lastLoggedEnabled_) {
            lastLoggedEnabled_ = false;
            hasPick_ = false;
            overlayText_.clear();
            Debug::log(Debug::Category::General,
                "[CREATE] enabled=0 rayHit=none outputSink=overlay\n");
        }
        return;
    }
    lastLoggedEnabled_ = true;

    WorldObjectRef ref;
    const bool hit = pick(world, origin, direction, ref);
    const bool isEntity = hit && ref.kind != "triangle";
    if (hit) {
        lastPick_ = ref;
        hasPick_ = true;
        lastPickDistance_ = glm::length(ref.position - origin);

        // Compact always-on overlay built from the same inspection API.
        const EntityInspection inspection = inspectEntity((EntityId)ref.entity);
        std::ostringstream o;
        o << "entity=" << ref.entity << " kind=" << ref.kind;
        o << " dist=" << lastPickDistance_ << "\n";
        o << "components:";
        if (inspection.components.empty())
            o << " (none)";
        else
            for (const std::string& c : inspection.components)
                o << " " << c;
        if (inspection.hasConstraint) {
            const Physics::Constraint& c = inspection.constraint.constraint;
            o << "\nconstraint serial=" << inspection.constraint.constraintSerial
              << " bodyA=" << c.bodyA << " bodyB=" << c.bodyB
              << " strength=" << c.strength;
        } else if (inspection.linkedConstraintSerial != 0) {
            o << "\nlinkedConstraint=" << inspection.linkedConstraintSerial;
        }
        overlayText_ = o.str();
    } else {
        hasPick_ = false;
        lastPick_ = WorldObjectRef{};
        lastPickDistance_ = 0.0f;
        overlayText_ = "no target under crosshair";
    }

    // Change-only diagnostics (never per tick).
    const bool changed = (hit != lastLoggedHit_) ||
        (hit && ref.entity != lastLoggedEntity_) ||
        (hit && (int)ref.sourceIndex != lastLoggedTriangle_);
    if (changed) {
        Debug::log(Debug::Category::General,
            "[CREATE] enabled=1 rayHit=%s distance=%.2f entity=%llu kind=%s "
            "triangle=%d inspectOk=%d outputSink=overlay\n",
            hit ? (isEntity ? "entity" : "world") : "none",
            (double)lastPickDistance_, (unsigned long long)(hit ? ref.entity : 0),
            hit ? ref.kind.c_str() : "-", hit ? (int)ref.sourceIndex : -1,
            hit ? 1 : 0);
    }
    lastLoggedHit_ = hit;
    lastLoggedEntity_ = hit ? ref.entity : 0;
    lastLoggedTriangle_ = hit ? (int)ref.sourceIndex : -1;
}

std::string CreationMode::describe(const WorldObjectRef& ref) const
{
    std::ostringstream out;
    out << "[CREATE INSPECT]\n";
    out << "kind=" << ref.kind << " sourceIndex=" << ref.sourceIndex << "\n";
    out << "position=" << vec3Text(ref.position) << "\n";
    out << "rotation=" << vec3Text(ref.rotation) << "\n";
    out << "scale=" << vec3Text(ref.scale) << "\n";
    out << "baseMapHash=" << baseMapHash_ << "\n";
    out << "meshHash=" << ref.meshHash << "\n";
    out << "materialHash=" << ref.materialHash << "\n";
    if (ref.entity != 0)
        out << inspectEntity((EntityId)ref.entity).toText();
    out << "forkHash=" << forkHash() << "\n";
    return out.str();
}

std::string CreationMode::describeSelection() const
{
    return "selected=" + std::to_string(selected_);
}

std::uint64_t CreationMode::duplicate(const WorldObjectRef& ref, const glm::vec3& offset)
{
    const std::uint32_t serial = (std::uint32_t)nextEntitySerial_++;
    const std::uint64_t newId = ensureWorldObjectEntity(0x40000000u | serial, baseMapHash_, "");
    PatchOp op;
    op.kind = PatchOp::Kind::Duplicate;
    op.sourceEntity = ref.entity;
    op.newEntity = newId;
    op.position = ref.position + offset;
    op.rotation = ref.rotation;
    op.scale = ref.scale;
    op.size = ref.size;
    patch_.push_back(op);
    recordChange(Project::ChangeOp::Add, newId,
                 std::to_string(ref.entity), forkHash());
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
    recordChange(Project::ChangeOp::Modify, entity, "", forkHash());

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
    recordChange(Project::ChangeOp::Delete, entity, "", forkHash());
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

bool CreationMode::setLabel(std::uint64_t entity, const std::string& label)
{
    if (entity == 0)
        return false;
    PatchOp op;
    op.kind = PatchOp::Kind::Label;
    op.sourceEntity = entity;
    op.label = label;
    patch_.push_back(op);
    redoStack_.clear();
    recordChange(Project::ChangeOp::Modify, entity, "", forkHash());
    return true;
}

bool CreationMode::setMaterial(std::uint64_t entity, const std::string& material)
{
    if (entity == 0)
        return false;
    PatchOp op;
    op.kind = PatchOp::Kind::Material;
    op.sourceEntity = entity;
    op.material = material;
    patch_.push_back(op);
    redoStack_.clear();
    recordChange(Project::ChangeOp::Modify, entity, "", forkHash());
    return true;
}

bool CreationMode::undo()
{
    if (patch_.empty())
        return false;
    redoStack_.push_back(patch_.back());
    patch_.pop_back();
    return true;
}

bool CreationMode::redo()
{
    if (redoStack_.empty())
        return false;
    patch_.push_back(redoStack_.back());
    redoStack_.pop_back();
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
        canonical += ':';
        canonical += op.label;
        canonical += '\n';
    }
    return LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
}

void CreationMode::recordChange(Project::ChangeOp op, std::uint64_t target,
                                const std::string& before, const std::string& after)
{
    Project::ChangeEntry entry;
    entry.op = op;
    entry.path = "entity/" + std::to_string(target);
    if (!before.empty())
        entry.before = Project::ContentId{"sha256", before};
    if (!after.empty())
        entry.after = Project::ContentId{"sha256", after};
    changeSet_.changes.push_back(entry);
    changeSet_.parentTreeHash = versionChain_.empty() ? baseMapHash_
                                                      : versionChain_.back().treeHash;
    changeSet_.newTreeHash = forkHash();
    if (changeSet_.changeId.empty())
        changeSet_.changeId = "change-" + std::to_string(nextChangeSerial_++);
}

Project::ProjectVersion CreationMode::commit(const std::string& label)
{
    Project::ProjectVersion version;
    version.parentHash = versionChain_.empty() ? baseMapHash_
                                               : versionChain_.back().versionHash;
    version.treeHash = forkHash();
    version.changeSetHash = LiveCodeHash::sha256Bytes(
        changeSet_.changeId.data(), changeSet_.changeId.size());
    version.changeId = changeSet_.changeId;
    version.label = label;
    version.timestampMs = (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string canonical = version.parentHash + "|" + version.treeHash + "|" +
                                  version.changeId + "|" + std::to_string(version.timestampMs);
    version.versionHash = LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
    versionChain_.push_back(version);

    // Start the next change set from the committed tree.
    changeSet_ = Project::ChangeSet{};
    changeSet_.parentTreeHash = version.treeHash;
    nextChangeSerial_ = 1;
    return version;
}

void CreationMode::clear()
{
    patch_.clear();
    redoStack_.clear();
    selected_ = 0;
    nextEntitySerial_ = 1;
    changeSet_ = Project::ChangeSet{};
    versionChain_.clear();
    nextChangeSerial_ = 1;
}

} // namespace Editor
