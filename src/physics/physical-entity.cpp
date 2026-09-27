// 2026-09-27
/* purpose
* Implement the one generic moving physical entity and its small owner, plus the
* actor-vs-entity contact pass that reuses the single actor-triangle contact
* routine. Proves moving-support carry and velocity inheritance deterministically.
* Does NOT render, play effects, send packets, or own gameplay rules.
* Does NOT delete or replace any existing collision owner.
*/

#include "physics/physical-entity.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/config.h"
#include "physics/movement/actor-triangle-solver.h"
#include "physics/movement/physics-collision-shared.h"
#include "world/world.h"
#include "entities/player.h"
#include "map/map-loader-collision.h"
#include "debug/debug-visuals.h"
#include "camera.h"

PhysicalEntitySystem& PhysicalEntitySystem::instance()
{
    static PhysicalEntitySystem system;
    return system;
}

void PhysicalEntitySystem::clear()
{
    mEntities.clear();
    mNextId = 1;
}

uint32_t PhysicalEntitySystem::add(
    const std::vector<CollisionTriangle>& localTriangles,
    const glm::mat4& transform,
    PhysicalEntityMotion motion,
    uint32_t materialId)
{
    PhysicalEntity e;
    e.id = mNextId++;
    e.localTriangles = localTriangles;
    e.transform = transform;
    e.previousTransform = transform;
    e.motion = motion;
    e.materialId = materialId;
    mEntities.push_back(std::move(e));
    return mEntities.back().id;
}

void PhysicalEntitySystem::beginTick()
{
    for (PhysicalEntity& e : mEntities)
        e.previousTransform = e.transform;
}

PhysicalEntity* PhysicalEntitySystem::moveKinematic(uint32_t id,
                                                    const glm::mat4& transform,
                                                    float dt)
{
    PhysicalEntity* e = find(id);
    if (!e)
        return nullptr;
    const glm::vec3 previous(e->transform[3]);
    const glm::vec3 next(transform[3]);
    if (dt > 0.0f)
        e->velocity = (next - previous) / dt;
    else
        e->velocity = glm::vec3(0.0f);
    e->transform = transform;
    return e;
}

PhysicalEntity* PhysicalEntitySystem::find(uint32_t id)
{
    for (PhysicalEntity& e : mEntities)
        if (e.id == id)
            return &e;
    return nullptr;
}

void PhysicalEntitySystem::advanceKinematics(float dt)
{
    if (dt <= 0.0f)
        return;
    for (PhysicalEntity& e : mEntities)
    {
        if (e.motion != PhysicalEntityMotion::Kinematic)
            continue;
        if (glm::dot(e.velocity, e.velocity) <= 0.0f)
            continue;
        e.previousTransform = e.transform;
        e.transform[3] += glm::vec4(e.velocity * dt, 0.0f);
    }
}

void buildBoxCollisionTriangles(std::vector<CollisionTriangle>& out,
                                const glm::vec3& center,
                                const glm::vec3& half)
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
        {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {1, 2, 6, 5}, {3, 0, 4, 7}
    };
    for (int q = 0; q < 6; ++q)
    {
        const int* ix = quads[q];
        auto push = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            if (len < 1e-6f)
                return;
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c; t.normal = n / len;
            out.push_back(t);
        };
        push(v[ix[0]], v[ix[1]], v[ix[2]]);
        push(v[ix[0]], v[ix[2]], v[ix[3]]);
    }
}

namespace {

AABB entityWorldAABB(const PhysicalEntity& e)
{
    AABB box;
    box.min = glm::vec3(1e30f);
    box.max = glm::vec3(-1e30f);
    for (const CollisionTriangle& t : e.localTriangles)
        for (const glm::vec3& v : {t.a, t.b, t.c})
        {
            const glm::vec3 w = glm::vec3(e.transform * glm::vec4(v, 1.0f));
            box.min = glm::min(box.min, w);
            box.max = glm::max(box.max, w);
        }
    return box;
}

bool aabbOverlapsPadded(const AABB& a, const AABB& b, float pad)
{
    return a.min.x - pad <= b.max.x && a.max.x + pad >= b.min.x &&
           a.min.y - pad <= b.max.y && a.max.y + pad >= b.min.y &&
           a.min.z - pad <= b.max.z && a.max.z + pad >= b.min.z;
}

} // namespace

