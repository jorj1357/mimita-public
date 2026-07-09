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

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)
#define BODY_LOG(...) Debug::logThrottled(Debug::Category::Collision, "body-weapon", 1.0f, __VA_ARGS__)

static int runBodyWeaponPass(
    Player& p, const World& world, bool& groundedThisFrame,
    bool& groundedByWeapon, int pass, int maxPasses)
{
    gBW = BWInvestigate{};

    auto t0 = std::chrono::steady_clock::now();
    p.updateModelWorldTransforms();
    recomputeWeaponCapsule(p);
    std::vector<BodyWeaponSphere> bwSpheres = collectBodyWeaponSpheres(p);
    if (bwSpheres.empty()) return -1;

    char rootTag[64];
    std::snprintf(rootTag, sizeof(rootTag), "Player_Body_RootPass_%d", pass);
    Capsule rootCap = p.getCapsule();
    std::vector<int> sharedCandidates = gatherGLBTriangles(world, rootCap, p.aimDirection * 1.5f, rootTag);
    gBW.candidateCount = (int)sharedCandidates.size();

    std::vector<RecoveryContact> bwContacts = collectBodyWeaponContacts(p, world, sharedCandidates, bwSpheres);
    for (const auto& c : bwContacts) {
        p.ground.realWorldContactThisFrame = true;
        p.ground.hasWorldContact = true;
        p.ground.worldContactLostTimer = 0.033f;
        applyTouchResets(p);
    }

    if (bwContacts.empty())
        return (pass == 0) ? -1 : 0;

    std::vector<RecoveryContact> walkableContacts, bodyPushContacts;
    for (const auto& c : bwContacts) {
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT)
            walkableContacts.push_back(c);
        else
            bodyPushContacts.push_back(c);
    }

    // Walkable contacts: weapon contacts support player weight by default
    for (const RecoveryContact& wc : walkableContacts) {
        applyCollisionContact(p, groundedThisFrame, wc.normal, wc.point, wc.penetration, wc.triangleIndex, wc.label);
        if (!groundedByWeapon && wc.label && std::strcmp(wc.label, "weapon") == 0) {
            groundedByWeapon = true;
            groundedThisFrame = true;
            applyTouchResets(p);
            if (p.vel.z < 0.0f) p.vel.z = 0.0f;
        }
    }

    // Root capsule contacts
    std::vector<RecoveryContact> bwRootContacts = collectCapsuleRecoveryContacts(world, rootCap, sharedCandidates);

    // Solver: combine root + body push contacts
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
        }
    }

    if (pass == maxPasses - 1) {
        for (const auto& pc : bodyPushContacts) projectVelocityAgainstNormal(p, pc.normal);
    }

    auto t1 = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    BODY_LOG(
        "[BODY PASS] pass=%d spheres=%zu candidates=%zu contacts=%zu elapsedMs=%.2f\n",
        pass, bwSpheres.size(), sharedCandidates.size(),
        bwContacts.size(), totalMs);

    return (int)bwContacts.size();
}

void doBodyWeaponCollisionPhase(Player& p, const World& world, bool& groundedThisFrame)
{
    auto t0 = std::chrono::steady_clock::now();
    constexpr int MAX_PASSES = 3;
    int passesUsed = 0;
    bool groundedByWeapon = false;

    for (int pass = 0; pass < MAX_PASSES; ++pass) {
        int result = runBodyWeaponPass(p, world, groundedThisFrame, groundedByWeapon, pass, MAX_PASSES);
        if (result < 0) { passesUsed = pass + 1; break; }
        passesUsed = pass + 1;
    }

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    BODY_LOG("[BODY PHASE] passes=%d totalMs=%.2f\n", passesUsed, elapsedMs);
}
