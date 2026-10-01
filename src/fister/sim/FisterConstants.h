#pragma once

#include "FisterMath.h"

// Constants ported from the Fister `dev` branch. Names match the TypeScript sources:
// shared/gameplay/Player.ts, GameWorld.ts, combatConstants.ts, movementConstants.ts,
// airKickTargeting.ts.
namespace fister::k
{
	// Simulation
	inline constexpr float SIMULATION_DT = 1.0f / 60.0f;
	inline constexpr float DEFAULT_GRAVITY = -9.82f;
	inline constexpr float RAGDOLL_MASS = 72.0f;

	// Movement
	inline constexpr float BASE_WALK_SPEED = 10.0f;
	inline constexpr float CHARGE_SPRINT_MULTIPLIER = 2.0f;
	inline constexpr float NORMAL_BLEND_FACTOR = 0.7f;
	inline constexpr float SPRINT_RAMP_TIME = 3.0f;
	inline constexpr float WALL_CONTACT_BLEND = 0.25f;
	inline constexpr float WALL_CONTACT_TIMER = 0.1f;
	inline constexpr float PLAYER_ACROBATIC_FLIP_DURATION = 0.5f;
	inline constexpr float GROUND_SUPPORT_CLEARANCE_EPSILON = 0.25f;
	inline constexpr float AIR_KICK_MIN_GROUND_CLEARANCE = 0.7f;

	// Landing slide
	inline constexpr float LANDING_SLIDE_MIN_AIR_TIME = 0.08f;
	inline constexpr float LANDING_SLIDE_MIN_DESCENT_SPEED = 0.5f;
	inline constexpr float LANDING_SLIDE_START_SPEED_MULTIPLIER = 1.4f;
	inline constexpr float LANDING_SLIDE_CANDIDATE_BUFFER_SECONDS = 0.2f;
	inline constexpr float LANDING_SLIDE_MAX_DURATION = 0.75f;
	inline constexpr float LANDING_SLIDE_COAST_DURATION = 0.75f;
	inline constexpr float LANDING_SLIDE_MIN_GROUND_NORMAL_UP = 0.45f;
	inline constexpr float LANDING_SLIDE_SUPPORT_GRACE = 0.05f;

	// Health
	inline constexpr float BASE_HEALTH = 200.0f;

	// Punch
	inline constexpr float MAX_CHARGE_TIME = 1.0f;
	inline constexpr float MIN_RELEASE_CHARGE = 0.05f;
	inline constexpr float PUNCH_COOLDOWN_DURATION = 0.4f;
	inline constexpr float TAP_PUNCH_STRENGTH = 0.18f;
	inline constexpr float TAP_PUNCH_COOLDOWN_DURATION = 0.12f;
	inline constexpr int MAX_BUFFERED_TAP_PUNCHES = 12;
	inline constexpr float SELF_PARRY_INSTANT_CHARGE_DURATION = 5.0f;
	inline constexpr float OVERCHARGE_WARNING_TIME = 9.0f;
	inline constexpr float OVERCHARGE_TIME = 10.0f;
	inline constexpr float KNOCKBACK_DURATION = 0.4f;
	inline constexpr float RAGDOLL_PUNCH_THRESHOLD = 0.6f;
	inline constexpr float PUNCH_BASE_VELOCITY = 15.0f;
	inline constexpr float PUNCH_MAX_VELOCITY = 40.0f;
	inline constexpr float PLAYER_PUNCH_RANGE = 5.0f;
	inline constexpr float PLAYER_PUNCH_ARC = kPi * 0.75f;
	inline constexpr float PLAYER_PUNCH_HIT_RADIUS = 0.5f;
	inline constexpr float PLAYER_PUNCH_SWEEP_RADIUS = 0.55f;
	inline constexpr float PLAYER_PUNCH_SWEEP_REAR_ALLOWANCE = 0.2f;
	inline constexpr float PLAYER_PUNCH_PROFILE_RADIUS = 0.78f;
	inline constexpr float WALL_PUNCH_RECOIL_IMPULSE = 400.0f;

	// Critical fist / dash
	inline constexpr float CRITICAL_FIST_CHARGE_TIME = 7.0f;
	inline constexpr float CRITICAL_IMPULSE_MULTIPLIER = 10.0f;
	inline constexpr float CRITICAL_DASH_SPEED = 46.0f;
	inline constexpr float CRITICAL_DASH_DURATION = 0.28f;
	inline constexpr float CRITICAL_DASH_RANGE = 18.0f;
	inline constexpr float CRITICAL_DASH_ARC = kPi * 0.3f;
	inline constexpr float CRITICAL_DASH_LANE_RADIUS = 2.2f;
	inline constexpr float CRITICAL_DASH_MAX_VERTICAL = 2.5f;

