// 08 02 2026, 00 00
/* purpose
* Implements the movement tuning preset loader and hot-reload watcher.
* Reads config/movement.json for the preset name, then loads that preset from config/movement/.
* Resolves by the preset "name" field, falls back to the filename, then to built-in defaults.
* Applies JSON overrides on top of the current runtime base so missing keys keep current behavior.
* Does NOT run movement formulas, poll input, send packets, render, or decide authority.
* Does NOT own the shared movement kernel or replace physics/config.h constants.
*/

#include "config/movement-config.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "physics/config.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

bool parseWalkMode(const json& value, MovementWalkMode& out)
{
    if (!value.is_string())
        return false;

    const std::string mode = value.get<std::string>();
    if (mode == "override") {
        out = MovementWalkMode::Override;
        return true;
    }
    if (mode == "accel") {
        out = MovementWalkMode::Accel;
        return true;
    }
    return false;
}

bool parseSpeedCapMode(const json& value, MovementSpeedCapMode& out)
{
    if (!value.is_string())
        return false;

    const std::string mode = value.get<std::string>();
    if (mode == "none") {
        out = MovementSpeedCapMode::None;
        return true;
    }
    if (mode == "hard") {
        out = MovementSpeedCapMode::Hard;
        return true;
    }
    if (mode == "soft") {
        out = MovementSpeedCapMode::Soft;
        return true;
    }
    return false;
}

bool parseStationaryCameraInputMode(const json& value,
                                    StationaryCameraInputMode& out)
{
    if (!value.is_string())
        return false;

    const std::string mode = value.get<std::string>();
    if (mode == "strict") {
        out = StationaryCameraInputMode::Strict;
        return true;
    }
    if (mode == "steering") {
        out = StationaryCameraInputMode::Steering;
        return true;
    }
    return false;
}

// Base used before JSON overrides. Mirrors the constants in src/physics/config.h.
MovementConfig defaultMovementConfig()
{
    MovementConfig config;
    config.simulationHz = 60;
    config.fixedDeltaSeconds = 1.0f / 60.0f;
    config.maximumDeltaSeconds = 0.033f;
    config.groundSpeed = PHYS.moveSpeed;
    config.airSpeed = AIR_SPEED;
    config.movementSpeedSizeExponent = 0.5f;
    config.gravityZ = PHYS.gravity;
    config.maximumFallSpeed = MAX_FALL_SPEED;
    config.jumpVerticalSpeed = PHYS.jumpStrength;
    config.jumpHeightSizeExponent = 0.5f;
    config.jumpBufferSeconds = JUMP_BUFFER_TIME;
    config.coyoteSeconds = COYOTE_JUMP_TIME;
    config.maximumAirJumps = AIR_JUMPS_MAX;
    config.groundDashImpulse = DASH_IMPULSE;
    config.airDashImpulse = AIR_DASH_IMPULSE;
    config.dashHorizontalImpulse = AIR_DASH_IMPULSE;
    config.downDashVerticalSpeed = DOWN_DASH_SPEED;
    config.groundReturnVerticalSpeed = GROUND_RETURN_SPEED;
    config.freezeDurationSeconds = FREEZE_MAX_TIME;
    config.freezeCurveExponent = 4.0f;
    config.freezeDashMinimumPassThrough = 0.001f;
    config.maximumExternalImpulseSpeed = MAX_EXTERNAL_IMPULSE_SPEED;
    config.externalImpulseDecay = EXTERNAL_IMPULSE_DECAY;
    config.externalImpulseSteerRate = EXTERNAL_IMPULSE_STEER_RATE;
    config.externalImpulseBrakeRate = EXTERNAL_IMPULSE_BRAKE_RATE;
    config.groundFrictionAmount = GROUND_FRICTION_AMOUNT;
    config.airFrictionAmount = AIR_FRICTION_AMOUNT;
    config.frictionSizeExponent = -0.5f;
    config.almostZeroSpeed = ALMOST_ZERO;
    config.walkableSlopeDot = MAX_WALKABLE_SLOPE_DOT;
    config.collisionSkin = COLLISION_SKIN;
    config.maximumStepHeight = MAX_STEP_HEIGHT;
    config.stableGroundGraceSeconds = 0.08f;
    config.landingMinimumAirborneSeconds = 0.08f;
    config.landingCooldownResetSeconds = 0.3f;

    config.walkMode = MovementWalkMode::Override;
    config.airControlEnabled = true;
    config.bunnyHopEnabled = false;
    config.autoBhopEnabled = true;
    config.preserveStraightSpeed = true;
    config.minimumStrafeAngleDegrees = 0.0f;
    config.maximumAccelerationPerTick = 0.0f;
    config.diagonalInputNormalization = true;
    config.speedCapEnabled = false;
    config.maximumBhopSpeedMode = MovementSpeedCapMode::None;
    config.accelerationFalloffNearCap = 0.0f;
    config.landingSpeedRetention = 0.0f;
    config.debugDrawEnabled = false;
    config.requireActiveWishRotation = true;
    config.stationaryCameraInputMode = StationaryCameraInputMode::Strict;
    config.airSteeringRateDegreesPerSecond = 0.0f;
    config.maximumSteeringDegreesPerSecond = 0.0f;
    config.minimumCameraYawDeltaDegrees = 0.25f;
    config.minimumWishRotationDegrees = 0.25f;
    config.strafeAngularToleranceDegrees = 60.0f;
    config.softCapStart = 0.0f;
    config.groundAcceleration = 55.0f;
    config.groundDeceleration = 0.0f;
    config.groundDirectionChangeResponse = 0.0f;
    config.airAcceleration = 22.0f;
    config.airMaxWishspeed = 0.0f;
    config.airControl = 0.0f;
    config.stopspeed = 0.0f;
    config.bunnyHopSpeedCap = 0.0f;
    return config;
}

