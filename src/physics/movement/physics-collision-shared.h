// 07 21 2026, 17 25
/* purpose
* Declares shared helpers used by the Player collision implementation.
* Keeps collision response, contact fact emission, and diagnostics reachable across collision files.
* Bridges current local collision facts into neutral movement contact data.
* Does NOT own movement reset formulas, packet transport, rendering, audio, or weapon behavior.
* Does NOT decide network authority, projectile damage, or server/client reconciliation.
* Does NOT replace the collision pipeline implementations declared below.
*/

#pragma once

#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <utility> // broadphase candidate ranges use pointer pairs
#include <glm/glm.hpp>

#include "physics/physics-types.h"
#include "physics/config.h"
#include "entities/player.h"
#include "config/collision-config.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/movement-types.h"
#include "physics/movement/actor-collision-mesh.h"

class Player;
class CollisionTriangle;

// Collision entity context: set before calling collision pipeline
// so gatherGLBTriangles can identify which entity is doing the query.
void setCollisionEntityContext(const char* entityId, unsigned int entityNumId, bool isNpc);
bool isCurrentEntityNpc();
void clearCollisionEntityContext();

// =====================================================
// Shared helper declarations extracted from
// physics-collision.cpp
// =====================================================

inline bool isFiniteVec3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline void projectVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    glm::vec3* velocities[] =
    {
        &p.vel,
        &p.externalImpulse
    };

    for (glm::vec3* v : velocities)
    {
        float into = glm::dot(*v, normal);

        if (into >= 0.0f)
            continue;

        *v -= normal * into;
    }
}

inline void clampVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    projectVelocityAgainstNormal(p, normal);
}

// Actor-part sweeps are stored as a displacement over the simulation step.
// The response formula consumes velocity, so convert at the fixed-tick
// boundary instead of treating a one-tick displacement as a speed.
inline glm::vec3 actorSweepVelocity(const glm::vec3& sweepDelta, float dt)
{
    return dt > 0.000001f ? sweepDelta / dt : glm::vec3(0.0f);
}

