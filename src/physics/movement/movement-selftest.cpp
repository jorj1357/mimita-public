// 09 14 2026
/* purpose
* Implements the kernel capsule movement self-test.
* Does NOT run the game or own movement policy.
*/
#include "physics/movement/movement-selftest.h"

#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "physics/movement/move-capsule.h"
#include "physics/physical-body.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void addFloor(World& world)
{
    CollisionTriangle tri;
    tri.a = glm::vec3(-50.0f, -50.0f, 0.0f);
    tri.b = glm::vec3(50.0f, -50.0f, 0.0f);
    tri.c = glm::vec3(0.0f, 50.0f, 0.0f);
    tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    world.collisionMesh.triangles.push_back(tri);
    world.collisionMesh.boundsMin = glm::vec3(-50.0f, -50.0f, 0.0f);
    world.collisionMesh.boundsMax = glm::vec3(50.0f, 50.0f, 0.0f);
}

} // namespace

bool runMovementSelfTest(std::string& report)
{
    bool ok = true;

    // Free-fall determinism + gravity (no world).
    MovementStateV1 a{};
    a.position[2] = 10.0f;
    a.radius = 0.4f;
    a.halfHeight = 0.9f;
    MovementStateV1 b = a;
    for (int i = 0; i < 120; ++i) {
        Physics::moveCapsuleStep(a, nullptr, kDt);
        Physics::moveCapsuleStep(b, nullptr, kDt);
    }
    ok &= check(a.position[2] == b.position[2] && a.velocity[2] == b.velocity[2],
                "free-fall deterministic", report);
    ok &= check(a.velocity[2] < -1.0f && a.position[2] < 10.0f, "gravity applied", report);
    ok &= check(std::isfinite(a.position[2]) && std::isfinite(a.velocity[2]),
                "state is finite", report);

    // Parity with a reference integrate() (no world).
    MovementStateV1 c{};
    c.position[2] = 10.0f;
    c.radius = 0.4f;
    c.halfHeight = 0.9f;
    Physics::moveCapsuleStep(c, nullptr, kDt);
    RigidBody ref;
    ref.position = glm::vec3(0.0f, 0.0f, 10.0f);
    ref.capsuleRadius = 0.4f;
    ref.capsuleHalfHeight = 0.9f;
    setBodyMass(ref, 1.0f);
    integrate(ref, glm::vec3(0.0f, 0.0f, -9.81f), kDt);
    ok &= check(std::fabs(c.position[2] - ref.position.z) < 1e-4f &&
                    std::fabs(c.velocity[2] - ref.linearVelocity.z) < 1e-4f,
                "matches integrate() reference", report);
    ok &= check(c.grounded == 0, "not grounded without world", report);

    // Floor collision: a capsule above a floor settles instead of falling through.
    World world;
    addFloor(world);
    MovementStateV1 s{};
    s.position[2] = 2.0f;
    s.radius = 0.4f;
    s.halfHeight = 0.9f;
    bool everGrounded = false;
    for (int i = 0; i < 300; ++i) {
        Physics::moveCapsuleStep(s, &world, kDt);
        if (s.grounded)
            everGrounded = true;
    }
    ok &= check(s.position[2] > 0.5f, "capsule does not fall through floor", report);
    ok &= check(everGrounded, "capsule becomes grounded on floor", report);
    ok &= check(std::fabs(s.velocity[2]) < 0.5f, "vertical velocity settles", report);

    return ok;
}
