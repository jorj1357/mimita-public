#pragma once

#include "FisterConstants.h"
#include "FisterMath.h"

#include <cstdint>
#include <vector>

// Port of the default (bare-fists) player from Fister's shared/gameplay/Player.ts and the
// self-affecting parts of GameWorld.ts. It owns the player's velocity and move state and
// knows nothing about Skyrim: the host feeds it input and world contacts each 60 Hz tick
// and reads back a velocity plus a list of events to resolve against the world.
namespace fister
{
	struct Input
	{
		float moveX{ 0.0f };  // strafe, +right
		float moveY{ 0.0f };  // +forward
		float yaw{ 0.0f };
		float pitch{ 0.0f };  // +up
		bool charging{ false };
		bool railingChargeIntent{ false };
		bool parrying{ false };
		bool grabbing{ false };
		std::uint32_t tapPunchSeq{ 0 };
	};

	struct Environment
	{
		Vec3 position{};  // body centre
		bool groundSupport{ false };
		Vec3 groundNormal{ 0.0f, 0.0f, 1.0f };
		float groundClearance{ 0.0f };
		// Static surface the body runs into this tick along its velocity.
		bool contact{ false };
		Vec3 contactNormal{};
		Vec3 contactPoint{};
		float gravity{ k::DEFAULT_GRAVITY };
		bool hostLimp{ false };  // the host engine has the player ragdolled / knocked down
		bool dead{ false };
	};

	enum class EventType : std::uint8_t
	{
		ChargeReady,
		PunchThrown,        // strength, value = charge time, critical, flag = uppercut attempt, kind = PunchKind
		RocketPunchLaunch,  // strength
		DiveStart,
		DiveImpact,         // value = intensity, position, flag = direct hit already landed
		DiveObstacle,       // value = intensity, position
		AirKickStart,       // direction
		AirKickStop,
		AirKickWhiff,       // value = self damage fraction-free damage, position
		RailingStart,       // value = speed
		RailingStop,
		RailingBounce,      // value = speed, flag = lethal (mega boosted)
		RailingMegaBoost,
		GroundPunchHop,     // position
		ParryStart,         // kind = ParryKind
		ParryEndlag,
		ParrySuccess,       // kind = ParryKind
		GrabStart,
		GrabMiss,
		GrabCancel,
		GrabThrow,          // direction, value = speed, flag = piledriver
		PiledriverImpact,   // position, value = shockwave charge
		SlideStart,
		SlideStop,
		AirTech,
		Flip,
		OverchargeWarning,
		OverchargeExplode,
		Ragdoll,            // value = duration, flag = hard knockdown
		RagdollImpact,      // value = damage, position
		Recover,
		SelfDamage          // value = damage
	};

	enum class PunchKind : int { Normal = 0, Tap = 1, CriticalDash = 2, Rocket = 3 };
	enum class ParryKind : int { Standing = 0, Ground = 1, Surface = 2, RagdollWall = 3 };

	struct Event
	{
		EventType type{};
		float strength{ 0.0f };
		float value{ 0.0f };
		Vec3 position{};
		Vec3 direction{};
		bool critical{ false };
		bool flag{ false };
		int kind{ 0 };
	};

	// Railing charge curves (combatConstants.ts).
	[[nodiscard]] float RailingTuningProgress(float a_chargeTime);
	[[nodiscard]] float RailingTravelSpeed(float a_chargeTime);

	class Sim
	{
	public:
		void Reset();
		void Tick(const Input& a_input, const Environment& a_env);

		[[nodiscard]] const Vec3& Velocity() const { return vel_; }
		void SetVelocity(const Vec3& a_velocity) { vel_ = a_velocity; }
		[[nodiscard]] std::vector<Event>& Events() { return events_; }

		// --- host -> sim notifications (the GameWorld side of the original) ---
		void GrantInstantPunchCharge();
		void MarkParrySuccessful(ParryKind a_kind);
		void ApplyWallPunchRecoil(float a_strength);
		void OnHitByAttack(float a_strength, const Vec3& a_impulseVelocity);
		void OnAirKickHit();
		void OnRailingHit(bool a_flatten);
		void OnRocketAscentParried(const Vec3& a_away);
		void OnGrabCaught();
		void OnGrabBroken();
		void ResolveDiveImpact(bool a_hitAnyone);
		void SetAirKickHomingTarget(bool a_valid, const Vec3& a_position);
		void EnterRagdoll(float a_duration, bool a_hardKnockdown, float a_hardKnockdownSeconds = k::HARD_KNOCKDOWN_DURATION);
		void ApplyImpulseVelocity(const Vec3& a_delta) { vel_ += a_delta; }

