// 2026-09-27
/* purpose
* Define one generic moving physical entity: stable identity, previous/current
* transform, velocity, entity-local collision triangles, motion state, and
* material id. No MovingPlatform class and no per-feature subclass; a crate, a
* door, or a vehicle is just an entity with a motion state and a collider.
* Provide a small owner (PhysicalEntitySystem) that updates kinematic transforms
* and exposes entities to the collision query.
* Does NOT render, play effects, send packets, or own gameplay rules.
* Does NOT delete or replace any existing collision owner.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "physics/physics-types.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/physics-collision.h"

// Static never moves; Kinematic is moved by its owner (transform is set, velocity
// derived) and is infinite-mass; Dynamic is free to be moved by physics later.
enum class PhysicalEntityMotion : uint8_t {
    Static,
    Kinematic,
    Dynamic
};

struct PhysicalEntity {
    uint32_t id = 0;
    PhysicalEntityMotion motion = PhysicalEntityMotion::Static;
    glm::mat4 previousTransform{1.0f};
    glm::mat4 transform{1.0f};
    glm::vec3 velocity{0.0f};
    std::vector<CollisionTriangle> localTriangles;   // entity-local
    uint32_t materialId = 0;
    bool collidesWithActors = true;
};

class Camera;

class PhysicalEntitySystem {
public:
    static PhysicalEntitySystem& instance();

    void clear();

    // Adds an entity and returns its stable id. For Kinematic/Dynamic entities
    // previousTransform starts equal to transform.
    uint32_t add(const std::vector<CollisionTriangle>& localTriangles,
                 const glm::mat4& transform,
                 PhysicalEntityMotion motion,
                 uint32_t materialId = 0);

    // Snapshot the last committed transform before the entity is moved this
    // tick, so relative motion (and swept actor tests) stay correct.
    void beginTick();

    // Moves a kinematic entity to `transform` and derives its velocity from the
    // transform delta over `dt`. A non-positive dt leaves velocity unchanged.
    PhysicalEntity* moveKinematic(uint32_t id, const glm::mat4& transform, float dt);

    // Advances every kinematic entity that has a nonzero velocity by velocity*dt,
    // and snaps previousTransform to the pre-move pose. Called once per tick.
    void advanceKinematics(float dt);

    PhysicalEntity* find(uint32_t id);

    const std::vector<PhysicalEntity>& entities() const { return mEntities; }
    std::vector<PhysicalEntity>& entities() { return mEntities; }

private:
    PhysicalEntitySystem() = default;

    std::vector<PhysicalEntity> mEntities;
    uint32_t mNextId = 1;
};

// Appends the 12 triangles of an axis-aligned box centered at `center` with half
// extents `half`, in entity-local space, with outward normals.
void buildBoxCollisionTriangles(std::vector<CollisionTriangle>& out,
                                const glm::vec3& center,
                                const glm::vec3& half);

// One actor-vs-entity contact. `contact.label` is the actor part, `contact.entityId`
// is the support entity, and `contact.surfaceVelocity` is that entity's velocity.
struct EntityActorContact {
    RecoveryContact contact;
    uint32_t entityId = 0;
};

// Tests every actor collision mesh against nearby moving physical entities,
// reusing the one actor-triangle contact routine (a temporary world view of the
// entity's world-space triangles). Dynamic entities thus participate alongside
// static world triangles in the same canonical contact vocabulary.
std::vector<EntityActorContact> collectActorEntityContacts(
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<PhysicalEntity>& entities,
    const glm::vec3& actorPos);

// Draws every entity as a filled box (and wireframe outline) so a spawned crate
// is visible while testing. Goes through the always-rendered production triangle
// flush, not the debug-visuals master gate.
void drawPhysicalEntities(const Camera& camera);

// Deterministic moving-crate proof: the actor is carried by the crate, walking
// adds to the supported motion, jumping preserves the inherited velocity, and
// leaving the crate removes support without zeroing the inherited velocity.
bool physicalEntitySelfTest(std::string* outSummary = nullptr);