void applyPresetOverrides(const json& root, MovementConfig& config)
{
    if (root.contains("movement_mode") &&
        !parseWalkMode(root["movement_mode"], config.walkMode)) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Invalid movement_mode; expected \"override\" or \"accel\".\n");
    }

    if (root.contains("air_strafing") && root["air_strafing"].is_boolean())
        config.airControlEnabled = root["air_strafing"].get<bool>();
    if (root.contains("bunny_hop") && root["bunny_hop"].is_boolean())
        config.bunnyHopEnabled = root["bunny_hop"].get<bool>();
    if (root.contains("auto_bhop_enabled") && root["auto_bhop_enabled"].is_boolean())
        config.autoBhopEnabled = root["auto_bhop_enabled"].get<bool>();
    if (root.contains("preserve_straight_speed") && root["preserve_straight_speed"].is_boolean())
        config.preserveStraightSpeed = root["preserve_straight_speed"].get<bool>();
    if (root.contains("diagonal_input_normalization") &&
        root["diagonal_input_normalization"].is_boolean())
        config.diagonalInputNormalization = root["diagonal_input_normalization"].get<bool>();
    if (root.contains("speed_cap_enabled") && root["speed_cap_enabled"].is_boolean())
        config.speedCapEnabled = root["speed_cap_enabled"].get<bool>();
    if (root.contains("debug_draw_enabled") && root["debug_draw_enabled"].is_boolean())
        config.debugDrawEnabled = root["debug_draw_enabled"].get<bool>();
    if (root.contains("maximum_bhop_speed_mode") &&
        !parseSpeedCapMode(root["maximum_bhop_speed_mode"], config.maximumBhopSpeedMode)) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Invalid maximum_bhop_speed_mode; "
            "expected \"none\", \"hard\", or \"soft\".\n");
    }
    if (root.contains("stationary_camera_input_mode") &&
        !parseStationaryCameraInputMode(root["stationary_camera_input_mode"],
                                        config.stationaryCameraInputMode)) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Invalid stationary_camera_input_mode; "
            "expected \"strict\" or \"steering\".\n");
    }

    const auto readBool = [&root](const char* key, bool& target) {
        if (root.contains(key) && root[key].is_boolean())
            target = root[key].get<bool>();
    };
    const auto readFloat = [&root](const char* key, float& target) {
        if (root.contains(key) && root[key].is_number())
            target = root[key].get<float>();
    };

    readBool("require_active_wish_rotation", config.requireActiveWishRotation);
    readBool("preserve_speed_when_not_strafing", config.preserveStraightSpeed);

    readFloat("minimum_strafe_angle", config.minimumStrafeAngleDegrees);
    config.minimumStrafeAngleDegrees =
        std::clamp(config.minimumStrafeAngleDegrees, 0.0f, 90.0f);
    readFloat("maximum_acceleration_per_tick", config.maximumAccelerationPerTick);
    readFloat("maximum_speed_gain_per_tick", config.maximumAccelerationPerTick);
    readFloat("acceleration_falloff_near_cap", config.accelerationFalloffNearCap);
    config.accelerationFalloffNearCap =
        std::clamp(config.accelerationFalloffNearCap, 0.0f, 1.0f);
    readFloat("landing_speed_retention", config.landingSpeedRetention);
    config.landingSpeedRetention =
        std::clamp(config.landingSpeedRetention, 0.0f, 1.0f);

    readFloat("air_steering_rate_degrees_per_second",
              config.airSteeringRateDegreesPerSecond);
    config.airSteeringRateDegreesPerSecond =
        std::max(0.0f, config.airSteeringRateDegreesPerSecond);
    readFloat("maximum_steering_degrees_per_second",
              config.maximumSteeringDegreesPerSecond);
    config.maximumSteeringDegreesPerSecond =
        std::max(0.0f, config.maximumSteeringDegreesPerSecond);
    readFloat("minimum_camera_yaw_delta_degrees",
              config.minimumCameraYawDeltaDegrees);
    config.minimumCameraYawDeltaDegrees =
        std::clamp(config.minimumCameraYawDeltaDegrees, 0.0f, 180.0f);
    readFloat("minimum_wish_rotation_degrees", config.minimumWishRotationDegrees);
    config.minimumWishRotationDegrees =
        std::clamp(config.minimumWishRotationDegrees, 0.0f, 180.0f);
    readFloat("strafe_angular_tolerance_degrees",
              config.strafeAngularToleranceDegrees);
    config.strafeAngularToleranceDegrees =
        std::clamp(config.strafeAngularToleranceDegrees, 0.0f, 90.0f);
    readFloat("soft_cap_start", config.softCapStart);
    config.softCapStart = std::max(0.0f, config.softCapStart);

    readFloat("ground_speed", config.groundSpeed);
    readFloat("air_speed", config.airSpeed);
    readFloat("ground_acceleration", config.groundAcceleration);
    readFloat("ground_deceleration", config.groundDeceleration);
    readFloat("ground_direction_change_response",
              config.groundDirectionChangeResponse);
    readFloat("air_acceleration", config.airAcceleration);
    readFloat("air_speed_gain_acceleration", config.airAcceleration);
    readFloat("air_max_wishspeed", config.airMaxWishspeed);
    readFloat("air_wish_speed", config.airMaxWishspeed);
    readFloat("air_control", config.airControl);
    readFloat("stopspeed", config.stopspeed);
    readFloat("bhop_speed_cap", config.bunnyHopSpeedCap);
    readFloat("speed_cap", config.bunnyHopSpeedCap);
    readFloat("gravity", config.gravityZ);
    readFloat("max_fall_speed", config.maximumFallSpeed);
    readFloat("jump_strength", config.jumpVerticalSpeed);
    readFloat("jump_buffer_time", config.jumpBufferSeconds);
    readFloat("coyote_time", config.coyoteSeconds);
    readFloat("ground_friction", config.groundFrictionAmount);
    readFloat("air_friction", config.airFrictionAmount);
    readFloat("external_impulse_decay", config.externalImpulseDecay);
    readFloat("max_external_impulse_speed", config.maximumExternalImpulseSpeed);
    readFloat("ground_dash_impulse", config.groundDashImpulse);
    readFloat("air_dash_impulse", config.airDashImpulse);
    readFloat("down_dash_speed", config.downDashVerticalSpeed);

    if (root.contains("max_air_jumps") && root["max_air_jumps"].is_number())
        config.maximumAirJumps = std::max(0, root["max_air_jumps"].get<int>());
}

} // namespace