		// --- state queries for the host / pose ---
		[[nodiscard]] float Charge() const { return charge_; }
		[[nodiscard]] float ChargeTime() const { return chargeTime_; }
		[[nodiscard]] float SprintChargeTimer() const { return sprintChargeTimer_; }
		[[nodiscard]] bool IsCharging() const { return charge_ > 0.0f; }
		[[nodiscard]] bool IsHoldingFullChargePunch() const;
		[[nodiscard]] bool InstantChargeReady() const { return selfParryInstantChargeReady_; }
		[[nodiscard]] bool IsParrying() const { return isParrying_; }
		[[nodiscard]] float ParryCooldown() const { return parryCooldown_; }
		[[nodiscard]] float ParryEndlag() const { return parryEndlag_; }
		[[nodiscard]] bool IsGrounded() const { return grounded_; }
		[[nodiscard]] bool IsSliding() const { return isSliding_; }
		[[nodiscard]] bool IsLimp() const { return isLimp_; }
		[[nodiscard]] bool IsHardKnockdown() const { return hardKnockdownTimer_ > 0.0f; }
		[[nodiscard]] bool IsGrabbing() const { return isGrabbing_; }
		[[nodiscard]] float GrabProgress() const { return 1.0f - Clamp01(grabTimer_ / k::PLAYER_GRAB_SWIPE_DURATION); }
		[[nodiscard]] bool GrabStartedAirborne() const { return grabStartedAirborne_; }
		[[nodiscard]] bool IsHoldingGrabbed() const { return isHoldingGrabbed_; }
		[[nodiscard]] float ThrowProgress() const { return throwTimer_ > 0.0f ? 1.0f - Clamp01(throwTimer_ / k::PLAYER_GRAB_THROW_DURATION) : 0.0f; }
		[[nodiscard]] bool IsThrowing() const { return throwTimer_ > 0.0f; }
		[[nodiscard]] bool IsPiledriving() const { return isPiledriving_; }
		[[nodiscard]] bool IsRocketPunchAscending() const { return isRocketPunchAscending_; }
		[[nodiscard]] float RocketAscentTimer() const { return rocketPunchAscentTimer_; }
		[[nodiscard]] float RocketAscentCharge() const { return rocketPunchAscentCharge_; }
		[[nodiscard]] bool InRocketPunchFlight() const { return inRocketPunchFlight_; }
		[[nodiscard]] bool IsRocketPunchDiving() const { return isRocketPunchDiving_; }
		[[nodiscard]] float DiveSpeed() const { return rocketPunchDiveSpeed_; }
		[[nodiscard]] bool IsAirKickDashing() const { return isAirKickDashing_; }
		[[nodiscard]] Vec3 AirKickDirection() const { return airKickDir_; }
		[[nodiscard]] float AirKickSpeed() const { return airKickDashSpeed_; }
		[[nodiscard]] float AirKickCharge() const { return airKickDashCharge_; }
		[[nodiscard]] float AirKickImpactScale() const;
		[[nodiscard]] float AirKickLockProgress() const { return Clamp01(airKickLockTimer_ / k::AIR_KICK_TARGET_LOCK_SECONDS); }
		[[nodiscard]] bool IsOverchargeDashing() const { return isOverchargeDashing_; }
		[[nodiscard]] bool IsOverchargeDashCharging() const;
		[[nodiscard]] bool IsOverchargeDashMegaBoosted() const { return overchargeDashMegaBoostActive_; }
		[[nodiscard]] float OverchargeDashSpeed() const { return overchargeDashSpeed_; }
		[[nodiscard]] float OverchargeDashLaunchChargeTime() const { return overchargeDashLaunchChargeTime_; }
		[[nodiscard]] Vec3 OverchargeDashDirection() const { return overchargeDashDir_; }
		[[nodiscard]] bool IsCriticalDashing() const { return criticalDashTimer_ > 0.0f; }
		[[nodiscard]] float KnockbackTimer() const { return knockbackTimer_; }
		[[nodiscard]] float FlipTimer() const { return flipTimer_; }
		[[nodiscard]] bool IsInvulnerable() const { return airTechInvulnTimer_ > 0.0f || midairParryInvulnTimer_ > 0.0f; }
		[[nodiscard]] float HitFreezeTimer() const { return hitFreezeTimer_; }
		[[nodiscard]] float RagdollTime() const { return ragdollElapsed_; }
		[[nodiscard]] bool IsAirborneForFullChargePunch() const;
		[[nodiscard]] bool IsGroundedForAction(float a_windowSeconds) const;
		[[nodiscard]] bool WantsVelocityControl() const { return !env_.hostLimp && !env_.dead; }
		void SetAirKickLockCandidate(bool a_onTarget) { airKickLockOnTarget_ = a_onTarget; }
		void AddHitFreeze(float a_seconds) { hitFreezeTimer_ = std::max(hitFreezeTimer_, a_seconds); }

