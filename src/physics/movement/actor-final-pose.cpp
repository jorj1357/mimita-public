#include "physics/movement/actor-final-pose.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "entities/player.h"
#include "game/spawn-utils.h"
#include "map/map-loader-collision.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision.h"
#include "physics/ray-utils.h"
#include "world/world.h"

bool actorRootInsideClosedSolid(const World& world, const glm::vec3& point)
{
    const auto& mesh = world.collisionMesh;
    if (mesh.triangles.empty() || mesh.solidIds.size() != mesh.triangles.size())
        return false;
    const int floorTriangle =
        selectWorldTriangle(world, point, glm::vec3(0.0f, 0.0f, -1.0f));
    if (floorTriangle < 0 ||
        static_cast<std::size_t>(floorTriangle) >= mesh.solidIds.size())
        return false;
    const std::uint32_t solidId = mesh.solidIds[floorTriangle];
    if (solidId == 0)
        return false;
    const glm::vec3 direction =
        glm::normalize(glm::vec3(1.0f, 0.3713907f, 0.193f));
    int hits = 0;
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
        if (mesh.solidIds[i] != solidId)
            continue;
        float distance = 0.0f;
        if (rayTriangle(point, direction, mesh.triangles[i], distance) &&
            distance > 1.0e-4f)
            ++hits;
    }
    return (hits % 2) != 0;
}

