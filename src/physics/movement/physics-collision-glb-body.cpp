#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)
#define BODY_LOG(...) Debug::logThrottled(Debug::Category::Collision, "body-weapon", 1.0f, __VA_ARGS__)

static int runBodyWeaponPass(
    Player& p, const World& world, bool& groundedThisFrame,
    const std::unordered_map<std::string, const WeaponColliderConfig*>& cfgMap,
    bool& groundedByWeapon, int pass, int maxPasses)
{
    gBW = BWInvestigate{};

    auto t0 = std::chrono::steady_clock::now();
    p.updateModelWorldTransforms();
    recomputeWeaponCapsule(p);
    std::vector<BodyWeaponSphere> bwSpheres = collectBodyWeaponSpheres(p);
    if (bwSpheres.empty()) return -1;

    // ── Single broadphase query using the PLAYER'S root capsule AABB ──
    // Expanded by the weapon's estimated reach (1.5m) to ensure weapon
    // tip triangles are included. Without this expansion the weapon can
    // miss collision detection and enter geometry.
    char rootTag[64];
    std::snprintf(rootTag, sizeof(rootTag), "Player_Body_RootPass_%d", pass);
    Capsule rootCap = p.getCapsule();
    std::vector<int> sharedCandidates = gatherGLBTriangles(world, rootCap, p.aimDirection * 1.5f, rootTag);
    gBW.candidateCount = (int)sharedCandidates.size();

    // ── Body/weapon sphere contact testing against shared candidates ──
    std::vector<RecoveryContact> bwContacts = collectBodyWeaponContacts(p, world, sharedCandidates, bwSpheres);
    for (const auto& c : bwContacts) {
        p.ground.realWorldContactThisFrame = true;
        p.ground.hasWorldContact = true;
        p.ground.worldContactLostTimer = 0.033f;
        applyTouchResets(p);
    }

    if (bwContacts.empty())
        return (pass == 0) ? -1 : 0;

    // ── Classify contacts ──
    // All body and weapon contacts push the player root — the weapon is as
    // authoritative as the player's own body. Only walkable contacts are
    // handled separately (grounding, not position push).
    std::vector<RecoveryContact> walkableContacts, bodyPushContacts, weaponPushContacts;
    for (const auto& c : bwContacts) {
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT) {
            walkableContacts.push_back(c);
        } else {
            bodyPushContacts.push_back(c);
        }
    }

    // ── Walkable contacts ──
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

    // ── Root capsule contacts (reuses the same shared candidates) ──
    std::vector<RecoveryContact> bwRootContacts = collectCapsuleRecoveryContacts(world, rootCap, sharedCandidates);

    // ── Solver: combines root + body push contacts ──
    std::vector<RecoveryContact> solverContacts;
    solverContacts.reserve(bwRootContacts.size() + bodyPushContacts.size());
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

    auto t1 = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // BW report
    Debug::logThrottled(Debug::Category::Collision, "bw-report", 1.0f,
        "[BW REPORT] pass=%d totalMs=%.2f\n"
        "  spheres=%d (body=%d weaponCapsule=%d config=%d colliders=%d) sharedCandidates=%d\n"
        "  triangleTests=%d sweepTests=%d staticTests=%d contacts=%d\n"
        "  collectSpheres=%.3fms collectContacts=%.3fms\n"
        "  sweepTriangle=%.3fms sphereContact=%.3fms\n",
        pass, totalMs,
        gBW.sphereCount, gBW.bodyPartSphereCount, gBW.weaponCapsuleSphereCount,
        gBW.configSphereCount, gBW.configColliderCount,
        gBW.candidateCount,
        gBW.triangleTests, gBW.sweepTests, gBW.staticTests, gBW.contactsProduced,
        gBW.collectSpheresMs, gBW.collectContactsMs,
        gBW.sweepSphereTriangleMs, gBW.sphereTriangleContactMs);

    BODY_LOG(
        "[BODY PASS] pass=%d spheres=%zu sharedCandidates=%zu "
        "bwContacts=%zu walkable=%zu bodyPush=%zu weaponPush=%zu rootContacts=%zu solverContacts=%zu elapsedMs=%.2f\n",
        pass, bwSpheres.size(), sharedCandidates.size(),
        bwContacts.size(), walkableContacts.size(), bodyPushContacts.size(), weaponPushContacts.size(),
        bwRootContacts.size(), solverContacts.size(), totalMs);

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
        if (dbgSet) appendChunkTrianglesForAABB(world, dbgBounds, 0.5f, dbgCandidates, "debugBodyWeaponPhase");
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
                    Debug::log(Debug::Category::Weapons, "[WEAPON_TIP_CONTACT] weapon=%s collider=%s tip=end pen=%.3f normal=(%.2f %.2f %.2f)\n",
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
    auto t0 = std::chrono::steady_clock::now();
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

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    BODY_LOG(
        "[BODY PHASE] passes=%d totalMs=%.2f groundedByWeapon=%d\n",
        passesUsed, elapsedMs, (int)groundedByWeapon);

    if (DebugConfig::DEBUG_WEAPON_COLLISION)
        debugBodyWeaponPhase(p, world, cfgMap, groundedByWeapon, passesUsed);
}