		// Tunables the host may override.
		float walkSpeed{ k::BASE_WALK_SPEED };
		float maxHealth{ k::BASE_HEALTH };

	private:
		void Emit(EventType a_type, float a_strength = 0.0f, float a_value = 0.0f, bool a_flag = false, int a_kind = 0);
		Event& EmitEvent(EventType a_type);

		void UpdateTimers(float a_dt);
		void UpdateSupport(float a_dt);
		void UpdateLimp(float a_dt);
		void UpdateParryInput(float a_dt);
		void UpdateGrab(float a_dt);
		void UpdateChargeAndPunch(float a_dt);
		void UpdateSpecialMoves(float a_dt);
		void UpdateLandingSlide(float a_dt);
		void UpdateWalkingVelocityController();
		void UpdateRocketFlightControl();
		void IntegrateGravity(float a_dt);
		void ResolveStaticContact();

		[[nodiscard]] bool CanApplyMovementController() const;
		[[nodiscard]] bool CanFireQueuedTapPunch() const;
		void ConsumeQueuedTapPunch();
		void Punch();
		void ClearNormalPunchCharge();
		void ClearSelfParryInstantCharge();
		void ResetOverchargeDashChargeTracking();
		void UpdateOverchargeDashChargeEligibility(float a_dt);
		[[nodiscard]] bool HasOverchargeDashMoveIntent() const;
		[[nodiscard]] bool IsStandingStillForOverchargeDash() const;
		[[nodiscard]] bool CanReleaseOverchargeDash() const;
		bool StartOverchargeDash();
		void StopOverchargeDash(bool a_intoSlide);
		void StartCriticalDash(const Vec3& a_forward, float a_speed, float a_duration);
		void StartAirKickDash(const Vec3& a_direction, float a_charge, const Vec3& a_startVelocity);
		void StopAirKickDash();
		void CancelAirKickDashPreservingMomentum();
		void AirKickWhiff(const Vec3& a_normal);
		[[nodiscard]] float AirKickDashSpeedAtTime(float a_time) const;
		[[nodiscard]] float DiveSpeedAtTime(float a_time) const;
		[[nodiscard]] float DiveImpactIntensity(float a_impactZ, float a_impactSpeed) const;
		void FinishDive(const Vec3& a_point, bool a_obstacle);
		void BeginGroundPunchHop(float a_horizontalSpeed);
		void BeginMomentumFlight(float a_horizontalSpeed);
		void StartFlip();

		[[nodiscard]] bool CanStartStandingParry() const;
		[[nodiscard]] bool CanStartSurfaceParry() const;
		bool TryApplyStandingGroundSelfParryBounce();
		bool TrySurfaceParryRebound(const Vec3& a_normal, bool a_afterDive);

		void QueueLandingSlideCandidate(const Vec3& a_velocity, bool a_fromDashCancel);
		[[nodiscard]] bool CanUseLandingSlide() const;
		bool StartLandingSlideFromHorizontalVelocity(float a_vx, float a_vy);
		void StopLandingSlide();

		void StartGrabLunge();
		void EndGrab(bool a_cooldown);
		void ThrowGrabbed();

		Input input_{};
		Input prevInput_{};
		Environment env_{};
		Vec3 vel_{};
		Vec3 preStepVel_{};
		std::vector<Event> events_{};

		// support
		bool grounded_{ true };
		bool wasGrounded_{ true };
		float airborneDuration_{ 0.0f };
		float timeSinceGroundSupport_{ 0.0f };
		float takeoffGraceTimer_{ 0.0f };
		float recentWallContactTimer_{ 0.0f };

		// charge / punch
		float charge_{ 0.0f };
		float chargeTime_{ 0.0f };
		float sprintChargeTimer_{ 0.0f };
		bool chargeStartedAirborne_{ false };
		bool wasFullyChargedLastTick_{ false };
		bool suppressChargeUntilRelease_{ false };
		float punchCooldown_{ 0.0f };
		int pendingTapPunchCount_{ 0 };
		std::uint32_t lastTapPunchSeq_{ 0 };
		bool tapSeqInitialised_{ false };
		float lastPunchStrength_{ 0.0f };
		float lastChargeTime_{ 0.0f };
		float lastPunchSprintTimer_{ 0.0f };
		bool selfParryInstantChargeReady_{ false };
		float selfParryInstantChargeTimer_{ 0.0f };
		bool overchargeWarningSent_{ false };

