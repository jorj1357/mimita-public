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

        if (!bwContacts.empty())
        {
            std::vector<RecoveryContact> pushContacts;
            for (const auto& c : bwContacts)
            {
                if (c.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                    pushContacts.push_back(c);
            }

            if (!pushContacts.empty())
            {
                PHYS_LOG("[PHYS][BODY-WEAPON] %zu contacts (%zu push) at final pos\n",
                         bwContacts.size(), pushContacts.size());

                Capsule bwRootCap = p.getCapsule();
                std::vector<int> bwRootCandidates = gatherGLBTriangles(world, bwRootCap, glm::vec3(0.0f));
                std::vector<RecoveryContact> bwRootContacts =
                    collectCapsuleRecoveryContacts(world, bwRootCap, bwRootCandidates);

                std::vector<RecoveryContact> allContacts;
                allContacts.insert(allContacts.end(), bwRootContacts.begin(), bwRootContacts.end());
                allContacts.insert(allContacts.end(), pushContacts.begin(), pushContacts.end());

                glm::vec3 combinedCorrection = solveBatchedCorrection(allContacts, 0.01f);
                float combLen = glm::length(combinedCorrection);
                if (combLen > 0.001f)
                {
                    constexpr float MAX_BW_CORRECTION = 0.5f;
                    if (combLen > MAX_BW_CORRECTION)
                        combinedCorrection *= MAX_BW_CORRECTION / combLen;

                    p.pos += combinedCorrection;
                    DebugVis::recordDepenetration(p.pos - combinedCorrection, combinedCorrection, "body-weapon-unified");

                    for (const RecoveryContact& pc : pushContacts)
                    {
                        if (pc.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                            projectVelocityAgainstNormal(p, pc.normal);
                    }
                }
            }
        }
    }
}
