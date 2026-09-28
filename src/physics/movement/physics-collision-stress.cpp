#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "map/map-loader-collision.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "debug/debug-log.h"

static void addStressTriangle(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    if (glm::length(n) < 0.000001f)
        return;

    CollisionTriangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.normal = glm::normalize(n);
    world.collisionMesh.triangles.push_back(tri);
}

static void addStressQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
{
    addStressTriangle(world, a, b, c);
    addStressTriangle(world, a, c, d);
}

static void addStressFloor(World& world)
{
    addStressQuad(world,
        {-8.0f, -8.0f, 0.0f},
        { 8.0f, -8.0f, 0.0f},
        { 8.0f,  8.0f, 0.0f},
        {-8.0f,  8.0f, 0.0f});
}

static void addStressWedge(World& world, float degrees)
{
    const float halfRad = glm::radians(std::max(0.5f, degrees) * 0.5f);
    const float backX = -5.0f;
    const float apexX = 4.0f;
    const float width = std::max(0.08f, std::tan(halfRad) * (apexX - backX));
    const float topZ = 4.0f;

    addStressQuad(world,
        {apexX, 0.0f, 0.0f},
        {backX, width, 0.0f},
        {backX, width, topZ},
        {apexX, 0.0f, topZ});
    addStressQuad(world,
        {backX, -width, 0.0f},
        {apexX,  0.0f, 0.0f},
        {apexX,  0.0f, topZ},
        {backX, -width, topZ});
}

static void addStressCone(World& world)
{
    constexpr int SIDES = 16;
    const float radius = 1.2f;
    const float height = 3.0f;
    glm::vec3 tip(1.5f, 0.0f, height);
    glm::vec3 center(1.5f, 0.0f, 0.0f);

    for (int i = 0; i < SIDES; ++i)
    {
        float a0 = (float)i / (float)SIDES * 6.2831853f;
        float a1 = (float)(i + 1) / (float)SIDES * 6.2831853f;
        glm::vec3 p0(center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius, 0.0f);
        glm::vec3 p1(center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, 0.0f);
        addStressTriangle(world, p0, p1, tip);
    }
}

struct CollisionStressResult
{
    std::string summary;
    float maxPenetration = 0.0f;
    glm::vec3 finalPos{0.0f};
    bool grounded = false;
    bool finite = true;
    bool emergency = false;
};