// Response against a surface.
//   partVelocity - the swept velocity of the body part / weapon that produced
//                  the contact; zero for root capsule.
//   bodyContact  - true when this came from a body/weapon contact rather than the
//                  root capsule.
//   penetration  - the contact's penetration depth.
//   actorPart    - the contact's actor part label, or null.
//   worldTriangle- the world triangle index, or -1.
// When a moving limb/weapon is the dominant impact it pushes the whole body
// outward even though the root velocity is not moving into the surface. A body
// contact that is simply embedded (penetrating but not moving) also gets the
// configured minimum push, so a limb can no longer stay stuck inside a wall.
// Root-capsule contacts (bodyContact == false) keep the old behavior.
// When actorPart and worldTriangle identify the contact, a repeated response for
// the SAME contact in the same tick is suppressed (per-contact dedup); a valid
// different contact is never muted by a global cooldown. Callers without an
// identity keep the legacy bounceCooldown gate.
inline void respondVelocityAgainstNormal(Player& p, const glm::vec3& normal,
                                         const glm::vec3& partVelocity = glm::vec3(0.0f),
                                         bool bodyContact = false,
                                         float penetration = 0.0f,
                                         const char* actorPart = nullptr,
                                         int worldTriangle = -1,
                                         float bounceScale = 1.0f)
{
    const CollisionConfig& cfg = CollisionConfig::instance();
    glm::vec3* velocities[] =
    {
        &p.vel,
        &p.externalImpulse
    };

    // Per-contact bounce scale. Weapon contacts pass their configured
    // "player_bounce" (0 by default), so a weapon brushing the world blocks
    // movement but does not launch the player from the weapon's sweep velocity.
    const float strength = cfg.bounceStrength() * std::max(0.0f, bounceScale);
    if (!cfg.bounceEnabled() || strength <= 0.0f)
    {
        projectVelocityAgainstNormal(p, normal);
        return;
    }

    const bool hasIdentity = (actorPart != nullptr && worldTriangle >= 0);
    if (hasIdentity)
    {
        // Dedupe by contact identity, not by a global timer: a duplicate of the
        // same limb/triangle in one tick is projected, but a different valid
        // impact still bounces even if another contact bounced this tick.
        if (p.collision.alreadyRespondedToContact(
                (const void*)actorPart, worldTriangle, p.movementSimulationTick))
        {
            projectVelocityAgainstNormal(p, normal);
            return;
        }
        p.collision.markRespondedToContact(
            (const void*)actorPart, worldTriangle, p.movementSimulationTick);
    }
    else if (p.collision.bounceCooldown > 0.0f)
    {
        // Legacy callers without a contact identity keep the old cooldown gate.
        projectVelocityAgainstNormal(p, normal);
        return;
    }

    float totalInto = 0.0f;
    for (glm::vec3* v : velocities)
        totalInto += std::max(0.0f, -glm::dot(*v, normal));

    const float partInto = std::max(0.0f, -glm::dot(partVelocity, normal));
    const float impact = std::max(totalInto, partInto);

    if (impact < cfg.bounceMinSpeed())
    {
        // A valid, very-low-speed body/weapon contact still nudges the whole
        // body outward. Fires when the part is sweeping into the surface OR is
        // simply embedded (penetrating), so a stick limb is pushed out. Root
        // contacts keep their old project-only behavior.
        const bool embedded =
            bodyContact && (partInto > 0.0f || penetration > 0.0f);
        if (embedded && cfg.bounceMinPush() > 0.0f)
        {
            if (bodyContact &&
                p.collision.rootBounceAppliedTick == p.movementSimulationTick)
            {
                projectVelocityAgainstNormal(p, normal);
                return;
            }
            const float retention =
                p.rollLowFriction ? 1.0f : (1.0f - cfg.bounceFriction());
            const glm::vec3 tangent = p.vel - normal * glm::dot(p.vel, normal);
            p.vel = tangent * retention + normal * cfg.bounceMinPush();
            if (bodyContact)
                p.collision.rootBounceAppliedTick = p.movementSimulationTick;
            p.collision.bounceCooldown = cfg.bounceCooldown();
            return;
        }
        projectVelocityAgainstNormal(p, normal);
        return;
    }

    const float maxInto = cfg.bounceMaxSpeed();
    const float retention = 1.0f - cfg.bounceFriction();
    const bool rootBounceAlreadyApplied = bodyContact &&
        p.collision.rootBounceAppliedTick == p.movementSimulationTick;
    for (glm::vec3* v : velocities)
    {
        float into = -glm::dot(*v, normal);
        if (into <= 0.0f)
            continue;
        glm::vec3 tangent = *v - normal * glm::dot(*v, normal);
        if (!rootBounceAlreadyApplied)
            *v = tangent * retention + normal * (std::min(into, maxInto) * strength);
        else
            *v = tangent * retention;
    }

    // Part-driven push: the moving limb/weapon is the dominant impact, so give
    // the root body an outward velocity along the surface normal.
    if (partInto > totalInto && !rootBounceAlreadyApplied)
    {
        const glm::vec3 tangent = p.vel - normal * glm::dot(p.vel, normal);
        p.vel = tangent * retention +
                normal * (std::min(partInto, maxInto) * strength);
    }

    if (bodyContact)
        p.collision.rootBounceAppliedTick = p.movementSimulationTick;

    p.collision.bounceCooldown = cfg.bounceCooldown();
}

void applyCollisionContact(
    Player& p,
    bool& groundedThisFrame,
    const glm::vec3& normal,
    glm::vec3 point,
    float penetration,
    int triangleIndex,
    const char* label
);

MovementContactKind classifyCollisionMovementContactKind(const glm::vec3& normal,
                                                         bool grounded,
                                                         bool step = false);
