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

static int runBodyWeaponPass(
    Player& p, const World& world, bool& groundedThisFrame,
    const std::unordered_map<std::string, const WeaponColliderConfig*>& cfgMap,
    bool& groundedByWeapon, int pass, int maxPasses)
{
    p.updateModelWorldTransforms();
    recomputeWeaponCapsule(p);
    std::vector<BodyWeaponSphere> bwSpheres = collectBodyWeaponSpheres(p);
    if (bwSpheres.empty()) return -1;

    AABB bwBounds;
    bool boundsSet = false;
    for (const auto& bs : bwSpheres) {
        glm::vec3 expand(bs.radius + 0.5f);
        if (!boundsSet) { bwBounds.min = bs.center - expand; bwBounds.max = bs.center + expand; boundsSet = true; }
        else { bwBounds.min = glm::min(bwBounds.min, bs.center - expand); bwBounds.max = glm::max(bwBounds.max, bs.center + expand); }
    }

    std::vector<int> bwCandidates;
    if (boundsSet)
        appendChunkTrianglesForAABB(world, bwBounds, 0.5f, bwCandidates);

    std::vector<RecoveryContact> bwContacts = collectBodyWeaponContacts(p, world, bwCandidates, bwSpheres);
    for (const auto& c : bwContacts) {
        p.ground.realWorldContactThisFrame = true;
        p.ground.hasWorldContact = true;
        p.ground.worldContactLostTimer = 0.033f;
        applyTouchResets(p);
    }

    if (bwContacts.empty())
        return (pass == 0) ? -1 : 0;

    PHYS_LOG("[PHYS][BODY-WEAPON] pass=%d contacts=%zu\n", pass, bwContacts.size());

    std::vector<RecoveryContact> walkableContacts, bodyPushContacts, weaponPushContacts;
    for (const auto& c : bwContacts) {
        bool isConfigWeapon = c.label && std::strcmp(c.label, "weapon") != 0;
        const WeaponColliderConfig* cfgPtr = nullptr;
        if (isConfigWeapon) {
            auto it = cfgMap.find(c.label);
            if (it != cfgMap.end()) cfgPtr = it->second;
        }
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT) {
            walkableContacts.push_back(c);
        } else if (isConfigWeapon && cfgPtr) {
            if (cfgPtr->blocksWorld || cfgPtr->pushPlayerRoot)
                bodyPushContacts.push_back(c);
            else
                weaponPushContacts.push_back(c);
        } else if (c.label && std::strcmp(c.label, "weapon") == 0) {
            weaponPushContacts.push_back(c);
        } else {
            bodyPushContacts.push_back(c);
        }
    }

    for (const RecoveryContact& wc : walkableContacts) {
        applyCollisionContact(p, groundedThisFrame, wc.normal, wc.point, wc.penetration, wc.triangleIndex, wc.label);
        if (!groundedByWeapon && wc.label && std::strcmp(wc.label, "weapon") != 0) {
            auto it = cfgMap.find(wc.label);
            if (it != cfgMap.end() && it->second->supportPlayerWeight) {
                groundedByWeapon = true;
                groundedThisFrame = true;
                applyTouchResets(p);
                if (p.vel.z < 0.0f) p.vel.z = 0.0f;
                DebugVis::recordGroundNormal(wc.point, wc.normal, wc.label);
            }
        }
    }

    Capsule bwRootCap = p.getCapsule();
    std::vector<int> bwRootCandidates = gatherGLBTriangles(world, bwRootCap, glm::vec3(0.0f));
    std::vector<RecoveryContact> bwRootContacts = collectCapsuleRecoveryContacts(world, bwRootCap, bwRootCandidates);

    std::vector<RecoveryContact> solverContacts;
    solverContacts.insert(solverContacts.end(), bwRootContacts.begin(), bwRootContacts.end());
    solverContacts.insert(solverContacts.end(), bodyPushContacts.begin(), bodyPushContacts.end());

    if (!solverContacts.empty()) {
        glm::vec3 correction = solveBatchedCorrection(solverContacts, 0.01f);
        float corrLen = glm::length(correction);
        if (corrLen > 0.001f) {
            constexpr float MAX_CORR = 0.5f;
            if (corrLen > MAX_CORR) correction *= MAX_CORR / corrLen;
            p.pos += correction;
            DebugVis::recordDepenetration(p.pos - correction, correction, "body-weapon-combined");
        }
    }

    if (pass == maxPasses - 1) {
        for (const auto& pc : bodyPushContacts) projectVelocityAgainstNormal(p, pc.normal);
        for (const auto& pc : weaponPushContacts) projectVelocityAgainstNormal(p, pc.normal);
    }

    return (int)bwContacts.size();
}

