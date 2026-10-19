// 2026-10-19
/* purpose
* Loads and hot-reloads the shared Melee rules from config/melee.json.
* Keeps curve interpolation and shield presentation math in one owner.
* Does NOT apply results, move actors, render, or own network authority.
*/

#include "combat/melee-rules-config.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

namespace MimitaMeleeRules {
namespace {

std::filesystem::file_time_type writeTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

glm::vec3 readColor(const nlohmann::json& j, const glm::vec3& fallback)
{
    if (!j.is_array() || j.size() < 3)
        return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec3 readVec3(const nlohmann::json& j, const glm::vec3& fallback)
{
    if (!j.is_array() || j.size() < 3)
        return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

void readShield(const nlohmann::json& j, Rules& r)
{
    if (!j.is_object())
        return;
    r.shieldDurability = std::max(1.0f, j.value("max_durability", r.shieldDurability));
    r.shieldDecayTicks = std::max(1.0f, j.value("decay_ticks", r.shieldDecayTicks));
    r.shieldBreakRecoveryTicks =
        std::max(1.0f, j.value("break_recovery_ticks", r.shieldBreakRecoveryTicks));
    r.shieldReleaseRecoveryTicks =
        std::max(1.0f, j.value("release_recovery_ticks", r.shieldReleaseRecoveryTicks));
    r.shieldBreakUpImpulse =
        std::max(0.0f, j.value("break_upward_impulse", r.shieldBreakUpImpulse));

    const float minMul = j.value("min_damage_multiplier", r.shieldMinMultiplier);
    const float maxMul = j.value("max_damage_multiplier", r.shieldMaxMultiplier);
    r.shieldMinMultiplier = std::clamp(minMul, 0.0f, 1.0f);
    r.shieldMaxMultiplier =
        std::clamp(maxMul, r.shieldMinMultiplier, 1.0f);

    if (j.contains("visual") && j["visual"].is_object()) {
        const auto& v = j["visual"];
        ShieldVisual& out = r.shieldVisual;
        out.enabled = v.value("enabled", out.enabled);
        if (v.contains("center_offset_m"))
            out.centerOffset = readVec3(v["center_offset_m"], out.centerOffset);
        if (v.contains("size_m"))
            out.size = readVec3(v["size_m"], out.size);
        if (v.contains("full_color"))
            out.fullColor = readColor(v["full_color"], out.fullColor);
        if (v.contains("empty_color"))
            out.emptyColor = readColor(v["empty_color"], out.emptyColor);
        out.fullAlpha = std::clamp(v.value("full_alpha", out.fullAlpha), 0.0f, 1.0f);
        out.emptyAlpha = std::clamp(v.value("empty_alpha", out.emptyAlpha), 0.0f, 1.0f);
        out.size.x = std::max(0.01f, out.size.x);
        out.size.y = std::max(0.01f, out.size.y);
        out.size.z = std::max(0.01f, out.size.z);
    }
}

void readForce(const nlohmann::json& j, Rules& r)
{
    if (!j.is_object())
        return;
    r.forceSpeedScale = std::max(0.0f, j.value("speed_scale", r.forceSpeedScale));
    r.forceDirectnessScale =
        std::max(0.0f, j.value("directness_scale", r.forceDirectnessScale));
    r.forceMaxImpulse = std::max(0.0f, j.value("max_impulse", r.forceMaxImpulse));
    r.forceMinImpulse = std::clamp(
        j.value("min_impulse", r.forceMinImpulse), 0.0f, r.forceMaxImpulse);
    r.referenceBaseImpulse =
        std::max(0.001f, j.value("reference_base_impulse", r.referenceBaseImpulse));
    r.referenceSeconds =
        std::max(0.01f, j.value("reference_seconds", r.referenceSeconds));

    if (j.contains("knockback_curve") && j["knockback_curve"].is_array()) {
        std::vector<KnockbackPoint> points;
        for (const auto& entry : j["knockback_curve"]) {
            if (!entry.is_object())
                continue;
            KnockbackPoint point;
            point.targetPercent = entry.value("target_percent", -1.0f);
            point.distanceM = entry.value("distance_m", -1.0f);
            if (point.targetPercent < 0.0f || point.distanceM < 0.0f)
                continue;
            points.push_back(point);
        }
        if (!points.empty()) {
            std::sort(points.begin(), points.end(),
                      [](const KnockbackPoint& a, const KnockbackPoint& b) {
                          return a.targetPercent < b.targetPercent;
                      });
            r.knockbackCurve = std::move(points);
        }
    }
}

} // namespace

Config& Config::instance()
{
    static Config config;
    return config;
}

bool Config::load(const std::string& path)
{
    mPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Weapons,
            "[MELEE RULES] could not open %s; keeping defaults\n", path.c_str());
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file, nullptr, true, true);
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Weapons,
            "[MELEE RULES] invalid %s: %s; keeping previous rules\n",
            path.c_str(), e.what());
        return false;
    }
    if (!root.is_object())
        return false;

    Rules next;
    if (root.contains("shield"))
        readShield(root["shield"], next);
    if (root.contains("grab") && root["grab"].is_object()) {
        const auto& g = root["grab"];
        next.grabRadius = std::max(0.01f, g.value("radius", next.grabRadius));
        next.grabPullTicks = std::max(1, g.value("pull_ticks", next.grabPullTicks));
        next.grabAntiChainTicks = (uint32_t)std::max(
            0, g.value("anti_chain_ticks", (int)next.grabAntiChainTicks));
    }
    if (root.contains("force"))
        readForce(root["force"], next);

    mRules = next;
    mWrite = writeTime(path);
    mLoaded = true;
    Debug::warn(Debug::Category::Weapons,
        "[MELEE RULES] loaded %s shield=%.1f decay=%.0f curve_points=%zu max_impulse=%.1f\n",
        path.c_str(), mRules.shieldDurability, mRules.shieldDecayTicks,
        mRules.knockbackCurve.size(), mRules.forceMaxImpulse);
    return true;
}

