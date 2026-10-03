#include "config/camera-config.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

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

} // namespace

CamConfig& CamConfig::instance()
{
    static CamConfig config;
    return config;
}

bool CamConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        root = json::parse(file, nullptr, true, true);

        CameraConfigData next;
        if (root.contains("thirdPerson")) {
            const auto& tp = root["thirdPerson"];

            if (tp.contains("offset")) {
                next.offset.x = tp["offset"].value("x", next.offset.x);
                next.offset.y = tp["offset"].value("y", next.offset.y);
                next.offset.z = tp["offset"].value("z", next.offset.z);
            }
            next.fov = tp.value("fov", next.fov);
            next.positionStiffness = tp.value("positionStiffness", next.positionStiffness);
            next.rotationStiffness = tp.value("rotationStiffness", next.rotationStiffness);
            next.stiffnessEnabled = tp.value("stiffnessEnabled", next.stiffnessEnabled);
            next.collisionEnabled = tp.value("collisionEnabled", next.collisionEnabled);
            next.collisionPushEnabled = tp.value("collisionPushEnabled", next.collisionPushEnabled);
            next.collisionPushback = tp.value("collisionPushback", next.collisionPushback);
            next.lookAheadDistance = tp.value("lookAheadDistance", next.lookAheadDistance);
        }

        next.lensDistortion = root.value("lensDistortion", next.lensDistortion);
        if (!std::isfinite(next.lensDistortion))
            next.lensDistortion = 0.0f;
        next.lensDistortion = std::clamp(next.lensDistortion, 0.0f, 10000.0f);
        next.lensDistortionCurve = root.value("lensDistortionCurve", next.lensDistortionCurve);
        next.lensDistortionZoom = root.value("lensDistortionZoom", next.lensDistortionZoom);
        next.lensDistortionEdgeMode = root.value("lensDistortionEdgeMode", next.lensDistortionEdgeMode);
        next.lensDistortionEdgeRadius = root.value("lensDistortionEdgeRadius", next.lensDistortionEdgeRadius);
        next.lensDistortionEdgeSoftness = root.value("lensDistortionEdgeSoftness", next.lensDistortionEdgeSoftness);
        next.lensDistortionEdgeDarkness = root.value("lensDistortionEdgeDarkness", next.lensDistortionEdgeDarkness);
        next.lensDistortionPeripheralBlur = root.value("lensDistortionPeripheralBlur", next.lensDistortionPeripheralBlur);
        if (!std::isfinite(next.lensDistortionCurve)) next.lensDistortionCurve = 1.0f;
        if (!std::isfinite(next.lensDistortionZoom)) next.lensDistortionZoom = 1.0f;
        if (!std::isfinite(next.lensDistortionEdgeRadius)) next.lensDistortionEdgeRadius = 0.72f;
        if (!std::isfinite(next.lensDistortionEdgeSoftness)) next.lensDistortionEdgeSoftness = 0.30f;
        if (!std::isfinite(next.lensDistortionEdgeDarkness)) next.lensDistortionEdgeDarkness = 1.0f;
        if (!std::isfinite(next.lensDistortionPeripheralBlur)) next.lensDistortionPeripheralBlur = 1.0f;
        if (next.lensDistortionEdgeMode != "black" && next.lensDistortionEdgeMode != "vignette" &&
            next.lensDistortionEdgeMode != "clamp" && next.lensDistortionEdgeMode != "repeat" &&
            next.lensDistortionEdgeMode != "circle")
            next.lensDistortionEdgeMode = "circle";
        next.lensDistortionCurve = std::clamp(next.lensDistortionCurve, 0.05f, 8.0f);
        next.lensDistortionZoom = std::clamp(next.lensDistortionZoom, 0.1f, 5.0f);
        next.lensDistortionEdgeRadius = std::clamp(next.lensDistortionEdgeRadius, 0.05f, 1.2f);
        next.lensDistortionEdgeSoftness = std::clamp(next.lensDistortionEdgeSoftness, 0.0f, 1.0f);
        next.lensDistortionEdgeDarkness = std::clamp(next.lensDistortionEdgeDarkness, 0.0f, 1.0f);
        next.lensDistortionPeripheralBlur = std::clamp(next.lensDistortionPeripheralBlur, 0.0f, 4.0f);

        if (root.contains("hitFlinch") && root["hitFlinch"].is_object()) {
            const auto& flinch = root["hitFlinch"];
            next.hitFlinchEnabled = flinch.value("enabled", next.hitFlinchEnabled);
            next.hitFlinchLow = flinch.value("low", next.hitFlinchLow);
            next.hitFlinchHigh = flinch.value("high", next.hitFlinchHigh);
            next.hitFlinchDamageAtHigh = flinch.value("damageAtHigh", next.hitFlinchDamageAtHigh);
            next.hitFlinchPitch = flinch.value("pitch", next.hitFlinchPitch);
            next.hitFlinchYaw = flinch.value("yaw", next.hitFlinchYaw);
            next.hitFlinchRandomness = flinch.value("randomness", next.hitFlinchRandomness);
            next.hitFlinchDistance = flinch.value("distance", next.hitFlinchDistance);
            next.hitFlinchDistanceExponent = flinch.value("distanceExponent", next.hitFlinchDistanceExponent);
        }
        if (!std::isfinite(next.hitFlinchLow)) next.hitFlinchLow = 1.0f;
        if (!std::isfinite(next.hitFlinchHigh)) next.hitFlinchHigh = 9.0f;
        if (!std::isfinite(next.hitFlinchDamageAtHigh)) next.hitFlinchDamageAtHigh = 100.0f;
        if (!std::isfinite(next.hitFlinchPitch)) next.hitFlinchPitch = -1.0f;
        if (!std::isfinite(next.hitFlinchYaw)) next.hitFlinchYaw = 0.0f;
        if (!std::isfinite(next.hitFlinchRandomness)) next.hitFlinchRandomness = 0.0f;
        if (!std::isfinite(next.hitFlinchDistance)) next.hitFlinchDistance = 30.0f;
        if (!std::isfinite(next.hitFlinchDistanceExponent)) next.hitFlinchDistanceExponent = 1.0f;
        next.hitFlinchLow = std::max(0.0f, next.hitFlinchLow);
        next.hitFlinchHigh = std::max(next.hitFlinchLow, next.hitFlinchHigh);
        next.hitFlinchDamageAtHigh = std::max(0.001f, next.hitFlinchDamageAtHigh);
        next.hitFlinchRandomness = std::max(0.0f, next.hitFlinchRandomness);
        next.hitFlinchDistance = std::max(0.001f, next.hitFlinchDistance);
        next.hitFlinchDistanceExponent = std::max(0.001f, next.hitFlinchDistanceExponent);

        if (root.contains("cameraSway") && root["cameraSway"].is_object()) {
            const auto& sway = root["cameraSway"];
            next.cameraSwayEnabled = sway.value("enabled", next.cameraSwayEnabled);
            next.cameraSwayAmount = sway.value("amount", next.cameraSwayAmount);
            next.cameraSwayLandingThreshold = sway.value("landingThreshold", next.cameraSwayLandingThreshold);
            next.cameraSwayLandingPitchImpulse = sway.value("landingPitchImpulse", next.cameraSwayLandingPitchImpulse);
            next.cameraSwayLandingRollImpulse = sway.value("landingRollImpulse", next.cameraSwayLandingRollImpulse);
            next.cameraSwaySpringStiffness = sway.value("springStiffness", next.cameraSwaySpringStiffness);
            next.cameraSwaySpringDamping = sway.value("springDamping", next.cameraSwaySpringDamping);
            next.cameraSwayMaxPitch = sway.value("maxPitch", next.cameraSwayMaxPitch);
            next.cameraSwayMaxRoll = sway.value("maxRoll", next.cameraSwayMaxRoll);
            if (!std::isfinite(next.cameraSwayAmount) || next.cameraSwayAmount < 0.0f)
                next.cameraSwayAmount = 1.0f;
            next.cameraSwayAmount = std::clamp(next.cameraSwayAmount, 0.01f, 100.0f);
            next.cameraSwayLandingThreshold = std::max(0.0f, next.cameraSwayLandingThreshold);
            if (!std::isfinite(next.cameraSwayLandingPitchImpulse)) next.cameraSwayLandingPitchImpulse = 5.0f;
            if (!std::isfinite(next.cameraSwayLandingRollImpulse)) next.cameraSwayLandingRollImpulse = 3.0f;
            if (!std::isfinite(next.cameraSwaySpringStiffness) || next.cameraSwaySpringStiffness <= 0.0f) next.cameraSwaySpringStiffness = 28.0f;
            if (!std::isfinite(next.cameraSwaySpringDamping) || next.cameraSwaySpringDamping < 0.0f) next.cameraSwaySpringDamping = 10.0f;
            if (!std::isfinite(next.cameraSwayMaxPitch) || next.cameraSwayMaxPitch <= 0.0f) next.cameraSwayMaxPitch = 10.0f;
            if (!std::isfinite(next.cameraSwayMaxRoll) || next.cameraSwayMaxRoll <= 0.0f) next.cameraSwayMaxRoll = 6.0f;
        }

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Loaded successfully: %s  "
            "offset=(%.1f %.1f %.1f) fov=%.0f lensDistortion=%.1f "
            "stiffness=%.2f stiffEnabled=%d collision=%d pushEnabled=%d pushback=%.2f\n",
            fileName.c_str(),
            mData.offset.x, mData.offset.y, mData.offset.z,
            mData.fov, mData.lensDistortion, mData.positionStiffness,
            (int)mData.stiffnessEnabled, (int)mData.collisionEnabled,
            (int)mData.collisionPushEnabled, mData.collisionPushback);
        Debug::log(Debug::Category::General,
            "[CAM CONFIG] camera sway settings updated; weapon recoil decay source unchanged\n");
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::General,
            "[CAM CONFIG] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::General,
            "[CAM CONFIG] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool CamConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::General,
        "[CAM CONFIG] Detected change: %s\n", fileNameOf(mPath).c_str());
    Debug::warn(Debug::Category::General,
        "[CAM CONFIG] Reloading...\n");
    return load(mPath);
}
