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
#include <string>
#include <unordered_map>
#include <vector>

struct VehicleBoostProfile {
    float max = 100.0f;
    float fullDrainTicks = 300.0f;
    float drainPerTick = 0.3333333f;
    float force = 42000.0f;
    float airForce = 42000.0f;
};

struct VehicleJumpProfile {
    float impulse = 52000.0f;
    float heldForce = 21000.0f;
    float heldWindowSeconds = 0.2f;
    float minimumHoldSeconds = 0.025f;
    float doubleJumpImpulse = 52000.0f;
    float secondJumpWindowSeconds = 1.25f;
};

struct VehicleAirControlProfile {
    float pitchTorque = 130.0f;
    float rollTorque = 400.0f;
    float pitchDamping = 30.0f;
    float rollDamping = 50.0f;
    bool yawEnabled = false;
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
    bool automatic = true;
    bool engineAutoStart = true;
    bool arcade = true;
    VehicleBoostProfile boost;
    VehicleJumpProfile jump;
    VehicleAirControlProfile airControl;
    VehicleSteeringProfile steering;
    VehiclePowerslideProfile powerslide;
    VehicleWallProfile wall;
    VehicleBallContactProfile ballContact;
    // Logical action -> input action name (for example "boost" -> "mouse_left").
    std::unordered_map<std::string, std::string> bindings;
};

class VehicleProfileRegistry {
public:
    static VehicleProfileRegistry& instance();

    bool loadDirectory(const std::string& directory = "config/vehicle-profiles");
    bool pollReload();
    const VehicleProfile* find(const std::string& id) const;
    std::vector<std::string> ids() const;

    uint64_t revision() const { return mRevision; }
    const std::string& directory() const { return mDirectory; }

private:
    VehicleProfileRegistry() = default;

    bool loadFile(const std::string& path, VehicleProfile& out) const;

    std::unordered_map<std::string, VehicleProfile> mProfiles;
    std::unordered_map<std::string, std::string> mProfilePaths;
    std::unordered_map<std::string, int64_t> mFileTimes;
    std::string mDirectory = "config/vehicle-profiles";
    uint64_t mRevision = 0;
};
