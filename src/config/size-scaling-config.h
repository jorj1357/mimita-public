#pragma once

#include <filesystem>
#include <string>
#include <cmath>

struct SizeScalingData {
    float movementSpeedExponent = 0.5f;
    float jumpHeightExponent = 0.5f;
    float dashImpulseExponent = 0.5f;
    float gravityScaleExponent = 0.0f;
    float healthExponent = 3.0f;
    float damageExponent = 3.0f;
    float knockbackExponent = 3.0f;
    float hitfxRadiusExponent = 3.0f;
    float hitfxCountExponent = 3.0f;
    float hitfxLifetimeExponent = 0.0f;
    float debrisCountExponent = 2.0f;
    float debrisSizeExponent = 1.0f;
    float weaponSizeExponent = 1.0f;
    float projectileSizeExponent = 1.0f;
    float projectileDamageExponent = 3.0f;
    float explosionRadiusExponent = 1.0f;
    float soundVolumeExponent = 2.0f;
    float soundPitchExponent = -0.25f;
    float footstepVolumeExponent = 2.0f;
    float footstepPitchExponent = -0.25f;
    float collisionRadiusExponent = 1.0f;
    float collisionHeightExponent = 1.0f;
    float capsuleRadiusExponent = 1.0f;
    float capsuleHeightExponent = 1.0f;
    float bodySampleRadiusExponent = 1.0f;
    float recoilExponent = 1.0f;
    float cameraShakeExponent = 2.0f;

    float scale(float base, float exponent, float size) const {
        return base * std::pow(size, exponent);
    }
};

class SizeScalingConfig {
public:
    static SizeScalingConfig& instance();
    bool load(const std::string& path = "config/size_scaling.json");
    bool pollReload();
    const SizeScalingData& data() const { return mData; }
    SizeScalingData& data() { return mData; }
private:
    SizeScalingConfig() = default;
    SizeScalingData mData;
    std::string mPath = "config/size_scaling.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
