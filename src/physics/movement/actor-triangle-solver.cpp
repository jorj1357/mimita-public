// 2026-09-27
/* purpose
* One authoritative actor-triangle collision solver.
* Sweeps every actor collision mesh (body parts + weapon) from its safe previous
* pose to its desired pose, builds one contact manifold, applies one position
* correction, one velocity response per distinct surface, then a final
* penetration validation.
* First version; runs alongside the legacy pipeline, not yet called by
* doCollisions.
* Does NOT render, play effects, send packets, or own movement reset formulas.
* Does NOT delete or replace the legacy owners.
*/

#include "physics/movement/actor-triangle-solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/config.h"
#include "config/collision-config.h"
#include "debug/structured-log.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/collision-aabb-tree.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/physical-entity.h"
#include "map/map-loader-collision.h"
#include "world/world.h"
#include "entities/player.h"
#include "effects/effect-part.h"

namespace {

constexpr float kSlop = 0.01f;
constexpr float kMaxCorrection = 2.0f;
constexpr int kMaxIterations = 4;
constexpr float kTouchingFaceSeamDistance = 0.015f;
constexpr float kNearGroundVerticalSpeed = 0.05f;

bool finiteAabb(const AABB& a)
{
    return std::isfinite(a.min.x) && std::isfinite(a.min.y) && std::isfinite(a.min.z) &&
           std::isfinite(a.max.x) && std::isfinite(a.max.y) && std::isfinite(a.max.z) &&
           a.max.x >= a.min.x && a.max.y >= a.min.y && a.max.z >= a.min.z;
}

nlohmann::json vec3Json(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

// Combines contacts whose normals point the same way into one surface, keeping
// the deepest penetration and the strongest part impact. This is what makes the
// actor bounce once per surface instead of once per body part.
void mergeContactsByNormal(const std::vector<RecoveryContact>& contacts,
                           std::vector<RecoveryContact>& merged)
{
    merged.clear();
    for (const RecoveryContact& c : contacts)
    {
        bool found = false;
        for (RecoveryContact& existing : merged)
        {
            // Never merge two different support entities into one surface, or the
            // moving-support identity would be lost from the manifold.
            if (existing.entityId == c.entityId &&
                glm::dot(existing.normal, c.normal) >= 0.95f)
            {
                existing.normal = glm::normalize(existing.normal + c.normal);
                existing.point = (existing.point + c.point) * 0.5f;
                existing.penetration = std::max(existing.penetration, c.penetration);
                existing.timeOfImpact = std::min(existing.timeOfImpact, c.timeOfImpact);
                if (glm::dot(c.sweepDelta, c.sweepDelta) >
                    glm::dot(existing.sweepDelta, existing.sweepDelta))
                    existing.sweepDelta = c.sweepDelta;
                found = true;
                break;
            }
        }
        if (!found)
            merged.push_back(c);
    }
}

glm::vec3 manifoldResponseNormal(const RecoveryContact& contact)
{
    glm::vec3 response =
        isFiniteVec3(contact.responseNormal) &&
                glm::dot(contact.responseNormal, contact.responseNormal) > 0.5f
            ? glm::normalize(contact.responseNormal)
            : contact.normal;
    if (isFiniteVec3(contact.surfaceNormal) &&
        glm::dot(contact.surfaceNormal, contact.surfaceNormal) > 0.5f)
    {
        const glm::vec3 face = glm::normalize(contact.surfaceNormal);
        if (face.z > MAX_WALKABLE_SLOPE_DOT)
            response = face;
    }
    return response;
}

// Two authored block faces can occupy the same plane with opposite normals.
// They are an internal seam, not two physical walls. Remove the non-blocking
// side (or the shallower side when both block) before velocity response.
void removeTouchingFaceSeams(std::vector<RecoveryContact>& contacts,
                             const glm::vec3& intendedMove)
{
    std::vector<bool> removed(contacts.size(), false);
    for (size_t i = 0; i < contacts.size(); ++i)
    {
        if (removed[i] || contacts[i].entityId != 0)
            continue;
        for (size_t j = i + 1; j < contacts.size(); ++j)
        {
            if (removed[j] || contacts[j].entityId != 0)
                continue;
            if (glm::dot(contacts[i].normal, contacts[j].normal) > -0.98f ||
                glm::length(contacts[i].point - contacts[j].point) >
                    kTouchingFaceSeamDistance)
                continue;

            const float moveI = glm::dot(intendedMove,
                                         manifoldResponseNormal(contacts[i]));
            const float moveJ = glm::dot(intendedMove,
                                         manifoldResponseNormal(contacts[j]));
            if ((moveI < 0.0f) != (moveJ < 0.0f))
                removed[moveI < 0.0f ? j : i] = true;
            else
                removed[contacts[i].penetration >= contacts[j].penetration ? j : i] = true;
        }
    }

    std::vector<RecoveryContact> filtered;
    filtered.reserve(contacts.size());
    for (size_t i = 0; i < contacts.size(); ++i)
        if (!removed[i])
            filtered.push_back(contacts[i]);
    contacts.swap(filtered);
}

// A slope and its connected wall can both be reported by the independent
// triangle feature queries. When they are really the same local edge on one
// actor part, retain the surface that blocks the current movement direction;
// otherwise the two normals form an artificial wedge and snag the actor.
void collapseCloseFeatureContacts(std::vector<RecoveryContact>& contacts,
                                  const glm::vec3& intendedMove)
{
    const float maxDistance = MOVEMENT_FEATURE_SMOOTHNESS * 0.75f;
    std::vector<bool> removed(contacts.size(), false);
    for (size_t i = 0; i < contacts.size(); ++i)
    {
        if (removed[i])
            continue;
        for (size_t j = i + 1; j < contacts.size(); ++j)
        {
            if (removed[j] || contacts[i].entityId != contacts[j].entityId ||
                !contacts[i].label || !contacts[j].label ||
                std::strcmp(contacts[i].label, contacts[j].label) != 0)
                continue;
            const float normalAlignment =
                glm::dot(manifoldResponseNormal(contacts[i]),
                         manifoldResponseNormal(contacts[j]));
            if (normalAlignment <= -0.5f || normalAlignment >= 0.98f ||
                glm::length(contacts[i].point - contacts[j].point) > maxDistance)
                continue;

            const float blockingI = -glm::dot(
                intendedMove, manifoldResponseNormal(contacts[i]));
            const float blockingJ = -glm::dot(
                intendedMove, manifoldResponseNormal(contacts[j]));
            const bool removeJ = blockingI > blockingJ + 0.001f ||
                (std::fabs(blockingI - blockingJ) <= 0.001f &&
                 contacts[i].penetration >= contacts[j].penetration);
            removed[removeJ ? j : i] = true;
            if (!removeJ)
                break;
        }
    }

    std::vector<RecoveryContact> filtered;
    filtered.reserve(contacts.size());
    for (size_t i = 0; i < contacts.size(); ++i)
        if (!removed[i])
            filtered.push_back(contacts[i]);
    contacts.swap(filtered);
}

// Lowest point of the actor's DESIRED pose (not the swept union), used to tell
// foot ground from a limb resting on a ledge.
float desiredLowestZ(const std::vector<ActorCollisionMesh>& meshes)
{
    float lowest = std::numeric_limits<float>::max();
    for (const ActorCollisionMesh& m : meshes)
    {
        if (!m.localTriangles)
            continue;
        for (const CollisionTriangle& t : *m.localTriangles)
            for (const glm::vec3& v : {t.a, t.b, t.c})
                lowest = std::min(lowest,
                    glm::vec3(m.desiredTransform * glm::vec4(v, 1.0f)).z);
    }
    return lowest;
}

} // namespace

bool solveActorTriangleCollision(
    Player& player,
    const World& world,
    const glm::vec3& desiredMovement,
    ActorTriangleCollisionResult& result,
    const std::vector<PhysicalEntity>* entities,
    float dt)
{
    result = ActorTriangleCollisionResult{};
    result.startPos = player.pos;
    result.correctedPos = player.pos;
    result.remainingMovement = desiredMovement;
    const glm::vec3 velocityBeforeSolve = player.vel;
    const auto solveStart = std::chrono::steady_clock::now();
    gActorNarrowphase = ActorNarrowphaseStats{};

    const bool hasEntities = entities && !entities->empty();
    if (world.collisionMesh.triangles.empty() && !hasEntities)
        return false;

    // Capture the safe pose ONCE. The safe previous transforms must not be
    // overwritten between correction iterations, or a depenetration that moves
    // the actor up across a floor would look like a fresh downward sweep and
    // flip the normal. Corrections accumulate and are applied once at the end.
    const CollisionConfig& collisionConfig = CollisionConfig::instance();
    const std::vector<ActorCollisionMesh> baseMeshes =
        collectActorCollisionMeshes(player);
    if (baseMeshes.empty())
        return false;

    // Weapon collision: a configured triangle hitbox (weaponcollisions.json
    // source "boxes"/triangle mode) is already part of `baseMeshes`, so the
    // legacy JSON sphere/capsule injection must be skipped to keep ONE owner.
    // The sphere path remains only for weapons without a triangle config.
    const bool weaponTriangleMode = player.weaponCollisionDebug.usesJsonMesh;
    const std::vector<BodyWeaponSphere> weaponSpheres =
        weaponTriangleMode ? std::vector<BodyWeaponSphere>{}
                           : collectBodyWeaponSpheres(player, false);

    std::vector<int> candidates;
    AABB box = makeSweptActorMeshAABB(baseMeshes, desiredMovement);
    if (!finiteAabb(box))
        return false;
    const float queryMargin = std::max(
        collisionConfig.collisionSkin(), MOVEMENT_FEATURE_SMOOTHNESS);
    box.min -= glm::vec3(queryMargin);
    box.max += glm::vec3(queryMargin);
    appendChunkTrianglesForAABB(world, box, queryMargin,
                                candidates, "actorTriangleSolve");
    result.candidates = (int)candidates.size();
    result.queryBoxSize = box.max - box.min;

    // Bounded diagnostic: how many returned candidates are the "large" class
    // (their own AABB would exceed kMaxChunksPerTriangle chunks). Replaces the
    // removed always-large list as the before/after comparison metric.
    if (world.collisionMesh.triangleAABBs.size() ==
            world.collisionMesh.triangles.size() &&
        world.collisionChunkSize > 0.001f)
    {
        int large = 0;
        for (int ci : candidates)
        {
            if (ci < 0 || ci >= (int)world.collisionMesh.triangleAABBs.size())
                continue;
            const AABB& b = world.collisionMesh.triangleAABBs[ci];
            glm::ivec3 c0((int)std::floor(b.min.x / world.collisionChunkSize),
                          (int)std::floor(b.min.y / world.collisionChunkSize),
                          (int)std::floor(b.min.z / world.collisionChunkSize));
            glm::ivec3 c1((int)std::floor(b.max.x / world.collisionChunkSize),
                          (int)std::floor(b.max.y / world.collisionChunkSize),
                          (int)std::floor(b.max.z / world.collisionChunkSize));
            int64_t cells = (int64_t)(c1.x - c0.x + 1) *
                            (c1.y - c0.y + 1) * (c1.z - c0.z + 1);
            if (cells > kMaxChunksPerTriangle)
                ++large;
        }
        result.largeTriangles = large;
    }

    // AABB tree over the gathered world candidates, built once per solve and
    // reused by every correction iteration. It only prunes pairs; the
    // narrowphase and response are unchanged.
    const bool accelerated = collisionConfig.actorCollisionAccelerated();
    static thread_local AabbTree s_worldTree;
    const AabbTree* worldTree = nullptr;
    if (accelerated && !candidates.empty() &&
        world.collisionMesh.triangleAABBs.size() ==
            world.collisionMesh.triangles.size())
    {
        s_worldTree.build(candidates, world.collisionMesh.triangleAABBs);
        worldTree = &s_worldTree;
    }
    const bool comparison = collisionConfig.actorCollisionComparison();

    // Reused fixed-tick scratch. The correction loop no longer allocates a mesh
    // vector, a contact vector, or an accumulated-contact vector per iteration.
    static thread_local std::vector<ActorCollisionMesh> s_meshes;
    static thread_local std::vector<RecoveryContact> s_contacts;
    static thread_local std::vector<RecoveryContact> s_allContacts;
    static thread_local std::vector<RecoveryContact> s_manifold;
    s_allContacts.clear();

    glm::vec3 accumulated(0.0f);

    for (int iter = 0; iter < kMaxIterations &&
                       (!candidates.empty() || hasEntities); ++iter)
    {
        // Shift the whole capture (safe + desired) by the accumulated
        // correction so the sweep direction stays the tick's real motion.
        s_meshes = baseMeshes;
        const glm::mat4 shift = glm::translate(glm::mat4(1.0f), accumulated);
        for (ActorCollisionMesh& m : s_meshes)
        {
            m.previousTransform = shift * m.previousTransform;
            m.desiredTransform = shift * m.desiredTransform;
        }

        const AABB poseBox = makeSweptActorMeshAABB(s_meshes, glm::vec3(0.0f));
        const glm::vec3 refPoint = (poseBox.min + poseBox.max) * 0.5f;

        collectActorMeshContactsInto(world, s_meshes, candidates, refPoint, s_contacts,
                                     true, -1.0f, worldTree, comparison);

        // The weapon sphere/capsule collector uses the same world-triangle
        // contact facts and response owner, but only needs to run once per
        // correction pass. Do not re-add identical weapon contacts after the
        // first positional correction.
        if (iter == 0 && !weaponSpheres.empty())
        {
            std::vector<RecoveryContact> weaponContacts =
                collectBodyWeaponContacts(player, world, weaponSpheres);
            s_contacts.insert(s_contacts.end(), weaponContacts.begin(), weaponContacts.end());
        }

        // Moving physical entities join the same manifold, carrying the support
        // entity id and surface velocity.
        if (hasEntities)
        {
            std::vector<EntityActorContact> entityHits =
                collectActorEntityContacts(s_meshes, *entities, refPoint);
            for (EntityActorContact& eh : entityHits)
                s_contacts.push_back(eh.contact);
        }

        if (s_contacts.empty())
            break;

        result.iterations = iter + 1;
        result.anyImpact = true;

        float iterMaxPen = 0.0f;
        for (const RecoveryContact& c : s_contacts)
            iterMaxPen = std::max(iterMaxPen, c.penetration);
        // Report the residual of the latest iteration, not the peak: it is the
        // penetration the actor is left with after correction.
        result.maxPenetration = iterMaxPen;

        s_allContacts.insert(s_allContacts.end(), s_contacts.begin(), s_contacts.end());

        if (iterMaxPen <= kSlop)
            break;

        glm::vec3 correction = solveBatchedCorrection(
            s_contacts, kSlop, nullptr, nullptr, desiredMovement,
            player.pos + accumulated);
        if (!isFiniteVec3(correction))
            break;

        const float len = glm::length(correction);
        if (len < 1e-5f)
            break;
        const float maxCorrection = std::max(
            kMaxCorrection,
            glm::length(desiredMovement) + MOVEMENT_FEATURE_SMOOTHNESS + kSlop);
        if (len > maxCorrection)
            correction *= maxCorrection / len;

        accumulated += correction;
    }

    if (!s_allContacts.empty())
    {
        player.pos += accumulated;
        player.updateModelWorldTransforms();
    }

    if (s_allContacts.empty())
    {
        result.correctedPos = player.pos;
        return false;
    }

    // One manifold for the whole actor.
    s_manifold.clear();
    mergeContactsByNormal(s_allContacts, s_manifold);
    removeTouchingFaceSeams(s_manifold, desiredMovement);
    collapseCloseFeatureContacts(s_manifold, desiredMovement);
    std::vector<RecoveryContact>& manifold = s_manifold;

    // Strongest impact first, so the dominant surface owns the response and any
    // later rate limiting cannot mute it.
    std::sort(manifold.begin(), manifold.end(),
        [](const RecoveryContact& a, const RecoveryContact& b) {
            const float ia = a.penetration + glm::length(a.sweepDelta);
            const float ib = b.penetration + glm::length(b.sweepDelta);
            return ia > ib;
        });

    // Grounding comes from the actor's own geometry: the lowest point of the
    // desired pose. A walkable normal whose contact is near that lowest point is
    // ground; a walkable contact high on the body (a hand on a ledge) is not.
    const std::vector<ActorCollisionMesh> finalMeshes =
        collectActorCollisionMeshes(player);
    const float lowestZ = desiredLowestZ(finalMeshes);

    for (const RecoveryContact& c : manifold)
    {
        ActorWorldContact wc;
        wc.normal = c.normal;
        wc.point = c.point;
        wc.impactVelocity = c.sweepDelta;
        wc.penetration = c.penetration;
        wc.timeOfImpact = c.timeOfImpact;
        wc.worldTriangle = c.triangleIndex;
        wc.actorPart = c.label;
        wc.entityId = c.entityId;
        wc.surfaceVelocity = c.surfaceVelocity;
        result.contacts.push_back(wc);

        glm::vec3 responseNormal = manifoldResponseNormal(c);
        const bool walkable = responseNormal.z > MAX_WALKABLE_SLOPE_DOT;
        const bool nearFeet = c.point.z <= lowestZ + 0.15f;
        // Only a numerically flat floor is canonicalized to world-up.  A
        // real walkable slope must keep its oriented surface normal so the
        // shared bounce response can return momentum relative to that slope.
        if (walkable && nearFeet && responseNormal.z > 0.995f)
            responseNormal = glm::vec3(0.0f, 0.0f, 1.0f);

        // Build and write the per-contact diagnostic JSON ONLY when the
        // collision trace category is actually enabled. Constructing this
        // nlohmann::json unconditionally per contact per fixed tick was a large
        // hidden cost (and flooded events.jsonl with millions of records).
        const bool traceContact = StructuredLogger::instance().shouldLog(
            StructuredCategory::Collision, StructuredLevel::Trace);
        nlohmann::json contactFields;
        if (traceContact)
        {
            contactFields = {
                {"actor_position", vec3Json(player.pos)},
                {"actor_velocity_before", vec3Json(player.vel)},
                {"intended_movement", vec3Json(desiredMovement)},
                {"remaining_movement_before", vec3Json(result.remainingMovement)},
                {"contact_index", static_cast<int>(result.contacts.size() - 1)},
                {"triangle_index", c.triangleIndex},
                {"actor_part", c.label ? c.label : ""},
                {"entity_id", c.entityId},
                {"contact_point", vec3Json(c.point)},
                {"depenetration_normal", vec3Json(c.normal)},
                {"response_normal", vec3Json(responseNormal)},
                {"surface_normal", vec3Json(c.surfaceNormal)},
                {"penetration", c.penetration},
                {"time_of_impact", c.timeOfImpact},
                {"walkable", walkable},
                {"near_feet", nearFeet}
            };
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Trace,
                "collision.contact.before_response", "slope-edge-investigation",
                "fixed-tick actor-triangle contact before velocity response",
                static_cast<uint32_t>(player.movementSimulationTick), contactFields,
                __FILE__, __LINE__, __FUNCTION__);
        }

        player.ground.realWorldContactThisFrame = true;
        player.ground.hasWorldContact = true;
        player.ground.worldContactLostTimer = 0.033f;

        if (walkable && nearFeet)
        {
            result.grounded = true;
            appendPlayerMovementContactForNormal(
                player, true, false, responseNormal, c.point, c.penetration, c.triangleIndex);
        }
        else
        {
            appendPlayerMovementContactForNormal(
                player, false, false, responseNormal, c.point, c.penetration, c.triangleIndex);
        }

        // Triangle geometry is authoritative for contact detection and
        // depenetration. The shared collision config now owns the response:
        // bounce.enabled=false projects inward velocity only; true restores
        // the authoritative limb/weapon rebound and applies it to the root
        // player velocity. Moving-entity support carry is handled separately
        // below from surfaceVelocity; explosions and weapon forces remain
        // separate gameplay impulses.
        const bool dynamicEntity = c.entityId != 0 && c.surfaceMass > 0.0f;
        if (dynamicEntity)
            PhysicalEntitySystem::instance().applyPlayerContactPush(
                c.entityId, player, c.point, responseNormal);
        if (dynamicEntity && CollisionConfig::instance().bounceEnabled())
        {
            constexpr float kPlayerMass = 80.0f;
            const float objectMass = std::max(c.surfaceMass, 0.0001f);
            const glm::vec3 incoming = player.vel + player.externalImpulse;
            const float normalSpeed = glm::dot(incoming, responseNormal);
            if (normalSpeed < 0.0f)
            {
                const float restitution = glm::clamp(c.surfaceRestitution, 0.0f, 1.0f);
                const float impulse = -(1.0f + restitution) * normalSpeed /
                    (1.0f / kPlayerMass + 1.0f / objectMass);
                player.vel += responseNormal * (impulse / kPlayerMass);
            }
            else
            {
                projectVelocityAgainstNormal(player, responseNormal);
            }
        }
        else
        {
            // Weapon contacts block movement but do not launch the player from
            // the weapon's sweep velocity. The weapon's JSON "player_bounce"
            // (default 0) scales the shared bounce for this contact only.
            const bool weaponContact =
                c.label && std::strcmp(c.label, "weapon") == 0;
            const float bounceScale = weaponContact
                ? player.weaponCollisionDebug.playerBounce
                : 1.0f;
            respondVelocityAgainstNormal(player, responseNormal,
                                         actorSweepVelocity(c.sweepDelta, dt), true,
                                         c.penetration, c.label, c.triangleIndex,
                                         bounceScale);
        }

        // Sliding: strip the blocked component from the intended move.
        const float vn = glm::dot(result.remainingMovement, responseNormal);
        if (vn < 0.0f)
            result.remainingMovement -= responseNormal * vn;

        if (traceContact)
        {
            nlohmann::json afterFields = contactFields;
            afterFields["actor_position_after"] = vec3Json(player.pos);
            afterFields["actor_velocity_after"] = vec3Json(player.vel);
            afterFields["remaining_movement_after"] = vec3Json(result.remainingMovement);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Trace,
                "collision.contact.after_response", "slope-edge-investigation",
                "fixed-tick actor-triangle contact after velocity response",
                static_cast<uint32_t>(player.movementSimulationTick), afterFields,
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    result.correctedPos = player.pos;
    result.solveMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - solveStart).count();
    // The solve summary is a debug record; only build the JSON when enabled.
    if (StructuredLogger::instance().shouldLog(StructuredCategory::Collision,
                                               StructuredLevel::Verbose))
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Verbose,
            "collision.solve.summary", "slope-edge-investigation",
            "actor-triangle solve completed",
            static_cast<uint32_t>(player.movementSimulationTick),
            {{"position_before", vec3Json(result.startPos)},
             {"position_after", vec3Json(player.pos)},
             {"velocity_before", vec3Json(velocityBeforeSolve)},
             {"velocity_after", vec3Json(player.vel)},
             {"desired_movement", vec3Json(desiredMovement)},
             {"remaining_movement", vec3Json(result.remainingMovement)},
             {"contact_count", result.contacts.size()},
             {"iterations", result.iterations},
             {"max_penetration", result.maxPenetration},
             {"grounded", result.grounded},
             {"candidates", result.candidates},
             {"large_triangles", result.largeTriangles},
             {"query_box", vec3Json(result.queryBoxSize)},
             {"candidate_pairs", gActorNarrowphase.candidatePairs},
             {"triangle_tests", gActorNarrowphase.triangleTests},
             {"rounded_feature_calls", gActorNarrowphase.roundedFeatureCalls},
             {"solve_ms", result.solveMs}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    commitActorCollisionMeshes(player);
    return true;
}

bool runActorTriangleCollisionStep(
    Player& player,
    const World& world,
    bool& groundedThisFrame,
    float dt)
{
    if (world.collisionMesh.triangles.empty())
        return false;
    // Without body triangles the solver has no geometry; let the legacy path
    // handle this actor instead of freezing it.
    if (player.physicalBody.parts.empty())
        return false;

    const auto stepStart = std::chrono::steady_clock::now();
    glm::vec3 totalMove = (player.vel + player.externalImpulse) * dt;
    const float maxZStep = PLAYER_RADIUS;
    if (totalMove.z < -maxZStep)
        totalMove.z = -maxZStep;

    // Initialize the JSON weapon collider before collecting actor meshes.
    // Without this call, weaponCollisionDebug.valid remains false for the
    // first active tick and the render-mesh triangle fallback is mistakenly
    // added to the authoritative actor query, causing large FPS spikes.
    recomputeWeaponCapsule(player);

    // Keep the fallback weapon mesh in sync with the equipped weapon. When the
    // JSON collider is valid, collectActorCollisionMeshes excludes this mesh.
    ensureActorWeaponColliderMeshFromEquipped(player);

    // Apply the desired pose: previous = safe, world = desired.
    player.pos += totalMove;
    player.updateModelWorldTransforms();

    ActorTriangleCollisionResult result;
    const std::vector<PhysicalEntity>& entities =
        PhysicalEntitySystem::instance().entities();
    solveActorTriangleCollision(player, world, totalMove, result, &entities, dt);

    if (result.grounded)
        groundedThisFrame = true;
    else if (player.ground.hasWorldContact &&
             std::fabs(totalMove.z) <= kNearGroundVerticalSpeed &&
             std::fabs(player.vel.z) <= kNearGroundVerticalSpeed)
    {
        // Preserve grounded state across tiny rounded-feature/manifold gaps.
        // A jump or down-dash has a larger vertical velocity and will not use
        // this tolerance.
        player.vel.z = 0.0f;
        groundedThisFrame = true;
        player.ground.hasWorldContact = true;
        player.ground.worldContactLostTimer = 0.033f;
    }

    // ── Moving-support carry ──────────────────────────────
    // When the actor is grounded on a moving physical entity, carry it with the
    // entity this tick. When support ends, hand the entity's velocity to the
    // actor so jumping off preserves it instead of silently dropping it.
    uint32_t supportId = 0;
    glm::vec3 supportVelocity(0.0f);
    if (result.grounded)
    {
        for (const ActorWorldContact& c : result.contacts)
        {
            if (c.entityId != 0 && c.normal.z > MAX_WALKABLE_SLOPE_DOT)
            {
                supportId = c.entityId;
                supportVelocity = c.surfaceVelocity;
                break;
            }
        }
    }

    CollisionState& collisionState = player.collision;
    if (collisionState.supportEntityId != 0 && supportId == 0)
        player.vel += collisionState.supportVelocity;

    if (supportId != 0 && glm::dot(supportVelocity, supportVelocity) > 0.0f)
    {
        player.pos += supportVelocity * dt;
        player.updateModelWorldTransforms();
    }

    collisionState.supportEntityId = supportId;
    collisionState.supportVelocity = supportVelocity;

    // Dynamic physical entities receive the actor's horizontal contact impulse
    // after the actor manifold has been solved. This preserves one collision
    // owner while allowing a player to push a crate on the next fixed tick.
    PhysicalEntitySystem::instance().applyPlayerPush(player, dt);

    // Body-contact spark: same boundary as the legacy body phase, fed by the
    // solver's final contact point. The weapon label does not spawn a body spark.
    if (player.bodySparkTick != player.movementSimulationTick)
    {
        for (const ActorWorldContact& c : result.contacts)
        {
            if (c.actorPart && std::strcmp(c.actorPart, "weapon") == 0)
                continue;
            EffectPart* spawned = EffectPartSystem::instance()
                .spawnBodyContactSpark(player.pos, c.point, player.vel, 0.1f);
            if (spawned)
            {
                player.bodySparkTick = player.movementSimulationTick;
                break;
            }
        }
    }

    // Total collision-frame time for the active actor path: broadphase query +
    // narrowphase/solve + support carry + entity push + spark. Structured only,
    // matching the solve summary, so it is a bounded diagnostic record.
    result.totalMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - stepStart).count();
    if (StructuredLogger::instance().shouldLog(StructuredCategory::Collision,
                                               StructuredLevel::Verbose))
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Verbose,
            "collision.solve.frame", "cylinder-static-broadphase",
            "actor collision frame total (query + solve + post)",
            static_cast<uint32_t>(player.movementSimulationTick),
            {{"query_box", vec3Json(result.queryBoxSize)},
             {"candidates", result.candidates},
             {"large_triangles", result.largeTriangles},
             {"triangle_tests", gActorNarrowphase.triangleTests},
             {"solve_ms", result.solveMs},
             {"total_ms", result.totalMs},
             {"grounded", result.grounded},
             {"iterations", result.iterations}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    return true;
}

// ── Deterministic self-test ────────────────────────────────────────

namespace {

void addQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
{
    auto add = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) {
        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(n);
        if (len < 1e-6f)
            return;
        CollisionTriangle t;
        t.a = p0; t.b = p1; t.c = p2; t.normal = n / len;
        world.collisionMesh.triangles.push_back(t);
    };
    add(a, b, c);
    add(a, c, d);
}

World makeFloorWorld()
{
    World w;
    addQuad(w, {-8,-8,0}, {8,-8,0}, {8,8,0}, {-8,8,0});
    buildCollisionChunks(w, nullptr);
    return w;
}

World makeWallWorld()
{
    World w;
    // Plane x = 0.5, normal (-1,0,0).
    addQuad(w, {0.5f,-2,0}, {0.5f,-2,4}, {0.5f,2,4}, {0.5f,2,0});
    buildCollisionChunks(w, nullptr);
    return w;
}

World makeCornerWorld()
{
    World w = makeFloorWorld();
    addQuad(w, {0.5f,-2,0}, {0.5f,-2,4}, {0.5f,2,4}, {0.5f,2,0});
    buildCollisionChunks(w, nullptr);
    return w;
}

void buildBoxTriangles(std::vector<CollisionTriangle>& out,
                       glm::vec3 center, glm::vec3 half);

World makeGiantWallWorld(float x)
{
    World w;
    addQuad(w, {x,-40,0}, {x,-40,80}, {x,40,80}, {x,40,0});  // normal (-1,0,0)
    buildCollisionChunks(w, nullptr);
    return w;
}

World makeCrateWorld()
{
    World w = makeFloorWorld();
    std::vector<CollisionTriangle> box;
    buildBoxTriangles(box, glm::vec3(1.0f, 0.0f, 0.5f), glm::vec3(0.5f, 0.5f, 0.5f));
    w.collisionMesh.triangles.insert(w.collisionMesh.triangles.end(),
                                     box.begin(), box.end());
    buildCollisionChunks(w, nullptr);
    return w;
}

// One rigid box body part attached to the root. Its node-local triangles form a
// box centered at (0,0,halfZ) so the bottom face sits at root z = 0.
void buildBoxTriangles(std::vector<CollisionTriangle>& out,
                       glm::vec3 center, glm::vec3 half)
{
    const glm::vec3 mn = center - half;
    const glm::vec3 mx = center + half;
    const glm::vec3 v[8] = {
        mn,
        {mx.x, mn.y, mn.z},
        {mx.x, mx.y, mn.z},
        {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z},
        {mx.x, mn.y, mx.z},
        {mx.x, mx.y, mx.z},
        {mn.x, mx.y, mx.z},
    };
    const int quads[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {3,0,4,7}
    };
    for (int q = 0; q < 6; ++q) {
        const int* ix = quads[q];
        auto push = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            if (len < 1e-6f) return;
            CollisionTriangle t; t.a=a; t.b=b; t.c=c; t.normal=n/len;
            out.push_back(t);
        };
        push(v[ix[0]], v[ix[1]], v[ix[2]]);
        push(v[ix[0]], v[ix[2]], v[ix[3]]);
    }
}

void setupBoxActor(Player& p, glm::vec3 pos, float halfXY, float halfZ)
{
    p.pos = pos;
    p.yaw = 0.0f;

    PhysicalBodyPart part;
    part.name = "torso";
    part.nodeIndex = 0;
    Collider& col = part.collider;
    col.name = "torso";
    col.localMin = glm::vec3(0.0f, 0.0f, halfZ) - glm::vec3(halfXY, halfXY, halfZ);
    col.localMax = glm::vec3(0.0f, 0.0f, halfZ) + glm::vec3(halfXY, halfXY, halfZ);
    buildBoxTriangles(col.triangles, glm::vec3(0.0f, 0.0f, halfZ),
                      glm::vec3(halfXY, halfXY, halfZ));

    p.physicalBody.parts.clear();
    p.physicalBody.parts.push_back(part);
    p.nodes.resize(1);
    p.restLocalTransforms.assign(1, glm::mat4(1.0f));
    p.perfectPoseSkeleton.nodes.resize(1);
    p.perfectPoseSkeleton.nodes[0].name = "root";
    p.perfectPoseSkeleton.nodes[0].parent = -1;
    p.perfectPoseSkeleton.nodes[0].localTransform = glm::mat4(1.0f);
    p.perfectPoseSkeleton.restLocalTransforms.assign(1, glm::mat4(1.0f));
    p.updateModelWorldTransforms();
}

} // namespace