void appendPlayerMovementContact(Player& p,
                                 MovementContactKind kind,
                                 const glm::vec3& normal,
                                 glm::vec3 point,
                                 float penetration,
                                 int triangleIndex,
                                 MovementContactSource source =
                                     MovementContactSource::StaticWorld);
void appendPlayerMovementContactForNormal(Player& p,
                                          bool grounded,
                                          bool step,
                                          const glm::vec3& normal,
                                          glm::vec3 point,
                                          float penetration,
                                          int triangleIndex,
                                          MovementContactSource source =
                                              MovementContactSource::StaticWorld);

void recoverInvalidPlayerCollisionState(Player& p, const glm::vec3& frameStart, const char* phase);

bool rejectBelowBlockTopContact(
    const Capsule& cap,
    const AABB& block,
    const RecoveryContact& contact);

void appendUniqueTriangleIndices(std::vector<int>& dst, const std::vector<int>& src);

// =====================================================
// AABB helpers
// =====================================================

inline AABB makeSweptCapsuleAABB(const Capsule& cap, const glm::vec3& move)
{
    glm::vec3 mn = glm::min(glm::min(cap.a, cap.b), glm::min(cap.a + move, cap.b + move));
    glm::vec3 mx = glm::max(glm::max(cap.a, cap.b), glm::max(cap.a + move, cap.b + move));
    return {mn - glm::vec3(cap.r), mx + glm::vec3(cap.r)};
}

inline AABB makeTriangleAABB(const CollisionTriangle& tri)
{
    return {
        glm::min(glm::min(tri.a, tri.b), tri.c),
        glm::max(glm::max(tri.a, tri.b), tri.c)
    };
}

// Cached world-triangle bounds. Falls back to a live computation when a caller
// built triangles without populating the cache (tests, direct pushes). The cache
// is filled by buildCollisionChunks for every world that has chunks.
inline AABB collisionTriangleAABB(const CollisionMeshCache& mesh, int index)
{
    if (index >= 0 && index < (int)mesh.triangleAABBs.size())
        return mesh.triangleAABBs[index];
    return makeTriangleAABB(mesh.triangles[index]);
}

inline AABB makePlayerAABB(const Player& p)
{
    float s = std::max(p.sizeScale, 0.001f);
    glm::vec3 half(
        PLAYER_WIDTH  * 0.5f * s,
        PLAYER_DEPTH  * 0.5f * s,
        PLAYER_HEIGHT * 0.5f * s
    );
    return { p.pos - half, p.pos + half };
}

inline bool overlaps(const AABB& a, const AABB& b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

inline bool sweptTriangleOverlapsTriangleOBB(
    const CollisionTriangleOBB& obb,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
    const glm::vec3& pa, const glm::vec3& pb, const glm::vec3& pc,
    float margin)
{
    if (!obb.valid)
        return true;
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    for (const glm::vec3& p : {a, b, c, pa, pb, pc})
    {
        const glm::vec3 d = p - obb.origin;
        const glm::vec3 q(glm::dot(d, obb.axisU),
                          glm::dot(d, obb.axisV),
                          glm::dot(d, obb.axisN));
        mn = glm::min(mn, q);
        mx = glm::max(mx, q);
    }
    const float e = std::max(0.0f, margin);
    return mn.x <= obb.localMax.x + e && mx.x >= obb.localMin.x - e &&
           mn.y <= obb.localMax.y + e && mx.y >= obb.localMin.y - e &&
           mn.z <= obb.localMax.z + e && mx.z >= obb.localMin.z - e;
}

// Conservative first-stage rejection for a world triangle's thin oriented
// prism.  It projects the actor query box onto the prism axes; it can retain
// extra pairs, but can never remove an exact triangle contact.  This is what
// keeps a very large visible map triangle from pairing with every avatar
// triangle just because its axis-aligned bounds cover the whole room.
inline bool aabbOverlapsTriangleOBB(
    const CollisionTriangleOBB& obb, const AABB& aabb, float margin)
{
    if (!obb.valid)
        return true;
    const glm::vec3 center = (aabb.min + aabb.max) * 0.5f;
    const glm::vec3 half = (aabb.max - aabb.min) * 0.5f;
    const float e = std::max(0.0f, margin);
    const auto overlapsAxis = [&](const glm::vec3& axis,
                                  float localMin, float localMax) {
        const glm::vec3 delta = center - obb.origin;
        const float projectedCenter = glm::dot(delta, axis);
        const float projectedHalf = glm::dot(glm::abs(axis), half);
        return projectedCenter - projectedHalf <= localMax + e &&
               projectedCenter + projectedHalf >= localMin - e;
    };
    return overlapsAxis(obb.axisU, obb.localMin.x, obb.localMax.x) &&
           overlapsAxis(obb.axisV, obb.localMin.y, obb.localMax.y) &&
           overlapsAxis(obb.axisN, obb.localMin.z, obb.localMax.z);
}

inline glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
}

