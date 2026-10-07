#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <glm/glm.hpp>

struct CameraConfigData {
    glm::vec3 offset{2.0f, -3.5f, 1.0f};
    float fov = 100.0f;
    // Full-screen barrel/fisheye distortion. 0 = neutral; values above 100
    // remain meaningful for deliberately extreme experiments.
    float lensDistortion = 0.0f;
    float lensDistortionCurve = 1.0f;
    float lensDistortionZoom = 1.0f;
    std::string lensDistortionEdgeMode = "circle";
    float lensDistortionEdgeRadius = 0.72f;
    float lensDistortionEdgeSoftness = 0.30f;
    float lensDistortionEdgeDarkness = 1.0f;
    float lensDistortionPeripheralBlur = 1.0f;
    // Damage-driven camera flinch layered onto the existing camera punch.
    bool hitFlinchEnabled = true;
    float hitFlinchLow = 1.0f;
    float hitFlinchHigh = 9.0f;
    float hitFlinchDamageAtHigh = 100.0f;
    float hitFlinchPitch = -1.0f;
    float hitFlinchYaw = 0.0f;
    float hitFlinchRandomness = 0.0f;
    float hitFlinchDistance = 30.0f;
    float hitFlinchDistanceExponent = 1.0f;
    float positionStiffness = 1.0f;
    float rotationStiffness = 1.0f;
    bool stiffnessEnabled = true;
    bool collisionEnabled = true;
    bool collisionPushEnabled = true;
    float collisionPushback = 0.3f;
    float lookAheadDistance = 0.0f;
    bool cameraSwayEnabled = true;
    float cameraSwayAmount = 1.0f;
    float cameraSwayLandingThreshold = 0.25f;
    float cameraSwayLandingPitchImpulse = 5.0f;
    float cameraSwayLandingRollImpulse = 3.0f;
    float cameraSwaySpringStiffness = 28.0f;
    float cameraSwaySpringDamping = 10.0f;
    float cameraSwayMaxPitch = 10.0f;
    float cameraSwayMaxRoll = 6.0f;
};

class CamConfig {
public:
    static CamConfig& instance();

    bool load(const std::string& path = "config/camconfig.json");
    bool pollReload();

    void setHitFlinchOverride(bool enabled) { mHitFlinchOverride = enabled; }
    void clearHitFlinchOverride() { mHitFlinchOverride.reset(); }
    bool hitFlinchEnabled() const {
        return mHitFlinchOverride.has_value()
            ? *mHitFlinchOverride : mData.hitFlinchEnabled;
    }

    const CameraConfigData& data() const { return mData; }
    CameraConfigData& data() { return mData; }

private:
    CamConfig() = default;

    CameraConfigData mData;
    std::string mPath = "config/camconfig.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
    std::optional<bool> mHitFlinchOverride;
};
