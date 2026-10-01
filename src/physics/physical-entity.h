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
#include "impact/destructible-geometry.h"

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

    // Server-assigned identity used by destruction replication. Local entities
    // use their runtime id; client mirrors keep the server's value and are
    // marked serverDriven so they are never simulated locally.
    uint32_t networkId = 0;
    bool serverDriven = false;
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
    // Authoritative destruction state (box minus spherical cuts). `enabled`
    // replaces the old boolean flag; generated triangles live in
    // localTriangles. See impact/destructible-geometry.h.
    MimitaImpact::DestructibleGeometry destructible;
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
struct AabbTree;

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

    // Finds a replicated mirror by its server network id. Local (authoritative)
    // entities are not returned; mirrors are the only serverDriven entities.
    PhysicalEntity* findByNetworkId(uint32_t networkId);

    // Creates a client mirror of a server entity: same network id, marked
    // serverDriven so the fixed tick never simulates or pushes it. Returns the
    // local id, or 0 when a mirror for `networkId` already exists.
    uint32_t addReplicated(uint32_t networkId,
                           const std::vector<CollisionTriangle>& localTriangles,
                           const glm::mat4& transform,
                           PhysicalEntityMotion motion,
                           uint32_t materialId);

    // Removes one entity by id. Returns true when an entity was removed.
    bool remove(uint32_t id);

    // Removes a replicated mirror by network id. Local entities are untouched.
    bool removeByNetworkId(uint32_t networkId);

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

// Re-derives mass, center of mass, and inertia: from the cached cut surface
// when the entity is destructible and has been cut, otherwise from the box.
// Same owner as the per-tick refresh; callers use it right after a cut so the
// change is visible immediately instead of one fixed tick later.
void refreshEntityMassProperties(PhysicalEntity& entity);

// Applies one authoritative impulse to a Dynamic entity at a world-space point.
// The offset from the center of mass produces torque, so a projectile hit spins
// and pushes the object instead of teleporting it. No-op for non-Dynamic or
// zero-mass entities. Shared by the server impact path and client prediction.
void applyPhysicalEntityImpulse(PhysicalEntity& entity, const glm::vec3& impulse,
                                const glm::vec3& worldPoint);

// Appends the 12 triangles of an axis-aligned box centered at `center` with half
// extents `half`, in entity-local space, with outward normals.
void buildBoxCollisionTriangles(std::vector<CollisionTriangle>& out,
                                const glm::vec3& center,
                                const glm::vec3& half);

// Cached world-space expansion of one entity's local collision surface.
struct EntitySurfaceCacheView
{
    CollisionMeshCache* meshCache = nullptr; // world triangles + per-tri AABBs
    const AabbTree* tree = nullptr;          // index over meshCache->triangles
};

// Returns `entity`'s cached world-space collision surface, rebuilding it only
// when the entity id, destructible geometry revision, transform, or triangle
// count changed. Shared by the actor contact pass and the projectile entity
// sweep so the transform work and spatial index are built once per revision
// instead of once per query. Thread-local and transient: the returned pointers
// are invalidated by the next call for a different revision or pose.
EntitySurfaceCacheView cachedEntitySurface(const PhysicalEntity& entity);

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