static CollisionStressResult runCollisionStressCase(const std::string& caseName)
{
    World world;
    addStressFloor(world);

    float speed = 60.0f;
    std::string selected = caseName.empty() ? "wedge5" : caseName;
    glm::vec3 startPos(-4.0f, 0.0f, PLAYER_HEIGHT * 0.5f + 0.05f);
    glm::vec3 startVel(speed, 0.0f, 0.0f);

    if (selected == "floor")
    {
        speed = 18.0f;
        startVel = glm::vec3(speed, 0.0f, 0.0f);
    }
    else if (selected == "floor_drop")
    {
        startPos = glm::vec3(0.0f, 0.0f, 8.0f);
        startVel = glm::vec3(0.0f, 0.0f, -180.0f);
    }
    else if (selected == "corner")
    {
        addStressQuad(world,
            {1.0f, -5.0f, 0.0f},
            {1.0f, -5.0f, 4.0f},
            {1.0f,  5.0f, 4.0f},
            {1.0f,  5.0f, 0.0f});
        addStressQuad(world,
            {-5.0f, 1.0f, 0.0f},
            { 5.0f, 1.0f, 0.0f},
            { 5.0f, 1.0f, 4.0f},
            {-5.0f, 1.0f, 4.0f});
        startPos = glm::vec3(-4.0f, -4.0f, PLAYER_HEIGHT * 0.5f + 0.05f);
        startVel = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)) * 60.0f;
    }
    else if (selected == "cone")
    {
        addStressCone(world);
    }
    else
    {
        float angle = 5.0f;
        if (selected == "wedge1") angle = 1.0f;
        else if (selected == "wedge10") angle = 10.0f;
        else if (selected == "wedge20") angle = 20.0f;
        else if (selected == "dash") { angle = 5.0f; speed = 120.0f; }
        addStressWedge(world, angle);
    }

    Player testPlayer(false);
    testPlayer.pos = startPos;
    testPlayer.vel = startVel;
    testPlayer.syncLegacyStateToLayers();

    bool grounded = false;
    constexpr int TICKS = 60;
    constexpr float DT = 1.0f / 60.0f;
    for (int i = 0; i < TICKS; ++i)
    {
        grounded = false;
        doCollisions(testPlayer, world, grounded, DT);
    }

    // TODO-DELETE (consolidate): this case verifies penetration with the legacy
    // root capsule (collectCapsuleRecoveryContacts). Once the active path is
    // triangle-only, the stress cases must verify with the actor triangle meshes
    // (ActorTriangleCollisionResult / collectActorMeshContacts) instead, so the
    // test measures the same authority it exercises. Keep capsule verification
    // until the toggle path is accepted by human gameplay testing.
    Capsule cap = testPlayer.getCapsule();
    std::vector<int> candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));
    std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(world, cap, candidates);
    float maxPen = 0.0f;
    for (const RecoveryContact& c : contacts)
        maxPen = std::max(maxPen, c.penetration);

    CollisionStressResult result;
    result.maxPenetration = maxPen;
    result.finalPos = testPlayer.pos;
    result.grounded = grounded;
    result.finite = isFiniteVec3(testPlayer.pos) && isFiniteVec3(testPlayer.vel);
    result.emergency = gLastCollisionTrace.emergencyEscaped;

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[COLLISION STRESS] case=%s ticks=%d final=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) contacts=%zu maxPen=%.4f grounded=%d %s",
        selected.c_str(), TICKS,
        testPlayer.pos.x, testPlayer.pos.y, testPlayer.pos.z,
        testPlayer.vel.x, testPlayer.vel.y, testPlayer.vel.z,
        contacts.size(), maxPen, (int)grounded,
        collisionLastTraceSummary().c_str());
    result.summary = std::string(buf);
    return result;
}

std::string collisionStressRun(const std::string& caseName)
{
    return runCollisionStressCase(caseName).summary;
}