bool Config::pollReload()
{
    const auto write = writeTime(mPath);
    if (!mLoaded)
        return load(mPath);
    if (write == std::filesystem::file_time_type{} || write == mWrite)
        return false;
    return load(mPath);
}

float Config::curveDistance(float preHitPercent) const
{
    const auto& points = mRules.knockbackCurve;
    if (points.empty())
        return 0.5f;
    const float percent = std::max(0.0f, preHitPercent);
    if (percent <= points.front().targetPercent)
        return points.front().distanceM;
    for (size_t i = 1; i < points.size(); ++i) {
        if (percent <= points[i].targetPercent) {
            const KnockbackPoint& a = points[i - 1];
            const float span = points[i].targetPercent - a.targetPercent;
            const float t = span > 0.0001f
                ? (percent - a.targetPercent) / span : 1.0f;
            return a.distanceM + (points[i].distanceM - a.distanceM) * t;
        }
    }
    if (points.size() < 2)
        return points.back().distanceM;
    const auto& a = points[points.size() - 2];
    const auto& b = points.back();
    const float span = b.targetPercent - a.targetPercent;
    if (span <= 0.0001f)
        return b.distanceM;
    const float slope = (b.distanceM - a.distanceM) / span;
    return std::max(0.0f, b.distanceM + (percent - b.targetPercent) * slope);
}

float Config::shieldReceivedMultiplier(float remainingFraction) const
{
    const float remaining = std::clamp(remainingFraction, 0.0f, 1.0f);
    return std::clamp(1.0f - remaining,
                      mRules.shieldMinMultiplier, mRules.shieldMaxMultiplier);
}

glm::vec4 Config::shieldColor(float remainingFraction) const
{
    const float t = std::clamp(remainingFraction, 0.0f, 1.0f);
    const auto& v = mRules.shieldVisual;
    const glm::vec3 color = glm::mix(v.emptyColor, v.fullColor, t);
    return glm::vec4(color, shieldAlpha(t));
}

float Config::shieldAlpha(float remainingFraction) const
{
    const float t = std::clamp(remainingFraction, 0.0f, 1.0f);
    const auto& v = mRules.shieldVisual;
    return glm::mix(v.emptyAlpha, v.fullAlpha, t);
}

} // namespace MimitaMeleeRules
