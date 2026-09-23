// 09 23 2026
/* purpose
* Implements the fixed-tick C++/JSON behavior parity harness. It reports, per
* domain, whether the compiled C++ implementation and the JSON-authored
* implementation agree for the same fixed ticks:
*   movement  - resolved tuning fields + a 60-tick collision-free trace
*   collision - response/ground/bounce policy values
*   animation - idle/walk/return_to_idle sampling at fixed times
* The harness is report-only: known tuning differences are printed, not fatal.
* Does NOT own runtime behavior. Does NOT link into the EXE.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-behavior-parity.h"

#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-behavior-source.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-movement-presets.h"
#include "hot-reload/packages/collision/collision-abi.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kTol = 1.0e-3f;
constexpr float kDt = 1.0f / 60.0f;

int g_mismatches = 0;
float g_maxDev = 0.0f;

void cmpFloat(const char* domain, const char* field, float cpp, float json)
{
    const float dev = std::fabs(cpp - json);
    if (dev > kTol) {
        ++g_mismatches;
        if (dev > g_maxDev)
            g_maxDev = dev;
        std::printf("[PARITY] %s.%s cpp=%.4f json=%.4f dev=%.4f\n", domain,
                    field, cpp, json, dev);
    }
}

void cmpU32(const char* domain, const char* field, std::uint32_t cpp,
            std::uint32_t json)
{
    if (cpp != json) {
        ++g_mismatches;
        std::printf("[PARITY] %s.%s cpp=%u json=%u\n", domain, field, cpp, json);
    }
}

// One fixed-tick collision-free movement trace through the shared hot movement
// functions, used to compare the C++ and JSON tunings end to end.
struct Trace {
    float x = 0.0f, y = 0.0f, z = 2.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
};

Trace runMovementTrace(const GameMovementTuningV1& m)
{
    using namespace MimitaHotMovement;
    Trace t;
    for (int i = 0; i < 60; ++i) {
        GameGravityV1 gv{};
        gv.velocityZ = t.vz;
        gv.gravityZ = -m.gravityMagnitude;
        gv.maximumFallSpeed = m.maxFallSpeed;
        gv.dt = kDt;
        gravity(gv, t.vz);

        GameSpeedPolicyV1 sp{};
        sp.baseMaxSpeed = m.walkSpeed;
        sp.baseFallbackSpeed = m.walkSpeed;
        sp.sizeScale = 1.0f;
        sp.sizeExponent = 0.0f;
        sp.speedLimit = m.speedLimitEnabled ? m.speedLimit : 0.0f;
        sp.speedLimitFixed = (m.speedLimitMode == 1u) ? 1u : 0u;
        sp.airMaxWishspeed = m.airMaxWishspeed;
        sp.rawWishSpeed = m.walkSpeed;
        float optMax = m.walkSpeed;
        float optWish = m.walkSpeed;
        speedPolicy(sp, optMax, optWish);

        GameAirAccelerateV1 air{};
        air.velocity[0] = t.vx;
        air.velocity[1] = t.vy;
        air.wishDir[0] = 1.0f;
        air.wishDir[1] = 0.0f;
        air.wishSpeed = m.sourceAirAccelerateBugCompatible ? optMax : optWish;
        air.wishspd = optWish;
        air.maxSpeed = optMax;
        air.airAcceleration = m.airAcceleration;
        air.surfaceFriction = m.surfaceFriction;
        air.airSpeedGainMultiplier = m.airSpeedGainMultiplier;
        air.dt = kDt;
        air.currentSpeed = t.vx;
        air.blendedAddSpeed = air.wishspd - air.currentSpeed;
        air.movementModel = m.sourceAirAccelerateBugCompatible ? 1u : 0u;
        float out[2] = {t.vx, t.vy};
        airAccelerate(air, out);
        t.vx = out[0];
        t.vy = out[1];

        t.x += t.vx * kDt;
        t.y += t.vy * kDt;
        t.z += t.vz * kDt;
    }
    return t;
}

void compareMovement()
{
    using namespace MimitaHotMovement;
    const MovementPresetId id = kActiveMovementPreset;
    const GameMovementTuningV1 cpp = getMovementPreset(id).tuning;
    GameMovementTuningV1 json{};
    if (!loadJsonMovementPreset(getMovementPreset(id).name, json)) {
        std::printf("[PARITY] movement: no JSON preset '%s' (C++ only)\n",
                    getMovementPreset(id).name);
        return;
    }
    cmpFloat("movement", "walkSpeed", cpp.walkSpeed, json.walkSpeed);
    cmpFloat("movement", "groundSpeed", cpp.groundSpeed, json.groundSpeed);
    cmpFloat("movement", "airSpeed", cpp.airSpeed, json.airSpeed);
    cmpFloat("movement", "groundAcceleration", cpp.groundAcceleration,
             json.groundAcceleration);
    cmpFloat("movement", "airAcceleration", cpp.airAcceleration,
             json.airAcceleration);
    cmpFloat("movement", "groundFriction", cpp.groundFriction, json.groundFriction);
    cmpFloat("movement", "sourceFriction", cpp.sourceFriction, json.sourceFriction);
    cmpFloat("movement", "stopspeed", cpp.stopspeed, json.stopspeed);
    cmpFloat("movement", "airMaxWishspeed", cpp.airMaxWishspeed,
             json.airMaxWishspeed);
    cmpFloat("movement", "airSpeedGainMultiplier", cpp.airSpeedGainMultiplier,
             json.airSpeedGainMultiplier);
    cmpFloat("movement", "surfaceFriction", cpp.surfaceFriction,
             json.surfaceFriction);
    cmpFloat("movement", "gravityMagnitude", cpp.gravityMagnitude,
             json.gravityMagnitude);
    cmpFloat("movement", "jumpSpeed", cpp.jumpSpeed, json.jumpSpeed);
    cmpFloat("movement", "maxFallSpeed", cpp.maxFallSpeed, json.maxFallSpeed);
    cmpFloat("movement", "jumpBufferSeconds", cpp.jumpBufferSeconds,
             json.jumpBufferSeconds);
    cmpFloat("movement", "coyoteSeconds", cpp.coyoteSeconds, json.coyoteSeconds);
    cmpFloat("movement", "groundDashImpulse", cpp.groundDashImpulse,
             json.groundDashImpulse);
    cmpFloat("movement", "airDashImpulse", cpp.airDashImpulse,
             json.airDashImpulse);
    cmpFloat("movement", "downDashSpeed", cpp.downDashSpeed, json.downDashSpeed);
    cmpFloat("movement", "freezeDurationSeconds", cpp.freezeDurationSeconds,
             json.freezeDurationSeconds);
    cmpFloat("movement", "freezeCurveExponent", cpp.freezeCurveExponent,
             json.freezeCurveExponent);
    cmpFloat("movement", "speedLimit", cpp.speedLimit, json.speedLimit);
    cmpU32("movement", "maximumAirJumps", cpp.maximumAirJumps,
           json.maximumAirJumps);
    cmpU32("movement", "autoBhopEnabled", cpp.autoBhopEnabled, json.autoBhopEnabled);
    cmpU32("movement", "sourceAirAccelerateBugCompatible",
           cpp.sourceAirAccelerateBugCompatible,
           json.sourceAirAccelerateBugCompatible);
    cmpU32("movement", "speedLimitEnabled", cpp.speedLimitEnabled,
           json.speedLimitEnabled);
    cmpU32("movement", "speedLimitMode", cpp.speedLimitMode, json.speedLimitMode);
    cmpU32("movement", "walkMode", cpp.walkMode, json.walkMode);

    // Fixed-tick trace parity: the resolved tuning difference, if any, must show
    // up here as a root position/velocity divergence.
    const Trace a = runMovementTrace(cpp);
    const Trace b = runMovementTrace(json);
    cmpFloat("movement.trace", "x", a.x, b.x);
    cmpFloat("movement.trace", "y", a.y, b.y);
    cmpFloat("movement.trace", "z", a.z, b.z);
    cmpFloat("movement.trace", "vx", a.vx, b.vx);
    cmpFloat("movement.trace", "vy", a.vy, b.vy);
    cmpFloat("movement.trace", "vz", a.vz, b.vz);
}

void compareCollision()
{
    using namespace HotCollisionPackage;
    const CollisionBehaviorV1 fallback{0u, 0u, 1u, 0.35f, 0.0f, 0.01f,
                                       999999.0f, 0.01f};
    const CollisionBehaviorV1 active = collisionBehavior();
    cmpU32("collision", "groundBounce", fallback.groundBounce,
           active.groundBounce);
    cmpU32("collision", "bounceEnabled", fallback.bounceEnabled,
           active.bounceEnabled);
    cmpFloat("collision", "bounceStrength", fallback.bounceStrength,
             active.bounceStrength);
    cmpFloat("collision", "bounceFriction", fallback.bounceFriction,
             active.bounceFriction);
    cmpFloat("collision", "bounceMinSpeed", fallback.bounceMinSpeed,
             active.bounceMinSpeed);
    cmpFloat("collision", "bounceCooldown", fallback.bounceCooldown,
             active.bounceCooldown);
}

void compareAnimation()
{
    using namespace HotAnim;
    const std::uint64_t actions[] = {HOT_ACTION_IDLE, HOT_ACTION_WALK,
                                     HOT_ACTION_RETURN_TO_IDLE};
    const float times[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.9f};
    for (std::uint64_t action : actions) {
        if (!jsonClipApplied(action))
            continue;
        const ActionClip clip = actionClip(action);
        for (float time : times) {
            Pose cpp{};
            Pose json{};
            afad20aSampleClip(clip, time, 1.0f, cpp);
            sampleClip(clip, time, json);
            float dev = 0.0f;
            for (std::uint32_t p = 0; p < PartCount; ++p)
                for (int k = 0; k < 6; ++k)
                    dev = std::max(dev, std::fabs(cpp.part[p].trans[k] -
                                                  json.part[p].trans[k]));
            if (dev > kTol) {
                ++g_mismatches;
                if (dev > g_maxDev)
                    g_maxDev = dev;
                std::printf(
                    "[PARITY] animation.%s t=%.2f cpp/json dev=%.4f\n",
                    actionConfigName(action), time, dev);
            }
        }
    }
}

} // namespace

bool runBehaviorParitySelfTest(char* message, std::uint32_t messageSize)
{
    g_mismatches = 0;
    g_maxDev = 0.0f;

    const MimitaBehavior::Source movement = MimitaBehavior::movementSource();
    const MimitaBehavior::Source collision = MimitaBehavior::collisionSource();
    const MimitaBehavior::Source animation = MimitaBehavior::animationSource();

    compareMovement();
    compareCollision();
    compareAnimation();

    std::printf(
        "[PARITY] sources movement=%s collision=%s animation=%s mismatches=%d "
        "maxDev=%.4f\n",
        MimitaBehavior::name(movement), MimitaBehavior::name(collision),
        MimitaBehavior::name(animation), g_mismatches, g_maxDev);

    if (message && messageSize)
        std::snprintf(message, messageSize,
                      "behavior parity: m=%s c=%s a=%s mismatches=%d maxDev=%.3f",
                      MimitaBehavior::name(movement),
                      MimitaBehavior::name(collision),
                      MimitaBehavior::name(animation), g_mismatches, g_maxDev);
    // Report-only: finite comparison is success; divergences are surfaced, not
    // fatal, because the active C++/JSON source is an explicit choice.
    return std::isfinite(g_maxDev);
}

#endif // MIMITA_GAME_DLL
