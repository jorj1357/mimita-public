// 2026-11-24
/* purpose
* Load and hot-reload named vehicle control profiles from
* config/vehicle-profiles/*.json.
* A profile selects arcade behavior on the shared vehicle solver. It does not
* own a second car solver and does not replace vehicle templates.
* Does NOT spawn vehicles, simulate them, render, or send packets.
*/
#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

struct VehicleBoostProfile {
    float max = 100.0f;
    float fullDrainTicks = 300.0f;
    float drainPerTick = 0.3333333f;
    // Acceleration style (SI). Rocket League applies force, not impulse.
    float groundAccelMps2 = 9.91666f;
    float airAccelMps2 = 10.58333f;
    float minBoostSeconds = 0.1f;
    float force = 0.0f;
    float airForce = 0.0f;
};

// Rocket League jump model: one instantaneous velocity change, then a sustained
// acceleration while the button is held, minus a short surface-adhesion term.
// Jump height is an output of gravity and these values, never a target.
struct VehicleJumpProfile {
    float immediateDeltaVelMps = 2.9167f;   // 291.667 uu/s
    float sustainedAccelMps2 = 14.5833f;    // 1460 uu/s^2
    float sustainedWindowSeconds = 0.2f;    // JUMP_MAX_TIME
    float minSustainSeconds = 0.025f;       // JUMP_MIN_TIME
    float preMinAccelScale = 0.62f;         // pre-minimum acceleration scale
    float stickyAccelMps2 = 3.25f;          // 325 uu/s^2
    int stickyTicks = 3;
    float doubleJumpDeltaVelMps = 2.9167f;  // 291.667 uu/s
    float doubleJumpWindowSeconds = 1.25f;  // DOUBLEJUMP_MAX_DELAY
    // Dodge activates from the magnitude of the rotational inputs.
    float dodgeDeadzone = 0.1f;
    // Test/dev only: allow more than one air jump. Default off; Rocket League
    // arms a flip only after all four wheels touch a surface again.
    bool infiniteJumps = false;
};

// Finite dodge program. Direction comes from (-pitch, yaw + roll). When the
// horizontal inputs cancel, direction and torque are zero but the dodge is
// still consumed, which is the Rocket League stall.
struct VehicleFlipProfile {
    float angularAccelPitch = 21.5f;
    float angularAccelRoll = 25.0f;
    float torqueSeconds = 0.65f;
    // A dodge is not air control. Without its own limit the shared air-control
    // clamp (5.5 rad/s) caps every flip at about 205 degrees.
    float maxAngularVelocity = 20.0f;
    float pitchLockSeconds = 1.0f;
    float initialDeltaVelMps = 5.0f;
    float forwardMaxSpeedScale = 1.0f;
    float sideMaxSpeedScale = 1.9f;
    float backwardMaxSpeedScale = 2.5f;
    float backwardScaleX = 16.0f / 15.0f;
    float zDampFactor = 0.35f;
    float zDampStartSeconds = 0.15f;
    float zDampEndSeconds = 0.21f;
    float directionCancelThreshold = 0.1f;
};

// Per-axis angular acceleration and damping. There is no shared budget: each
// axis obeys alpha = A*u - D*w, and only the combined |w| is clamped.
struct VehicleAirControlProfile {
    float pitchAccel = 12.46f;
    float yawAccel = 9.11f;
    float rollAccel = 38.34f;
    float pitchDamping = 2.88f;
    float yawDamping = 1.92f;
    float rollDamping = 4.80f;
    float maxAngularVelocity = 5.5f;
    bool inputScaledDamping = true;
    float throttleAirAccelMps2 = 0.6667f;
};

struct VehicleGroundProfile {
    float maxSpeedMps = 23.0f;
    float supersonicMps = 22.0f;
    float maxUnboostedMps = 14.1f;
    float coastingDecelMps2 = 5.25f;
    float brakingDecelMps2 = 35.0f;
    float throttleDeadzone = 0.001f;
    float powerslideRisePerSecond = 5.0f;
    float powerslideFallPerSecond = 2.0f;
    float powerslideLatFrictionScale = 0.1f;
    float powerslideLongFrictionMin = 0.5f;
    float powerslideLongFrictionMax = 0.9f;
    // Rocket League cars are rigid on the ground: tyre and suspension forces
    // never pitch or roll the chassis, so accelerating or reversing cannot tip
    // the car. When true, a grounded car keeps only its yaw rate and follows
    // the averaged wheel contact normal.
    bool attitudeLock = false;
    bool alignToSurface = true;
    // Grounded steering owns the yaw rate. A car cannot rotate in place, so a
    // car stopped or barely rolling turns less; the rate is the steered
    // maximum scaled by min(1, speed / 6). Radians per second.
    float maxYawRateRadPerSecond = 3.4f;
    float yawAccelRadPerSecond2 = 14.0f;
    // Degrees per second the chassis may rotate to follow the surface normal.
    float alignRateDegPerSecond = 480.0f;
};

