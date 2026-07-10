#include "config/size-scaling-config.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::last_write_time(path, ec);
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

} // namespace

SizeScalingConfig& SizeScalingConfig::instance()
{
    static SizeScalingConfig config;
    return config;
}

bool SizeScalingConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Weapons,
            "[SIZE SCALING CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[SIZE SCALING CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        SizeScalingData next;
        auto rd = [&](const char* key, float& field) {
            if (root.contains(key)) field = root[key].get<float>();
        };

        rd("movement_speed_exponent", next.movementSpeedExponent);
        rd("jump_height_exponent", next.jumpHeightExponent);
        rd("dash_impulse_exponent", next.dashImpulseExponent);
        rd("gravity_scale_exponent", next.gravityScaleExponent);
        rd("health_exponent", next.healthExponent);
        rd("damage_exponent", next.damageExponent);
        rd("knockback_exponent", next.knockbackExponent);
        rd("hitfx_radius_exponent", next.hitfxRadiusExponent);
        rd("hitfx_count_exponent", next.hitfxCountExponent);
        rd("hitfx_lifetime_exponent", next.hitfxLifetimeExponent);
        rd("debris_count_exponent", next.debrisCountExponent);
        rd("debris_size_exponent", next.debrisSizeExponent);
        rd("weapon_size_exponent", next.weaponSizeExponent);
        rd("projectile_size_exponent", next.projectileSizeExponent);
        rd("projectile_damage_exponent", next.projectileDamageExponent);
        rd("explosion_radius_exponent", next.explosionRadiusExponent);
        rd("sound_volume_exponent", next.soundVolumeExponent);
        rd("sound_pitch_exponent", next.soundPitchExponent);
        rd("footstep_volume_exponent", next.footstepVolumeExponent);
        rd("footstep_pitch_exponent", next.footstepPitchExponent);
        rd("collision_radius_exponent", next.collisionRadiusExponent);
        rd("collision_height_exponent", next.collisionHeightExponent);
        rd("capsule_radius_exponent", next.capsuleRadiusExponent);
        rd("capsule_height_exponent", next.capsuleHeightExponent);
        rd("body_sample_radius_exponent", next.bodySampleRadiusExponent);
        rd("recoil_exponent", next.recoilExponent);
        rd("camera_shake_exponent", next.cameraShakeExponent);

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[SIZE SCALING CONFIG] Loaded successfully: %s\n", fileName.c_str());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[SIZE SCALING CONFIG] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[SIZE SCALING CONFIG] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool SizeScalingConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Weapons,
        "[SIZE SCALING CONFIG] Detected change: %s\n", fileNameOf(mPath).c_str());
    Debug::warn(Debug::Category::Weapons,
        "[SIZE SCALING CONFIG] Reloading...\n");
    return load(mPath);
}