std::vector<EntityActorContact> collectActorEntityContacts(
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<PhysicalEntity>& entities,
    const glm::vec3& actorPos)
{
    std::vector<EntityActorContact> out;
    if (meshes.empty() || entities.empty())
        return out;

    const AABB actorBox = makeSweptActorMeshAABB(meshes, glm::vec3(0.0f));
    constexpr float kPad = 0.05f;

    for (const PhysicalEntity& e : entities)
    {
        if (!e.collidesWithActors || e.localTriangles.empty())
            continue;
        if (!aabbOverlapsPadded(actorBox, entityWorldAABB(e), kPad))
            continue;

        // A temporary world view of the entity's current world-space triangles.
        // collectActorMeshContacts owns the one triangle-vs-triangle routine; the
        // entity reuses it instead of duplicating the math.
        World temp;
        temp.collisionMesh.triangles.reserve(e.localTriangles.size());
        for (const CollisionTriangle& lt : e.localTriangles)
        {
            CollisionTriangle wt;
            wt.a = glm::vec3(e.transform * glm::vec4(lt.a, 1.0f));
            wt.b = glm::vec3(e.transform * glm::vec4(lt.b, 1.0f));
            wt.c = glm::vec3(e.transform * glm::vec4(lt.c, 1.0f));
            const glm::vec3 n = glm::cross(wt.b - wt.a, wt.c - wt.a);
            const float len = glm::length(n);
            wt.normal = len > 1e-8f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            temp.collisionMesh.triangles.push_back(wt);
        }

        std::vector<int> candidates(temp.collisionMesh.triangles.size());
        std::iota(candidates.begin(), candidates.end(), 0);

        std::vector<RecoveryContact> contacts =
            collectActorMeshContacts(temp, meshes, candidates, actorPos);
        for (RecoveryContact& c : contacts)
        {
            c.entityId = e.id;
            c.surfaceVelocity = e.velocity;
            out.push_back(EntityActorContact{c, e.id});
        }
    }
    return out;
}

void drawPhysicalEntities(const Camera& camera)
{
    const std::vector<PhysicalEntity>& entities =
        PhysicalEntitySystem::instance().entities();
    for (const PhysicalEntity& e : entities)
    {
        if (e.localTriangles.empty())
            continue;
        const AABB box = entityWorldAABB(e);
        const glm::vec3 center = (box.min + box.max) * 0.5f;
        const glm::vec3 half = (box.max - box.min) * 0.5f;
        const glm::vec4 color =
            (e.motion == PhysicalEntityMotion::Kinematic)
                ? glm::vec4(0.85f, 0.45f, 0.12f, 1.0f)
                : glm::vec4(0.45f, 0.45f, 0.50f, 1.0f);
        DebugVis::drawFilledBox(camera, center, half, color);
        DebugVis::drawWireBox(camera, center, half,
                              glm::vec4(1.0f, 0.9f, 0.2f, 1.0f));
    }
}

// ── Deterministic moving-crate proof ───────────────────────────────