// =====================================================
// Sphere-triangle helpers needed by body collision
// =====================================================

bool sweepSphereTriangle(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    const CollisionTriangle& tri,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    Contact& contact
);

// =====================================================
// Triangle gathering helpers needed by stress tests
// =====================================================

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller = nullptr
);

// Scratch-buffer version: writes into `out` instead of allocating a new vector.
void gatherGLBTriangles(
    std::vector<int>& out,
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller = nullptr
);

// TODO-DELETE (conditional): superseded for actors by collectActorMeshContacts,
// but still used by the legacy pipeline, remote geometry safety, ragdoll, and
// stress tests. See the TODO-DELETE comment at the definition.
std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates,
    const char* label = "glb-recovery"
);

// =====================================================
// Conversion helper for body collision
// =====================================================

inline bool rejectBelowTopFaceContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    const glm::vec3& normal,
    const glm::vec3& point,
    int triangleIndex,
    const char* phase)
{
    (void)cap; (void)tri; (void)normal; (void)point; (void)triangleIndex; (void)phase;
    return false;
}

// =====================================================
// Collision trace snapshot (used by stress tests + summary)
// =====================================================

struct CollisionTraceSnapshot
{
    glm::vec3 startPos{0.0f};
    glm::vec3 finalPos{0.0f};
    glm::vec3 inputMove{0.0f};
    int initialCandidates = 0;
    int maxCandidates = 0;
    int sweepIterations = 0;
    int sweepHits = 0;
    int maxSimultaneousTOI = 0;
    int maxSlideContacts = 0;
    int maxRecoveryContacts = 0;
    int finalContacts = 0;
    int finalSafetyContacts = 0;
    int resweepHits = 0;
    int faceHits = 0;
    int edgeHits = 0;
    int vertexHits = 0;
    float maxPenetration = 0.0f;
    bool emergencyEscaped = false;
};

extern CollisionTraceSnapshot gLastCollisionTrace;

struct BWInvestigate {
    int sphereCount = 0;
    int bodyPartSphereCount = 0;
    int weaponCapsuleSphereCount = 0;
    int weaponCapsuleContactCount = 0;
    int candidateCount = 0;
    int triangleTests = 0;
    int sweepTests = 0;
    int staticTests = 0;
    int contactsProduced = 0;
    double collectSpheresMs = 0.0;
    double collectContactsMs = 0.0;
    double sweepSphereTriangleMs = 0.0;
    double sphereTriangleContactMs = 0.0;
    double bodyPartSpheresMs = 0.0;
    double weaponCapsuleSpheresMs = 0.0;
};
extern BWInvestigate gBW;

