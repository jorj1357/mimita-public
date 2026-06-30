#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config.h"

#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

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
            //   walkableContacts     → applyCollisionContact for floor contact detection
            //   bodyPushContacts     → combined with root contacts → pushes root position
            //   weaponPushContacts   → velocity projection only (no position push)
            //
            // Weapon contacts from configurable colliders (non-"weapon" labels) are treated
            // as body push contacts when the collision config says push_player_root is true.
            std::vector<RecoveryContact> walkableContacts;
            std::vector<RecoveryContact> bodyPushContacts;
            std::vector<RecoveryContact> weaponPushContacts;
            bool weaponCfgPushes = p.weaponCollisionConfig.enabled && p.weaponCollisionConfig.authoritative;

            // Build a name->config lookup so we can check per-collider flags
            std::unordered_map<std::string, const WeaponColliderConfig*> colliderConfigMap;
            for (const auto& cc : p.weaponCollisionConfig.colliders)
                colliderConfigMap[cc.name] = &cc;

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
                else if (c.label && weaponCfgPushes)
                {
                    // Configurable weapon collider contact → push root like body parts
                    bodyPushContacts.push_back(c);
                }
                else
                {
                    bodyPushContacts.push_back(c);
                }
            }

            // Walkable body contacts set floor contact state (foot on floor).
            // This uses the standard feet-proximity check; weapon contacts far
            // above feet will be rejected for grounding here.
            bool groundedByWeapon = false;
            for (const RecoveryContact& wc : walkableContacts)
            {
                applyCollisionContact(
                    p, groundedThisFrame,
                    wc.normal, wc.point, wc.penetration,
                    wc.triangleIndex, wc.label);

                // If this is a weapon collider contact with support_player_weight=true,
                // force grounding even if the contact is above the player's feet.
                if (!groundedByWeapon && wc.label && std::strcmp(wc.label, "weapon") != 0) {
                    auto cfgIt = colliderConfigMap.find(wc.label);
                    if (cfgIt != colliderConfigMap.end() && cfgIt->second->supportPlayerWeight) {
                        groundedByWeapon = true;
                        groundedThisFrame = true;
                        applyTouchResets(p);
                        if (p.vel.z < 0.0f) p.vel.z = 0.0f;
                        DebugVis::recordGroundNormal(wc.point, wc.normal, wc.label);
                    }
                }
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

            for (const RecoveryContact& pc : bodyPushContacts)
                projectVelocityAgainstNormal(p, pc.normal);
            for (const RecoveryContact& pc : weaponPushContacts)
                projectVelocityAgainstNormal(p, pc.normal);

            // Debug console logging for weapon colliders
            if (DebugConfig::DEBUG_WEAPON_COLLISION)
            {
                static float wcDebugTimer = 0.0f;
                wcDebugTimer -= 0.016f;
                if (wcDebugTimer <= 0.0f) {
                    wcDebugTimer = 0.25f;
                    int contactCount = 0, pushCount = 0, supportCount = 0, blockCount = 0;
                    float maxPen = 0.0f;
                    for (const auto& c : bwContacts) {
                        if (c.label && std::strcmp(c.label, "weapon") == 0) continue;
                        ++contactCount;
                        if (c.penetration > maxPen) maxPen = c.penetration;
                        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT) ++supportCount;
                        else ++blockCount;
                        auto cfgIt = colliderConfigMap.find(c.label ? c.label : "");
                        bool pushes = cfgIt != colliderConfigMap.end() && cfgIt->second->pushPlayerRoot;
                        if (pushes) ++pushCount;
                        printf("[WEAPON_COLLISION]   contact label=%s pen=%.3f normal=(%.2f %.2f %.2f) push=%d support=%d\n",
                               c.label ? c.label : "?", c.penetration, c.normal.x, c.normal.y, c.normal.z,
                               (int)pushes, (int)(c.normal.z > MAX_WALKABLE_SLOPE_DOT));
                    }
                    printf("[WEAPON_COLLISION] colliders=%zu contacts=%d push=%d support=%d block=%d maxPen=%.3f groundedByWeapon=%d\n",
                           p.weaponCollisionConfig.colliders.size(), contactCount, pushCount, supportCount, blockCount,
                           maxPen, (int)groundedByWeapon);

                    // Print per-collider support info
                    for (const auto& c : bwContacts) {
                        if (c.label && std::strcmp(c.label, "weapon") == 0) continue;
                        if (c.normal.z <= MAX_WALKABLE_SLOPE_DOT) continue;
                        auto cfgIt = colliderConfigMap.find(c.label ? c.label : "");
                        if (cfgIt != colliderConfigMap.end() && cfgIt->second->supportPlayerWeight) {
                            printf("[WEAPON_SUPPORT] weapon=%s collider=%s normal=(%.2f %.2f %.2f) pen=%.3f groundedByWeapon=%d\n",
                                   p.equippedWeaponId.c_str(), c.label ? c.label : "?",
                                   c.normal.x, c.normal.y, c.normal.z, c.penetration, (int)groundedByWeapon);
                        }
                    }
                }
            }
        }
    }
}