namespace {

void addFloorQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
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

// One rigid box body part whose bottom face sits at the actor root z.
void setupTestBoxActor(Player& p, glm::vec3 pos, float halfXY, float halfZ)
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
    buildBoxCollisionTriangles(col.triangles, glm::vec3(0.0f, 0.0f, halfZ),
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

bool physicalEntitySelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    constexpr float dt = 1.0f / 60.0f;
    const float crateHalf = 0.5f;
    const float crateTop0 = crateHalf * 2.0f;   // local box z range [0, 1]

    World world;
    addFloorQuad(world, {-20, -20, 0}, {20, -20, 0}, {20, 20, 0}, {-20, 20, 0});
    buildCollisionChunks(world, nullptr);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();

    // 1. Standing on a moving crate: the actor is carried and reports support.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, crateHalf),
                                   glm::vec3(crateHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, crateTop0 - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(2.0f, 0.0f, 0.0f);
        glm::vec3 cratePos(0.0f);
        const float startXCrate = cratePos.x;
        bool grounded = false;
        constexpr int kTicks = 30;
        for (int i = 0; i < kTicks; ++i)
        {
            p.vel.z -= 58.0f * dt;   // gravity keeps the actor pressed onto the crate
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            grounded = false;
            runActorTriangleCollisionStep(p, world, grounded, dt);
        }

        const float crateMoved = cratePos.x - startXCrate;
        check(grounded, "actor on moving crate stays grounded");
        check(std::fabs((p.pos.x - 0.0f) - crateMoved) < 0.05f,
              "actor world motion includes crate velocity");
        check(p.collision.supportEntityId == crateId,
              "canonical contact identifies the support entity");
        check(std::fabs(p.collision.supportVelocity.x - crateVel.x) < 1e-4f,
              "support surfaces exposes the entity velocity");
    }

    // 2. Walking adds to the support motion. A wide crate keeps the actor fully
    //    supported while it walks relative to the crate.
    {
        system.clear();
        const float wideHalf = 4.0f;
        const float wideTop = wideHalf * 2.0f;
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, wideHalf),
                                   glm::vec3(wideHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, wideTop - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(2.0f, 0.0f, 0.0f);
        const float walk = 1.0f;
        glm::vec3 cratePos(0.0f);
        const float startX = p.pos.x;
        bool grounded = false;
        constexpr int kTicks = 30;
        for (int i = 0; i < kTicks; ++i)
        {
            // Local walking, as the movement controller would assign it; gravity
            // keeps contact so the actor stays supported.
            p.vel.x = walk;
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            grounded = false;
            runActorTriangleCollisionStep(p, world, grounded, dt);
        }

        const float expected = (crateVel.x + walk) * dt * (float)kTicks;
        check(grounded, "walking actor stays supported");
        check(std::fabs((p.pos.x - startX) - expected) < 0.08f,
              "local walking adds to the support velocity");
    }

    // 3. Jumping preserves the inherited support velocity, and leaving the crate
    //    does not zero it.
    {
        system.clear();
        std::vector<CollisionTriangle> crate;
        buildBoxCollisionTriangles(crate, glm::vec3(0.0f, 0.0f, crateHalf),
                                   glm::vec3(crateHalf));
        const uint32_t crateId = system.add(
            crate, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)),
            PhysicalEntityMotion::Kinematic);

        Player p(false);
        setupTestBoxActor(p, glm::vec3(0.0f, 0.0f, crateTop0 - 0.02f), 0.4f, 0.5f);

        const glm::vec3 crateVel(3.0f, 0.0f, 0.0f);
        glm::vec3 cratePos(0.0f);
        // Settle onto the moving crate.
        for (int i = 0; i < 10; ++i)
        {
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            bool g = false;
            runActorTriangleCollisionStep(p, world, g, dt);
        }
        check(p.collision.supportEntityId == crateId, "actor is settled on the crate");

        // Jump: set an upward velocity from rest (x/y zero to isolate inheritance)
        // and tick until the actor leaves the crate. The departure tick must add
        // the support velocity to the actor's own velocity.
        p.vel = glm::vec3(0.0f, 0.0f, 19.0f);
        float inheritedAfterJump = 0.0f;
        bool airborne = false;
        for (int i = 0; i < 10 && !airborne; ++i)
        {
            p.vel.z -= 58.0f * dt;
            cratePos.x += crateVel.x * dt;
            system.moveKinematic(crateId, glm::translate(glm::mat4(1.0f), cratePos), dt);
            bool g = false;
            runActorTriangleCollisionStep(p, world, g, dt);
            if (!g)
            {
                airborne = true;
                inheritedAfterJump = p.vel.x;
            }
        }
        check(airborne, "jump lifts the actor off the crate");
        check(std::fabs(inheritedAfterJump - crateVel.x) < 0.05f,
              "jumping preserves the inherited support velocity");

        // Leave: remove the crate entirely and tick once. Support must clear but
        // the inherited velocity must survive.
        system.clear();
        const float velBeforeLeave = p.vel.x;
        bool g = false;
        runActorTriangleCollisionStep(p, world, g, dt);
        check(p.collision.supportEntityId == 0, "leaving the crate clears support");
        check(std::fabs(p.vel.x - velBeforeLeave) < 1e-4f,
              "leaving the crate does not zero inherited velocity");
        check(std::fabs(p.vel.x) > 0.1f, "inherited velocity remains after departure");
    }

    system.clear();

    if (outSummary)
        *outSummary = report;
    return ok;
}
