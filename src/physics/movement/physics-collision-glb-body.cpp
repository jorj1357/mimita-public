// 07 21 2026, 17 25
/* purpose
* Runs local body-part and equipped-weapon collision against GLB world geometry.
* Emits typed body/weapon-sourced movement contacts while preserving correction behavior.
* Keeps body/weapon collision diagnostics bounded and local to the collision system.
* Does NOT own movement reset formulas, weapon firing, damage, packets, rendering, or audio.
* Does NOT network body or weapon contacts, projectile hits, or explosion resets.
* Does NOT replace root capsule, sweep-slide, safety, or block collision phases.
*/

#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config.h"
#include "effects/effect-part.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)
#define BODY_LOG(...) Debug::logThrottled(Debug::Category::Collision, "body-weapon", 1.0f, __VA_ARGS__)

static MovementContactSource bodyWeaponContactSource(const char* label)
{
    return label && std::strcmp(label, "weapon") == 0
        ? MovementContactSource::Weapon
        : MovementContactSource::PlayerBody;
}

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

    std::vector<RecoveryContact> bwContacts = collectBodyWeaponContacts(p, world, bwSpheres);

    // Weapon capsule: the weapon's solid bounding volume, tested exactly against
    // nearby world triangles. The single smooth capsule has no discrete gaps, so
    // it slides over edges/corners instead of catching, and the solver pushes the
    // player so the weapon never sinks into a surface. JSON spheres are the
    // opt-in legacy path (source "json").
    if (p.weaponCollisionDebug.capsuleMode && p.collision.hasWeaponCollisionCapsule) {
        std::vector<int> weaponCands = gatherGLBTriangles(
            world, p.weaponCollisionCapsule, glm::vec3(0.0f), "weaponGather");
        std::vector<RecoveryContact> weaponCapsuleContacts =
            collectCapsuleRecoveryContacts(world, p.weaponCollisionCapsule, weaponCands, "weapon");
        gBW.weaponCapsuleContactCount = (int)weaponCapsuleContacts.size();
        bwContacts.insert(bwContacts.end(), weaponCapsuleContacts.begin(), weaponCapsuleContacts.end());
    }

    for (const auto& c : bwContacts) {
        p.ground.realWorldContactThisFrame = true;
        p.ground.hasWorldContact = true;
        p.ground.worldContactLostTimer = 0.033f;
        appendPlayerMovementContactForNormal(
            p,
            c.normal.z > MAX_WALKABLE_SLOPE_DOT,
            false,
            c.normal,
            c.point,
            c.penetration,
            c.triangleIndex,
            bodyWeaponContactSource(c.label));

        if (c.label && std::strcmp(c.label, "weapon") != 0 &&
            p.bodySparkTick != p.movementSimulationTick) {
            EffectPart* spawned = EffectPartSystem::instance().spawnBodyContactSpark(p.pos, c.point, p.vel, 0.1f);
            if (spawned) p.bodySparkTick = p.movementSimulationTick;
        }
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
        glm::vec3 correction = solveBatchedCorrection(solverContacts, 0.01f, nullptr, nullptr, p.vel);
        // NaN/infinity guard
        if (!std::isfinite(correction.x) || !std::isfinite(correction.y) || !std::isfinite(correction.z)) {
            BODY_LOG("[BODY WEAPON SAFETY] NaN correction in pass %d — zeroing", pass);
            correction = glm::vec3(0.0f);
        }
        float corrLen = glm::length(correction);
        if (corrLen > 0.001f) {
            constexpr float MAX_CORR_PER_PASS = 0.5f;
            if (corrLen > MAX_CORR_PER_PASS) correction *= MAX_CORR_PER_PASS / corrLen;
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
    constexpr float MAX_TOTAL_CORRECTION = 1.5f;
    int passesUsed = 0;
    bool groundedByWeapon = false;
    glm::vec3 totalCorrection(0.0f);

    for (int pass = 0; pass < MAX_PASSES; ++pass) {
        glm::vec3 beforePos = p.pos;
        int result = runBodyWeaponPass(p, world, groundedThisFrame, groundedByWeapon, pass, MAX_PASSES);
        if (result < 0) { passesUsed = pass + 1; break; }
        passesUsed = pass + 1;
        totalCorrection += p.pos - beforePos;
    }

    // Clamp total correction for the entire phase
    float totalCorrLen = glm::length(totalCorrection);
    if (totalCorrLen > MAX_TOTAL_CORRECTION) {
        BODY_LOG("[BODY WEAPON SAFETY] total correction %.3f exceeds max %.3f — clamping",
            totalCorrLen, MAX_TOTAL_CORRECTION);
        // Undo excess: scale back the final position
        glm::vec3 excess = totalCorrection * (1.0f - MAX_TOTAL_CORRECTION / totalCorrLen);
        p.pos -= excess;
    }

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    BODY_LOG("[BODY PHASE] passes=%d totalMs=%.2f totalCorr=%.3f\n",
        passesUsed, elapsedMs, totalCorrLen);
}
