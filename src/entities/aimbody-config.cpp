// 08 15 2026, 22 40
/* purpose
* Implements the hot-reloadable aimbody.json config: per-limb pitch/yaw/roll
* gains that the animation system applies when the camera looks up/down.
* Watches the file mtime on a 250ms throttle so edits take effect live.
* Does NOT own the animation math, the pose pipeline, or firing logic.
*/
#include "aimbody-config.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64_t modifiedTimeNs(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

bool readLimb(const json& j, LimbAim& out)
{
    if (!j.is_object())
        return false;
    if (j.contains("pitch") && j["pitch"].is_number())
        out.pitch = j["pitch"].get<float>();
    if (j.contains("yaw") && j["yaw"].is_number())
        out.yaw = j["yaw"].get<float>();
    if (j.contains("roll") && j["roll"].is_number())
        out.roll = j["roll"].get<float>();
    return true;
}

} // namespace

AimBodyConfig& AimBodyConfig::instance()
{
    static AimBodyConfig config;
    return config;
}

bool AimBodyConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        mEnabled = true;
        mMode = "default";
        mArmsMode = "hybrid";
        mRightArmPointingMode = "off";
        mRightArmPointingCenterOffset = glm::vec3(0.2f, 0.0f, 0.4f);
        mRightArmPointingRotationDegrees = glm::vec3(0.0f);
        mRightArmPointingBlendRate = 10.0f;
        mRightArmPointingAimStrength = 10.0f;
        mRightArmPointingFovEnabled = true;
        mRightArmPointingFovMultiplier = 0.5f;
        mRightArmPointingFovDuration = 0.5f;
        mRightArmPointingFovEasing = "ease_in_out";
        mSmoothingFactor = 1.0f;
        mLimbs.clear();
        Debug::warn(Debug::Category::Animation,
            "[AIMBODY] missing %s; defaults active (enabled=1, pitch-only)\n", path.c_str());
        save();
        return false;
    }

    try {
        json j;
        file.clear();
        file.seekg(0);
        j = parseJsonConfig(file);
        // Commit only after parsing succeeds. A malformed or partially-written
        // hot-reload must not leave enabled=true with an empty limb map.
        mEnabled = true;
        mMode = "default";
        mSmoothingFactor = 1.0f;
        mLimbs.clear();
        if (j.contains("enabled"))
            mEnabled = j.value("enabled", true);
        const std::string mode = j.value("mode", std::string("default"));
        if (mode == "smooth")
            mMode = "smooth";
        else if (mode == "physical")
            mMode = "physical";
        else if (mode == "hybrid")
            mMode = "hybrid";
        else
            mMode = "default";
        const std::string armsMode = j.value("arms_mode", std::string("hybrid"));
        mArmsMode = armsMode == "default" ? "default" : "hybrid";
        mRightArmPointingCenterOffset = glm::vec3(0.2f, 0.0f, 0.4f);
        mRightArmPointingRotationDegrees = glm::vec3(0.0f);
        mRightArmPointingBlendRate = 10.0f;
        mRightArmPointingAimStrength = 10.0f;
        mRightArmPointingFovEnabled = true;
        mRightArmPointingFovMultiplier = 0.5f;
        mRightArmPointingFovDuration = 0.5f;
        mRightArmPointingFovEasing = "ease_in_out";
        if (j.contains("right_arm_pointing")) {
            const auto& pointing = j["right_arm_pointing"];
            if (pointing.is_object()) {
                const std::string pointingMode =
                    pointing.value("mode", std::string("off"));
                mRightArmPointingMode = pointingMode == "rmb" ? "rmb" : "off";
                if (pointing.contains("center_offset") &&
                    pointing["center_offset"].is_array() &&
                    pointing["center_offset"].size() >= 3) {
                    mRightArmPointingCenterOffset = glm::vec3(
                        pointing["center_offset"][0].get<float>(),
                        pointing["center_offset"][1].get<float>(),
                        pointing["center_offset"][2].get<float>());
                }
                if (pointing.contains("rotation_degrees") &&
                    pointing["rotation_degrees"].is_array() &&
                    pointing["rotation_degrees"].size() >= 3) {
                    mRightArmPointingRotationDegrees = glm::vec3(
                        pointing["rotation_degrees"][0].get<float>(),
                        pointing["rotation_degrees"][1].get<float>(),
                        pointing["rotation_degrees"][2].get<float>());
                }
                mRightArmPointingBlendRate = std::clamp(
                    pointing.value("blend_rate", mRightArmPointingBlendRate),
                    0.1f, 60.0f);
                mRightArmPointingAimStrength = std::clamp(
                    pointing.value("aim_strength", mRightArmPointingAimStrength),
                    0.1f, 100.0f);
                if (pointing.contains("fov") && pointing["fov"].is_object()) {
                    const auto& fov = pointing["fov"];
                    mRightArmPointingFovEnabled = fov.value("enabled", mRightArmPointingFovEnabled);
                    mRightArmPointingFovMultiplier = std::clamp(
                        fov.value("multiplier", mRightArmPointingFovMultiplier), 0.05f, 1.0f);
                    mRightArmPointingFovDuration = std::clamp(
                        fov.value("duration", mRightArmPointingFovDuration), 0.01f, 10.0f);
                    const std::string easing = fov.value("easing", mRightArmPointingFovEasing);
                    if (easing == "linear" || easing == "ease_in" || easing == "ease_out" ||
                        easing == "ease_in_out" || easing == "exponential" || easing == "bounce")
                        mRightArmPointingFovEasing = easing;
                }
            } else {
                const std::string pointingMode = pointing.get<std::string>();
                mRightArmPointingMode = pointingMode == "rmb" ? "rmb" : "off";
            }
        } else {
            mRightArmPointingMode = "off";
        }
        const float factor = j.value("smoothingFactor", 1.0f);
        mSmoothingFactor = std::isfinite(factor) && factor > 0.0f
            ? factor : 1.0f;
        if (j.contains("limbs") && j["limbs"].is_object()) {
            for (auto it = j["limbs"].begin(); it != j["limbs"].end(); ++it) {
                LimbAim limb;
                if (readLimb(it.value(), limb))
                    mLimbs[it.key()] = limb;
            }
        }
        mLastModified = modifiedTimeNs(path);
        Debug::log(Debug::Category::Animation,
            "[AIMBODY] loaded %s enabled=%d limbs=%zu\n",
            path.c_str(), (int)mEnabled, mLimbs.size());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Animation,
            "[AIMBODY] parse failed: %s\n", e.what());
        return false;
    }
}