// The one deliberate Rocket League gameplay impulse: a non-conserving velocity
// kick added to the ball on the first tick of a car contact. Configurable and
// switchable so pure physical simulation remains available.
struct VehicleBallHitProfile {
    bool extraImpulseEnabled = true;
    float extraImpulseScale = 1.0f;
    float extraImpulseZScale = 0.35f;
    float extraImpulseForwardScale = 0.65f;
    float extraImpulseMaxDeltaVelMps = 46.0f;
    std::vector<glm::vec2> extraImpulseCurve{
        {0.0f, 0.65f}, {5.0f, 0.65f}, {23.0f, 0.55f}, {46.0f, 0.30f}};
    float carBallFriction = 2.0f;
    float carBallRestitution = 0.0f;
};

// Surface behaviour that keeps cars on their wheels without a scripted pose.
struct VehicleSettleProfile {
    // Non-RL fallback. Rocket League has no guaranteed no-input righting; this
    // remains available for experiments but is off in the RL profiles.
    bool manualRightingEnabled = false;
    float stickyForceGravityScale = 0.5f;
    float autoFlipImpulseMps = 2.0f;
    float autoFlipAngularAccel = 4.8f;
    float autoFlipSeconds = 0.4f;
    float autoFlipNormalZThreshold = 0.7071f;
    float autoFlipRollThresholdRad = 2.8f;
    float autoRollForceMps2 = 1.0f;
    float autoRollAngularAccel = 7.68f;
};


struct VehicleSteeringProfile {
    float maxAngleDeg = 35.0f;
    float speedFalloff = 0.45f;
    float returnRate = 6.0f;
};

struct VehiclePowerslideProfile {
    float gripScale = 0.3333333f;
};

struct VehicleWallProfile {
    float adhesionForce = 18000.0f;
    float adhesionMaxNormalUpDot = 0.35f;
    float slideGravityScale = 1.0f;
};

struct VehicleBallContactProfile {
    float wheelImpulseScale = 0.18f;
    float chassisImpulseScale = 1.0f;
};

struct VehicleProfile {
    std::string id;
    std::string description;
    // Provenance for the whole profile: which reference the numbers came from.
    std::string source = "rocketsim";
    bool automatic = true;
    bool engineAutoStart = true;
    bool arcade = true;
    // SI world constants used by the profile's derived values.
    float gravityMps2 = 6.5f;
    VehicleBoostProfile boost;
    VehicleJumpProfile jump;
    VehicleFlipProfile flip;
    VehicleAirControlProfile airControl;
    VehicleGroundProfile ground;
    VehicleSteeringProfile steering;
    VehiclePowerslideProfile powerslide;
    VehicleWallProfile wall;
    VehicleBallContactProfile ballContact;
    VehicleBallHitProfile ballHit;
    VehicleSettleProfile settle;
    // Logical action -> input action name (for example "boost" -> "mouse_left").
    std::unordered_map<std::string, std::string> bindings;
};

// Sample a profile piecewise-linear curve (input -> output), clamped at the
// ends. Used for the Rocket League extra-impulse scale.
float vehicleProfileCurveSample(const std::vector<glm::vec2>& curve, float input,
                                float fallback);

class VehicleProfileRegistry {
public:
    // A profile snapshot. Reloads publish a new snapshot instead of mutating the
    // live one, so a pointer handed to a caller can never be destroyed under it
    // by a concurrent reload. This is the fix for the launch crash where the
    // server tick rebuilt the registry while another thread read it.
    using ProfilePtr = std::shared_ptr<const VehicleProfile>;

    static VehicleProfileRegistry& instance();

    bool loadDirectory(const std::string& directory = "config/vehicle-profiles");
    bool pollReload();
    ProfilePtr find(const std::string& id) const;
    // Fallback when nothing named a profile (a client can miss the gamemode's
    // id and then has no key bindings at all). Deterministic: "rl_soccar" when
    // it exists, otherwise the first id in sorted order.
    ProfilePtr defaultProfile() const;
    std::vector<std::string> ids() const;

    uint64_t revision() const { return mRevision; }
    const std::string& directory() const { return mDirectory; }

private:
    VehicleProfileRegistry() = default;

    bool loadFile(const std::string& path, VehicleProfile& out) const;

    mutable std::shared_mutex mProfilesMutex;
    std::unordered_map<std::string, ProfilePtr> mProfiles;
    std::unordered_map<std::string, std::string> mProfilePaths;
    std::unordered_map<std::string, int64_t> mFileTimes;
    std::string mDirectory = "config/vehicle-profiles";
    uint64_t mRevision = 0;
};
