// 10 04 2026
/* purpose
* Focused, world-independent tests for the RMB aim-FOV blend/easing/apply owner.
* Runs as a plain check() + exit-code harness (no gtest) and links only
* src/entities/aim-fov.cpp.
*/

#include "entities/aim-fov.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int gChecks = 0;

void check(bool condition, const char* message)
{
    ++gChecks;
    if (!condition) {
        std::printf("[aim-fov-test] FAIL %s\n", message);
        std::exit(1);
    }
}

void testEasing()
{
    check(std::fabs(aimFovEase("linear", 0.0f)) < 1e-5f, "linear(0)=0");
    check(std::fabs(aimFovEase("linear", 1.0f) - 1.0f) < 1e-5f, "linear(1)=1");
    check(std::fabs(aimFovEase("ease_in_out", 0.0f)) < 1e-5f, "ease_in_out(0)=0");
    check(std::fabs(aimFovEase("ease_in_out", 1.0f) - 1.0f) < 1e-5f, "ease_in_out(1)=1");
    check(std::fabs(aimFovEase("unknown", 0.3f) - aimFovEase("ease_in_out", 0.3f)) < 1e-5f,
          "unknown easing falls back to ease_in_out");
}

void testBlendAndApply()
{
    float blend = updateAimFovBlend(0.0f, true, 0.5f, 0.5f);
    check(std::fabs(blend - 1.0f) < 1e-5f, "held full duration reaches 1");
    check(applyAimFov(100.0f, 0.5f, blend) < 100.0f, "held narrows FOV");
    check(std::fabs(applyAimFov(100.0f, 0.5f, blend) - 50.0f) < 1e-3f,
          "held full blend is base*multiplier");

    blend = updateAimFovBlend(blend, false, 0.5f, 0.5f);
    check(std::fabs(blend) < 1e-5f, "release full duration returns 0");
    check(std::fabs(applyAimFov(100.0f, 0.5f, blend) - 100.0f) < 1e-4f,
          "released restores base FOV");

    // Clamps never overshoot.
    blend = updateAimFovBlend(0.9f, true, 10.0f, 0.5f);
    check(blend <= 1.0f, "blend clamps to 1");
    blend = updateAimFovBlend(0.1f, false, 10.0f, 0.5f);
    check(blend >= 0.0f, "blend clamps to 0");
}

} // namespace

int main()
{
    std::string report;
    check(aimFovSelfTest(report), "aimFovSelfTest");
    testEasing();
    testBlendAndApply();
    std::printf("[aim-fov-test] PASS (%d checks)\n", gChecks);
    return 0;
}
