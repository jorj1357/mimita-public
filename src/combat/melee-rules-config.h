// 2026-10-19
/* purpose
* Owns the shared Melee rules loaded from config/melee.json: percent-to-knockback
* curve, contact-force calibration, shield durability/decay/break policy, and
* shield presentation tuning.
* Hot-reloads the whole validated file and keeps the previous valid rules on a
* missing or invalid save.
* Does NOT apply damage, move actors, own hitboxes, render, or decide authority.
* Does NOT duplicate per-move values; those stay in config/weapons.json.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MimitaMeleeRules {

struct KnockbackPoint {
    float targetPercent = 0.0f;
    float distanceM = 0.0f;
};

struct ShieldVisual {
    bool enabled = true;
    glm::vec3 centerOffset{0.0f, 0.0f, 1.0f};
    glm::vec3 size{1.0f, 1.0f, 2.0f};
    glm::vec3 fullColor{0.0f, 1.0f, 1.0f};
    glm::vec3 emptyColor{0.0f, 0.25f, 0.35f};
    float fullAlpha = 0.5f;
    float emptyAlpha = 0.1f;
};

struct Rules {
    float shieldDurability = 50.0f;
    float shieldDecayTicks = 300.0f;
    float shieldBreakRecoveryTicks = 120.0f;
    float shieldReleaseRecoveryTicks = 90.0f;
    float shieldBreakUpImpulse = 5.0f;
    float shieldMinMultiplier = 0.01f;
    float shieldMaxMultiplier = 0.99f;
    ShieldVisual shieldVisual;

    float grabRadius = 1.0f;
    int grabPullTicks = 30;
    uint32_t grabAntiChainTicks = 300;

    float forceSpeedScale = 1.0f;
    float forceDirectnessScale = 1.0f;
    float forceMaxImpulse = 120.0f;
    float forceMinImpulse = 1.0f;
    float referenceBaseImpulse = 30.0f;
    float referenceSeconds = 1.0f;

    // Required anchors: 0% -> 0.5 m, 100% -> 10 m, 200% -> 50 m.
    // Linear interpolation between points; the last segment extrapolates up to
    // the impulse cap.
    std::vector<KnockbackPoint> knockbackCurve{
        {0.0f, 0.5f}, {100.0f, 10.0f}, {200.0f, 50.0f}};
};

class Config {
public:
    static Config& instance();

    // Reads config/melee.json. A missing/invalid file keeps the defaults or the
    // previous valid rules. Returns false without partially applying a file.
    bool load(const std::string& path = "config/melee.json");
    bool pollReload();

    const Rules& rules() const { return mRules; }

    float curveDistance(float preHitPercent) const;
    float shieldReceivedMultiplier(float remainingFraction) const;
    glm::vec4 shieldColor(float remainingFraction) const;
    float shieldAlpha(float remainingFraction) const;

private:
    Config() = default;

    Rules mRules;
    std::string mPath = "config/melee.json";
    std::filesystem::file_time_type mWrite{};
    bool mLoaded = false;
};

} // namespace MimitaMeleeRules