	// Uppercut
	inline constexpr float PLAYER_UPPERCUT_MIN_CHARGE = 0.6f;
	inline constexpr float PLAYER_UPPERCUT_MIN_PITCH = 0.42f;
	inline constexpr float PLAYER_UPPERCUT_RANGE = 4.25f;
	inline constexpr float PLAYER_UPPERCUT_LATERAL_RADIUS = 0.95f;
	inline constexpr float PLAYER_UPPERCUT_MIN_VERTICAL = 0.05f;
	inline constexpr float PLAYER_UPPERCUT_MAX_VERTICAL = 2.85f;
	inline constexpr float PLAYER_UPPERCUT_AIRBORNE_MIN_CLEARANCE = 0.45f;
	inline constexpr float UPPERCUT_KILL_IMPULSE = 8000.0f;

	// Rocket punch
	inline constexpr float ROCKET_PUNCH_LOOK_UP_PITCH = kPi / 6.0f;
	inline constexpr float ROCKET_PUNCH_BASE_LAUNCH = 15.0f;
	inline constexpr float ROCKET_PUNCH_CHARGE_LAUNCH = 20.0f;
	inline constexpr float ROCKET_PUNCH_ASCENT_MAX_TIME = 2.0f;
	inline constexpr float ROCKET_PUNCH_ASCENT_HIT_RADIUS = 1.5f;
	inline constexpr float ROCKET_PUNCH_ASCENT_ARM_TIME = 0.05f;
	inline constexpr float ROCKET_FLIGHT_STEER_BLEND = 0.35f;
	inline constexpr float MOMENTUM_FLIGHT_STEER_BLEND = 0.18f;

	// Dive bomb
	inline constexpr float DIVE_STRAIGHT_DOWN_PITCH = -1.45f;
	inline constexpr float DIVE_MIN_INITIAL_SPEED = 40.0f;
	inline constexpr float ROCKET_PUNCH_DIVE_START_SPEED_MULTIPLIER = 0.86f;
	inline constexpr float ROCKET_PUNCH_DIVE_MAX_SPEED_MULTIPLIER = 1.55f;
	inline constexpr float ROCKET_PUNCH_DIVE_RAMP_TIME = 0.78f;
	inline constexpr float ROCKET_PUNCH_DIVE_MAX_IMPACT_INTENSITY = 1.35f;
	inline constexpr float DIVE_DIRECT_HIT_RADIUS = 1.0f;
	inline constexpr float DIVE_LANDING_KILL_RADIUS = 2.0f;
	inline constexpr float DIVE_WHIFF_RAGDOLL_SECONDS = 3.0f;
	inline constexpr float SHOCKWAVE_KILL_RADIUS = 0.75f;
	inline constexpr float SHOCKWAVE_BASE_VELOCITY = 16.25f;
	inline constexpr float SHOCKWAVE_HEIGHT_BAND = 4.0f;

	// Ground-punch hop / ground parry
	inline constexpr float GROUND_PARRY_LOOK_DOWN_PITCH = -0.5f;
	inline constexpr float GROUND_PUNCH_HOP_LAUNCH_VELOCITY = 13.5f;
	inline constexpr float GROUND_PUNCH_HOP_PUNCH_LAUNCH = 12.0f;
	inline constexpr float GROUND_PUNCH_HOP_RADIUS = 3.5f;
	inline constexpr float GROUND_PUNCH_HOP_KNOCKBACK = 0.22f;
	inline constexpr float GROUND_PUNCH_HOP_TAKEOFF_GRACE = 0.34f;
	inline constexpr float STANDING_GROUND_SELF_PARRY_BUFFER = 0.12f;
	inline constexpr float SURFACE_PARRY_FLOOR_HORIZONTAL_SPEED_MULTIPLIER = 1.1f;

