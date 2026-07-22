// 07 21 2026, 21 45
/* purpose
* Tests generic physical-contact weapon shape overlap and sweep detection.
* Covers Godball-like spheres, Swordsword-like capsules, misses, and randomized bounded cases.
* Verifies contact output remains finite and target identity stays server-owned.
* Does NOT apply health, send damage confirmations, open sockets, or render weapon visuals.
* Does NOT test projectile collisions, hitscan traces, or client packet receive handling.
* Does NOT depend on local Godball or Swordsword presentation state.
*/

#include "combat/weapon-execution.h"

#include <cmath>
#include <cstdio>

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

static WeaponExecution::PlayerTarget makeTarget(uint32_t id, glm::vec3 position)
{
    WeaponExecution::PlayerTarget target;
    target.playerId = id;
    target.spawnGeneration = 3;
    target.position = position;
    target.radius = 0.65f;
    target.height = 3.5f;
    return target;
}

static void testSphereContact()
{
    WeaponExecution::PhysicalContactShape sphere;
    sphere.kind = WeaponExecution::PhysicalShapeKind::Sphere;
    sphere.previousA = glm::vec3(0.0f);
    sphere.currentA = glm::vec3(1.0f, 0.0f, 0.0f);
    sphere.radius = 0.5f;
    WeaponExecution::PhysicalContactHit hit;
    check(WeaponExecution::testPhysicalContact(sphere, makeTarget(2, glm::vec3(1.6f, 0.0f, 0.0f)), hit),
          "sphere overlaps target");
    check(hit.targetPlayerId == 2, "sphere hit preserves target id");
}

static void testSweptSphereContact()
{
    WeaponExecution::PhysicalContactShape sphere;
    sphere.kind = WeaponExecution::PhysicalShapeKind::Sphere;
    sphere.previousA = glm::vec3(-3.0f, 0.0f, 0.0f);
    sphere.currentA = glm::vec3(3.0f, 0.0f, 0.0f);
    sphere.radius = 0.25f;
    WeaponExecution::PhysicalContactHit hit;
    check(WeaponExecution::testPhysicalContact(sphere, makeTarget(3, glm::vec3(0.0f, 0.0f, 0.0f)), hit),
          "swept sphere detects tunneled contact");
}

static void testCapsuleContactAndMiss()
{
    WeaponExecution::PhysicalContactShape capsule;
    capsule.kind = WeaponExecution::PhysicalShapeKind::Capsule;
    capsule.previousA = glm::vec3(0.0f, -2.0f, 0.0f);
    capsule.previousB = glm::vec3(0.0f, 2.0f, 0.0f);
    capsule.currentA = glm::vec3(0.0f, -2.0f, 0.0f);
    capsule.currentB = glm::vec3(0.0f, 2.0f, 0.0f);
    capsule.radius = 0.35f;
    WeaponExecution::PhysicalContactHit hit;
    check(WeaponExecution::testPhysicalContact(capsule, makeTarget(4, glm::vec3(0.5f, 0.0f, 0.0f)), hit),
          "capsule contacts nearby target");
    WeaponExecution::PhysicalContactHit miss;
    check(!WeaponExecution::testPhysicalContact(capsule, makeTarget(5, glm::vec3(5.0f, 5.0f, 0.0f)), miss),
          "capsule misses far target");
}

static void testRandomizedContacts()
{
    uint32_t seed = 0x7134a2u;
    int cases = 2000;
    for (int i = 0; i < cases; ++i)
    {
        seed = seed * 1103515245u + 12345u;
        float x = ((int)(seed & 1023) - 512) / 64.0f;
        seed = seed * 1103515245u + 12345u;
        float y = ((int)(seed & 1023) - 512) / 64.0f;
        WeaponExecution::PhysicalContactShape shape;
        shape.kind = (i & 1) ? WeaponExecution::PhysicalShapeKind::Capsule
                             : WeaponExecution::PhysicalShapeKind::Sphere;
        shape.previousA = glm::vec3(x - 0.25f, y, 0.0f);
        shape.currentA = glm::vec3(x + 0.25f, y, 0.0f);
        shape.previousB = shape.previousA + glm::vec3(0.0f, 1.0f, 0.0f);
        shape.currentB = shape.currentA + glm::vec3(0.0f, 1.0f, 0.0f);
        shape.radius = 0.2f + (float)(seed & 31) / 100.0f;
        WeaponExecution::PhysicalContactHit hit;
        bool touched = WeaponExecution::testPhysicalContact(
            shape, makeTarget(6, glm::vec3(x, y, 0.0f)), hit);
        check(touched, "random local contact touches centered target");
        check(std::isfinite(hit.hitPosition.x) && std::isfinite(hit.normal.z),
              "random contact output finite");
    }
    std::printf("[physical-contact-weapon-test] randomized seed=0x%08x cases=%d\n", seed, cases);
}

int main()
{
    testSphereContact();
    testSweptSphereContact();
    testCapsuleContactAndMiss();
    testRandomizedContacts();
    if (gFailures)
    {
        std::printf("[physical-contact-weapon-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[physical-contact-weapon-test] PASS\n");
    return 0;
}