namespace {

constexpr float kContactEpsilon = 0.001f;
constexpr float kClearanceEpsilon = 0.0005f;
constexpr int kMaxContainmentIterations = 6;
constexpr float kMaxCorrectionPerIteration = 1.0f;
constexpr float kMaxTotalCorrection = 3.0f;

struct PoseSample
{
    std::vector<RecoveryContact> contacts;
    float maxPenetration = 0.0f;
    bool budgetExceeded = false;
};

PoseSample sampleFinalPose(Player& actor, const World& world, float standoff)
{
    PoseSample sample;
    if (world.collisionMesh.triangles.empty())
        return sample;

    std::vector<ActorCollisionMesh> meshes = collectActorCollisionMeshes(actor);
    if (meshes.empty())
        return sample;
    for (ActorCollisionMesh& mesh : meshes)
        mesh.previousTransform = mesh.desiredTransform;

    const AABB bounds = makeSweptActorMeshAABB(meshes, glm::vec3(0.0f));
    if (bounds.min.x > bounds.max.x)
        return sample;

    static thread_local std::vector<int> candidates;
    candidates.clear();
    appendChunkTrianglesForAABB(world, bounds,
                                standoff + MOVEMENT_FEATURE_SMOOTHNESS,
                                candidates, "actorFinalPose");
    if (candidates.empty())
        return sample;

    gActorNarrowphase = ActorNarrowphaseStats{};
    collectActorMeshContactsInto(world, meshes, candidates, actor.pos,
                                 sample.contacts, true, standoff,
                                 nullptr, false, MOVEMENT_FEATURE_SMOOTHNESS);
    sample.budgetExceeded = gActorNarrowphase.budgetExceeded;

    const std::vector<BodyWeaponSphere> weaponSpheres =
        collectBodyWeaponSpheres(actor, false);
    if (!weaponSpheres.empty())
    {
        std::vector<RecoveryContact> weaponContacts =
            collectBodyWeaponContacts(actor, world, weaponSpheres);
        sample.contacts.insert(sample.contacts.end(), weaponContacts.begin(),
                               weaponContacts.end());
    }

    for (const RecoveryContact& contact : sample.contacts)
    {
        if (contact.entityId != 0)
            continue;
        sample.maxPenetration = std::max(sample.maxPenetration,
                                         contact.penetration);
    }
    return sample;
}

void copyContacts(const std::vector<RecoveryContact>& source,
                  ActorFinalPoseResult& result,
                  const World& world)
{
    result.contacts.clear();
    result.contacts.reserve(4);
    for (const RecoveryContact& contact : source)
    {
        if (contact.entityId != 0)
            continue;
        ActorFinalPoseContact copy;
        copy.actorPart = contact.label ? contact.label : "";
        copy.point = contact.point;
        copy.normal = contact.normal;
        copy.surfaceNormal = contact.surfaceNormal;
        copy.penetration = contact.penetration;
        copy.worldTriangle = contact.triangleIndex;
        copy.solidId = contact.triangleIndex >= 0 &&
            static_cast<size_t>(contact.triangleIndex) < world.collisionMesh.solidIds.size()
                ? world.collisionMesh.solidIds[static_cast<size_t>(contact.triangleIndex)]
                : 0u;
        result.contacts.push_back(copy);
        if (result.contacts.size() >= 4)
            break;
    }
}

glm::vec3 deepestNormal(const PoseSample& sample)
{
    const RecoveryContact* deepest = nullptr;
    for (const RecoveryContact& contact : sample.contacts)
    {
        if (contact.entityId != 0)
            continue;
        if (!deepest || contact.penetration > deepest->penetration)
            deepest = &contact;
    }
    if (!deepest)
        return glm::vec3(0.0f);
    glm::vec3 normal = deepest->normal;
    if (glm::dot(normal, normal) < 0.5f)
        normal = deepest->surfaceNormal;
    const float length = glm::length(normal);
    if (length < 1e-5f)
        return glm::vec3(0.0f);
    return normal / length;
}

void translateRoot(Player& actor, const glm::vec3& delta)
{
    actor.pos += delta;
    if (!actor.weaponColliderMesh.empty())
        actor.weaponModelTransform =
            glm::translate(glm::mat4(1.0f), delta) * actor.weaponModelTransform;
    actor.updateModelWorldTransforms();
}

void acceptClearPose(Player& actor, ActorFinalPoseState& state)
{
    state.hasClearPose = true;
    state.clearPosition = actor.pos;
    state.clearYaw = actor.yaw;
    state.clearOnGround = actor.ground.hasWorldContact ||
                          actor.ground.realWorldContactThisFrame;
    state.clearTick = actor.movementSimulationTick;
}

bool validateAt(Player& actor, const World& world, const glm::vec3& position,
                float standoff, PoseSample& sample)
{
    const glm::vec3 before = actor.pos;
    translateRoot(actor, position - before);
    sample = sampleFinalPose(actor, world, standoff);
    const bool clear = !actorRootInsideClosedSolid(world, position) &&
        !sample.budgetExceeded &&
        sample.maxPenetration <=
            std::max(standoff, kClearanceEpsilon) + 0.01f;
    if (!clear)
        translateRoot(actor, before - actor.pos);
    return clear;
}

void restoreRoot(Player& actor, const glm::vec3& position, float yaw,
                 bool onGround)
{
    translateRoot(actor, position - actor.pos);
    actor.yaw = yaw;
    actor.vel = glm::vec3(0.0f);
    actor.externalImpulse = glm::vec3(0.0f);
    actor.ground.onGround = onGround;
    actor.ground.wasOnGround = onGround;
    actor.ground.hasWorldContact = onGround;
    actor.ground.realWorldContactThisFrame = onGround;
    actor.updateModelWorldTransforms();
}

bool findBoundedSafePoint(Player& actor, const World& world,
                          const ActorFinalPoseState& state, float standoff,
                          glm::vec3& outPosition, PoseSample& outSample)
{
    const glm::vec3 base = state.hasClearPose ? state.clearPosition : actor.pos;
    constexpr float kRings[] = {0.0f, 0.5f, 1.0f, 2.0f};
    // A safe point is a nearby standing correction, not a ladder to a roof.
    // Larger lifts made wall overlaps teleport actors 8-12 m upward and then
    // reset repeatedly. If the nearby search fails, use last-clear/respawn.
    constexpr float kLifts[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f};
    for (float ring : kRings)
    {
        for (int direction = 0; direction < 8; ++direction)
        {
            if (ring <= 0.0f && direction > 0)
                break;
            const float angle = static_cast<float>(direction) *
                glm::two_pi<float>() / 8.0f;
            const glm::vec3 horizontal(std::cos(angle) * ring,
                                       std::sin(angle) * ring, 0.0f);
            for (float lift : kLifts)
            {
                const glm::vec3 candidate = base + horizontal +
                    glm::vec3(0.0f, 0.0f, lift);
                if (actorRootInsideClosedSolid(world, candidate))
                    continue;
                if (!validateAt(actor, world, candidate, standoff, outSample))
                {
                    if (outSample.budgetExceeded)
                        return false;
                    continue;
                }
                bool hasSupport = false;
                for (const RecoveryContact& contact : outSample.contacts)
                {
                    if (contact.entityId == 0 && contact.surfaceNormal.z > 0.8f)
                    {
                        hasSupport = true;
                        break;
                    }
                }
                // A clear point floating in the air is not a safe standing
                // point. Without this, a wall overlap could repeatedly lift an
                // NPC 2 m upward instead of resolving or respawning it.
                if (hasSupport)
                {
                    outPosition = candidate;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

bool containActorFinalPose(Player& actor,
                           const World& world,
                           ActorFinalPoseState& state,
                           ActorFinalPoseResult& result,
                           float standoff,
                           const char* stage)
{
    result = ActorFinalPoseResult{};
    result.stage = stage ? stage : "final";
    result.partCount = static_cast<int>(actor.physicalBody.parts.size());
    result.extraPartCount =
        static_cast<int>(actor.physicalBody.extraParts.size());
    standoff = std::max(standoff, 0.0f);
    if (world.collisionMesh.triangles.empty())
    {
        result.skipReason = "no_world";
        acceptClearPose(actor, state);
        return true;
    }
    if (actor.physicalBody.parts.empty() &&
        actor.physicalBody.extraParts.empty())
    {
        result.skipReason = "no_parts";
        acceptClearPose(actor, state);
        return true;
    }

    PoseSample sample = sampleFinalPose(actor, world, standoff);
    const bool rootInsideClosedSolid =
        actorRootInsideClosedSolid(world, actor.pos);
    result.rootInsideClosedSolid = rootInsideClosedSolid;
    const float clearTolerance =
        std::max(standoff, kClearanceEpsilon) + 0.01f;
    result.initialPenetration = sample.maxPenetration;
    result.finalPenetration = sample.maxPenetration;
    result.budgetExceeded = sample.budgetExceeded;
    copyContacts(sample.contacts, result, world);
    if (!rootInsideClosedSolid && !sample.budgetExceeded &&
        sample.maxPenetration <= clearTolerance)
    {
        acceptClearPose(actor, state);
        return true;
    }

    glm::vec3 totalCorrection(0.0f);
    float previousPenetration = sample.maxPenetration;
    for (int iteration = 0; iteration < kMaxContainmentIterations; ++iteration)
    {
        const glm::vec3 normal = deepestNormal(sample);
        if (glm::dot(normal, normal) < 0.5f)
            break;

        // Move only the excess beyond the allowed standoff. Adding the full
        // standoff every tick makes a grounded actor drift upward forever
        // because ordinary floor skin contact is re-detected each tick.
        glm::vec3 correction =
            normal * (sample.maxPenetration - clearTolerance + kClearanceEpsilon);
        const float correctionLength = glm::length(correction);
        if (correctionLength > kMaxCorrectionPerIteration)
            correction *= kMaxCorrectionPerIteration / correctionLength;
        if (correctionLength < kContactEpsilon)
            break;

        result.iterations = iteration + 1;
        result.corrected = true;
        translateRoot(actor, correction);
        totalCorrection += correction;
        result.correction = totalCorrection;

        sample = sampleFinalPose(actor, world, standoff);
        result.finalPenetration = sample.maxPenetration;
        result.budgetExceeded = result.budgetExceeded ||
            sample.budgetExceeded;
        copyContacts(sample.contacts, result, world);
        if (!sample.budgetExceeded &&
            !actorRootInsideClosedSolid(world, actor.pos) &&
            sample.maxPenetration <= clearTolerance)
        {
            result.clear = true;
            acceptClearPose(actor, state);
            return true;
        }
        if (sample.maxPenetration >= previousPenetration - kContactEpsilon)
            break;
        if (sample.budgetExceeded)
            break;
        if (glm::length(totalCorrection) >= kMaxTotalCorrection)
            break;
        previousPenetration = sample.maxPenetration;
    }

    result.clear = false;
    if (state.hasClearPose)
    {
        PoseSample restoredSample;
        if (validateAt(actor, world, state.clearPosition, standoff,
                       restoredSample))
        {
            restoreRoot(actor, state.clearPosition, state.clearYaw,
                        state.clearOnGround);
            result.restored = true;
            result.resetReason = "last_clear";
            result.finalPenetration = restoredSample.maxPenetration;
            return false;
        }
        result.budgetExceeded = result.budgetExceeded ||
            restoredSample.budgetExceeded;
    }

    PoseSample safeSample;
    glm::vec3 safePoint{0.0f};
    if (findBoundedSafePoint(actor, world, state, standoff, safePoint,
                             safeSample))
    {
        restoreRoot(actor, safePoint, actor.yaw, false);
        result.usedSafePoint = true;
        result.resetReason = "safe_point";
        result.finalPenetration = safeSample.maxPenetration;
        acceptClearPose(actor, state);
        return false;
    }
    result.budgetExceeded = result.budgetExceeded || safeSample.budgetExceeded;

    if (state.hasClearPose)
        restoreRoot(actor, state.clearPosition, state.clearYaw,
                    state.clearOnGround);
    result.failed = true;
    result.resetReason = "no_valid_pose";
    return false;
}

namespace {

void addQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
{
    auto add = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) {
        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        const float length = glm::length(n);
        if (length < 1e-6f)
            return;
        CollisionTriangle triangle;
        triangle.a = p0;
        triangle.b = p1;
        triangle.c = p2;
        triangle.normal = n / length;
        world.collisionMesh.triangles.push_back(triangle);
    };
    add(a, b, c);
    add(a, c, d);
}

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
        {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {1, 2, 6, 5}, {3, 0, 4, 7}
    };
    for (const auto& quad : quads)
    {
        auto push = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 n = glm::cross(b - a, c - a);
            const float length = glm::length(n);
            if (length < 1e-6f)
                return;
            CollisionTriangle triangle;
            triangle.a = a;
            triangle.b = b;
            triangle.c = c;
            triangle.normal = n / length;
            out.push_back(triangle);
        };
        push(v[quad[0]], v[quad[1]], v[quad[2]]);
        push(v[quad[0]], v[quad[2]], v[quad[3]]);
    }
}

void setupBoxActor(Player& actor, glm::vec3 center, float halfXY)
{
    actor.pos = center;
    actor.yaw = 0.0f;

    PhysicalBodyPart part;
    part.name = "torso";
    part.nodeIndex = 0;
    part.collider.name = "torso";
    part.collider.localMin = glm::vec3(-halfXY, -halfXY, -halfXY);
    part.collider.localMax = glm::vec3(halfXY, halfXY, halfXY);
    buildBoxTriangles(part.collider.triangles, glm::vec3(0.0f),
                      glm::vec3(halfXY));
    actor.physicalBody.parts.clear();
    actor.physicalBody.parts.push_back(part);
    actor.nodes.resize(1);
    actor.restLocalTransforms.assign(1, glm::mat4(1.0f));
    actor.perfectPoseSkeleton.nodes.resize(1);
    actor.perfectPoseSkeleton.nodes[0].name = "root";
    actor.perfectPoseSkeleton.nodes[0].parent = -1;
    actor.perfectPoseSkeleton.nodes[0].localTransform = glm::mat4(1.0f);
    actor.perfectPoseSkeleton.restLocalTransforms.assign(1, glm::mat4(1.0f));
    actor.updateModelWorldTransforms();
}

} // namespace

bool actorFinalPoseSelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool condition, const char* name) {
        report += condition ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!condition)
            ok = false;
    };

