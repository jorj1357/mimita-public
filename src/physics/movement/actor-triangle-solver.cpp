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
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/config.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/physics-collision-shared.h"
#include "map/map-loader-collision.h"
#include "world/world.h"
#include "entities/player.h"
#include "effects/effect-part.h"

namespace {

constexpr float kSlop = 0.01f;
constexpr float kMaxCorrection = 2.0f;
constexpr int kMaxIterations = 4;

bool finiteAabb(const AABB& a)
{
    return std::isfinite(a.min.x) && std::isfinite(a.min.y) && std::isfinite(a.min.z) &&
           std::isfinite(a.max.x) && std::isfinite(a.max.y) && std::isfinite(a.max.z) &&
           a.max.x >= a.min.x && a.max.y >= a.min.y && a.max.z >= a.min.z;
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
            if (glm::dot(existing.normal, c.normal) >= 0.95f)
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
    ActorTriangleCollisionResult& result)
{
    result = ActorTriangleCollisionResult{};
    result.startPos = player.pos;
    result.correctedPos = player.pos;
    result.remainingMovement = desiredMovement;

    if (world.collisionMesh.triangles.empty())
        return false;

    // Capture the safe pose ONCE. The safe previous transforms must not be
    // overwritten between correction iterations, or a depenetration that moves
    // the actor up across a floor would look like a fresh downward sweep and
    // flip the normal. Corrections accumulate and are applied once at the end.
    const std::vector<ActorCollisionMesh> baseMeshes =
        collectActorCollisionMeshes(player);
    if (baseMeshes.empty())
        return false;

    AABB box = makeSweptActorMeshAABB(baseMeshes, desiredMovement);
    if (!finiteAabb(box))
        return false;
    box.min -= glm::vec3(0.02f);
    box.max += glm::vec3(0.02f);

    std::vector<int> candidates;
    appendChunkTrianglesForAABB(world, box, COLLISION_GATHER_EXPANSION,
                                candidates, "actorTriangleSolve");
    result.candidates = (int)candidates.size();

    std::vector<RecoveryContact> allContacts;
    glm::vec3 accumulated(0.0f);

    for (int iter = 0; iter < kMaxIterations && !candidates.empty(); ++iter)
    {
        // Shift the whole capture (safe + desired) by the accumulated
        // correction so the sweep direction stays the tick's real motion.
        std::vector<ActorCollisionMesh> meshes = baseMeshes;
        const glm::mat4 shift = glm::translate(glm::mat4(1.0f), accumulated);
        for (ActorCollisionMesh& m : meshes)
        {
            m.previousTransform = shift * m.previousTransform;
            m.desiredTransform = shift * m.desiredTransform;
        }

        const AABB poseBox = makeSweptActorMeshAABB(meshes, glm::vec3(0.0f));
        const glm::vec3 refPoint = (poseBox.min + poseBox.max) * 0.5f;

        std::vector<RecoveryContact> contacts =
            collectActorMeshContacts(world, meshes, candidates, refPoint);
        if (contacts.empty())
            break;

        result.iterations = iter + 1;
        result.anyImpact = true;

        float iterMaxPen = 0.0f;
        for (const RecoveryContact& c : contacts)
            iterMaxPen = std::max(iterMaxPen, c.penetration);
        // Report the residual of the latest iteration, not the peak: it is the
        // penetration the actor is left with after correction.
        result.maxPenetration = iterMaxPen;

        allContacts.insert(allContacts.end(), contacts.begin(), contacts.end());

        if (iterMaxPen <= kSlop)
            break;

        glm::vec3 correction = solveBatchedCorrection(
            contacts, kSlop, nullptr, nullptr, desiredMovement,
            player.pos + accumulated);
        if (!isFiniteVec3(correction))
            break;

        const float len = glm::length(correction);
        if (len < 1e-5f)
            break;
        if (len > kMaxCorrection)
            correction *= kMaxCorrection / len;

        accumulated += correction;
    }

    if (!allContacts.empty())
    {
        player.pos += accumulated;
        player.updateModelWorldTransforms();
    }

    if (allContacts.empty())
    {
        result.correctedPos = player.pos;
        return false;
    }

    // One manifold for the whole actor.
    std::vector<RecoveryContact> manifold;
    mergeContactsByNormal(allContacts, manifold);

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
        result.contacts.push_back(wc);

        const bool walkable = c.normal.z > MAX_WALKABLE_SLOPE_DOT;
        const bool nearFeet = c.point.z <= lowestZ + 0.15f;

        player.ground.realWorldContactThisFrame = true;
        player.ground.hasWorldContact = true;
        player.ground.worldContactLostTimer = 0.033f;

        if (walkable && nearFeet)
        {
            result.grounded = true;
            appendPlayerMovementContactForNormal(
                player, true, false, c.normal, c.point, c.penetration, c.triangleIndex);
        }
        else
        {
            appendPlayerMovementContactForNormal(
                player, false, false, c.normal, c.point, c.penetration, c.triangleIndex);
        }

        // One response per distinct surface. The body/weapon part velocity is
        // passed so a moving limb or weapon pushes the whole body. The contact
        // identity is passed so a duplicate of the same contact is deduped
        // instead of relying on a global cooldown.
        respondVelocityAgainstNormal(player, c.normal, c.sweepDelta, true,
                                     c.penetration, c.label, c.triangleIndex);

        // Sliding: strip the blocked component from the intended move.
        const float vn = glm::dot(result.remainingMovement, c.normal);
        if (vn < 0.0f)
            result.remainingMovement -= c.normal * vn;
    }

    result.correctedPos = player.pos;
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

    glm::vec3 totalMove = (player.vel + player.externalImpulse) * dt;
    const float maxZStep = PLAYER_RADIUS;
    if (totalMove.z < -maxZStep)
        totalMove.z = -maxZStep;

    // Keep the weapon collider mesh in sync with the equipped weapon.
    ensureActorWeaponColliderMeshFromEquipped(player);

    // Apply the desired pose: previous = safe, world = desired.
    player.pos += totalMove;
    player.updateModelWorldTransforms();

    ActorTriangleCollisionResult result;
    solveActorTriangleCollision(player, world, totalMove, result);

    if (result.grounded)
        groundedThisFrame = true;

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
        check(r.maxPenetration <= 0.05f, "floor penetration is within tolerance");
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
        check(r.maxPenetration <= 0.05f, "embedded penetration is resolved");
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
    }

    // 10. 200 m/s impact: tangential momentum preserved, normal reflected by
    // restitution, and velocity never zeroed by the collision.
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
        check(p.vel.x < 0.0f, "200 m/s normal momentum is reflected");
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

