// 09 29 2026
/* purpose
* Verify the pure physical-aim controller math in src/ragdoll/physical-aim.h:
* sign, zero-error, no-snap, speed cap, and damping behavior.
* Does NOT test the solver, joints, collision, config parsing, or rendering.
*/

#include <cstdio>
#include <cmath>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ragdoll/physical-aim.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-52s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

static float vecAngle(const glm::vec3& a, const glm::vec3& b)
{
    float la = glm::length(a), lb = glm::length(b);
    if (la < 1e-6f || lb < 1e-6f) return 0.0f;
    return std::acos(glm::clamp(glm::dot(a / la, b / lb), -1.0f, 1.0f));
}

// World-space integration step for the test only.
static glm::quat integrate(const glm::quat& q, const glm::vec3& w, float dt)
{
    float speed = glm::length(w);
    if (speed < 1e-8f) return glm::normalize(q);
    glm::quat dq = glm::angleAxis(speed * dt, w / speed);
    return glm::normalize(dq * q);
}

static void testZeroError()
{
    TEST("zero error -> near-zero torque");
    PhysicalAimConfig cfg;
    glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 torque = computeAimTorque(id, id, glm::vec3(0.0f), cfg, 1.0f);
    CHECK(glm::length(torque) < 1e-5f, "torque=%.6f", (double)glm::length(torque));
    PASS();
}

static void testLookRightLeft()
{
    TEST("look right and left produce opposite torque");
    PhysicalAimConfig cfg;
    glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::quat right = aimLookRotation(glm::vec3(1.0f, 0.0f, 0.0f), up);
    glm::quat left = aimLookRotation(glm::vec3(-1.0f, 0.0f, 0.0f), up);

    glm::vec3 tRight = computeAimTorque(id, right, glm::vec3(0.0f), cfg, 1.0f);
    glm::vec3 tLeft = computeAimTorque(id, left, glm::vec3(0.0f), cfg, 1.0f);

    CHECK(glm::length(tRight) > 1.0f && glm::length(tLeft) > 1.0f,
          "torque magnitudes right=%.3f left=%.3f",
          (double)glm::length(tRight), (double)glm::length(tLeft));
    CHECK(glm::dot(tRight, tLeft) < 0.0f,
          "torques should oppose: dot=%.3f", (double)glm::dot(tRight, tLeft));
    CHECK(std::fabs(tRight.z + tLeft.z) < 1e-3f,
          "right/left torque about Z should cancel: %.4f vs %.4f",
          (double)tRight.z, (double)tLeft.z);
    PASS();
}

static void testNoSnap()
{
    TEST("no direct orientation snap");
    PhysicalAimConfig cfg;
    glm::quat current(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat desired = aimLookRotation(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat before = current;

    // One small tick: the controller must not jump the orientation to desired.
    glm::vec3 w = computeAimTorque(current, desired, glm::vec3(0.0f), cfg, 1.0f) * (1.0f / 60.0f);
    glm::quat after = integrate(current, w, 1.0f / 60.0f);
    float err = vecAngle(glm::vec3(after * glm::vec3(0.0f, 1.0f, 0.0f)),
                         glm::vec3(desired * glm::vec3(0.0f, 1.0f, 0.0f)));
    CHECK(current == before, "controller must not mutate current");
    CHECK(err > 0.5f, "orientation should still lag after one tick, err=%.4f", (double)err);
    PASS();
}

static void testSpeedCap()
{
    TEST("desired angular velocity respects cap");
    PhysicalAimConfig cfg;
    cfg.torqueGain = 500.0f;
    glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat flipped = glm::angleAxis(glm::radians(179.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 desiredVel = aimDesiredAngularVelocity(id, flipped, cfg, 1.0f);
    CHECK(glm::length(desiredVel) <= cfg.maxAngularSpeed + 1e-4f,
          "speed=%.4f cap=%.4f", (double)glm::length(desiredVel), (double)cfg.maxAngularSpeed);
    PASS();
}

static void testDampingReducesOvershoot()
{
    TEST("damping reduces overshoot and still converges");
    glm::quat id(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat desired = aimLookRotation(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const float dt = 1.0f / 60.0f;

    auto run = [&](float damping) {
        PhysicalAimConfig cfg;
        cfg.damping = PhysicalAimDamping::Physical;
        cfg.torqueGain = 28.0f;
        cfg.angularDamping = damping;
        glm::quat q = id;
        glm::vec3 w(0.0f);
        float peak = 0.0f;
        for (int i = 0; i < 240; ++i) {
            glm::vec3 torque = computeAimTorque(q, desired, w, cfg, 1.0f);
            w += torque * dt;
            q = integrate(q, w, dt);
            peak = std::max(peak, glm::length(w));
        }
        float err = vecAngle(glm::vec3(q * glm::vec3(0.0f, 1.0f, 0.0f)),
                             glm::vec3(desired * glm::vec3(0.0f, 1.0f, 0.0f)));
        return std::pair<float, float>(peak, err);
    };

    auto low = run(2.0f);
    auto high = run(12.0f);

    CHECK(high.first <= low.first + 1e-3f,
          "higher damping should not increase peak: low=%.3f high=%.3f",
          (double)low.first, (double)high.first);
    CHECK(high.second < glm::radians(5.0f),
          "high damping should converge: err=%.4f rad", (double)high.second);
    PASS();
}

int main()
{
    printf("physical-aim torque controller\n");
    testZeroError();
    testLookRightLeft();
    testNoSnap();
    testSpeedCap();
    testDampingReducesOvershoot();
    printf("\n%d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