bool actorTriangleSolverSelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    // 1. Floor rest: the body bottom is pushed out of the floor and grounded.
    {
        World world = makeFloorWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(0.0f, 0.0f, 0.01f), 0.4f, 0.5f);
        const glm::vec3 move(0.0f, 0.0f, -0.05f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        const bool hit = solveActorTriangleCollision(p, world, move, r);
        check(hit, "actor triangle solver handles no overlap");
        check(r.grounded, "floor rest is grounded");
        check(r.correctedPos.z > -0.02f, "actor is not left inside the floor");
        check(r.maxPenetration <= MOVEMENT_FEATURE_SMOOTHNESS + 0.01f,
              "floor penetration is within rounded-feature tolerance");
    }

    // 2. High-speed wall: swept contact stops before the wall and preserves
    // tangential momentum (and bounces when configured).
    {
        World world = makeWallWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, -0.5f), 0.4f, 0.5f);
        p.collision.bounceCooldown = 0.0f;
        p.vel = glm::vec3(5.0f, 3.0f, 0.0f);
        const glm::vec3 move(0.8f, 0.0f, 0.0f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        const bool hit = solveActorTriangleCollision(p, world, move, r);
        check(hit, "high-speed wall produces a contact");
        check(r.correctedPos.x <= 0.12f, "actor is stopped before the wall");
        check(std::fabs(p.vel.y - 3.0f) < 0.05f, "tangential momentum is preserved");
        check(p.vel.x <= 0.05f, "wall contact removes inward velocity without bounce launch");
        check(glm::length(p.vel) > 0.01f, "collision does not zero velocity");
    }

    // 3. Leaving an old contact: the previous pose only touched the wall, the
    // current pose moved away, so there must be no wall contact.
    {
        World world = makeWallWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(0.1f, 0.0f, -0.5f), 0.4f, 0.5f);   // right face exactly 0.5
        const glm::vec3 move(-0.5f, 0.0f, 0.0f);
        p.pos += move;                                                 // right face -0.1
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        bool wallContact = false;
        for (const ActorWorldContact& c : r.contacts)
            if (std::fabs(c.normal.x) > 0.5f) wallContact = true;
        check(!wallContact, "actor is not glued to an old wall contact");
    }

    // 4. Corner: floor and wall produce two distinct surface normals.
    {
        World world = makeCornerWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, 0.01f), 0.4f, 0.5f);
        const glm::vec3 move(0.8f, 0.0f, -0.05f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        bool up = false, side = false;
        for (const ActorWorldContact& c : r.contacts) {
            if (c.normal.z > 0.5f) up = true;
            if (c.normal.x < -0.5f) side = true;
        }
        check(up && side, "corner produces floor and wall contacts at once");
    }

    // 5. Giant wall 30 m away: no contact, no false positive.
    {
        World world = makeGiantWallWorld(30.0f);
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, -0.5f), 0.4f, 0.5f);
        const glm::vec3 move(0.5f, 0.0f, 0.0f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        check(r.contacts.empty(), "giant wall 30m away produces no contact");
    }

    // 5b. Long wall hit near its middle: the persistent world tree returns the
    // long triangle only because its own AABB overlaps the actor query, and the
    // exact triangle contact is unchanged.
    {
        World world = makeGiantWallWorld(0.5f);
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, -0.5f), 0.4f, 0.5f);
        const glm::vec3 move(0.8f, 0.0f, 0.0f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        bool wallHit = false;
        for (const ActorWorldContact& c : r.contacts)
            if (c.normal.x < -0.5f) wallHit = true;
        check(wallHit, "long wall middle produces a blocking contact");
        check(r.correctedPos.x <= 0.12f, "long wall middle stops the actor");
    }

    // 6. Deeply embedded actor is depenetrated and grounded.
    {
        World world = makeFloorWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(0.0f, 0.0f, -0.5f), 0.4f, 0.5f);  // bottom 0.5 in floor
        const glm::vec3 move(0.0f, 0.0f, 0.0f);
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        check(r.grounded, "deeply embedded actor grounds");
        check(r.correctedPos.z > -0.05f, "deeply embedded actor is depenetrated");
        check(r.correctedPos.z >= -0.05f,
              "embedded penetration is resolved");
    }

    // 7. Crate blocks the actor.
    {
        World world = makeCrateWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, 0.01f), 0.4f, 0.5f);  // right face -0.1
        const glm::vec3 move(1.2f, 0.0f, 0.0f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        bool crateHit = false;
        for (const ActorWorldContact& c : r.contacts)
            if (c.normal.x < -0.5f) crateHit = true;
        check(crateHit, "crate produces a blocking contact");
        check(r.correctedPos.x <= 0.15f, "actor stops at the crate");
    }

    // 8. Thin-wall crossing at high speed must not tunnel.
    {
        World world = makeWallWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-1.0f, 0.0f, -0.5f), 0.4f, 0.5f);
        const glm::vec3 move(3.0f, 0.0f, 0.0f);   // desired pose fully past the wall
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        bool wallHit = false;
        for (const ActorWorldContact& c : r.contacts)
            if (c.normal.x < -0.5f) wallHit = true;
        check(wallHit, "high-speed actor detects a thin wall crossing");
        check(r.correctedPos.x <= 0.15f, "high-speed actor does not tunnel through");
    }

    // 9. Weapon render-mesh triangles collide through the same solver.
    {
        World world = makeWallWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-3.0f, 0.0f, 2.0f), 0.2f, 0.2f);  // body clear of wall
        p.weaponColliderMesh.clear();
        buildBoxTriangles(p.weaponColliderMesh, glm::vec3(0.0f), glm::vec3(0.1f));
        p.weaponColliderMeshPath = "test-weapon";
        const glm::mat4 wSafe = glm::translate(glm::mat4(1.0f), glm::vec3(0.2f, 0.0f, 0.5f));
        p.weaponModelTransform = wSafe;
        p.previousWeaponModelTransform = wSafe;

        p.pos += glm::vec3(1.0f, 0.0f, 0.0f);
        p.updateModelWorldTransforms();
        p.weaponModelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.2f, 0.0f, 0.5f));

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, glm::vec3(1.0f, 0.0f, 0.0f), r);
        bool weaponHit = false;
        for (const ActorWorldContact& c : r.contacts)
            if (c.actorPart && std::strcmp(c.actorPart, "weapon") == 0) weaponHit = true;
        check(weaponHit, "weapon render-mesh triangles produce a contact");
        // Weapon contacts block but must not launch the player (player_bounce 0):
        // the weapon's own sweep velocity is not player velocity.
        check(std::fabs(p.vel.x) < 0.05f,
              "weapon contact does not launch the player");
    }

    // 10. 200 m/s impact: tangential momentum is preserved, but the normal
    // component is projected away instead of reflecting the actor outward.
    {
        World world = makeWallWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(-0.5f, 0.0f, -0.5f), 0.4f, 0.5f);
        p.collision.bounceCooldown = 0.0f;
        p.vel = glm::vec3(200.0f, 120.0f, 0.0f);
        const glm::vec3 move(0.8f, 0.0f, 0.0f);
        p.pos += move;
        p.updateModelWorldTransforms();

        ActorTriangleCollisionResult r;
        solveActorTriangleCollision(p, world, move, r);
        const bool bounceEnabled = CollisionConfig::instance().bounceEnabled();
        check(bounceEnabled ? p.vel.x < -0.05f : std::fabs(p.vel.x) < 0.05f,
              bounceEnabled ? "200 m/s normal momentum rebounds when bounce is enabled"
                            : "200 m/s normal momentum is removed when bounce is disabled");
        check(std::fabs(p.vel.y - 120.0f) < 0.5f, "200 m/s tangential momentum is preserved");
        check(glm::length(p.vel) > 1.0f, "200 m/s impact does not zero velocity");
    }

    // 11. Active-path wrapper: a falling actor lands and grounds through
    // runActorTriangleCollisionStep, the function the opt-in pipeline calls.
    {
        World world = makeFloorWorld();
        Player p(false);
        setupBoxActor(p, glm::vec3(0.0f, 0.0f, 2.0f), 0.4f, 0.5f);
        p.dash.dashAvailable = true;

        bool grounded = false;
        constexpr float dt = 1.0f / 60.0f;
        for (int i = 0; i < 240 && !grounded; ++i)
        {
            p.vel.z -= 58.0f * dt;   // gravity
            grounded = false;
            runActorTriangleCollisionStep(p, world, grounded, dt);
        }
        check(grounded, "active-path wrapper lands and grounds");
        check(p.pos.z > -0.05f, "active-path wrapper does not sink through the floor");
    }

    if (outSummary)
        *outSummary = report;
    return ok;
}