static void debugBodyWeaponPhase(
    Player& p, const World& world,
    const std::unordered_map<std::string, const WeaponColliderConfig*>& cfgMap,
    bool groundedByWeapon, int passesUsed)
{
    static float wcDebugTimer = 0.0f;
    wcDebugTimer -= 0.016f;
    if (wcDebugTimer > 0.0f) return;
    wcDebugTimer = 0.25f;

    p.updateModelWorldTransforms();
    recomputeWeaponCapsule(p);
    auto debugSpheres = collectBodyWeaponSpheres(p);

    int totalContacts = 0, pushCount = 0, supportCount = 0, blockCount = 0;
    float maxPen = 0.0f;
    bool tipPenetrating = false;
    const char* tipCollider = nullptr;

    if (!debugSpheres.empty()) {
        AABB dbgBounds;
        bool dbgSet = false;
        for (const auto& bs : debugSpheres) {
            glm::vec3 expand(bs.radius + 0.5f);
            if (!dbgSet) { dbgBounds.min = bs.center - expand; dbgBounds.max = bs.center + expand; dbgSet = true; }
            else { dbgBounds.min = glm::min(dbgBounds.min, bs.center - expand); dbgBounds.max = glm::max(dbgBounds.max, bs.center + expand); }
        }
        std::vector<int> dbgCandidates;
        if (dbgSet) appendChunkTrianglesForAABB(world, dbgBounds, 0.5f, dbgCandidates);
        auto dbgContacts = collectBodyWeaponContacts(p, world, dbgCandidates, debugSpheres);
        for (const auto& c : dbgContacts) {
            if (c.label && std::strcmp(c.label, "weapon") == 0) continue;
            ++totalContacts;
            if (c.penetration > maxPen) maxPen = c.penetration;
            if (c.normal.z > MAX_WALKABLE_SLOPE_DOT) ++supportCount; else ++blockCount;
            auto it = cfgMap.find(c.label ? c.label : "");
            if (it != cfgMap.end() && it->second->pushPlayerRoot) ++pushCount;
            // printf("[WEAPON_COLLISION]   contact label=%s pen=%.3f normal=(%.2f %.2f %.2f)\n",
            //        c.label ? c.label : "?", c.penetration, c.normal.x, c.normal.y, c.normal.z);
        }
        for (const auto& wc : p.weaponCollisionConfig.colliders) {
            glm::mat4 local(1.0f);
            local = glm::translate(local, wc.position);
            local = glm::rotate(local, glm::radians(wc.rotationDegrees.x), glm::vec3(1,0,0));
            local = glm::rotate(local, glm::radians(wc.rotationDegrees.y), glm::vec3(0,1,0));
            local = glm::rotate(local, glm::radians(wc.rotationDegrees.z), glm::vec3(0,0,1));
            glm::mat4 wX = p.weaponCollisionWorld * local;
            glm::vec3 hs = wc.size * 0.5f;
            float ex = std::fabs(hs.x), ey = std::fabs(hs.y), ez = std::fabs(hs.z);
            int da = 0; float dl = ex;
            if (ey > dl) { da = 1; dl = ey; }
            if (ez > dl) { da = 2; dl = ez; }
            glm::vec3 ad(0.0f); ad[da] = 1.0f;
            glm::vec3 wa = glm::normalize(glm::vec3(wX * glm::vec4(ad, 0.0f)));
            glm::vec3 tipPos = glm::vec3(wX * glm::vec4(0,0,0,1)) + wa * dl;
            for (int triIdx : dbgCandidates) {
                if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size()) continue;
                const auto& tri = world.collisionMesh.triangles[triIdx];
                float r = std::max(std::min(hs.x, hs.y), 0.10f);
                Contact tipContact;
                if (sphereTriangleContact(tipPos, r, tri, tipContact) && tipContact.penetration > 0.002f) {
                    tipPenetrating = true;
                    tipCollider = wc.name.c_str();
                    printf("[WEAPON_TIP_CONTACT] weapon=%s collider=%s tip=end pen=%.3f normal=(%.2f %.2f %.2f)\n",
                           p.equippedWeaponId.c_str(), wc.name.c_str(),
                           tipContact.penetration, tipContact.normal.x, tipContact.normal.y, tipContact.normal.z);
                }
            }
        }
    }

    // printf("[WEAPON_COLLISION] weapon=%s colliders=%zu samples=%zu contacts=%d push=%d support=%d block=%d"
    //        " maxPen=%.3f groundedByWeapon=%d tipPen=%d tipCollider=%s passes=%d\n",
    //        p.equippedWeaponId.c_str(), p.weaponCollisionConfig.colliders.size(), debugSpheres.size(),
    //        totalContacts, pushCount, supportCount, blockCount, maxPen,
    //        (int)groundedByWeapon, (int)tipPenetrating, tipCollider ? tipCollider : "none", passesUsed);
}

void doBodyWeaponCollisionPhase(Player& p, const World& world, bool& groundedThisFrame)
{
    constexpr int MAX_PASSES = 3;
    int passesUsed = 0;
    bool groundedByWeapon = false;

    std::unordered_map<std::string, const WeaponColliderConfig*> cfgMap;
    for (const auto& cc : p.weaponCollisionConfig.colliders)
        cfgMap[cc.name] = &cc;

    for (int pass = 0; pass < MAX_PASSES; ++pass) {
        int result = runBodyWeaponPass(p, world, groundedThisFrame, cfgMap, groundedByWeapon, pass, MAX_PASSES);
        if (result < 0) { passesUsed = pass + 1; break; }
        passesUsed = pass + 1;
    }

    if (DebugConfig::DEBUG_WEAPON_COLLISION)
        debugBodyWeaponPhase(p, world, cfgMap, groundedByWeapon, passesUsed);
}
