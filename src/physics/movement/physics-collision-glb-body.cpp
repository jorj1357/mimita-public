#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"

#include <cstdio>
#include <glm/glm.hpp>
#include <vector>

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

void doBodyWeaponCollisionPhase(Player& p, const World& world, bool& groundedThisFrame)
{
    p.updateModelWorldTransforms();
    recomputeWeaponCapsule(p);
    std::vector<BodyWeaponSphere> bwSpheres = collectBodyWeaponSpheres(p);

    if (!bwSpheres.empty())
    {
        AABB bwBounds;
        bool boundsSet = false;
        for (const auto& bs : bwSpheres)
        {
            glm::vec3 expand(bs.radius + 0.5f);
            if (!boundsSet) {
                bwBounds.min = bs.center - expand;
                bwBounds.max = bs.center + expand;
                boundsSet = true;
            } else {
                bwBounds.min = glm::min(bwBounds.min, bs.center - expand);
                bwBounds.max = glm::max(bwBounds.max, bs.center + expand);
            }
        }

        std::vector<int> bwCandidates;
        if (boundsSet)
            appendChunkTrianglesForAABB(world, bwBounds, 0.5f, bwCandidates);

        std::vector<RecoveryContact> bwContacts =
            collectBodyWeaponContacts(p, world, bwCandidates, bwSpheres);

        // Every body-weapon contact with world geometry resets abilities.
        // The collision manifold already contains only world triangles (GLB mesh),
        // so no entity filtering (players/NPCs/projectiles) is needed.
        for (const auto& c : bwContacts)
        {
            p.ground.realWorldContactThisFrame = true;
            p.ground.hasWorldContact = true;
            p.ground.worldContactLostTimer = 0.033f;
            applyTouchResets(p);
        }

        if (!bwContacts.empty())
        {
            PHYS_LOG("[PHYS][BODY-WEAPON] %zu contacts total\n", bwContacts.size());

            // Separate contacts by type:
            //   walkableContacts     → applyCollisionContact for grounded detection
            //   bodyPushContacts     → combined with root contacts → pushes root position
            //   weaponPushContacts   → velocity projection only (no position push)
            std::vector<RecoveryContact> walkableContacts;
            std::vector<RecoveryContact> bodyPushContacts;
            std::vector<RecoveryContact> weaponPushContacts;

            for (const auto& c : bwContacts)
            {
                if (c.normal.z > MAX_WALKABLE_SLOPE_DOT)
                {
                    walkableContacts.push_back(c);
                }
                else if (c.label && std::strcmp(c.label, "weapon") == 0)
                {
                    weaponPushContacts.push_back(c);
                }
                else
                {
                    bodyPushContacts.push_back(c);
                }
            }

            // Walkable body contacts set grounded state (e.g., foot on floor).
            for (const RecoveryContact& wc : walkableContacts)
            {
                applyCollisionContact(
                    p, groundedThisFrame,
                    wc.normal, wc.point, wc.penetration,
                    wc.triangleIndex, wc.label);
            }

            // Root capsule contacts + body push contacts combine into one solver.
            // Body contacts (head, torso, arms, legs) directly push the root capsule
            // so limbs cannot pass through walls. The skeleton follows the root, so
            // moving the root moves all body parts.
            // Weapon contacts are EXCLUDED from the solver to avoid snagging.
            Capsule bwRootCap = p.getCapsule();
            std::vector<int> bwRootCandidates = gatherGLBTriangles(world, bwRootCap, glm::vec3(0.0f));
            std::vector<RecoveryContact> bwRootContacts =
                collectCapsuleRecoveryContacts(world, bwRootCap, bwRootCandidates);

            {
                std::vector<RecoveryContact> solverContacts;
                solverContacts.insert(solverContacts.end(), bwRootContacts.begin(), bwRootContacts.end());
                solverContacts.insert(solverContacts.end(), bodyPushContacts.begin(), bodyPushContacts.end());

                if (!solverContacts.empty())
                {
                    glm::vec3 correction = solveBatchedCorrection(solverContacts, 0.01f);
                    float corrLen = glm::length(correction);
                    if (corrLen > 0.001f)
                    {
                        constexpr float MAX_BW_CORRECTION = 0.5f;
                        if (corrLen > MAX_BW_CORRECTION)
                            correction *= MAX_BW_CORRECTION / corrLen;

                        p.pos += correction;
                        DebugVis::recordDepenetration(
                            p.pos - correction, correction, "body-weapon-combined");
                    }
                }
            }

            // Project velocity against all non-walkable contacts for smooth sliding.
            // Weapons get velocity-only response (no position push) — this preserves
            // the feel of the weapon touching geometry without snagging.
            for (const RecoveryContact& pc : bodyPushContacts)
                projectVelocityAgainstNormal(p, pc.normal);
            for (const RecoveryContact& pc : weaponPushContacts)
                projectVelocityAgainstNormal(p, pc.normal);
        }
    }
}