bool collisionStressSelfTest(std::string* outSummary)
{
    const char* cases[] = {
        "floor",
        "floor_drop",
        "corner",
        "wedge1",
        "wedge5",
        "wedge10",
        "wedge20",
        "cone",
        "dash"
    };

    bool ok = true;
    std::string summary;
    for (const char* c : cases)
    {
        CollisionStressResult result = runCollisionStressCase(c);
        const bool caseOk =
            result.finite &&
            result.finalPos.z > -0.05f &&
            result.maxPenetration <= 0.04f &&
            !result.emergency;
        ok = ok && caseOk;

        summary += caseOk ? "PASS " : "FAIL ";
        summary += result.summary;
        summary += "\n";
    }

    // Direct response check for the moving-limb bounce (Phase A behavior): a
    // body part whose own sweep moves into a wall must push the whole body
    // outward even while the root velocity is still, and a very-low-speed part
    // contact must still apply the minimum push.
    {
        const CollisionConfig& cc = CollisionConfig::instance();
        if (cc.bounceEnabled() && cc.bounceStrength() > 0.0f)
        {
            const glm::vec3 wallNormal(1.0f, 0.0f, 0.0f);

            Player partPlayer(false);
            partPlayer.vel = glm::vec3(0.0f);
            partPlayer.externalImpulse = glm::vec3(0.0f);
            partPlayer.collision.bounceCooldown = 0.0f;
            respondVelocityAgainstNormal(partPlayer, wallNormal,
                                         glm::vec3(-2.0f, 0.0f, 0.0f));
            const bool partBounce = partPlayer.vel.x > 0.01f;
            ok = ok && partBounce;
            summary += partBounce ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] moving limb bounces still root\n";

            // A statically embedded body contact (no sweep, but penetrating)
            // must still get the minimum push so a limb cannot stay stuck.
            Player minPushPlayer(false);
            minPushPlayer.vel = glm::vec3(0.0f);
            minPushPlayer.externalImpulse = glm::vec3(0.0f);
            minPushPlayer.collision.bounceCooldown = 0.0f;
            respondVelocityAgainstNormal(minPushPlayer, wallNormal,
                                         glm::vec3(0.0f), true, 0.05f);
            const bool minPush = minPushPlayer.vel.x > 0.0f;
            ok = ok && minPush;
            summary += minPush ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] embedded limb contact min push\n";

            // A root-capsule contact with penetration but bodyContact == false
            // must NOT get the minimum push (no phantom motion).
            Player embeddedRoot(false);
            embeddedRoot.vel = glm::vec3(0.0f);
            embeddedRoot.externalImpulse = glm::vec3(0.0f);
            embeddedRoot.collision.bounceCooldown = 0.0f;
            respondVelocityAgainstNormal(embeddedRoot, wallNormal,
                                         glm::vec3(0.0f), false, 0.05f);
            const bool noPhantom = glm::length(embeddedRoot.vel) < 0.001f;
            ok = ok && noPhantom;
            summary += noPhantom ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] embedded root contact no push\n";

            // Root-only contacts keep the old behavior (no phantom push when
            // nothing is moving into the surface).
            Player stillPlayer(false);
            stillPlayer.vel = glm::vec3(0.0f);
            stillPlayer.externalImpulse = glm::vec3(0.0f);
            stillPlayer.collision.bounceCooldown = 0.0f;
            respondVelocityAgainstNormal(stillPlayer, wallNormal);
            const bool still = glm::length(stillPlayer.vel) < 0.001f;
            ok = ok && still;
            summary += still ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] resting root contact adds no push\n";
        }
        else
        {
            summary += "SKIP [COLLISION STRESS] bounce disabled in config\n";
        }
    }

    // A rounded sweep reaching the edge of a walkable floor must keep the
    // floor face normal. Otherwise the edge normal can launch the actor
    // sideways from a flat surface.
    {
        CollisionTriangle edgeFloor;
        edgeFloor.a = glm::vec3(0.0f, 0.0f, 0.0f);
        edgeFloor.b = glm::vec3(1.0f, 0.0f, 0.0f);
        edgeFloor.c = glm::vec3(0.0f, 1.0f, 0.0f);
        edgeFloor.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        float hitTime = 1.0f;
        glm::vec3 hitNormal(0.0f);
        glm::vec3 hitPoint(0.0f);
        const bool hit = sweepSphereTriangle(
            glm::vec3(1.15f, 0.2f, 1.0f), glm::vec3(0.0f, 0.0f, -2.0f),
            0.25f, edgeFloor, hitTime, hitNormal, hitPoint);
        const bool floorNormal = hit && hitNormal.z > 0.95f &&
                                 std::fabs(hitNormal.x) < 0.05f &&
                                 std::fabs(hitNormal.y) < 0.05f;
        ok = ok && floorNormal;
        summary += floorNormal ? "PASS " : "FAIL ";
        summary += "[COLLISION STRESS] walkable floor edge keeps face normal\n";
    }

    // Real mesh-triangle limb collision: an arm triangle crossing the floor
    // plane must produce a contact whose normal points up (toward the actor), so
    // the limb is pushed out of the floor instead of down through it.
    {
        const CollisionConfig& cc = CollisionConfig::instance();
        if (cc.bodyMeshCollision())
        {
            World meshWorld;
            addStressQuad(meshWorld,
                {-8.0f, -8.0f, 0.0f}, { 8.0f, -8.0f, 0.0f},
                { 8.0f,  8.0f, 0.0f}, {-8.0f,  8.0f, 0.0f});
            buildCollisionChunks(meshWorld, nullptr);

            Player meshPlayer(false);
            meshPlayer.pos = glm::vec3(0.0f, 0.0f, 1.0f); // actor above the floor

            PhysicalBodyPart arm;
            arm.name = "leftArm";
            CollisionTriangle tri;
            tri.a = glm::vec3(-1.0f, 0.0f, -0.2f); // pokes through the floor
            tri.b = glm::vec3( 1.0f, 0.0f, -0.2f);
            tri.c = glm::vec3( 0.0f, 1.0f,  0.3f);
            tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            arm.collider.triangles.push_back(tri);
            arm.collider.localMin = glm::vec3(-1.0f, 0.0f, -0.2f);
            arm.collider.localMax = glm::vec3(1.0f, 1.0f, 0.3f);
            meshPlayer.physicalBody.parts.push_back(arm);

            std::vector<RecoveryContact> meshContacts =
                collectBodyMeshContacts(meshPlayer, meshWorld);
            bool meshHit = false;
            bool upNormal = false;
            for (const RecoveryContact& c : meshContacts)
            {
                if (c.penetration <= 0.0f)
                    continue;
                meshHit = true;
                if (c.normal.z > 0.5f)
                    upNormal = true;
            }
            const bool meshOk = meshHit && upNormal;
            ok = ok && meshOk;
            summary += meshOk ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] mesh limb triangle hits floor, pushes up\n";

            // A thin wall crossed between ticks must still be found even when
            // the limb ends completely on the other side. This catches both
            // swept broadphase mistakes and the old current-pose-only test.
            World sweptWorld;
            addStressQuad(sweptWorld,
                {0.0f, -2.0f, -2.0f}, {0.0f, 2.0f, -2.0f},
                {0.0f, 2.0f, 2.0f}, {0.0f, -2.0f, 2.0f});
            buildCollisionChunks(sweptWorld, nullptr);

            Player sweptPlayer(false);
            sweptPlayer.pos = glm::vec3(0.0f, 0.0f, 1.0f);
            PhysicalBodyPart sweptArm;
            sweptArm.name = "sweptArm";
            CollisionTriangle sweptTri;
            sweptTri.a = glm::vec3(0.0f, -0.5f, -0.5f);
            sweptTri.b = glm::vec3(0.0f,  0.5f, -0.5f);
            sweptTri.c = glm::vec3(0.0f,  0.0f,  0.5f);
            sweptTri.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            sweptArm.collider.triangles.push_back(sweptTri);
            sweptArm.collider.localMin = glm::vec3(0.0f, -0.5f, -0.5f);
            sweptArm.collider.localMax = glm::vec3(0.0f,  0.5f,  0.5f);
            sweptArm.previousWorldTransform = glm::mat4(1.0f);
            sweptArm.previousWorldTransform[3].x = -1.0f;
            sweptArm.worldTransform = glm::mat4(1.0f);
            sweptArm.worldTransform[3].x = 1.0f;
            sweptPlayer.physicalBody.parts.push_back(sweptArm);

            const std::vector<RecoveryContact> sweptContacts =
                collectBodyMeshContacts(sweptPlayer, sweptWorld);
            bool sweptHit = false;
            bool opposedSweep = false;
            for (const RecoveryContact& c : sweptContacts) {
                if (c.penetration > 0.0f) {
                    sweptHit = true;
                    if (c.normal.x < -0.5f)
                        opposedSweep = true;
                }
            }
            const bool sweptOk = sweptHit && opposedSweep;
            ok = ok && sweptOk;
            summary += sweptOk ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] swept limb crosses thin wall\n";

            // A previous pose touching a wall must not glue the actor to it
            // after the current pose has moved away.
            PhysicalBodyPart leavingArm = sweptArm;
            leavingArm.name = "leavingArm";
            leavingArm.previousWorldTransform = glm::mat4(1.0f);
            leavingArm.previousWorldTransform[3].x = 0.0f;
            leavingArm.worldTransform = glm::mat4(1.0f);
            leavingArm.worldTransform[3].x = -1.0f;
            Player leavingPlayer(false);
            leavingPlayer.pos = glm::vec3(0.0f, 0.0f, 1.0f);
            leavingPlayer.physicalBody.parts.push_back(leavingArm);
            const std::vector<RecoveryContact> leavingContacts =
                collectBodyMeshContacts(leavingPlayer, sweptWorld);
            const bool leavingOk = leavingContacts.empty();
            ok = ok && leavingOk;
            summary += leavingOk ? "PASS " : "FAIL ";
            summary += "[COLLISION STRESS] limb leaves old wall contact\n";
        }
        else
        {
            summary += "SKIP [COLLISION STRESS] body mesh collision disabled\n";
        }
    }

    if (outSummary)
        *outSummary = summary;
    return ok;
}