bool AimBodyConfig::save()
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(mPath).parent_path(), ec);
    json j;
    j["comment"] = "default preserves immediate aimbody behavior; smooth treats camera look as a wish direction. World Z is vertical and body yaw rotates around world Z. smoothingFactor 1.0 is approximately a 250 ms response; larger is slower and smaller is faster. Smooth mode never snaps. Camera sway is configured separately in camconfig.json.";
    j["enabled"] = mEnabled;
    j["mode"] = mMode;
    j["arms_mode"] = mArmsMode;
    j["right_arm_pointing"] = {
        {"mode", mRightArmPointingMode},
        {"center_offset", {mRightArmPointingCenterOffset.x,
                            mRightArmPointingCenterOffset.y,
                            mRightArmPointingCenterOffset.z}},
        {"rotation_degrees", {mRightArmPointingRotationDegrees.x,
                               mRightArmPointingRotationDegrees.y,
                               mRightArmPointingRotationDegrees.z}},
        {"blend_rate", mRightArmPointingBlendRate},
        {"aim_strength", mRightArmPointingAimStrength},
        {"fov", {
            {"enabled", mRightArmPointingFovEnabled},
            {"multiplier", mRightArmPointingFovMultiplier},
            {"duration", mRightArmPointingFovDuration},
            {"easing", mRightArmPointingFovEasing}
        }}
    };
    j["smoothingFactor"] = mSmoothingFactor;
    json limbs = json::object();
    for (const auto& [name, limb] : mLimbs) {
        limbs[name] = {
            {"pitch", limb.pitch},
            {"yaw", limb.yaw},
            {"roll", limb.roll}
        };
    }
    j["limbs"] = limbs;
    std::ofstream file(mPath);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    file.close();
    mLastModified = modifiedTimeNs(mPath);
    return true;
}

float AimBodyConfig::smoothValue(float current, float desired, float dt) const
{
    if (!smoothMode()) return desired;
    const float response = std::max(0.0025f, 0.25f * mSmoothingFactor);
    const float alpha = 1.0f - std::exp(-std::max(0.0f, dt) / response);
    return current + (desired - current) * alpha;
}

float AimBodyConfig::smoothAngle(float current, float desired, float dt) const
{
    if (!smoothMode()) return desired;
    float delta = std::fmod(desired - current + 540.0f, 360.0f) - 180.0f;
    return current + delta * (1.0f - std::exp(
        -std::max(0.0f, dt) / std::max(0.0025f, 0.25f * mSmoothingFactor)));
}

float AimBodyConfig::updateRightArmPointingFovBlend(bool held, float dt)
{
    const float duration = std::max(0.01f, mRightArmPointingFovDuration);
    const float step = std::clamp(std::max(0.0f, dt) / duration, 0.0f, 1.0f);
    mRightArmPointingFovBlend += ((held && mRightArmPointingFovEnabled) ? step : -step);
    mRightArmPointingFovBlend = std::clamp(mRightArmPointingFovBlend, 0.0f, 1.0f);

    const float t = mRightArmPointingFovBlend;
    if (mRightArmPointingFovEasing == "linear") return t;
    if (mRightArmPointingFovEasing == "ease_in") return t * t;
    if (mRightArmPointingFovEasing == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (mRightArmPointingFovEasing == "exponential")
        return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    if (mRightArmPointingFovEasing == "bounce") {
        const auto bounceOut = [](float x) {
            if (x < 1.0f / 2.75f) return 7.5625f * x * x;
            if (x < 2.0f / 2.75f) { x -= 1.5f / 2.75f; return 7.5625f * x * x + 0.75f; }
            if (x < 2.5f / 2.75f) { x -= 2.25f / 2.75f; return 7.5625f * x * x + 0.9375f; }
            x -= 2.625f / 2.75f;
            return 7.5625f * x * x + 0.984375f;
        };
        return bounceOut(t);
    }
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

bool AimBodyConfig::reload()
{
    return load(mPath);
}

bool AimBodyConfig::pollReload()
{
    const int64_t now = nowMs();
    if (now - mLastPollMs < 250)
        return false;
    mLastPollMs = now;
    const int64_t current = modifiedTimeNs(mPath);
    if (current != 0 && current != mLastModified)
        return reload();
    return false;
}

const LimbAim* AimBodyConfig::limb(const std::string& name) const
{
    auto it = mLimbs.find(name);
    return it != mLimbs.end() ? &it->second : nullptr;
}
