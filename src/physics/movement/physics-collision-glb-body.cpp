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

                // Root capsule contacts correction (separate from weapon/body contacts).
                // Weapon/body contacts are NOT merged into the root solver — doing so would
                // cause weapon-wall contact to push the root capsule, creating snagging.
                Capsule bwRootCap = p.getCapsule();
                std::vector<int> bwRootCandidates = gatherGLBTriangles(world, bwRootCap, glm::vec3(0.0f));
                std::vector<RecoveryContact> bwRootContacts =
                    collectCapsuleRecoveryContacts(world, bwRootCap, bwRootCandidates);

                if (!bwRootContacts.empty())
                {
                    glm::vec3 rootCorrection = solveBatchedCorrection(bwRootContacts, 0.01f);
                    float rootLen = glm::length(rootCorrection);
                    if (rootLen > 0.001f)
                    {
                        constexpr float MAX_BW_CORRECTION = 0.5f;
                        if (rootLen > MAX_BW_CORRECTION)
                            rootCorrection *= MAX_BW_CORRECTION / rootLen;

                        p.pos += rootCorrection;
                        DebugVis::recordDepenetration(p.pos - rootCorrection, rootCorrection, "body-weapon-root");
                    }
                }

                // Project velocity against weapon/body push contacts for smooth sliding.
                // This lets the player feel the weapon touching geometry without the
                // weapon pushing the root capsule position.
                for (const RecoveryContact& pc : pushContacts)
                {
                    if (pc.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                        projectVelocityAgainstNormal(p, pc.normal);
                }
            }
        }
    }
}