	// Air kick dash
	inline constexpr float AIR_KICK_DASH_BASE_SPEED = 78.0f;
	inline constexpr float AIR_KICK_DASH_START_SPEED_MULTIPLIER = 0.84f;
	inline constexpr float AIR_KICK_DASH_MAX_SPEED_MULTIPLIER = 1.32f;
	inline constexpr float AIR_KICK_DASH_RAMP_TIME = 1.15f;
	inline constexpr float AIR_KICK_DASH_COAST_TIME = 1.15f;
	inline constexpr float AIR_KICK_DASH_STOP_TIME = 3.0f;
	inline constexpr float AIR_KICK_DASH_MIN_IMPACT_SCALE = 0.1f;
	inline constexpr float AIR_KICK_DASH_MAX_IMPACT_SCALE = 1.35f;
	inline constexpr float AIR_KICK_DASH_MOMENTUM_MIN_SPEED = 8.0f;
	inline constexpr float AIR_KICK_DASH_MOMENTUM_FULL_SPEED = 42.0f;
	inline constexpr float AIR_KICK_DASH_MOMENTUM_SPEED_BONUS = 18.0f;
	inline constexpr float AIR_KICK_DASH_MOMENTUM_IMPACT_BONUS = 0.18f;
	inline constexpr float AIR_KICK_DASH_HIT_RADIUS = 2.25f;
	inline constexpr float AIR_KICK_DASH_TARGET_HALF_HEIGHT = 0.65f;
	inline constexpr float AIR_KICK_DASH_LANDING_WHIFF_ARM_TIME = 0.14f;
	inline constexpr float AIR_KICK_DASH_WALL_WHIFF_MAX_NORMAL_UP = 0.35f;
	inline constexpr float AIR_KICK_DASH_LANDING_MIN_NORMAL_UP = 0.7f;
	inline constexpr float AIR_KICK_DASH_WALL_WHIFF_MIN_APPROACH_SPEED = 6.0f;
	inline constexpr float AIR_KICK_DASH_SLOPE_WHIFF_MIN_APPROACH_SPEED = 7.5f;
	inline constexpr float AIR_KICK_DASH_LANDING_WHIFF_MIN_APPROACH_SPEED = 3.0f;
	inline constexpr float AIR_KICK_IMPULSE = 72.0f * 40.0f * 10.0f;
	inline constexpr float AIR_KICK_PARRY_COUNTER_IMPULSE = 7500.0f;
	inline constexpr float AIR_KICK_TARGET_LOCK_SECONDS = 3.0f;
	inline constexpr float AIR_KICK_HOMING_TURN_RATE = 22.0f * kPi / 180.0f;
	inline constexpr float AIR_KICK_HOMING_MAX_OFF_AXIS = 60.0f * kPi / 180.0f;

	// Railing (overcharge dash)
	inline constexpr float OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME = 1.0f;
	inline constexpr float OVERCHARGE_DASH_MAX_CHARGE_TIME = 10.0f;
	inline constexpr float OVERCHARGE_DASH_READY_TIME = 10.0f;
	inline constexpr float RAILING_MIN_TRAVEL_SPEED = 68.0f;
	inline constexpr float RAILING_MAX_TRAVEL_SPEED = 240.0f;
	inline constexpr float RAILING_PLAYER_HIT_RADIUS = 2.4f;
	inline constexpr float RAILING_PLAYER_HIT_MOMENTUM_RETENTION = 0.65f;
	inline constexpr float RAILING_HARD_KNOCKDOWN_MIN_CHARGE = 3.0f;
	inline constexpr float OVERCHARGE_DASH_STATIONARY_MOVE_EPSILON = 0.3f;
	inline constexpr float OVERCHARGE_DASH_STATIONARY_SPEED = 3.5f;
	inline constexpr float OVERCHARGE_DASH_MOVE_INTENT_DISQUALIFY_TIME = 0.18f;
	inline constexpr float OVERCHARGE_DASH_SUPPORT_GRACE_TIME = 0.3f;
	inline constexpr float OVERCHARGE_DASH_MIN_TURN_RATE = 0.24f;
	inline constexpr float OVERCHARGE_DASH_MAX_TURN_RATE = 4.25f;
	inline constexpr float OVERCHARGE_DASH_TARGET_HALF_HEIGHT = 0.85f;
	inline constexpr float OVERCHARGE_DASH_BOUNCE_SPEED_SCALE = 1.1f;
	inline constexpr float OVERCHARGE_DASH_BOUNCE_MIN_SPEED = 42.0f;
	inline constexpr float OVERCHARGE_DASH_BOUNCE_MAX_SPEED = 280.0f;
	inline constexpr float OVERCHARGE_DASH_BOUNCE_KNOCKBACK = 0.34f;
	inline constexpr float OVERCHARGE_DASH_MEGA_BOOST_HOLD = 1.0f;
	inline constexpr float OVERCHARGE_DASH_MAX_VERTICAL = 4.0f;