// Per-solve actor-narrowphase counters. Reset by solveActorTriangleCollision
// before its correction loop and accumulated by collectActorMeshContactsInto,
// so the broadphase/narrowphase split can be measured from the solve summary.
struct ActorNarrowphaseStats {
    int candidatePairs = 0;      // actor-triangle x candidate AABB tests
    int triangleTests = 0;       // narrowphase triangle/segment tests
    int roundedFeatureCalls = 0; // closestRoundedTriangleFeatures calls
    int bvhLeafPairs = 0;         // exact actor/world leaf pairs emitted
    int orientedBoxRejected = 0;   // AABB pairs rejected by triangle prism
    int worldObbRejected = 0;      // gathered map candidates rejected before BVH
};
extern ActorNarrowphaseStats gActorNarrowphase;

// =====================================================
// Body / weapon capsule helpers
// =====================================================

// TODO-DELETE: this whole legacy body/weapon sphere + capsule group is
// superseded by collectActorCollisionMeshes + solveActorTriangleCollision
// (actor-collision-mesh.h / actor-triangle-solver.h). See the TODO-DELETE
// comments at each definition in physics-collision-body.cpp. Do not remove until
// the triangle path is accepted by human gameplay testing in Dust 3 Siberia,
// Trainkinda, and Chain of Judgement and weapon-swordsword's use of
// recomputeWeaponCapsule has migrated.

struct BodyWeaponSphere {
    glm::vec3 center;
    float radius;
    const char* label;
    glm::vec3 sweepDelta;
};

void recomputeWeaponCapsule(Player& p);
std::vector<BodyWeaponSphere> collectBodyWeaponSpheres(Player& p,
                                                       bool includeBodyParts = true);
std::vector<RecoveryContact> collectBodyWeaponContacts(
    const Player& p,
    const World& world,
    const std::vector<BodyWeaponSphere>& spheres
);
std::vector<glm::vec3> collectPlayerBodyCollisionSamples(Player& p);

// Body collision from each part's real mesh triangles (triangle-vs-triangle
// against the world). This is "what you see is what the hitbox is": the collider
// triangles loaded from the model are transformed by the part world transform
// (and the previous transform for sweep) and tested against world triangles.
// First version: brute-force within each part's broadphase AABB; optimize later.
std::vector<RecoveryContact> collectBodyMeshContacts(Player& p, const World& world);

// Generalized actor-mesh contact test: tests every triangle of every supplied
// ActorCollisionMesh (swept from its previousTransform to its desiredTransform,
// plus current-pose penetration) against the supplied world triangle
// candidates. Shares the exact triangle math and budgets with the body path.
// `actorPos` orients current-overlap normals toward the actor. The caller owns
// broadphase candidate gathering so the cache/scratch path can be reused.
std::vector<RecoveryContact> collectActorMeshContacts(
    const World& world,
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<int>& candidates,
    const glm::vec3& actorPos,
    bool filterCandidatesByMeshAabb = true,
    float contactSkin = -1.0f);

// Scratch-buffer form: appends into `out` (cleared first) so the fixed-tick
// solver never allocates a contact vector per correction iteration.
struct AabbTree;
// Exact actor/world broadphase result. `meshTriangleOffsets` maps each mesh to
// a compact flattened actor-triangle range, while `triangleCandidateOffsets`
// maps each flattened actor triangle to its world-triangle range. The IDs in
// `worldTriangleIds` are only
// original world triangle IDs produced by the actor-BVH/world-BVH traversal.
// This is an index/cache around the real triangles; it never replaces them.
struct ActorTriangleBroadphase
{
    std::vector<int> meshTriangleOffsets;
    std::vector<int> triangleCandidateOffsets;
    std::vector<int> worldTriangleIds;

    void clear()
    {
        meshTriangleOffsets.clear();
        triangleCandidateOffsets.clear();
        worldTriangleIds.clear();
    }

    bool validForMesh(size_t meshIndex, size_t triangleCount) const
    {
        return meshIndex + 1 < meshTriangleOffsets.size() &&
               meshTriangleOffsets[meshIndex + 1] - meshTriangleOffsets[meshIndex] ==
                   static_cast<int>(triangleCount);
    }