		// railing
		bool overchargeDashStationaryEligible_{ false };
		bool overchargeDashIntentLatched_{ false };
		bool overchargeDashMoveIntentDisqualified_{ false };
		float overchargeDashMoveIntentTimer_{ 0.0f };
		float overchargeDashActivationTimer_{ 0.0f };
		float overchargeDashSupportGapTimer_{ 0.0f };
		bool isOverchargeDashing_{ false };
		bool overchargeDashMegaBoostActive_{ false };
		float overchargeDashMegaBoostInputTime_{ 0.0f };
		bool overchargeDashMegaBoostInputConsumed_{ false };
		float overchargeDashImpactParryTimer_{ 0.0f };
		Vec3 overchargeDashDir_{};
		float overchargeDashSpeed_{ 0.0f };
		float overchargeDashLaunchChargeTime_{ 0.0f };

		// critical dash / knockback
		float criticalDashTimer_{ 0.0f };
		float knockbackTimer_{ 0.0f };
		float hitFreezeTimer_{ 0.0f };

		// rocket punch
		bool hasUsedRocketPunchInAir_{ false };
		bool isRocketPunchAscending_{ false };
		float rocketPunchAscentCharge_{ 0.0f };
		float rocketPunchAscentTimer_{ 0.0f };
		bool inRocketPunchFlight_{ false };
		bool momentumOnlyFlight_{ false };
		float rocketPunchLaunchSpeed_{ 0.0f };

		// dive
		bool isRocketPunchDiving_{ false };
		float rocketPunchInitialSpeed_{ 0.0f };
		float rocketPunchCharge_{ 0.0f };
		float rocketPunchDiveTimer_{ 0.0f };
		float rocketPunchDiveSpeed_{ 0.0f };
		float rocketPunchStartZ_{ 0.0f };
		bool rocketPunchHorizontalBoostActive_{ false };
		Vec3 rocketPunchBoostVel_{};
		bool awaitingDiveResolution_{ false };

		// air kick
		bool isAirKickDashing_{ false };
		bool dashCancelMomentumActive_{ false };
		float airKickDashCharge_{ 0.0f };
		float airKickDashTimer_{ 0.0f };
		float airKickDashMomentumScale_{ 0.0f };
		float airKickDashInitialSpeed_{ 0.0f };
		float airKickDashSpeed_{ 0.0f };
		Vec3 airKickDir_{};
		bool airKickHomingValid_{ false };
		Vec3 airKickHomingTarget_{};
		bool airKickLockOnTarget_{ false };
		float airKickLockTimer_{ 0.0f };
		bool airKickLocked_{ false };

		// parry
		bool isParrying_{ false };
		bool parryIsSurface_{ false };
		float parryTimer_{ 0.0f };
		bool parrySucceededThisWindow_{ false };
		float parryCooldown_{ 0.0f };
		float parryEndlag_{ 0.0f };
		bool previousParryInput_{ false };
		float standingGroundSelfParryBufferTimer_{ 0.0f };

		// grab
		bool isGrabbing_{ false };
		float grabTimer_{ 0.0f };
		float grabYaw_{ 0.0f };
		bool grabStartedAirborne_{ false };
		float grabCooldown_{ 0.0f };
		float grabInputBufferTimer_{ 0.0f };
		float grabSuppressTimer_{ 0.0f };
		bool isHoldingGrabbed_{ false };
		float grabHoldTimer_{ 0.0f };
		float throwTimer_{ 0.0f };
		bool isPiledriving_{ false };
		float piledriverTimer_{ 0.0f };
		bool previousGrabInput_{ false };
		bool grabCritical_{ false };

		// slide
		bool isSliding_{ false };
		bool landingSlideCandidate_{ false };
		Vec3 landingSlideCandidateVel_{};
		float landingSlideCandidateTimer_{ 0.0f };
		bool landingSlideCandidateFromDashCancel_{ false };
		float landingSlideDirX_{ 0.0f };
		float landingSlideDirY_{ 0.0f };
		float landingSlideSpeed_{ 0.0f };
		float landingSlideCoastStartSpeed_{ 0.0f };
		float landingSlideElapsed_{ 0.0f };
		bool landingSlideChargedPunchActive_{ false };

		// sim-owned ragdoll
		bool isLimp_{ false };
		float limpTimer_{ 0.0f };
		float ragdollElapsed_{ 0.0f };
		float hardKnockdownTimer_{ 0.0f };
		bool airTechUsed_{ false };
		float airTechLaunchTimer_{ 0.0f };
		float airTechInvulnTimer_{ 0.0f };
		bool midairParryUsedThisRagdoll_{ false };
		float midairParryWindowTimer_{ 0.0f };
		float midairParryInvulnTimer_{ 0.0f };
		float collisionDamageCooldown_{ 0.0f };
		bool airTechInputLatched_{ false };
		bool enteredHardKnockdown_{ false };
		bool ragdollDamageSuppressed_{ false };  // the ragdoll from a whiffed air kick takes no collision damage
		bool airTechFromHardKnockdown_{ false };

		float flipTimer_{ 0.0f };
	};
}
