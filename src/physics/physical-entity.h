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
#include <glm/gtc/quaternion.hpp>

#include "physics/physics-types.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/physics-collision.h"

// Static never moves; Kinematic is moved by its owner (transform is set, velocity
// derived) and is infinite-mass; Dynamic is integrated by the fixed-step rigid
// body owner with gravity, inertia, contact response, and sleep.
enum class PhysicalEntityMotion : uint8_t {
    Static,
    Kinematic,
    Dynamic
};

enum class PhysicalEntityShape : uint8_t {
    TriangleMesh,
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Cone
};

struct PhysicalEntity {
    uint32_t id = 0;
    std::string persistenceId;
    uint32_t ownerId = 0;
    uint32_t networkOwnerId = 0;
    PhysicalEntityMotion motion = PhysicalEntityMotion::Static;
    PhysicalEntityShape shape = PhysicalEntityShape::TriangleMesh;
    glm::mat4 previousTransform{1.0f};
    glm::mat4 transform{1.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    glm::vec3 centerOfMass{0.0f};
    glm::vec3 halfExtents{0.5f};
    glm::vec3 inertia{1.0f};
    glm::vec3 inverseInertia{1.0f};
    float mass = 1.0f;
    float density = 1.0f;
    float friction = 0.6f;
    float restitution = 0.0f;
    float gravityScale = 1.0f;
    float linearDamping = 0.15f;
    float angularDamping = 2.0f;
    float maxAngularSpeed = 8.0f;
    float rightingStrength = 18.0f;
    float restingUprightDot = 0.985f;
    float sleepLinearThreshold = 0.25f;
    float sleepAngularThreshold = 0.25f;
    uint16_t sleepTicks = 0;
    uint16_t sleepRequiredTicks = 45;
    uint8_t supportGraceTicks = 0;
    float strength = 100.0f;
    float health = 100.0f;
    bool destructible = false;
    bool sleeping = false;
    uint64_t lastPlayerPushTick = 0;
    std::vector<CollisionTriangle> localTriangles;   // entity-local
    uint32_t materialId = 0;
    std::string modelPath;
    std::string texturePath;
    bool collidesWithActors = true;
};

class Camera;
class Player;
struct World;

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
    void advanceKinematics(float dt, const World& world);

    // Applies the local player's horizontal contact impulse to Dynamic
    // entities. The actor solver calls this after its authoritative movement
    // step so a push is visible on the next fixed physics tick.
    void applyPlayerPush(const Player& player, float dt);

    // Applies one contact-owned player impulse immediately, before the actor
    // response projects the player's velocity away from the object.
    void applyPlayerContactPush(uint32_t entityId, const Player& player,
                                const glm::vec3& point,
                                const glm::vec3& contactNormal);

    PhysicalEntity* find(uint32_t id);

    // Removes one entity by id. Returns true when an entity was removed.
    bool remove(uint32_t id);

    const std::vector<PhysicalEntity>& entities() const { return mEntities; }
    std::vector<PhysicalEntity>& entities() { return mEntities; }

    uint64_t simulationTick() const { return mSimulationTick; }
    float renderAlpha() const;

private:
    PhysicalEntitySystem() = default;

    std::vector<PhysicalEntity> mEntities;
    uint32_t mNextId = 1;
    double mFixedAccumulator = 0.0;
    uint64_t mSimulationTick = 0;
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

// TODO-DELETE 2026-09-28 [Phase 2]: remove the temporary World allocation in
// collectActorEntityContacts and route entity-local triangles through the
// shared cached candidate/narrowphase path once Phase 3 owns object shapes.
// Tests every actor collision mesh against nearby moving physical entities,
// reusing the one actor-triangle contact routine. Dynamic entities thus
// participate alongside static world triangles in the same contact vocabulary.
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
