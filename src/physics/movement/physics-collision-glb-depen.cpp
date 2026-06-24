#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

extern std::vector<int> gatherGLBTriangles(const World& world, const Capsule& cap, const glm::vec3& move);

void glbPhaseBatchedDepenetration(Player& p, const World& world, bool& groundedThisFrame,
    CollisionTraceSnapshot& trace)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    for (int depenIter = 0; depenIter < 4; ++depenIter)
    {
        p.updateModelWorldTransforms();
        Capsule cap = p.getCapsule();
        std::vector<int> candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

        std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(
            world, cap, candidates
        );
        trace.maxRecoveryContacts = std::max(trace.maxRecoveryContacts, (int)contacts.size());

        if (contacts.empty())
            break;

        float iterMaxPen = 0.0f;
        glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, glm::vec3(0.0f), p.pos);
        trace.maxPenetration = std::max(trace.maxPenetration, iterMaxPen);

        float corrLen = glm::length(correction);
        if (corrLen > MAX_CORRECTION)
            correction *= MAX_CORRECTION / corrLen;

        p.pos += correction;

        for (const RecoveryContact& c : contacts)
        {
            applyCollisionContact(
                p, groundedThisFrame,
                c.normal, c.point, c.penetration,
                c.triangleIndex, c.label
            );
        }

        DebugVis::recordDepenetration(p.pos - correction, correction, "glb-batched-depen");

        PHYS_LOG(
            "[PHYS][GLB DEPEN] iter=%d contacts=%zu maxPen=%.4f correction=(%.4f %.4f %.4f)\n",
            depenIter, contacts.size(), iterMaxPen,
            correction.x, correction.y, correction.z
        );

        if (glm::dot(correction, correction) < 0.0000001f)
            break;
    }
}

void glbPhaseResweepRemaining(Player& p, const World& world, bool& groundedThisFrame,
    glm::vec3& remainingMove, CollisionTraceSnapshot& trace)
{
    constexpr float SURFACE_SLOP = 0.01f;

    p.updateModelWorldTransforms();
    glm::vec3 curMove = remainingMove;
    if (glm::length(curMove) > 0.001f)
    {
        Capsule resweepCap = p.getCapsule();
        std::vector<int> resweepCandidates = gatherGLBTriangles(world, resweepCap, curMove);
        SweepHit resweepHit;
        resweepHit.time = 1.0f;
        for (int triIndex : resweepCandidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            SweepHit hit;
            if (capsuleTriangleSweep(resweepCap, curMove, tri, triIndex, hit) &&
                !rejectBelowTopFaceContact(resweepCap, tri, hit.normal, hit.point, triIndex, "post-depen-resweep") &&
                hit.time < resweepHit.time)
                resweepHit = hit;
        }

        if (resweepHit.hit && resweepHit.time < 1.0f)
        {
            trace.resweepHits++;
            glm::vec3 resweepStep = curMove * resweepHit.time;
            p.pos += resweepStep;
            p.pos += resweepHit.normal * SURFACE_SLOP;
            p.updateModelWorldTransforms();

            glm::vec3 afterStep = curMove - resweepStep;
            float into = glm::dot(afterStep, resweepHit.normal);
            if (into < 0.0f)
                afterStep -= resweepHit.normal * into;
            p.pos += afterStep;
            p.updateModelWorldTransforms();

            applyCollisionContact(
                p, groundedThisFrame,
                resweepHit.normal, resweepHit.point, SURFACE_SLOP,
                resweepHit.triangleIndex, "post-depen-resweep");
        }
        else
        {
            p.pos += curMove;
            p.updateModelWorldTransforms();
        }
        remainingMove = glm::vec3(0.0f);
    }
}