    {
        World world;
        addQuad(world, {-8, -8, 0}, {8, -8, 0}, {8, 8, 0}, {-8, 8, 0});
        addQuad(world, {0.5f, -2, 0}, {0.5f, -2, 4}, {0.5f, 2, 4},
                {0.5f, 2, 0});
        buildCollisionChunks(world, nullptr);

        Player actor(false);
        setupBoxActor(actor, glm::vec3(0.4f, 0.0f, 3.0f), 0.4f);
        ActorFinalPoseState state;
        ActorFinalPoseResult result;
        const bool contained =
            containActorFinalPose(actor, world, state, result, 0.02f);
        check(contained, "final pose is contained");
        check(result.corrected, "final pose records a root correction");
        check(actor.pos.x <= 0.131f,
              "final pose root is moved within standoff of the wall");
        check(result.finalPenetration <= 0.031f,
              "final pose residual penetration is within standoff");
    }

    {
        World world;
        addQuad(world, {-8, -8, 0}, {8, -8, 0}, {8, 8, 0}, {-8, 8, 0});
        addQuad(world, {0.5f, -2, 0}, {0.5f, -2, 4}, {0.5f, 2, 4},
                {0.5f, 2, 0});
        buildCollisionChunks(world, nullptr);

        Player actor(false);
        setupBoxActor(actor, glm::vec3(0.4f, 0.0f, 3.0f), 0.4f);
        actor.physicalBody.extraParts = std::move(actor.physicalBody.parts);
        actor.physicalBody.parts.clear();
        ActorFinalPoseState state;
        ActorFinalPoseResult result;
        const bool contained =
            containActorFinalPose(actor, world, state, result, 0.02f);
        check(contained, "extra-only final pose is contained");
        check(result.corrected,
              "extra-only final pose records a root correction");
        check(actor.pos.x <= 0.131f,
              "extra-only final pose root is moved within standoff of the wall");
    }

    {
        World world;
        addQuad(world, {-8, -8, 0}, {8, -8, 0}, {8, 8, 0}, {-8, 8, 0});
        buildCollisionChunks(world, nullptr);

        Player actor(false);
        setupBoxActor(actor, glm::vec3(0.0f, 0.0f, 3.0f), 0.4f);
        ActorFinalPoseState state;
        ActorFinalPoseResult result;
        check(containActorFinalPose(actor, world, state, result),
              "clear final pose is accepted");
        check(state.hasClearPose, "clear final pose is remembered");
        check(result.contacts.empty(), "clear final pose reports no contact");
    }

    if (outSummary)
        *outSummary = report;
    return ok;
}
