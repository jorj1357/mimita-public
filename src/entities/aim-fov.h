// 10 04 2026
/* purpose
* Pure RMB aim-FOV blend/easing used by the first-person camera. A gamemode may
* own an "aim_fov" override (Counter-Strike); otherwise the camera falls back to
* config/aimbody.json's right-arm FOV values. This owner never reads config and
* never edits aimbody.json.
* Does NOT own the camera, input sampling, or the aim-body pose.
*/
#pragma once

#include <string>

struct AimFovSettings
{
    bool enabled = false;
    std::string input = "right_mouse";
    float multiplier = 0.5f;   // held FOV = base * multiplier
    float duration = 0.5f;     // seconds to reach the held FOV
    std::string easing = "ease_in_out";
};

// Pure easing on a 0..1 blend. Unknown names fall back to ease_in_out.
float aimFovEase(const std::string& easing, float t);

// Advance a 0..1 blend toward `held` at 1/duration per second. Returns the new
// blend clamped to [0,1].
float updateAimFovBlend(float blend, bool held, float dt, float duration);

// baseFov * mix(1, multiplier, blend).
float applyAimFov(float baseFov, float multiplier, float blend);

// World-independent selftest for easing, blend, and application.
bool aimFovSelfTest(std::string& report);