MovementJsonConfig& MovementJsonConfig::instance()
{
    static MovementJsonConfig config;
    return config;
}

MovementJsonConfig::MovementJsonConfig()
{
    mConfig = defaultMovementConfig();
}

std::string MovementJsonConfig::resolvePresetPath(const std::string& preset) const
{
    // Prefer matching by the preset's "name" field inside each config/movement/*.json file.
    std::error_code ec;
    if (std::filesystem::is_directory(mPresetDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(mPresetDir, ec)) {
            if (!entry.is_regular_file() ||
                entry.path().extension().string() != ".json")
                continue;

            json root;
            std::ifstream file(entry.path().string());
            if (!file.is_open())
                continue;
            try {
                file >> root;
            } catch (const json::parse_error&) {
                continue;
            }
            if (root.is_object() && root.contains("name") &&
                root["name"].is_string() &&
                root["name"].get<std::string>() == preset) {
                return entry.path().string();
            }
        }
    }

    // Fall back to a deterministic filename: config/movement/movement-<preset>.json
    const std::string byFile = mPresetDir + "/movement-" + preset + ".json";
    if (std::filesystem::exists(byFile, ec))
        return byFile;

    return "";
}

bool MovementJsonConfig::loadPresetFile(const std::string& path,
                                        const std::string& preset)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Could not open preset file %s\n", path.c_str());
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Parse error in %s: %s. Keeping previous settings.\n",
            path.c_str(), e.what());
        return false;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Error loading %s: %s. Keeping previous settings.\n",
            path.c_str(), e.what());
        return false;
    }

    if (!root.is_object()) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Preset file %s is not a JSON object. Keeping previous settings.\n",
            path.c_str());
        return false;
    }

    MovementConfig next = defaultMovementConfig();
    applyPresetOverrides(root, next);

    mConfig = next;
    mActivePreset = preset;
    mActivePresetPath = path;
    mPresetWrite = getLastWrite(path);

    Debug::warn(Debug::Category::Physics,
        "[MOVEMENT CONFIG] Active preset: %s (%s) mode=%s air_strafing=%d bhop=%d\n",
        mActivePreset.c_str(), fileNameOf(path).c_str(),
        mConfig.walkMode == MovementWalkMode::Accel ? "accel" : "override",
        (int)mConfig.airControlEnabled, (int)mConfig.bunnyHopEnabled);
    Debug::warn(Debug::Category::Physics,
        "[MOVEMENT CONFIG] ground_speed=%.1f air_speed=%.1f ground_accel=%.1f air_accel=%.1f "
        "gravity=%.1f jump=%.1f\n",
        mConfig.groundSpeed, mConfig.airSpeed,
        mConfig.groundAcceleration, mConfig.airAcceleration,
        mConfig.gravityZ, mConfig.jumpVerticalSpeed);
    return true;
}

