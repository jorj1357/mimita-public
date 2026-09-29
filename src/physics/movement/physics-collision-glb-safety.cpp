// 07 21 2026, 17 25
/* purpose
* Runs the remaining legacy GLB floor-recovery pass.
* Emits movement contact facts from recovery contacts without changing safety correction behavior.
* Does NOT own movement reset formulas, networking, rendering effects, audio, or damage.
* Does NOT change projectile, weapon, death, respawn, ICE, or packet behavior.
* Does NOT replace sweep-slide, body, or block collision phases.
*/

#include "physics/movement/physics-collision-glb-safety.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "physics/config.h"
#include "config/collision-config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"

#include <chrono>
#include <cfloat>
#include <glm/glm.hpp>
#include <vector>

#define SAFETY_LOG(...) Debug::logThrottled(Debug::Category::Collision, "safety-pass", 1.0f, __VA_ARGS__)

// TODO-DELETE: doFloorRecovery — legacy capsule lift out of a walkable surface.
// Superseded by solveActorTriangleCollision, which depenetrates the actor's real
// triangles along the world normal (and the triangle path no longer needs the
// capsule feet). Called by doGLBTriangleCollisions only when the
// "actorTriangleSolver" toggle is off.
// DO NOT DELETE until the legacy pipeline is removed AND human gameplay testing
// proves the triangle path in Dust 3 Siberia, Trainkinda, and Chain of Judgement.
void doFloorRecovery(Player& p, const World& world, bool& groundedThisFrame)
{
    auto t0 = std::chrono::steady_clock::now();
    {
        p.updateModelWorldTransforms();
        Capsule fCap = p.getCapsule();
        float feetZ = fCap.a.z - fCap.r;
        std::vector<int> fCandidates = gatherGLBTriangles(world, fCap, glm::vec3(0.0f), "Player_Capsule_FloorRecovery");

        float bestFloorZ = -FLT_MAX;
        int bestFloorTri = -1;
        for (int triIndex : fCandidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT) continue;
            glm::vec3 footPoint(fCap.a.x, fCap.a.y, feetZ);
            glm::vec3 closest = closestPointOnTriangle(footPoint, tri.a, tri.b, tri.c);
            float horizDist2 = (closest.x - fCap.a.x) * (closest.x - fCap.a.x)
                             + (closest.y - fCap.a.y) * (closest.y - fCap.a.y);
            if (horizDist2 > fCap.r * fCap.r) continue;
            if (closest.z > bestFloorZ && closest.z < fCap.a.z + 1.0f)
            {
                bestFloorZ = closest.z;
                bestFloorTri = triIndex;
            }
        }

        float liftAmount = 0.0f;
        if (bestFloorZ > -FLT_MAX && feetZ < bestFloorZ - 0.01f)
        {
            float lift = bestFloorZ - feetZ + 0.005f;
            if (lift > 0.0f && lift < 0.5f)
            {
                p.pos.z += lift;
                p.externalImpulse.z = std::min(p.externalImpulse.z, 0.0f);
                p.vel.z = std::min(p.vel.z, 0.0f);
                groundedThisFrame = true;
                liftAmount = lift;
                const CollisionTriangle& tri = world.collisionMesh.triangles[bestFloorTri];
                appendPlayerMovementContact(
                    p,
                    MovementContactKind::Ground,
                    tri.normal,
                    glm::vec3(fCap.a.x, fCap.a.y, bestFloorZ),
                    lift,
                    bestFloorTri);
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        SAFETY_LOG(
            "[FLOOR RECOVERY] candidates=%zu feetZ=%.3f bestFloorZ=%.3f lift=%.4f tri=%d elapsedMs=%.2f\n",
            fCandidates.size(), feetZ, bestFloorZ, liftAmount, bestFloorTri, elapsedMs);
    }
}