// Verifies the canonical contact adapters: producer-specific results
// (RecoveryContact, SweepHit) convert into one MovementContact vocabulary with
// kind/source/point/normal/penetration preserved and shape/subshape/sweep/
// surface metadata added, without changing contact dedup identity.
bool canonicalContactSelfTest(std::string* outSummary)
{
    std::string summary;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        summary += cond ? "  PASS: " : "  FAIL: ";
        summary += name;
        summary += "\n";
        if (!cond) ok = false;
    };

    const MovementLifecycleIdentity life{7u, 0u};
    const uint64_t tick = 1234u;
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
    const glm::vec3 point(1.0f, 2.0f, 3.0f);

    // 1. RecoveryContact metadata survives conversion.
    {
        RecoveryContact rc;
        rc.label = "leftArm";
        rc.triangleIndex = 5;
        rc.penetration = 0.03f;
        rc.normal = normal;
        rc.point = point;
        rc.sweepDelta = glm::vec3(0.2f, 0.0f, 0.0f);
        const MovementContact mc = movementContactFromRecoveryContact(
            rc, MovementContactKind::Wall, MovementContactSource::PlayerBody,
            MovementShapeKind::TriangleMesh, tick, life);
        check(mc.subshape == MovementSubshape::LeftArm,
              "recovery label maps to leftArm subshape");
        check(mc.surfaceId == 6u, "recovery triangle maps to surfaceId");
        check(mc.shapeKind == MovementShapeKind::TriangleMesh,
              "recovery shape kind preserved");
        check(mc.source == MovementContactSource::PlayerBody,
              "recovery source preserved");
        check(mc.simulationTick == tick, "recovery tick preserved");
        check(std::fabs(mc.penetrationDepth - 0.03f) < 1e-6f,
              "recovery penetration preserved");
        check(glm::length(mc.sweepVelocity - glm::vec3(0.2f, 0.0f, 0.0f)) < 1e-6f,
              "recovery sweep velocity preserved");
        check(glm::length(mc.point - point) < 1e-6f, "recovery point preserved");
        check(mc.targetLifecycle.spawnGeneration == 7u,
              "recovery lifecycle preserved");
    }

    // 2. Sphere and mesh producers describe the same surface with equivalent
    //    canonical fields and only the shape kind differs.
    {
        RecoveryContact sphere;
        sphere.label = "leftArm";
        sphere.triangleIndex = 9;
        sphere.penetration = 0.02f;
        sphere.normal = normal;
        sphere.point = point;
        RecoveryContact mesh = sphere;

        const MovementContact a = movementContactFromRecoveryContact(
            sphere, MovementContactKind::StaticWorld,
            MovementContactSource::PlayerBody, MovementShapeKind::Sphere,
            tick, life);
        const MovementContact b = movementContactFromRecoveryContact(
            mesh, MovementContactKind::StaticWorld,
            MovementContactSource::PlayerBody, MovementShapeKind::TriangleMesh,
            tick, life);
        check(a.subshape == b.subshape && a.surfaceId == b.surfaceId &&
                  a.kind == b.kind && a.source == b.source &&
                  std::fabs(a.penetrationDepth - b.penetrationDepth) < 1e-6f &&
                  glm::length(a.normal - b.normal) < 1e-6f &&
                  glm::length(a.point - b.point) < 1e-6f,
              "sphere and mesh contacts produce equivalent canonical fields");
        check(a.shapeKind != b.shapeKind,
              "sphere and mesh keep distinct shape kind");
    }

    // 3. SweepHit conversion carries sweep and surface velocity.
    {
        SweepHit hit;
        hit.hit = true;
        hit.time = 0.25f;
        hit.point = point;
        hit.normal = normal;
        hit.triangleIndex = 11;
        hit.colliderName = "weapon";
        const MovementContact mc = movementContactFromSweepHit(
            hit, MovementContactKind::Wall, MovementContactSource::Weapon,
            MovementShapeKind::Capsule, tick, life,
            glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(1.5f, 0.0f, 0.0f));
        check(mc.subshape == MovementSubshape::Weapon,
              "sweep hit collider maps to weapon subshape");
        check(mc.surfaceId == 12u, "sweep hit triangle maps to surfaceId");
        check(glm::length(mc.sweepVelocity - glm::vec3(0.5f, 0.0f, 0.0f)) < 1e-6f,
              "sweep hit sweep velocity preserved");
        check(glm::length(mc.surfaceVelocity - glm::vec3(1.5f, 0.0f, 0.0f)) < 1e-6f,
              "sweep hit surface velocity preserved");
        check(std::fabs(mc.penetrationDepth) < 1e-6f,
              "sweep hit has zero penetration");
    }

    // 4. Root capsule and world labels map correctly.
    {
        check(movementSubshapeFromLabel("glb-recovery") ==
                  MovementSubshape::RootCapsule,
              "recovery label maps to root capsule");
        check(movementSubshapeFromLabel("Player_Capsule_Depen") ==
                  MovementSubshape::RootCapsule,
              "player capsule label maps to root capsule");
        check(movementSubshapeFromLabel("weapon") == MovementSubshape::Weapon,
              "weapon label maps to weapon");
        check(movementSubshapeFromLabel(nullptr) == MovementSubshape::Unknown,
              "null label maps to unknown");
    }

    // 5. An arm contact on a walkable slope keeps the arm subshape and the
    //    contact point, so the foot-proximity decision stays with the consumer.
    {
        RecoveryContact arm;
        arm.label = "rightArm";
        arm.point = glm::vec3(0.0f, 0.0f, 1.4f);
        arm.normal = glm::normalize(glm::vec3(0.3f, 0.0f, 0.95f));
        arm.penetration = 0.01f;
        const MovementContact mc = movementContactFromRecoveryContact(
            arm,
            classifyCollisionMovementContactKind(arm.normal, false, false),
            MovementContactSource::PlayerBody, MovementShapeKind::TriangleMesh,
            tick, life);
        check(mc.subshape == MovementSubshape::RightArm,
              "slope arm contact keeps arm subshape");
        check(std::fabs(mc.point.z - 1.4f) < 1e-6f,
              "slope arm contact keeps its point for the foot check");
    }

    // 6. The new canonical fields do not change dedup identity, so gameplay
    //    behavior is unchanged for existing callers.
    {
        RecoveryContact rc;
        rc.label = "torso";
        rc.triangleIndex = 2;
        rc.penetration = 0.01f;
        rc.normal = normal;
        rc.point = point;
        MovementContact a = movementContactFromRecoveryContact(
            rc, MovementContactKind::StaticWorld,
            MovementContactSource::StaticWorld, MovementShapeKind::Sphere,
            tick, life);
        MovementContact b = a;
        b.shapeKind = MovementShapeKind::TriangleMesh;
        b.subshape = MovementSubshape::Weapon;
        b.sweepVelocity = glm::vec3(9.0f, 0.0f, 0.0f);
        b.materialId = 42u;
        b.targetEntityId = 99u;

        MovementContactSet set;
        set.addDeduplicated(a);
        const bool added = set.addDeduplicated(b);
        check(!added && set.duplicateCount == 1,
              "new canonical metadata does not change contact dedup identity");
    }

    if (outSummary)
        *outSummary = summary;
    return ok;
}