bool MovementJsonConfig::loadPresetByName(const std::string& preset)
{
    const std::string resolved = resolvePresetPath(preset);
    if (resolved.empty()) {
        if (preset == "default") {
            mConfig = defaultMovementConfig();
            mActivePreset = "default";
            mActivePresetPath.clear();
            mPresetWrite = {};
            Debug::warn(Debug::Category::Physics,
                "[MOVEMENT CONFIG] No preset 'default' found; using built-in defaults.\n");
            return false;
        }
        Debug::warn(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Unknown preset '%s'; falling back to 'default'.\n",
            preset.c_str());
        return loadPresetByName("default");
    }
    return loadPresetFile(resolved, preset);
}

bool MovementJsonConfig::load(const std::string& path)
{
    if (mSelectorPath != path) {
        mSelectorPath = path;
        mWatchLogged = false;
    }

    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Watching: %s\n", mSelectorPath.c_str());
        mWatchLogged = true;
    }

    mSelectorWrite = getLastWrite(mSelectorPath);

    json selector;
    {
        std::ifstream file(mSelectorPath);
        if (file.is_open()) {
            try {
                file >> selector;
            } catch (const json::parse_error& e) {
                Debug::error(Debug::Category::Physics,
                    "[MOVEMENT CONFIG] Parse error in %s: %s. Using built-in defaults.\n",
                    mSelectorPath.c_str(), e.what());
            } catch (const std::exception& e) {
                Debug::error(Debug::Category::Physics,
                    "[MOVEMENT CONFIG] Error loading %s: %s. Using built-in defaults.\n",
                    mSelectorPath.c_str(), e.what());
            }
        }
    }

    std::string preset = "default";
    if (selector.is_object() && selector.contains("preset") &&
        selector["preset"].is_string()) {
        preset = selector["preset"].get<std::string>();
    }

    return loadPresetByName(preset);
}