	// Parry
	inline constexpr float PARRY_WINDOW = 0.25f;
	inline constexpr float PARRY_COOLDOWN_DURATION = 3.0f;
	inline constexpr float PARRY_ENDLAG_DURATION = 0.15f;
	inline constexpr float PARRY_COUNTER_IMPULSE_MULTIPLIER = 2.5f;
	inline constexpr float PARRY_HIT_FREEZE = 0.15f;
	inline constexpr float SURFACE_PARRY_WINDOW = 0.16f;
	inline constexpr float SURFACE_PARRY_MIN_APPROACH_SPEED = 4.5f;
	inline constexpr float SURFACE_PARRY_REBOUND_MULTIPLIER = 1.175f;
	inline constexpr float AIR_KICK_SURFACE_PARRY_REBOUND_MULTIPLIER = 0.775f;
	inline constexpr float SURFACE_PARRY_MAX_REBOUND_SPEED = 90.0f;
	inline constexpr float AIR_KICK_SURFACE_PARRY_MAX_HORIZONTAL_SPEED = 42.0f;
	inline constexpr float SURFACE_PARRY_FLOOR_MIN_UPWARD_VELOCITY = 13.5f * 0.6f;
	inline constexpr float SURFACE_PARRY_MIN_UPWARD_VELOCITY = 3.5f;
	inline constexpr float SURFACE_PARRY_SPECIAL_WALL_MAX_UPWARD_VELOCITY = 5.0f;
	inline constexpr float SURFACE_PARRY_FLOOR_MIN_NORMAL_UP = 0.45f;
	inline constexpr float SURFACE_PARRY_KNOCKBACK = 0.16f;
	inline constexpr float MIDAIR_PARRY_MIN_VELOCITY = 5.0f;
	inline constexpr float MIDAIR_PARRY_WINDOW = 0.25f;
	inline constexpr float HARD_KNOCKDOWN_DURATION = 2.0f;

	// Grab
	inline constexpr float PLAYER_GRAB_RANGE = 4.0f;
	inline constexpr float PLAYER_GRAB_ANGLE = kPi / 2.0f;
	inline constexpr float PLAYER_GRAB_VERTICAL = 3.0f;
	inline constexpr float PLAYER_GRAB_AIR_RANGE = 4.75f;
	inline constexpr float PLAYER_GRAB_AIR_ANGLE = 115.0f * kPi / 180.0f;
	inline constexpr float PLAYER_GRAB_AIR_VERTICAL = 6.0f;
	inline constexpr float PLAYER_GRAB_SWIPE_DURATION = 0.5f;
	inline constexpr float PLAYER_GRAB_HOLD_DURATION = 0.5f;
	inline constexpr float PLAYER_GRAB_THROW_DURATION = 0.2f;
	inline constexpr float PLAYER_GRAB_THROW_FORCE = 27.0f;
	inline constexpr float PLAYER_GRAB_COOLDOWN = 2.0f;
	inline constexpr float PLAYER_GRAB_LUNGE_SPEED = 20.0f;
	inline constexpr float PLAYER_GRAB_LUNGE_GROUND_UP = 5.0f;
	inline constexpr float PLAYER_GRAB_INPUT_BUFFER_DURATION = 0.2f;
	inline constexpr float PLAYER_GRAB_THROWN_RAGDOLL_SECONDS = 2.0f;
	inline constexpr float PLAYER_GRAB_THROWN_HARD_KNOCKDOWN = 2.2f;
	inline constexpr float PLAYER_GRAB_HOLD_HEIGHT = 1.5f;
	inline constexpr float PLAYER_GRAB_HOLD_FORWARD = 0.8f;
	inline constexpr float PLAYER_GRAB_PILEDRIVER_TRAVEL_RAMP_TIME = 0.52f;
	inline constexpr float PLAYER_GRAB_PILEDRIVER_TRAVEL_MIN_SPEED = 68.0f;
	inline constexpr float PLAYER_GRAB_PILEDRIVER_TRAVEL_MAX_SPEED = 118.0f;

	// Ragdoll / air tech
	inline constexpr float RAGDOLL_RECOVERY_SPEED = 4.0f;
	inline constexpr float COLLISION_DAMAGE_MIN_SPEED = 3.0f;
	inline constexpr float COLLISION_DAMAGE_SCALE = 0.625f;
	inline constexpr float COLLISION_DAMAGE_COOLDOWN = 0.15f;
	inline constexpr float AIRTECH_DASH_SPEED = 18.0f;
	inline constexpr float AIRTECH_UPWARD_SPEED = 8.0f;
	inline constexpr float AIRTECH_MOMENTUM_RETENTION = 0.5f;
	inline constexpr float AIRTECH_INVINCIBILITY_DURATION = 0.25f;
	inline constexpr float AIRTECH_GRAB_SUPPRESS_DURATION = 1.0f;
	inline constexpr float AIRTECH_LAUNCH_DURATION = 0.3f;

	// Finishers
	inline constexpr float FINISHER_MIN_STRENGTH = 0.95f;
	inline constexpr float FINISHER_HEALTH = 40.0f;
	inline constexpr float FINISHER_KNOCKDOWN_HEALTH_FRACTION = 0.5f;
}