// Verifies the collision sub-grid broadphase returns exactly the same triangle
// set as the previous whole-chunk iteration (via brute-force overlap scan), and
// that a small query near a dense cluster touches a small fraction of triangles.
bool collisionSubGridSelfTest(std::string* outSummary)
{
    std::string summary;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        summary += cond ? "  PASS: " : "  FAIL: ";
        summary += name;
        summary += "\n";
        if (!cond) ok = false;
    };

    // Dense cluster of tiny triangles packed into one 6-unit chunk (~9600 tris).
    World world;
    world.collisionChunkSize = 6.0f;
    constexpr int GRID = 40;
    for (int x = 0; x < GRID; ++x)
    for (int y = 0; y < GRID; ++y)
    for (int z = 0; z < 3; ++z)
    {
        glm::vec3 p0(x * 0.1f, y * 0.1f, z * 1.0f);
        CollisionTriangle tri;
        tri.a = p0;
        tri.b = glm::vec3(p0.x + 0.1f, p0.y, p0.z);
        tri.c = glm::vec3(p0.x, p0.y + 0.1f, p0.z);
        tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        world.collisionMesh.triangles.push_back(tri);
    }

    buildCollisionChunks(world, nullptr);

    check(!world.collisionChunks.empty(), "chunks built");
    check(!world.collisionSubGrids.empty(), "subgrids built");
    check(!world.collisionMesh.triangles.empty(), "dense triangles present");

    // Brute-force reference using the same overlap filter as the broadphase.
    auto bruteForce = [&](const AABB& q, float expansion) {
        std::vector<int> out;
        for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i) {
            AABB tb = makeTriangleAABB(world.collisionMesh.triangles[i]);
            tb.min -= glm::vec3(expansion);
            tb.max += glm::vec3(expansion);
            if (overlaps(q, tb))
                out.push_back(i);
        }
        return out;
    };

    const float expansion = 0.1f;
    const AABB queries[] = {
        AABB{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        AABB{{-0.5f, -0.5f, -0.5f}, {2.0f, 2.0f, 2.0f}},
        AABB{{100.0f, 100.0f, 100.0f}, {101.0f, 101.0f, 101.0f}},
        AABB{{-1.0f, -1.0f, -1.0f}, {5.0f, 5.0f, 4.0f}}
    };

    for (int i = 0; i < 4; ++i) {
        std::vector<int> expected = bruteForce(queries[i], expansion);
        std::vector<int> got;
        appendChunkTrianglesForAABB(world, queries[i], expansion, got, "subgridSelftest");
        std::sort(expected.begin(), expected.end());
        std::sort(got.begin(), got.end());
        bool sameSet = (expected == got);
        char name[128];
        std::snprintf(name, sizeof(name),
            "query%d candidate set matches brute-force (%zu vs %zu)",
            i + 1, got.size(), expected.size());
        check(sameSet, name);
    }

    {
        std::vector<int> small;
        appendChunkTrianglesForAABB(world, queries[0], expansion, small, "subgridSelftestSmall");
        const float ratio = (float)small.size() / (float)world.collisionMesh.triangles.size();
        char name[128];
        std::snprintf(name, sizeof(name),
            "small query returns %zu/%zu tris (%.2f%% of world)",
            small.size(), world.collisionMesh.triangles.size(), ratio * 100.0f);
        check(!small.empty() && ratio < 0.5f, name);
    }

    if (outSummary)
        *outSummary = summary;
    return ok;
}