bool MovementJsonConfig::savePresetSelection(const std::string& preset)
{
    std::ofstream file(mSelectorPath);
    if (!file.is_open()) {
        Debug::error(Debug::Category::Physics,
            "[MOVEMENT CONFIG] Could not write selector %s\n", mSelectorPath.c_str());
        return false;
    }
    file << "{\n    \"comment\": \"Selects the active movement preset from config/movement/.\",\n";
    file << "    \"preset\": \"" << preset << "\"\n}\n";
    file.close();
    return load(mSelectorPath);
}

bool MovementJsonConfig::pollReload()
{
    const auto selectorWrite = getLastWrite(mSelectorPath);
    const auto presetWrite =
        mActivePresetPath.empty() ? std::filesystem::file_time_type{}
                                  : getLastWrite(mActivePresetPath);

    if (selectorWrite == std::filesystem::file_time_type{} &&
        presetWrite == std::filesystem::file_time_type{}) {
        return false;
    }
    if (selectorWrite == mSelectorWrite && presetWrite == mPresetWrite)
        return false;

    Debug::warn(Debug::Category::Physics,
        "[MOVEMENT CONFIG] Detected change; reloading.\n");
    return load(mSelectorPath);
}

std::vector<std::string> MovementJsonConfig::availablePresets() const
{
    std::vector<std::string> presets;
    std::error_code ec;
    if (!std::filesystem::is_directory(mPresetDir, ec))
        return presets;

    for (const auto& entry : std::filesystem::directory_iterator(mPresetDir, ec)) {
        if (!entry.is_regular_file() ||
            entry.path().extension().string() != ".json")
            continue;

        json root;
        std::ifstream file(entry.path().string());
        if (!file.is_open())
            continue;
        try {
            file >> root;
        } catch (const json::parse_error&) {
            continue;
        }

        if (root.is_object() && root.contains("name") && root["name"].is_string()) {
            presets.push_back(root["name"].get<std::string>());
        } else {
            std::string stem = entry.path().stem().string();
            const std::string prefix = "movement-";
            if (stem.rfind(prefix, 0) == 0)
                stem = stem.substr(prefix.size());
            presets.push_back(stem);
        }
    }
    std::sort(presets.begin(), presets.end());
    return presets;
}
