// 10 04 2026
// Pure RMB aim-FOV blend/easing/application.
#include "entities/aim-fov.h"

#include <algorithm>
#include <cmath>

float aimFovEase(const std::string& easing, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (easing == "linear") return t;
    if (easing == "ease_in") return t * t;
    if (easing == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (easing == "exponential")
        return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    if (easing == "bounce") {
        const auto bounceOut = [](float x) {
            if (x < 1.0f / 2.75f) return 7.5625f * x * x;
            if (x < 2.0f / 2.75f) { x -= 1.5f / 2.75f; return 7.5625f * x * x + 0.75f; }
            if (x < 2.5f / 2.75f) { x -= 2.25f / 2.75f; return 7.5625f * x * x + 0.9375f; }
            x -= 2.625f / 2.75f;
            return 7.5625f * x * x + 0.984375f;
        };
        return bounceOut(t);
    }
    // ease_in_out (default and unknown fallback).
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float updateAimFovBlend(float blend, bool held, float dt, float duration)
{
    const float safeDuration = std::max(0.01f, duration);
    const float step = std::clamp(std::max(0.0f, dt) / safeDuration, 0.0f, 1.0f);
    blend += held ? step : -step;
    return std::clamp(blend, 0.0f, 1.0f);
}

float applyAimFov(float baseFov, float multiplier, float blend)
{
    const float t = std::clamp(blend, 0.0f, 1.0f);
    return baseFov * (1.0f + (std::clamp(multiplier, 0.05f, 1.0f) - 1.0f) * t);
}

bool aimFovSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Easing endpoints and monotonicity.
    if (std::fabs(aimFovEase("linear", 0.0f)) > 1e-5f) fail("linear(0) != 0");
    if (std::fabs(aimFovEase("linear", 1.0f) - 1.0f) > 1e-5f) fail("linear(1) != 1");
    if (std::fabs(aimFovEase("ease_in_out", 0.0f)) > 1e-5f) fail("ease_in_out(0) != 0");
    if (std::fabs(aimFovEase("ease_in_out", 1.0f) - 1.0f) > 1e-5f) fail("ease_in_out(1) != 1");
    if (aimFovEase("ease_in_out", 0.5f) <= 0.0f) fail("ease_in_out midpoint should be > 0");
    if (std::fabs(aimFovEase("unknown", 0.25f) - aimFovEase("ease_in_out", 0.25f)) > 1e-5f)
        fail("unknown easing should fall back to ease_in_out");

    // Held narrows, released restores, and it clamps at the ends.
    float blend = 0.0f;
    blend = updateAimFovBlend(blend, true, 0.5f, 0.5f);
    if (std::fabs(blend - 1.0f) > 1e-5f) fail("held full duration should reach blend 1");
    if (!(applyAimFov(100.0f, 0.5f, blend) < 100.0f)) fail("held should narrow FOV");
    blend = updateAimFovBlend(blend, false, 0.5f, 0.5f);
    if (std::fabs(blend) > 1e-5f) fail("release full duration should return blend 0");
    if (std::fabs(applyAimFov(100.0f, 0.5f, blend) - 100.0f) > 1e-4f)
        fail("released should restore base FOV");

    // Half blend applies the eased midpoint.
    const float eased = aimFovEase("linear", 0.5f);
    if (std::fabs(applyAimFov(100.0f, 0.5f, 0.5f) - (100.0f * (1.0f + (0.5f - 1.0f) * eased))) > 1e-4f)
        fail("applyAimFov midpoint mismatch");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
