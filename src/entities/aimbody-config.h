// 08 15 2026, 22 40
/* purpose
* Hot-reloadable config that drives how each body limb rotates with the camera
* look pitch (up/down). One JSON file (config/aimbody.json) controls, per limb,
* how much it pitches/rolls/yaws and around which character axis, all live.
* Does NOT own the animation math, the pose pipeline, or firing logic.
* Does NOT define movement physics or camera behavior.
*/
#pragma once

#include <string>
#include <unordered_map>

struct LimbAim {
    float pitch = 0.0f;  // rotation about the character's left-right axis (tilt up/down)
    float yaw = 0.0f;    // rotation about the character's up axis (turn left/right)
    float roll = 0.0f;   // rotation about the character's forward axis (tilt sideways)
};

class AimBodyConfig {
public:
    static AimBodyConfig& instance();

    bool load(const std::string& path = "config/aimbody.json");
    bool save();
    bool reload();
    bool pollReload();

    bool enabled() const { return mEnabled; }
    void setEnabled(bool on) { mEnabled = on; }

    // Returns the per-limb aim gains for a body part name, or nullptr if the
    // limb is not configured.
    const LimbAim* limb(const std::string& name) const;

    const std::unordered_map<std::string, LimbAim>& limbs() const { return mLimbs; }
    void setLimb(const std::string& name, const LimbAim& v) { mLimbs[name] = v; }
    void setLimbPitch(const std::string& name, float v) { mLimbs[name].pitch = v; }
    void setLimbYaw(const std::string& name, float v) { mLimbs[name].yaw = v; }
    void setLimbRoll(const std::string& name, float v) { mLimbs[name].roll = v; }

    // Diagnostics for aimbody_dump: set by the animation when it applied a
    // rotation last frame.
    bool appliedLastFrame() const { return mAppliedLastFrame; }
    float lastLookPitch() const { return mLastLookPitch; }
    void setApplied(float lookPitch) { mAppliedLastFrame = true; mLastLookPitch = lookPitch; }
    void clearApplied() { mAppliedLastFrame = false; }

private:
    AimBodyConfig() = default;

    bool mEnabled = true;
    std::unordered_map<std::string, LimbAim> mLimbs;
    std::string mPath = "config/aimbody.json";
    int64_t mLastModified = 0;
    int64_t mLastPollMs = 0;

    bool mAppliedLastFrame = false;
    float mLastLookPitch = 0.0f;
};
