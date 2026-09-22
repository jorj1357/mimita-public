// 09 17 2026
/* purpose
* One hot C++ movement preset registry: the single runtime authority for
* movement tuning. Holds every supported preset (source, default, heavy,
* retrograd_fast, counterstrike) with the values previously owned by
* config/movement/*.json, plus a stable preset-selection API.
* The active preset is a fixed C++ constant (kActiveMovementPreset); changing it
* means editing this header and rebuilding the replaceable game DLL, not the EXE.
* Does NOT run movement math, own collision, or read JSON.
* Does NOT link into the EXE as the live authority; cold code may include this
* only to build a no-DLL fallback from the same single definition.
*/
#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "hot-reload/hot-movement-policy.h"

namespace MimitaHotMovement {

enum class MovementPresetId : std::uint8_t {
    Source = 0,
    Default = 1,
    Heavy = 2,
    RetrogradFast = 3,
    Counterstrike = 4,
    Count = 5
};

inline constexpr std::uint32_t kMovementPresetCount =
    static_cast<std::uint32_t>(MovementPresetId::Count);

// MovementWalkMode numeric mirror: 0 = Override (mimita), 1 = Accel, 2 = Source.
inline constexpr std::uint32_t kWalkModeMimita = 0;
inline constexpr std::uint32_t kWalkModeAccel = 1;
inline constexpr std::uint32_t kWalkModeSource = 2;
// v2.0.6 reference model: XOR ground friction/accelerate, additive air accel.
inline constexpr std::uint32_t kWalkModeV206 = 3;

// ── Base tuning ─────────────────────────────────────────────────────────────
// Exactly the built-in MiMITA base used before JSON overrides. Presets below
// override only the fields their JSON file set; every other field keeps this
// base value, matching the JSON loader's "override on top of base" semantics.
inline constexpr GameMovementTuningV1 movementTuningBase()
{
    GameMovementTuningV1 t{};
    t.walkMode = kWalkModeMimita;
    t.sourceWalkMode = 0;

    t.walkSpeed = 20.0f;
    t.groundSpeed = 20.0f;
    t.airSpeed = 20.0f;
    t.groundAcceleration = 55.0f;
    t.groundDeceleration = 0.0f;
    t.groundDirectionChangeResponse = 0.0f;
    t.airAcceleration = 22.0f;
    t.groundFriction = 0.0f;         // source friction (sv_friction)
    t.groundFrictionAmount = 10.0f;  // mimita ground friction amount
    t.airFrictionAmount = 2.0f;
    t.stopspeed = 0.0f;
    t.airMaxWishspeed = 0.0f;
    t.airSpeedGainMultiplier = 0.0f;
    t.airControl = 0.0f;
    t.surfaceFriction = 1.0f;

    t.gravityMagnitude = 58.0f;
    t.jumpSpeed = 19.0f;
    t.maxFallSpeed = 400.0f;
    t.jumpBufferSeconds = 0.12f;
    t.coyoteSeconds = 0.001f;

    t.dashImpulse = 50.0f;
    t.groundDashImpulse = 100.0f;
    t.airDashImpulse = 50.0f;
    t.dashCooldownSeconds = 0.0f;
    t.downDashSpeed = -100.0f;
    t.dashGraceSeconds = 0.0f;
    t.dashFrictionMultiplier = 1.0f;

    t.freezeDurationSeconds = 5.0f;
    t.freezeCurveExponent = 4.0f;

    t.externalImpulseDecay = 0.6f;
    t.maximumExternalImpulseSpeed = 120.0f;
    t.impulseCarrySeconds = 0.0f;
    t.impulseFrictionMode = 0u;  // exponential

    t.landingOverspeedBleed = 1.0f;
    t.landingSpeedRetention = 0.0f;
    t.velocityClipEpsilon = 0.01f;

    t.speedLimitEnabled = 0u;
    t.speedLimit = 0.0f;
    t.speedLimitMode = 0u;  // clamp
    t.bunnyHopSpeedCap = 0.0f;

    t.minimumStrafeAngleDegrees = 0.0f;
    t.maximumAccelerationPerTick = 0.0f;
    t.accelerationFalloffNearCap = 0.0f;
    t.airSteeringResponse = 1.0f;
    t.maximumSteeringDegreesPerSecond = 0.0f;
    t.minimumCameraYawDeltaDegrees = 0.25f;
    t.minimumWishRotationDegrees = 0.25f;
    t.strafeAngularToleranceDegrees = 60.0f;
    t.softCapStart = 0.0f;
    t.airInputBlending = 0.0f;
    t.airInputMouseThresholdDegrees = 2.0f;
    t.sourceMaxSpeed = 0.0f;
    t.sourceFriction = 0.0f;
    t.freeFlySpeed = 12.0f;

    t.maximumAirJumps = 1u;
    t.autoBhopEnabled = 1u;
    t.dashEnabled = 1u;
    t.downDashEnabled = 1u;
    t.freezeEnabled = 1u;
    t.airControlEnabled = 1u;
    t.bunnyHopEnabled = 0u;
    t.preserveStraightSpeed = 1u;
    t.diagonalInputNormalization = 1u;
    t.speedCapEnabled = 0u;
    t.maximumBhopSpeedMode = 0u;  // none
    t.debugDrawEnabled = 0u;
    t.requireActiveWishRotation = 1u;
    t.stationaryCameraInputMode = 0u;  // strict
    t.sourceAirAccelerateBugCompatible = 1u;
    t.groundSnap = 1u;
    t.airInputBlendingEnabled = 0u;
    return t;
}

// ── source ──────────────────────────────────────────────────────────────────
// config/movement/movement-source.json (fast Source/GoldSrc preset). This is the
// "source movement" the project wants; not to be confused with the built-in
// legacy default (movement-default.json).
inline constexpr GameMovementTuningV1 makeSourcePresetTuning()
{
    GameMovementTuningV1 t = movementTuningBase();
    t.walkMode = kWalkModeV206;
    t.sourceWalkMode = 1;
    t.groundSpeed = 20.0f;
    t.airSpeed = 20.0f;
    t.sourceMaxSpeed = 20.0f;
    t.groundAcceleration = 8.0f;
    t.groundFriction = 4.0f;
    t.groundFrictionAmount = 4.0f;
    t.sourceFriction = 4.0f;
    t.stopspeed = 0.0f;
    t.airSpeedGainMultiplier = 0.0f;
    t.airInputBlendingEnabled = 0u;
    t.airInputBlending = 1.0f;
    t.airInputMouseThresholdDegrees = 0.1f;  // loader clamps 0.0 -> 0.1
    t.airAcceleration = 222.0f;
    t.airMaxWishspeed = 1.0f;
    t.sourceAirAccelerateBugCompatible = 1u;
    t.gravityMagnitude = 58.0f;
    t.jumpSpeed = 19.0f;
    t.maxFallSpeed = 400.0f;
    t.autoBhopEnabled = 1u;
    t.jumpBufferSeconds = 0.12f;
    t.coyoteSeconds = 0.001f;
    t.maximumAirJumps = 1u;
    t.groundSnap = 1u;
    t.velocityClipEpsilon = 1.01f;
    t.surfaceFriction = 1.0f;
    t.landingOverspeedBleed = 0.0f;
    t.dashEnabled = 1u;
    t.downDashEnabled = 1u;
    t.freezeEnabled = 1u;
    t.dashGraceSeconds = 1.0f;
    t.dashFrictionMultiplier = 0.0f;
    t.groundDashImpulse = 100.0f;
    t.airDashImpulse = 50.0f;
    t.dashImpulse = 100.0f;
    t.downDashSpeed = -100.0f;
    t.externalImpulseDecay = 0.6f;
    t.maximumExternalImpulseSpeed = 120.0f;
    t.impulseFrictionMode = 0u;
    t.impulseCarrySeconds = 0.1f;
    t.airControlEnabled = 1u;
    t.debugDrawEnabled = 0u;
    t.speedLimitEnabled = 0u;
    t.speedLimit = 50.0f;
    t.speedLimitMode = 1u;  // fixed
    return t;
}

// ── default ─────────────────────────────────────────────────────────────────
// config/movement/movement-default.json: the original instant-control MiMITA
// movement. Selected name is "default".
inline constexpr GameMovementTuningV1 makeDefaultPresetTuning()
{
    GameMovementTuningV1 t = movementTuningBase();
    t.walkMode = kWalkModeMimita;
    t.sourceWalkMode = 0;
    t.groundSpeed = 20.0f;
    t.airSpeed = 20.0f;
    t.groundAcceleration = 55.0f;
    t.groundDeceleration = 0.0f;
    t.groundDirectionChangeResponse = 0.0f;
    t.airAcceleration = 22.0f;
    t.airMaxWishspeed = 0.0f;
    t.airControl = 0.0f;
    t.airSpeedGainMultiplier = 0.0f;
    t.stopspeed = 1.0f;
    t.gravityMagnitude = 58.0f;
    t.jumpSpeed = 18.0f;
    t.maxFallSpeed = 400.0f;
    t.jumpBufferSeconds = 0.12f;
    t.coyoteSeconds = 0.001f;
    t.maximumAirJumps = 1u;
    t.groundFrictionAmount = 1.0f;
    t.airFrictionAmount = 0.0f;
    t.externalImpulseDecay = 0.6f;
    t.maximumExternalImpulseSpeed = 120.0f;
    t.groundDashImpulse = 100.0f;
    t.airDashImpulse = 50.0f;
    t.dashImpulse = 50.0f;
    t.downDashSpeed = -100.0f;
    t.airControlEnabled = 1u;
    t.bunnyHopEnabled = 1u;
    t.autoBhopEnabled = 1u;
    t.preserveStraightSpeed = 1u;
    t.diagonalInputNormalization = 1u;
    t.minimumStrafeAngleDegrees = 0.0f;
    t.maximumAccelerationPerTick = 0.0f;
    t.speedCapEnabled = 0u;
    t.bunnyHopSpeedCap = 0.0f;
    t.maximumBhopSpeedMode = 0u;
    t.accelerationFalloffNearCap = 0.0f;
    t.landingSpeedRetention = 0.0f;
    t.requireActiveWishRotation = 1u;
    t.stationaryCameraInputMode = 0u;
    t.airSteeringResponse = 1.0f;
    t.maximumSteeringDegreesPerSecond = 0.0f;
    // 1110.25 in JSON is clamped to the 0..180 range by the loader.
    t.minimumCameraYawDeltaDegrees = 180.0f;
    t.minimumWishRotationDegrees = 0.25f;
    t.strafeAngularToleranceDegrees = 60.0f;
    t.softCapStart = 0.0f;
    t.debugDrawEnabled = 0u;
    t.speedLimitEnabled = 0u;
    return t;
}

// ── heavy ───────────────────────────────────────────────────────────────────
// config/movement/movement-heavy.json (role preset).
inline constexpr GameMovementTuningV1 makeHeavyPresetTuning()
{
    GameMovementTuningV1 t = movementTuningBase();
    t.walkMode = kWalkModeMimita;
    t.sourceWalkMode = 0;
    t.groundSpeed = 13.0f;
    t.airSpeed = 13.0f;
    t.groundAcceleration = 28.0f;
    t.airAcceleration = 9.0f;
    t.jumpSpeed = 13.0f;
    t.gravityMagnitude = 70.0f;
    t.groundDashImpulse = 40.0f;
    t.airDashImpulse = 20.0f;
    t.dashImpulse = 20.0f;
    t.downDashSpeed = -60.0f;
    t.dashEnabled = 0u;
    t.downDashEnabled = 0u;
    t.freezeEnabled = 0u;
    return t;
}

// ── retrograd_fast ──────────────────────────────────────────────────────────
// config/movement/movement-retrograd-fast.json (role preset).
inline constexpr GameMovementTuningV1 makeRetrogradFastPresetTuning()
{
    GameMovementTuningV1 t = movementTuningBase();
    t.walkMode = kWalkModeMimita;
    t.sourceWalkMode = 0;
    t.groundSpeed = 24.0f;
    t.airSpeed = 24.0f;
    t.groundAcceleration = 62.0f;
    t.airAcceleration = 28.0f;
    t.jumpSpeed = 19.0f;
    t.gravityMagnitude = 58.0f;
    t.groundDashImpulse = 118.0f;
    t.airDashImpulse = 62.0f;
    t.dashImpulse = 62.0f;
    t.downDashSpeed = -110.0f;
    t.dashEnabled = 1u;
    t.downDashEnabled = 1u;
    t.freezeEnabled = 1u;
    return t;
}

// ── counterstrike ───────────────────────────────────────────────────────────
// config/movement/movement-cs.json (Source/GoldSrc-family CS-style preset).
inline constexpr GameMovementTuningV1 makeCounterstrikePresetTuning()
{
    GameMovementTuningV1 t = movementTuningBase();
    t.walkMode = kWalkModeSource;
    t.sourceWalkMode = 1;
    t.groundSpeed = 20.0f;
    t.airSpeed = 20.0f;
    t.sourceMaxSpeed = 20.0f;
    t.groundAcceleration = 5.5f;
    t.groundFriction = 5.2f;
    t.sourceFriction = 5.2f;
    t.stopspeed = 3.75f;
    t.surfaceFriction = 1.0f;
    t.diagonalInputNormalization = 1u;
    t.bunnyHopEnabled = 1u;
    t.autoBhopEnabled = 1u;
    t.airControlEnabled = 1u;
    t.airAcceleration = 12.0f;
    t.airMaxWishspeed = 1.875f;
    t.sourceAirAccelerateBugCompatible = 1u;
    t.airSpeedGainMultiplier = 1.0f;
    t.gravityMagnitude = 58.0f;
    t.jumpSpeed = 19.0f;
    t.maxFallSpeed = 400.0f;
    t.jumpBufferSeconds = 0.12f;
    t.coyoteSeconds = 0.001f;
    t.maximumAirJumps = 0u;
    t.airFrictionAmount = 0.0f;
    t.externalImpulseDecay = 0.6f;
    t.maximumExternalImpulseSpeed = 120.0f;
    t.groundDashImpulse = 100.0f;
    t.airDashImpulse = 50.0f;
    t.dashImpulse = 50.0f;
    t.downDashSpeed = -100.0f;
    t.debugDrawEnabled = 0u;
    return t;
}

struct MovementPreset {
    const char* name;
    GameMovementTuningV1 tuning;
};

// The one preset table. Order matches MovementPresetId.
inline constexpr MovementPreset kMovementPresets[kMovementPresetCount] = {
    {"source", makeSourcePresetTuning()},
    {"default", makeDefaultPresetTuning()},
    {"heavy", makeHeavyPresetTuning()},
    {"retrograd_fast", makeRetrogradFastPresetTuning()},
    {"counterstrike", makeCounterstrikePresetTuning()},
};

// The active preset is a fixed C++ value: edit here, save, rebuild the DLL.
inline constexpr MovementPresetId kActiveMovementPreset = MovementPresetId::Source;

inline MovementPresetId movementPresetClamp(std::uint32_t raw)
{
    return raw < kMovementPresetCount ? static_cast<MovementPresetId>(raw)
                                      : MovementPresetId::Source;
}

inline const MovementPreset& getMovementPreset(MovementPresetId id)
{
    return kMovementPresets[static_cast<std::uint32_t>(movementPresetClamp(
        static_cast<std::uint32_t>(id)))];
}

inline const MovementPreset& getActiveMovementPreset()
{
    return getMovementPreset(kActiveMovementPreset);
}

inline MovementPresetId movementPresetIdFromName(const char* name)
{
    if (name && name[0]) {
        for (std::uint32_t i = 0; i < kMovementPresetCount; ++i) {
            if (std::strcmp(name, kMovementPresets[i].name) == 0)
                return static_cast<MovementPresetId>(i);
        }
    }
    return kActiveMovementPreset;
}

inline bool movementPresetNameExists(const char* name)
{
    if (!name || !name[0])
        return false;
    for (std::uint32_t i = 0; i < kMovementPresetCount; ++i) {
        if (std::strcmp(name, kMovementPresets[i].name) == 0)
            return true;
    }
    return false;
}

// Maps the per-actor ActorProfileState movementPresetHash (gameHash(name)) back
// to a preset. Unknown/zero falls back to the active preset.
inline MovementPresetId movementPresetIdFromHash(std::uint64_t hash)
{
    if (hash != 0u) {
        for (std::uint32_t i = 0; i < kMovementPresetCount; ++i) {
            if (gameHash(kMovementPresets[i].name) == hash)
                return static_cast<MovementPresetId>(i);
        }
    }
    return kActiveMovementPreset;
}

enum class MovementBehaviorSource : std::uint8_t { Cpp = 0, Json = 1 };

inline MovementBehaviorSource movementBehaviorSourceFromJson(
    std::string* outPreset = nullptr)
{
    if (outPreset)
        outPreset->clear();
    std::ifstream selector("config/movement.json");
    if (!selector)
        return MovementBehaviorSource::Cpp;
    try {
        const nlohmann::json j = nlohmann::json::parse(selector, nullptr, true, true);
        const std::string source = j.value("behaviorSource", "cpp");
        if (outPreset)
            *outPreset = j.value("preset", "source");
        return source == "json" ? MovementBehaviorSource::Json
                                  : MovementBehaviorSource::Cpp;
    } catch (...) {
        return MovementBehaviorSource::Cpp;
    }
}

inline std::string movementPresetJsonPath(const std::string& name)
{
    const std::string preset = name.empty() ? "source" : name;
    std::string stem = preset;
    if (preset == "counterstrike")
        stem = "cs";
    else if (preset == "retrograd_fast")
        stem = "retrograd-fast";
    return "config/movement/movement-" + stem + ".json";
}

inline bool loadJsonMovementPreset(const std::string& name,
                                   GameMovementTuningV1& out)
{
    const std::string preset = name.empty() ? "source" : name;
    const std::string filePath = movementPresetJsonPath(preset);
    if (!std::filesystem::exists(filePath))
        return false;
    std::ifstream file(filePath);
    if (!file)
        return false;
    try {
        const nlohmann::json j = nlohmann::json::parse(file, nullptr, true, true);
        GameMovementTuningV1 t = movementTuningBase();
        const std::string mode = j.value("movement_mode", "mimita");
        t.walkMode = mode == "source" ? kWalkModeSource
                    : mode == "v206" ? kWalkModeV206
                    : mode == "accel" ? kWalkModeAccel : kWalkModeMimita;
        t.sourceWalkMode = (mode == "source" || mode == "v206") ? 1u : 0u;
        auto f = [&](const char* key, float& v) { if (j.contains(key) && j[key].is_number()) v = j[key].get<float>(); };
        auto b = [&](const char* key, std::uint32_t& v) { if (j.contains(key) && j[key].is_boolean()) v = j[key].get<bool>() ? 1u : 0u; };
        auto u = [&](const char* key, std::uint32_t& v) { if (j.contains(key) && j[key].is_number_integer()) v = j[key].get<std::uint32_t>(); };
        f("max_speed", t.walkSpeed); f("max_speed", t.groundSpeed); f("max_speed", t.airSpeed);
        f("max_speed", t.sourceMaxSpeed);
        f("ground_speed", t.groundSpeed); f("ground_acceleration", t.groundAcceleration);
        f("friction", t.groundFriction); f("friction", t.sourceFriction);
        f("ground_friction", t.groundFrictionAmount);
        f("stop_speed", t.stopspeed); f("air_acceleration", t.airAcceleration);
        f("air_max_wishspeed", t.airMaxWishspeed); f("air_speed_gain_multiplier", t.airSpeedGainMultiplier);
        f("gravity", t.gravityMagnitude); t.gravityMagnitude = std::abs(t.gravityMagnitude);
        f("jump_velocity", t.jumpSpeed); f("max_fall_speed", t.maxFallSpeed);
        f("jump_buffer_time", t.jumpBufferSeconds); f("coyote_time", t.coyoteSeconds);
        f("ground_dash_impulse", t.groundDashImpulse); f("air_dash_impulse", t.airDashImpulse);
        f("dash_impulse", t.dashImpulse);
        f("down_dash_speed", t.downDashSpeed); f("dash_grace_seconds", t.dashGraceSeconds);
        f("dash_friction_multiplier", t.dashFrictionMultiplier);
        f("external_impulse_decay", t.externalImpulseDecay);
        f("max_external_impulse_speed", t.maximumExternalImpulseSpeed);
        f("impulse_carry_seconds", t.impulseCarrySeconds);
        f("speed_limit", t.speedLimit); f("velocity_clip_epsilon", t.velocityClipEpsilon);
        f("surface_friction", t.surfaceFriction); f("air_input_blending", t.airInputBlending);
        f("air_input_mouse_threshold_degrees", t.airInputMouseThresholdDegrees);
        if (t.airInputMouseThresholdDegrees < 0.1f)
            t.airInputMouseThresholdDegrees = 0.1f;
        f("landing_overspeed_bleed", t.landingOverspeedBleed);
        b("air_strafing", t.airControlEnabled); b("auto_bhop_enabled", t.autoBhopEnabled);
        b("dash_enabled", t.dashEnabled); b("down_dash_enabled", t.downDashEnabled);
        b("freeze_enabled", t.freezeEnabled); b("ground_snap", t.groundSnap);
        b("air_input_blending_enabled", t.airInputBlendingEnabled);
        b("source_airaccelerate_bug_compatible", t.sourceAirAccelerateBugCompatible);
        b("speed_limit_enabled", t.speedLimitEnabled); u("max_air_jumps", t.maximumAirJumps);
        t.speedLimitMode = j.value("speed_limit_mode", "clamp") == "fixed" ? 1u : 0u;
        t.impulseFrictionMode =
            j.value("impulse_friction_mode", "exponential") == "source" ? 1u : 0u;
        t.presetId = static_cast<std::uint32_t>(movementPresetIdFromName(preset.c_str()));
        out = t;
        return true;
    } catch (...) {
        return false;
    }
}

inline const char* movementBehaviorSourceName(MovementBehaviorSource source)
{
    return source == MovementBehaviorSource::Json ? "json" : "cpp";
}

} // namespace MimitaHotMovement
