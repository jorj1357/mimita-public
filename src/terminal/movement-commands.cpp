// 08 15 2026, 16 12
/* purpose
* Registers terminal commands for the movement tuning presets.
* Lets users switch presets, list presets, reload, and print active tuning values.
* Reuses the MovementJsonConfig singleton for all preset loading and persistence.
* Does NOT run movement physics, parse movement formulas, or own tuning defaults.
* Does NOT edit movement preset files or the selector file except through savePresetSelection.
*/

#include "terminal/movement-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include "config/movement-config.h"
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"

void registerMovementCommands()
{
    Terminal::instance().registerCommand({
        "movement_presets",
        "List available movement presets from config/movement/",
        "movement_presets",
        [](const std::vector<std::string>&) {
            const auto presets = MovementJsonConfig::instance().availablePresets();
            if (presets.empty()) {
                Terminal::instance().addLog(
                    "[MOVEMENT] No presets found in config/movement/");
                return;
            }
            std::string list = "[MOVEMENT] Available presets:";
            for (const auto& name : presets)
                list += " " + name;
            Terminal::instance().addLog(list);
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_preset",
        "Load a movement preset by name and persist the selection to config/movement.json",
        "movement_preset <name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[MOVEMENT] Usage: movement_preset <name>. Active: " +
                    MovementJsonConfig::instance().activePresetName());
                return;
            }
            if (!MovementJsonConfig::instance().savePresetSelection(args[0])) {
                Terminal::instance().addLog(
                    "[MOVEMENT] Failed to load preset: " + args[0]);
                return;
            }
            Terminal::instance().addLog(
                "[MOVEMENT] Active preset: " +
                MovementJsonConfig::instance().activePresetName());
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_reload",
        "Reload the active movement preset from disk",
        "movement_reload",
        [](const std::vector<std::string>&) {
            MovementJsonConfig::instance().load(
                MovementJsonConfig::instance().selectorPath());
            Terminal::instance().addLog(
                "[MOVEMENT] Reloaded. Active preset: " +
                MovementJsonConfig::instance().activePresetName());
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_debug",
        "Toggle the bhop/air-accel debug overlay (0=off, 1=on)",
        "movement_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[MOVEMENT] Usage: movement_debug <0|1>. Currently " +
                    std::string(MovementJsonConfig::instance().config().debugDrawEnabled
                                    ? "on" : "off"));
                return;
            }
            const bool enabled = args[0] == "1";
            MovementJsonConfig::instance().setDebugDrawEnabled(enabled);
            Terminal::instance().addLog(
                enabled ? "[MOVEMENT] bhop debug overlay ON"
                        : "[MOVEMENT] bhop debug overlay OFF");
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_print",
        "Print the active movement tuning values",
        "movement_print",
        [](const std::vector<std::string>&) {
            const auto& cfg = MovementJsonConfig::instance().config();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] preset=%s mode=%s air_strafing=%d bhop=%d auto_bhop=%d",
                MovementJsonConfig::instance().activePresetName().c_str(),
                cfg.walkMode == MovementWalkMode::Accel ? "accel" :
                cfg.walkMode == MovementWalkMode::Source ? "source" : "mimita",
                (int)cfg.airControlEnabled, (int)cfg.bunnyHopEnabled,
                (int)cfg.autoBhopEnabled);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] ground_speed=%.1f air_speed=%.1f ground_accel=%.1f air_accel=%.1f",
                cfg.groundSpeed, cfg.airSpeed,
                cfg.groundAcceleration, cfg.airAcceleration);
            Terminal::instance().addLog(buf);
            if (cfg.walkMode == MovementWalkMode::Source) {
                std::snprintf(buf, sizeof(buf),
                    "[MOVEMENT][source] max_speed=%.1f friction=%.2f stop_speed=%.2f "
                    "air_accel=%.1f air_cap=%.3f bug_compat=%d air_gain=%.2f surface_friction=%.2f",
                    cfg.sourceMaxSpeed, cfg.sourceFriction, cfg.stopspeed,
                    cfg.airAcceleration, cfg.airMaxWishspeed,
                    (int)cfg.sourceAirAccelerateBugCompatible,
                    cfg.airSpeedGainMultiplier, cfg.surfaceFriction);
                Terminal::instance().addLog(buf);
                std::snprintf(buf, sizeof(buf),
                    "[MOVEMENT][source] landing_bleed=%.2f dash_grace=%.2f dash_friction=%.2f "
                    "impulse_mode=%s impulse_carry=%.2f impulse_max=%.1f",
                    cfg.landingOverspeedBleed, cfg.dashGraceSeconds,
                    cfg.dashFrictionMultiplier,
                    cfg.impulseFrictionMode == MovementImpulseFrictionMode::Source
                        ? "source" : "exponential",
                    cfg.impulseCarrySeconds, cfg.maximumExternalImpulseSpeed);
                Terminal::instance().addLog(buf);
            }
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] air_max_wishspeed=%.1f air_control=%.1f stopspeed=%.1f bhop_cap=%.1f",
                cfg.airMaxWishspeed, cfg.airControl,
                cfg.stopspeed, cfg.bunnyHopSpeedCap);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] preserve_straight=%d strafe_angle=%.1f max_accel_tick=%.1f "
                "cap_mode=%s falloff=%.2f retention=%.2f",
                (int)cfg.preserveStraightSpeed, cfg.minimumStrafeAngleDegrees,
                cfg.maximumAccelerationPerTick,
                cfg.maximumBhopSpeedMode == MovementSpeedCapMode::Hard ? "hard" :
                cfg.maximumBhopSpeedMode == MovementSpeedCapMode::Soft ? "soft" : "none",
                cfg.accelerationFalloffNearCap, cfg.landingSpeedRetention);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] ground_accel=%.1f ground_decel=%.1f ground_response=%.1f ground_friction=%.1f",
                cfg.groundAcceleration, cfg.groundDeceleration,
                cfg.groundDirectionChangeResponse, cfg.groundFrictionAmount);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] air_steer_resp=%.2f steer_cap=%.0f gain=%.1f wish=%.1f "
                "mode=%s yawDelta>=%.2f wishDelta>=%.2f tol=%.0f softStart=%.1f",
                cfg.airSteeringResponse,
                cfg.maximumSteeringDegreesPerSecond,
                cfg.airAcceleration, cfg.airMaxWishspeed,
                cfg.stationaryCameraInputMode == StationaryCameraInputMode::Strict
                    ? "strict" : "steering",
                cfg.minimumCameraYawDeltaDegrees,
                cfg.minimumWishRotationDegrees,
                cfg.strafeAngularToleranceDegrees, cfg.softCapStart);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] gravity=%.1f jump=%.1f max_fall=%.1f air_jumps=%d",
                cfg.gravityZ, cfg.jumpVerticalSpeed,
                cfg.maximumFallSpeed, cfg.maximumAirJumps);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] ground_friction=%.1f air_friction=%.1f "
                "ground_dash=%.1f air_dash=%.1f down_dash=%.1f",
                cfg.groundFrictionAmount, cfg.airFrictionAmount,
                cfg.groundDashImpulse, cfg.airDashImpulse,
                cfg.downDashVerticalSpeed);
            Terminal::instance().addLog(buf);
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_velocity",
        "Print current velocity, grounded state, wish direction/speed, and impulse state",
        "movement_velocity",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            const auto& cfg = MovementJsonConfig::instance().config();
            char buf[384];

            const float hSpeed =
                glm::length(glm::vec2(player.vel.x, player.vel.y));
            std::snprintf(buf, sizeof(buf),
                "[VEL] pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) hspeed=%.2f vspeed=%.2f",
                player.pos.x, player.pos.y, player.pos.z,
                player.vel.x, player.vel.y, player.vel.z, hSpeed, player.vel.z);
            Terminal::instance().addLog(buf);

            std::snprintf(buf, sizeof(buf),
                "[VEL] ground=onGround:%d stable:%d worldContact:%d "
                "coyote=%.3f jumpBuf=%.3f airJumps=%d",
                (int)player.ground.onGround, (int)player.ground.stableOnGround,
                (int)player.ground.hasWorldContact,
                player.jump.coyoteTimer, player.jump.jumpIntentTimer,
                player.jump.airJumpsLeft);
            Terminal::instance().addLog(buf);

            const glm::vec2 wish = player.inputWishMove;
            std::snprintf(buf, sizeof(buf),
                "[VEL] wish=(%.2f %.2f) len=%.3f dashGrace=%.2f",
                wish.x, wish.y, glm::length(wish), player.dash.dashGraceTimer);
            Terminal::instance().addLog(buf);

            const glm::vec3& ext = player.externalImpulse;
            std::snprintf(buf, sizeof(buf),
                "[VEL] extImpulse=(%.2f %.2f %.2f) mag=%.1f carry=%.2f "
                "max=%.1f decay=%.2f mode=%s",
                ext.x, ext.y, ext.z,
                player.externalImpulseMagnitude,
                player.externalImpulseCarryTimer,
                cfg.maximumExternalImpulseSpeed, cfg.externalImpulseDecay,
                cfg.impulseFrictionMode == MovementImpulseFrictionMode::Source
                    ? "source" : "exponential");
            Terminal::instance().addLog(buf);
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_air",
        "Print the per-tick air-strafe projection (Source PM_AirAccelerate): speed, "
        "wishDir, currentSpeed, addSpeed, accelSpeed, and whether gain applied",
        "movement_air",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            const MovementAirDebug& a = player.airDebug;
            const auto& cfg = MovementJsonConfig::instance().config();
            char buf[384];
            std::snprintf(buf, sizeof(buf),
                "[AIR] mode=%s grounded=%d input=(forward=%.2f side=%.2f) "
                "wishvel=(%.2f,%.2f) wishdir=(%.2f,%.2f) wishspeed=%.2f capped=%.2f",
                cfg.walkMode == MovementWalkMode::Source ? "source" : "mimita",
                (int)a.grounded, a.forwardMove, a.sideMove,
                a.wishVelocity.x, a.wishVelocity.y,
                a.wishDir.x, a.wishDir.y, a.wishSpeed, a.cappedWishSpeed);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[AIR] hvel=(%.2f,%.2f) hspeed=%.2f currentSpeed=%.2f addSpeed=%.2f "
                "accelSpeed=%.2f applied=%d finalSpeed=%.2f bug_compat=%d",
                a.horizontalVelocity.x, a.horizontalVelocity.y,
                a.horizontalSpeed,
                a.currentSpeed, a.addSpeed, a.accelSpeed,
                (int)a.applied, a.finalHorizontalSpeed,
                (int)a.sourceBugCompatible);
            Terminal::instance().addLog(buf);
        }
    }, CommandCategory::Physics);
}