    std::pair<const int*, const int*> candidatesFor(
        size_t meshIndex, size_t triangleIndex) const
    {
        if (meshIndex + 1 >= meshTriangleOffsets.size())
            return {nullptr, nullptr};
        const int meshBegin = meshTriangleOffsets[meshIndex];
        const int meshEnd = meshTriangleOffsets[meshIndex + 1];
        if (triangleIndex >= static_cast<size_t>(std::max(0, meshEnd - meshBegin)))
            return {nullptr, nullptr};
        const int flat = meshBegin + static_cast<int>(triangleIndex);
        if (flat < 0 || flat + 1 >= static_cast<int>(triangleCandidateOffsets.size()))
            return {nullptr, nullptr};
        const int begin = triangleCandidateOffsets[flat];
        const int end = triangleCandidateOffsets[flat + 1];
        if (begin < 0 || end < begin ||
            end > static_cast<int>(worldTriangleIds.size()))
            return {nullptr, nullptr};
        return {worldTriangleIds.data() + begin, worldTriangleIds.data() + end};
    }

    int candidateCountFor(size_t meshIndex, size_t triangleIndex) const
    {
        if (meshIndex + 1 >= meshTriangleOffsets.size())
            return -1;
        const int meshBegin = meshTriangleOffsets[meshIndex];
        const int meshEnd = meshTriangleOffsets[meshIndex + 1];
        if (triangleIndex >= static_cast<size_t>(std::max(0, meshEnd - meshBegin)))
            return -1;
        const int flat = meshBegin + static_cast<int>(triangleIndex);
        if (flat < 0 || flat + 1 >= static_cast<int>(triangleCandidateOffsets.size()))
            return -1;
        const int begin = triangleCandidateOffsets[flat];
        const int end = triangleCandidateOffsets[flat + 1];
        if (begin < 0 || end < begin ||
            end > static_cast<int>(worldTriangleIds.size()))
            return -1;
        return end - begin;
    }
};

void collectActorMeshContactsInto(
    const World& world,
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<int>& candidates,
    const glm::vec3& actorPos,
    std::vector<RecoveryContact>& out,
    bool filterCandidatesByMeshAabb = true,
    float contactSkin = -1.0f,
    const AabbTree* worldTree = nullptr,
    bool comparison = false,
    // Rounded-feature radius when contactSkin < 0. The normal movement shell is
    // MOVEMENT_FEATURE_SMOOTHNESS; a deep-depenetration recovery pass passes a
    // larger radius so a body that is already embedded (e.g. fell through the
    // floor) is still detected and pushed out.
    float featureRadius = MOVEMENT_FEATURE_SMOOTHNESS,
    const ActorTriangleBroadphase* actorBroadphase = nullptr);

// Union swept AABB (previous + desired + move) of every supplied mesh, in local
// triangle space transformed to world. Used to gather broadphase candidates.
AABB makeSweptActorMeshAABB(
    const std::vector<ActorCollisionMesh>& meshes,
    const glm::vec3& move);

// =====================================================
// GLB collision pipeline
// =====================================================

void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
);

std::vector<RecoveryContact> collectGLBRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<glm::vec3>& bodySamples,
    const std::vector<int>& candidates,
    float bodySampleRadius
);

// =====================================================
// Block collision functions
// =====================================================

bool capsuleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const AABB& block,
    float& hitTime,
    glm::vec3& hitNormal
);

bool capsuleVsBlock(
    const Capsule& cap,
    const AABB& block,
    glm::vec3& correction,
    bool& outGrounded
);

std::vector<RecoveryContact> collectBlockContactsForCapsule(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks
);

bool findBlockFallbackEscape(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const std::vector<RecoveryContact>& contacts,
    const glm::vec3& weightedNormal,
    glm::vec3& outCorrection
);
