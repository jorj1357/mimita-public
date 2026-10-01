#include "FisterSim.h"

namespace fister
{
	namespace
	{
		[[nodiscard]] float UnboundedRampSpeedAtTime(float a_time, float a_startSpeed, float a_rampSpeed, float a_rampTime)
		{
			const float progress = std::max(0.0f, a_time) / std::max(0.001f, a_rampTime);
			const float delta = a_rampSpeed - a_startSpeed;
			if (progress <= 1.0f) {
				return a_startSpeed + delta * Smoothstep01(progress);
			}
			return a_startSpeed + delta * progress;
		}

		[[nodiscard]] float SmoothOverchargeDashProgress(float a_progress)
		{
			const float early = std::pow(Clamp01(a_progress), 0.72f);
			return early * early * (3.0f - 2.0f * early);
		}

		[[nodiscard]] float OverchargeDashChargeProgress(float a_chargeTime)
		{
			const float window = std::max(0.001f, k::OVERCHARGE_DASH_MAX_CHARGE_TIME - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME);
			return Clamp01((std::max(0.0f, a_chargeTime) - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME) / window);
		}
	}

	float RailingTuningProgress(float a_chargeTime)
	{
		return SmoothOverchargeDashProgress(OverchargeDashChargeProgress(a_chargeTime));
	}

	float RailingTravelSpeed(float a_chargeTime)
	{
		return Lerp(k::RAILING_MIN_TRAVEL_SPEED, k::RAILING_MAX_TRAVEL_SPEED, RailingTuningProgress(a_chargeTime));
	}

	void Sim::Reset()
	{
		const float keepWalk = walkSpeed;
		const float keepHealth = maxHealth;
		*this = Sim{};
		walkSpeed = keepWalk;
		maxHealth = keepHealth;
	}

	Event& Sim::EmitEvent(EventType a_type)
	{
		Event& e = events_.emplace_back();
		e.type = a_type;
		e.position = env_.position;
		return e;
	}

	void Sim::Emit(EventType a_type, float a_strength, float a_value, bool a_flag, int a_kind)
	{
		Event& e = EmitEvent(a_type);
		e.strength = a_strength;
		e.value = a_value;
		e.flag = a_flag;
		e.kind = a_kind;
	}

	bool Sim::IsGroundedForAction(float a_windowSeconds) const
	{
		if (grounded_) {
			return true;
		}
		// Coyote time must not classify an upward launch as grounded.
		if (vel_.z > 1.0f) {
			return false;
		}
		return timeSinceGroundSupport_ <= a_windowSeconds || env_.groundClearance <= k::GROUND_SUPPORT_CLEARANCE_EPSILON;
	}

	bool Sim::IsAirborneForFullChargePunch() const
	{
		return !grounded_ && env_.groundClearance >= k::AIR_KICK_MIN_GROUND_CLEARANCE && timeSinceGroundSupport_ > 0.09f;
	}

	bool Sim::IsHoldingFullChargePunch() const
	{
		return input_.charging && !input_.grabbing && !suppressChargeUntilRelease_ && charge_ >= 1.0f && !isParrying_ &&
		       !isLimp_ && parryEndlag_ <= 0.0f;
	}

	// ------------------------------------------------------------------ tick

	void Sim::Tick(const Input& a_input, const Environment& a_env)
	{
		const float dt = k::SIMULATION_DT;
		prevInput_ = input_;
		input_ = a_input;
		env_ = a_env;

		if (!tapSeqInitialised_) {
			lastTapPunchSeq_ = a_input.tapPunchSeq;
			tapSeqInitialised_ = true;
		}
		if (a_input.tapPunchSeq > lastTapPunchSeq_) {
			const int delta = static_cast<int>(a_input.tapPunchSeq - lastTapPunchSeq_);
			pendingTapPunchCount_ = std::min(k::MAX_BUFFERED_TAP_PUNCHES, pendingTapPunchCount_ + delta);
		} else if (a_input.tapPunchSeq < lastTapPunchSeq_) {
			pendingTapPunchCount_ = 0;
		}
		lastTapPunchSeq_ = a_input.tapPunchSeq;

		if (env_.dead) {
			const bool parryHeld = input_.parrying;
			const bool grabHeld = input_.grabbing;
			const auto seq = lastTapPunchSeq_;
			Reset();
			previousParryInput_ = parryHeld;
			previousGrabInput_ = grabHeld;
			lastTapPunchSeq_ = seq;
			tapSeqInitialised_ = true;
			return;
		}

		if (hitFreezeTimer_ > 0.0f) {
			hitFreezeTimer_ = std::max(0.0f, hitFreezeTimer_ - dt);
			return;
		}

		UpdateTimers(dt);
		UpdateSupport(dt);

		if (env_.hostLimp) {
			// The host engine owns the body (its own ragdoll / knockdown): drop everything transient.
			ClearNormalPunchCharge();
			pendingTapPunchCount_ = 0;
			if (isAirKickDashing_) StopAirKickDash();
			if (isOverchargeDashing_) StopOverchargeDash(false);
			isRocketPunchDiving_ = false;
			isRocketPunchAscending_ = false;
			inRocketPunchFlight_ = false;
			if (isGrabbing_ || isHoldingGrabbed_) EndGrab(true);
			StopLandingSlide();
			previousParryInput_ = input_.parrying;
			previousGrabInput_ = input_.grabbing;
			return;
		}

		if (isLimp_) {
			UpdateLimp(dt);
			previousParryInput_ = input_.parrying;
			previousGrabInput_ = input_.grabbing;
			return;
		}

		UpdateParryInput(dt);
		UpdateGrab(dt);
		UpdateChargeAndPunch(dt);
		UpdateSpecialMoves(dt);
		UpdateLandingSlide(dt);
		if (CanApplyMovementController()) {
			UpdateWalkingVelocityController();
		}
		UpdateRocketFlightControl();
		ResolveStaticContact();
		IntegrateGravity(dt);

		previousParryInput_ = input_.parrying;
		previousGrabInput_ = input_.grabbing;
	}

	void Sim::UpdateTimers(float a_dt)
	{
		auto dec = [a_dt](float& t) { t = std::max(0.0f, t - a_dt); };
		dec(punchCooldown_);
		dec(parryCooldown_);
		dec(parryEndlag_);
		dec(knockbackTimer_);
		dec(criticalDashTimer_);
		dec(grabCooldown_);
		dec(grabSuppressTimer_);
		dec(grabInputBufferTimer_);
		dec(throwTimer_);
		dec(airTechLaunchTimer_);
		dec(airTechInvulnTimer_);
		dec(midairParryInvulnTimer_);
		dec(midairParryWindowTimer_);
		dec(recentWallContactTimer_);
		dec(takeoffGraceTimer_);
		dec(flipTimer_);
		dec(collisionDamageCooldown_);
		dec(hardKnockdownTimer_);
		dec(overchargeDashImpactParryTimer_);

		if (selfParryInstantChargeReady_) {
			selfParryInstantChargeTimer_ -= a_dt;
			if (selfParryInstantChargeTimer_ <= 0.0f) {
				ClearSelfParryInstantCharge();
			}
		}
		if (standingGroundSelfParryBufferTimer_ > 0.0f) {
			standingGroundSelfParryBufferTimer_ = std::max(0.0f, standingGroundSelfParryBufferTimer_ - a_dt);
			if (TryApplyStandingGroundSelfParryBounce()) {
				standingGroundSelfParryBufferTimer_ = 0.0f;
			}
		}
	}

	void Sim::UpdateSupport(float a_dt)
	{
		// The near-ground clearance rule can hand the last airborne tick to the walking blend, so the
		// landing sample keeps whichever of the last two ticks carried more horizontal speed.
		const Vec3 olderVel = preStepVel_;
		preStepVel_ = vel_;
		const Vec3 landingVel = std::hypot(olderVel.x, olderVel.y) > std::hypot(vel_.x, vel_.y) ?
		                            Vec3{ olderVel.x, olderVel.y, vel_.z } : vel_;
		const bool takingOff = takeoffGraceTimer_ > 0.0f && vel_.z > 0.5f;
		const bool support = env_.groundSupport && !takingOff;
		wasGrounded_ = grounded_;
		grounded_ = support;

		if (support) {
			if (!wasGrounded_) {
				const bool specialLanding = isRocketPunchDiving_ || isAirKickDashing_ || isPiledriving_;
				if (!specialLanding) {
					if (airborneDuration_ >= k::LANDING_SLIDE_MIN_AIR_TIME || dashCancelMomentumActive_) {
						QueueLandingSlideCandidate(landingVel, dashCancelMomentumActive_);
					}
					hasUsedRocketPunchInAir_ = false;
					isRocketPunchAscending_ = false;
					rocketPunchAscentTimer_ = 0.0f;
					inRocketPunchFlight_ = false;
					momentumOnlyFlight_ = false;
					dashCancelMomentumActive_ = false;
					airTechFromHardKnockdown_ = false;
				}
			}
			airborneDuration_ = 0.0f;
			timeSinceGroundSupport_ = 0.0f;
			if (vel_.z < 0.0f && !isRocketPunchDiving_ && !isAirKickDashing_ && !isPiledriving_) {
				vel_.z = 0.0f;
			}
		} else {
			airborneDuration_ += a_dt;
			timeSinceGroundSupport_ += a_dt;
		}
	}

	void Sim::IntegrateGravity(float a_dt)
	{
		if (!grounded_) {
			vel_.z += env_.gravity * a_dt;
		}
	}

	void Sim::ResolveStaticContact()
	{
		if (!env_.contact) {
			return;
		}
		const Vec3& n = env_.contactNormal;
		const float vn = vel_.Dot(n);
		if (vn >= 0.0f) {
			return;
		}
		// Floors are resolved by ground support on the landing tick; only walls and ceilings here.
		if (n.z > 0.7f) {
			return;
		}
		recentWallContactTimer_ = k::WALL_CONTACT_TIMER;
		vel_ = vel_ - n * vn;
	}

	// ------------------------------------------------------------------ movement

	bool Sim::CanApplyMovementController() const
	{
		const bool grabRooted = isHoldingGrabbed_ || isGrabbing_ || isPiledriving_;
		return !isSliding_ && !isLimp_ && !grabRooted && parryEndlag_ <= 0.0f && airTechLaunchTimer_ <= 0.0f &&
		       !dashCancelMomentumActive_ && !inRocketPunchFlight_ && !isRocketPunchDiving_ && !isAirKickDashing_ &&
		       !isOverchargeDashing_ && knockbackTimer_ <= 0.0f && criticalDashTimer_ <= 0.0f;
	}

	void Sim::UpdateWalkingVelocityController()
	{
		const Vec3 forward = YawForward(input_.yaw);
		const Vec3 right = YawRight(input_.yaw);
		const float moveX = input_.moveX * right.x + input_.moveY * forward.x;
		const float moveY = input_.moveX * right.y + input_.moveY * forward.y;

		const bool groundedForSprint = IsGroundedForAction(0.3f);
		float speedMultiplier = 1.0f;
		if (IsHoldingFullChargePunch()) {
			speedMultiplier = std::max(speedMultiplier, k::CHARGE_SPRINT_MULTIPLIER);
			if (groundedForSprint || chargeStartedAirborne_) {
				speedMultiplier += sprintChargeTimer_ / k::SPRINT_RAMP_TIME;
			}
		}
		const float currentSpeed = walkSpeed * speedMultiplier;

		const float inputMagnitude = std::sqrt(moveX * moveX + moveY * moveY);
		float targetVelX = inputMagnitude > 0.01f ? (moveX / inputMagnitude) * currentSpeed : 0.0f;
		float targetVelY = inputMagnitude > 0.01f ? (moveY / inputMagnitude) * currentSpeed : 0.0f;

		const bool holdingNormalCharge = input_.charging && !input_.grabbing && !suppressChargeUntilRelease_ &&
		                                 !isParrying_ && parryEndlag_ <= 0.0f && charge_ >= 0.05f;
		const bool preserveAirborneHorizontalMomentum = !groundedForSprint && holdingNormalCharge;

		float blend = k::NORMAL_BLEND_FACTOR;
		if (recentWallContactTimer_ > 0.0f) {
			blend = std::min(blend, k::WALL_CONTACT_BLEND);
		}

		float minimumPreservedAirborneSpeed = 0.0f;
		if (preserveAirborneHorizontalMomentum) {
			const float currentSpeedH = std::hypot(vel_.x, vel_.y);
			const float targetSpeedH = std::hypot(targetVelX, targetVelY);
			const bool movingWithMomentum = targetSpeedH <= 0.01f || vel_.x * targetVelX + vel_.y * targetVelY >= 0.0f;
			if (movingWithMomentum && currentSpeedH > targetSpeedH) {
				minimumPreservedAirborneSpeed = currentSpeedH;
				if (targetSpeedH > 0.01f) {
					const float scale = currentSpeedH / targetSpeedH;
					targetVelX *= scale;
					targetVelY *= scale;
				} else {
					targetVelX = vel_.x;
					targetVelY = vel_.y;
				}
			}
		}

		float newVx = vel_.x * (1.0f - blend) + targetVelX * blend;
		float newVy = vel_.y * (1.0f - blend) + targetVelY * blend;
		if (minimumPreservedAirborneSpeed > 0.0f) {
			const float newSpeed = std::hypot(newVx, newVy);
			if (newSpeed > 0.001f && newSpeed < minimumPreservedAirborneSpeed) {
				const float scale = minimumPreservedAirborneSpeed / newSpeed;
				newVx *= scale;
				newVy *= scale;
			}
		}
		if (std::abs(targetVelX) < 0.001f && std::abs(newVx) < 0.01f) newVx = 0.0f;
		if (std::abs(targetVelY) < 0.001f && std::abs(newVy) < 0.01f) newVy = 0.0f;
		vel_.x = newVx;
		vel_.y = newVy;
	}

	void Sim::UpdateRocketFlightControl()
	{
		if (!inRocketPunchFlight_ || isRocketPunchDiving_ || isLimp_ || isAirKickDashing_ || isOverchargeDashing_ ||
		    isGrabbing_ || isPiledriving_) {
			return;
		}
		const Vec3 forward = YawForward(input_.yaw);
		const Vec3 right = YawRight(input_.yaw);
		const float moveX = input_.moveX * right.x + input_.moveY * forward.x;
		const float moveY = input_.moveX * right.y + input_.moveY * forward.y;
		const float inputMagnitude = std::sqrt(moveX * moveX + moveY * moveY);
		const float currentHorizSpeed = std::hypot(vel_.x, vel_.y);
		const float baseAirSpeed = momentumOnlyFlight_ ? std::max(rocketPunchLaunchSpeed_, currentHorizSpeed) :
		                                                  std::max({ rocketPunchLaunchSpeed_, currentHorizSpeed, walkSpeed });
		if (inputMagnitude > 0.01f) {
			const float airSpeed = momentumOnlyFlight_ ? std::max(baseAirSpeed, walkSpeed) : baseAirSpeed;
			const float inputDirX = moveX / inputMagnitude;
			const float inputDirY = moveY / inputMagnitude;
			const float currentDirX = currentHorizSpeed > 0.1f ? vel_.x / currentHorizSpeed : inputDirX;
			const float currentDirY = currentHorizSpeed > 0.1f ? vel_.y / currentHorizSpeed : inputDirY;
			const float blend = momentumOnlyFlight_ ? k::MOMENTUM_FLIGHT_STEER_BLEND : k::ROCKET_FLIGHT_STEER_BLEND;
			float steerX = currentDirX * (1.0f - blend) + inputDirX * blend;
			float steerY = currentDirY * (1.0f - blend) + inputDirY * blend;
			const float steerLength = std::hypot(steerX, steerY);
			if (steerLength > 0.001f) {
				steerX /= steerLength;
				steerY /= steerLength;
			} else {
				steerX = inputDirX;
				steerY = inputDirY;
			}
			vel_.x = steerX * airSpeed;
			vel_.y = steerY * airSpeed;
		} else if (currentHorizSpeed > 0.1f) {
			const float scale = baseAirSpeed / currentHorizSpeed;
			vel_.x *= scale;
			vel_.y *= scale;
		}
	}

	// ------------------------------------------------------------------ landing slide

	void Sim::QueueLandingSlideCandidate(const Vec3& a_velocity, bool a_fromDashCancel)
	{
		landingSlideCandidate_ = true;
		landingSlideCandidateVel_ = a_velocity;
		landingSlideCandidateTimer_ = k::LANDING_SLIDE_CANDIDATE_BUFFER_SECONDS;
		landingSlideCandidateFromDashCancel_ = a_fromDashCancel;
	}

	bool Sim::CanUseLandingSlide() const
	{
		return !isLimp_ && !isRocketPunchDiving_ && !isRocketPunchAscending_ && !isAirKickDashing_ && !isOverchargeDashing_ &&
		       !isGrabbing_ && !isHoldingGrabbed_ && !isPiledriving_ && knockbackTimer_ <= 0.0f &&
		       criticalDashTimer_ <= 0.0f && airTechLaunchTimer_ <= 0.0f;
	}

	bool Sim::StartLandingSlideFromHorizontalVelocity(float a_vx, float a_vy)
	{
		const float horizontalSpeed = std::hypot(a_vx, a_vy);
		const float walk = std::max(0.1f, walkSpeed);
		if (horizontalSpeed < walk * k::LANDING_SLIDE_START_SPEED_MULTIPLIER) {
			return false;
		}
		landingSlideDirX_ = a_vx / horizontalSpeed;
		landingSlideDirY_ = a_vy / horizontalSpeed;
		landingSlideSpeed_ = horizontalSpeed;
		landingSlideCoastStartSpeed_ = horizontalSpeed;
		landingSlideElapsed_ = 0.0f;
		landingSlideChargedPunchActive_ = false;
		landingSlideCandidate_ = false;
		isSliding_ = true;
		Emit(EventType::SlideStart, 0.0f, horizontalSpeed);
		return true;
	}

	void Sim::StopLandingSlide()
	{
		if (isSliding_) {
			Emit(EventType::SlideStop);
		}
		isSliding_ = false;
		landingSlideCandidate_ = false;
		landingSlideSpeed_ = 0.0f;
		landingSlideCoastStartSpeed_ = 0.0f;
		landingSlideElapsed_ = 0.0f;
		landingSlideChargedPunchActive_ = false;
	}

	void Sim::UpdateLandingSlide(float a_dt)
	{
		if (landingSlideCandidate_) {
			landingSlideCandidateTimer_ = std::max(0.0f, landingSlideCandidateTimer_ - a_dt);
			if (grounded_ && CanUseLandingSlide()) {
				const bool descending = landingSlideCandidateVel_.z <= -k::LANDING_SLIDE_MIN_DESCENT_SPEED;
				if (env_.groundNormal.z < k::LANDING_SLIDE_MIN_GROUND_NORMAL_UP ||
				    (!landingSlideCandidateFromDashCancel_ && !descending) ||
				    !StartLandingSlideFromHorizontalVelocity(landingSlideCandidateVel_.x, landingSlideCandidateVel_.y)) {
					landingSlideCandidate_ = false;
				}
			}
			if (landingSlideCandidate_ && landingSlideCandidateTimer_ <= 0.0f) {
				landingSlideCandidate_ = false;
			}
		}
		if (!isSliding_) {
			return;
		}
		const bool recentSupport = grounded_ || airborneDuration_ <= k::LANDING_SLIDE_SUPPORT_GRACE;
		if (!CanUseLandingSlide() || !recentSupport || recentWallContactTimer_ > 0.0f) {
			StopLandingSlide();
			return;
		}

		const bool normalPunchChargeHeld = input_.charging && !input_.grabbing && !suppressChargeUntilRelease_ &&
		                                   !isParrying_ && parryEndlag_ <= 0.0f;
		const bool chargedPunchSprintActive = normalPunchChargeHeld && charge_ >= 1.0f;
		if (chargedPunchSprintActive) {
			if (landingSlideChargedPunchActive_) {
				landingSlideSpeed_ += walkSpeed * std::max(0.0f, k::CHARGE_SPRINT_MULTIPLIER - 1.0f) * a_dt / k::SPRINT_RAMP_TIME;
			}
			landingSlideChargedPunchActive_ = true;
		} else if (landingSlideChargedPunchActive_) {
			landingSlideChargedPunchActive_ = false;
			landingSlideCoastStartSpeed_ = landingSlideSpeed_;
			landingSlideElapsed_ = 0.0f;
		}
		if (!normalPunchChargeHeld) {
			landingSlideElapsed_ += a_dt;
		}

		const Vec3 forward = YawForward(input_.yaw);
		const Vec3 right = YawRight(input_.yaw);
		const float moveX = input_.moveX * right.x + input_.moveY * forward.x;
		const float moveY = input_.moveX * right.y + input_.moveY * forward.y;
		const float moveMagnitude = std::hypot(moveX, moveY);
		const bool hasMovementDirection = moveMagnitude > 0.01f;
		if (hasMovementDirection) {
			landingSlideDirX_ = moveX / moveMagnitude;
			landingSlideDirY_ = moveY / moveMagnitude;
		}

		bool finishAfterVelocityUpdate = false;
		const float coastElapsed = landingSlideElapsed_ - k::LANDING_SLIDE_MAX_DURATION;
		if (coastElapsed > 0.0f) {
			const float coastProgress = std::min(1.0f, coastElapsed / k::LANDING_SLIDE_COAST_DURATION);
			landingSlideSpeed_ = landingSlideCoastStartSpeed_ * (1.0f - Smoothstep01(coastProgress));
			const float walk = std::max(0.1f, walkSpeed);
			if (hasMovementDirection && landingSlideSpeed_ <= walk) {
				landingSlideSpeed_ = std::min(landingSlideCoastStartSpeed_, walk);
				finishAfterVelocityUpdate = true;
			} else if (coastProgress >= 1.0f) {
				landingSlideSpeed_ = 0.0f;
				finishAfterVelocityUpdate = true;
			}
		}

		vel_.x = landingSlideDirX_ * landingSlideSpeed_;
		vel_.y = landingSlideDirY_ * landingSlideSpeed_;
		if (finishAfterVelocityUpdate) {
			StopLandingSlide();
		}
	}

	// ------------------------------------------------------------------ charge / punch

	void Sim::GrantInstantPunchCharge()
	{
		selfParryInstantChargeReady_ = true;
		selfParryInstantChargeTimer_ = k::SELF_PARRY_INSTANT_CHARGE_DURATION;
	}

	void Sim::ClearSelfParryInstantCharge()
	{
		selfParryInstantChargeReady_ = false;
		selfParryInstantChargeTimer_ = 0.0f;
	}

	void Sim::ClearNormalPunchCharge()
	{
		charge_ = 0.0f;
		chargeTime_ = 0.0f;
		sprintChargeTimer_ = 0.0f;
		chargeStartedAirborne_ = false;
		wasFullyChargedLastTick_ = false;
		overchargeWarningSent_ = false;
		airKickLockTimer_ = 0.0f;
		airKickLocked_ = false;
		ResetOverchargeDashChargeTracking();
	}

	void Sim::ResetOverchargeDashChargeTracking()
	{
		overchargeDashStationaryEligible_ = false;
		overchargeDashIntentLatched_ = false;
		overchargeDashMoveIntentDisqualified_ = false;
		overchargeDashMoveIntentTimer_ = 0.0f;
		overchargeDashActivationTimer_ = 0.0f;
		overchargeDashSupportGapTimer_ = 0.0f;
	}

	bool Sim::HasOverchargeDashMoveIntent() const
	{
		const float eps = k::OVERCHARGE_DASH_STATIONARY_MOVE_EPSILON;
		return input_.moveX * input_.moveX + input_.moveY * input_.moveY > eps * eps;
	}

	bool Sim::IsStandingStillForOverchargeDash() const
	{
		if (HasOverchargeDashMoveIntent() || !IsGroundedForAction(0.22f)) {
			return false;
		}
		const float limit = k::OVERCHARGE_DASH_STATIONARY_SPEED;
		return vel_.x * vel_.x + vel_.y * vel_.y <= limit * limit;
	}

	void Sim::UpdateOverchargeDashChargeEligibility(float a_dt)
	{
		if (!input_.railingChargeIntent) {
			ResetOverchargeDashChargeTracking();
			return;
		}
		if (!overchargeDashIntentLatched_) {
			overchargeDashIntentLatched_ = true;
			overchargeDashStationaryEligible_ = IsStandingStillForOverchargeDash();
			overchargeDashMoveIntentDisqualified_ = false;
			overchargeDashMoveIntentTimer_ = 0.0f;
			overchargeDashActivationTimer_ = 0.0f;
			overchargeDashSupportGapTimer_ = 0.0f;
			if (overchargeDashStationaryEligible_) {
				vel_.x = vel_.y = 0.0f;
			}
			return;
		}
		if (overchargeDashMoveIntentDisqualified_) {
			return;
		}
		if (!overchargeDashStationaryEligible_) {
			overchargeDashActivationTimer_ += a_dt;
			if (HasOverchargeDashMoveIntent()) {
				overchargeDashMoveIntentTimer_ += a_dt;
				if (overchargeDashMoveIntentTimer_ >= k::OVERCHARGE_DASH_MOVE_INTENT_DISQUALIFY_TIME) {
					overchargeDashMoveIntentDisqualified_ = true;
				}
				return;
			}
			overchargeDashMoveIntentTimer_ = 0.0f;
			if (IsStandingStillForOverchargeDash()) {
				overchargeDashStationaryEligible_ = true;
				overchargeDashActivationTimer_ = 0.0f;
				vel_.x = vel_.y = 0.0f;
			} else if (overchargeDashActivationTimer_ >= k::OVERCHARGE_DASH_SUPPORT_GRACE_TIME) {
				overchargeDashMoveIntentDisqualified_ = true;
			}
			return;
		}
		if (HasOverchargeDashMoveIntent()) {
			overchargeDashMoveIntentTimer_ += a_dt;
			if (overchargeDashMoveIntentTimer_ >= k::OVERCHARGE_DASH_MOVE_INTENT_DISQUALIFY_TIME) {
				overchargeDashMoveIntentDisqualified_ = true;
				overchargeDashStationaryEligible_ = false;
			}
			return;
		}
		overchargeDashMoveIntentTimer_ = 0.0f;
		if (IsGroundedForAction(0.22f)) {
			overchargeDashSupportGapTimer_ = 0.0f;
		} else {
			overchargeDashSupportGapTimer_ += a_dt;
			if (overchargeDashSupportGapTimer_ >= k::OVERCHARGE_DASH_SUPPORT_GRACE_TIME) {
				overchargeDashStationaryEligible_ = false;
				overchargeDashMoveIntentDisqualified_ = true;
			}
		}
	}

	bool Sim::IsOverchargeDashCharging() const
	{
		return overchargeDashStationaryEligible_ && overchargeDashIntentLatched_ && !isLimp_ && !isOverchargeDashing_;
	}

	bool Sim::CanReleaseOverchargeDash() const
	{
		return IsOverchargeDashCharging() && chargeTime_ >= k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME;
	}

	bool Sim::CanFireQueuedTapPunch() const
	{
		return pendingTapPunchCount_ > 0 && !input_.charging && !input_.grabbing && !isGrabbing_ && !isParrying_ &&
		       !isLimp_ && parryEndlag_ <= 0.0f && !isRocketPunchDiving_ && !isAirKickDashing_ && !isOverchargeDashing_;
	}

	void Sim::ConsumeQueuedTapPunch()
	{
		if (!CanFireQueuedTapPunch() || punchCooldown_ > 0.0f) {
			return;
		}
		pendingTapPunchCount_ = std::max(0, pendingTapPunchCount_ - 1);
		charge_ = k::TAP_PUNCH_STRENGTH;
		chargeTime_ = 0.0f;
		sprintChargeTimer_ = 0.0f;
		chargeStartedAirborne_ = false;
		wasFullyChargedLastTick_ = false;
		Punch();
		punchCooldown_ = k::TAP_PUNCH_COOLDOWN_DURATION;
		charge_ = 0.0f;
		chargeTime_ = 0.0f;
		ResetOverchargeDashChargeTracking();
	}

	void Sim::UpdateChargeAndPunch(float a_dt)
	{
		if (isHoldingGrabbed_ || isPiledriving_) {
			ClearNormalPunchCharge();
			return;
		}

		if (CanFireQueuedTapPunch()) {
			ConsumeQueuedTapPunch();
			return;
		}
		if (suppressChargeUntilRelease_) {
			if (!input_.charging) {
				suppressChargeUntilRelease_ = false;
			}
			ClearNormalPunchCharge();
			return;
		}

		if (input_.charging && !input_.grabbing && !isParrying_ && parryEndlag_ <= 0.0f) {
			if (chargeTime_ == 0.0f && !IsGroundedForAction(0.3f)) {
				chargeStartedAirborne_ = true;
			}
			if (selfParryInstantChargeReady_) {
				charge_ = 1.0f;
				chargeTime_ = std::max(chargeTime_, k::MAX_CHARGE_TIME);
			} else {
				charge_ = std::min(charge_ + a_dt / k::MAX_CHARGE_TIME, 1.0f);
			}

			const bool isFullyCharged = charge_ >= 1.0f;
			bool consumedDashCancelCharge = false;
			if (isFullyCharged && !wasFullyChargedLastTick_) {
				if (isAirKickDashing_) {
					CancelAirKickDashPreservingMomentum();
					charge_ = 1.0f;
					chargeTime_ = std::max(chargeTime_, k::MAX_CHARGE_TIME);
					sprintChargeTimer_ = 0.0f;
					chargeStartedAirborne_ = false;
					ResetOverchargeDashChargeTracking();
				}
				if (isOverchargeDashing_) {
					StopOverchargeDash(true);
					ClearNormalPunchCharge();
					suppressChargeUntilRelease_ = true;
					consumedDashCancelCharge = true;
				}
				if (!consumedDashCancelCharge) {
					Emit(EventType::ChargeReady);
				}
			}
			wasFullyChargedLastTick_ = consumedDashCancelCharge ? false : isFullyCharged;

			if (!consumedDashCancelCharge) {
				chargeTime_ += a_dt;
				UpdateOverchargeDashChargeEligibility(a_dt);
				if (charge_ >= 1.0f) {
					sprintChargeTimer_ += a_dt;
				} else {
					sprintChargeTimer_ = 0.0f;
					chargeStartedAirborne_ = false;
				}

				// Air-kick target lock: hold a full charge in the air with the crosshair on a target.
				if (charge_ >= 1.0f && IsAirborneForFullChargePunch() && airKickLockOnTarget_) {
					airKickLockTimer_ += a_dt;
					airKickLocked_ = airKickLockTimer_ >= k::AIR_KICK_TARGET_LOCK_SECONDS;
				} else {
					airKickLockTimer_ = 0.0f;
					airKickLocked_ = false;
				}

				const bool railingCharging = IsOverchargeDashCharging();
				if (chargeTime_ >= k::OVERCHARGE_WARNING_TIME && !railingCharging && !overchargeWarningSent_) {
					overchargeWarningSent_ = true;
					Emit(EventType::OverchargeWarning);
				}
				if (chargeTime_ >= k::OVERCHARGE_TIME && !railingCharging) {
					ClearNormalPunchCharge();
					lastPunchStrength_ = 0.0f;
					ClearSelfParryInstantCharge();
					suppressChargeUntilRelease_ = true;
					Emit(EventType::OverchargeExplode);
				}
			}
			return;
		}

		if (!input_.grabbing && !isGrabbing_) {
			if (charge_ > k::MIN_RELEASE_CHARGE && punchCooldown_ <= 0.0f) {
				const bool isInAir = IsAirborneForFullChargePunch();
				const bool isLookingStraightDown = input_.pitch <= k::DIVE_STRAIGHT_DOWN_PITCH;
				const bool hasEnoughCharge = charge_ >= 1.0f;
				const bool shouldPreferRocketPunch = hasEnoughCharge && input_.pitch > k::ROCKET_PUNCH_LOOK_UP_PITCH;
				const bool canDiveBomb = isInAir && isLookingStraightDown && hasEnoughCharge && !isLimp_;

				bool overchargeDashStarted = false;
				if (canDiveBomb) {
					rocketPunchInitialSpeed_ = std::max(k::DIVE_MIN_INITIAL_SPEED, vel_.Length());
					isRocketPunchAscending_ = false;
					rocketPunchAscentCharge_ = 0.0f;
					rocketPunchAscentTimer_ = 0.0f;
					isRocketPunchDiving_ = true;
					inRocketPunchFlight_ = false;
					rocketPunchCharge_ = charge_;
					rocketPunchDiveTimer_ = 0.0f;
					rocketPunchStartZ_ = env_.position.z;
					rocketPunchDiveSpeed_ = DiveSpeedAtTime(0.0f);
					rocketPunchHorizontalBoostActive_ = false;
					const float horizontalComponent = std::cos(input_.pitch);
					if (horizontalComponent > 0.3f) {
						const float launchVelocity = 20.0f + charge_ * 30.0f;
						const float horizontalSpeed = std::max(std::hypot(vel_.x, vel_.y), launchVelocity * horizontalComponent);
						const Vec3 f = YawForward(input_.yaw);
						rocketPunchHorizontalBoostActive_ = true;
						rocketPunchBoostVel_ = { f.x * horizontalSpeed * 1.4f, f.y * horizontalSpeed * 1.4f, 0.0f };
						vel_.x = rocketPunchBoostVel_.x;
						vel_.y = rocketPunchBoostVel_.y;
					}
					vel_.z = -rocketPunchDiveSpeed_;
					StopLandingSlide();
					Emit(EventType::DiveStart, charge_);
				} else if (!shouldPreferRocketPunch && CanReleaseOverchargeDash() && StartOverchargeDash()) {
					overchargeDashStarted = true;
				} else {
					// The original clears the parry-granted charge after punch(), but GameWorld resolves the
					// punch (and any new grant) later in the tick; clearing first keeps that order.
					ClearSelfParryInstantCharge();
					Punch();
				}
				if (overchargeDashStarted || isRocketPunchDiving_) {
					ClearSelfParryInstantCharge();
				}
				punchCooldown_ = k::PUNCH_COOLDOWN_DURATION;
			}
			ClearNormalPunchCharge();
		}
	}

	void Sim::Punch()
	{
		lastPunchStrength_ = charge_;
		lastChargeTime_ = chargeTime_;
		lastPunchSprintTimer_ = sprintChargeTimer_;
		const float strength = lastPunchStrength_;
		const float pitch = input_.pitch;
		const bool isLookingUp = pitch > k::ROCKET_PUNCH_LOOK_UP_PITCH;
		const bool isLookingStraightDown = pitch <= k::DIVE_STRAIGHT_DOWN_PITCH;
		const bool canRocketPunch = !hasUsedRocketPunchInAir_;
		const bool isAirborneFullCharge = IsAirborneForFullChargePunch();
		const bool isRocketPunch = strength >= 1.0f && isLookingUp;
		const bool isAirKickDash = strength >= 1.0f && isAirborneFullCharge && !isLookingUp && !isLookingStraightDown &&
		                           !isAirKickDashing_;
		const bool isCriticalFist = lastChargeTime_ >= k::CRITICAL_FIST_CHARGE_TIME && strength >= k::RAGDOLL_PUNCH_THRESHOLD;
		const bool isCriticalDash = isCriticalFist && !isRocketPunch && !isAirKickDash;
		const Vec3 forward = YawForward(input_.yaw);

		if (isCriticalDash) {
			StartCriticalDash(forward, k::CRITICAL_DASH_SPEED, k::CRITICAL_DASH_DURATION);
		}

		bool didRocketLaunch = false;
		if (isRocketPunch && canRocketPunch) {
			const float launchVelocity = k::ROCKET_PUNCH_BASE_LAUNCH + strength * k::ROCKET_PUNCH_CHARGE_LAUNCH;
			StopLandingSlide();
			rocketPunchLaunchSpeed_ = std::hypot(vel_.x, vel_.y);
			vel_.z = launchVelocity * std::sin(pitch);
			isRocketPunchAscending_ = true;
			rocketPunchAscentCharge_ = strength;
			rocketPunchAscentTimer_ = 0.0f;
			hasUsedRocketPunchInAir_ = true;
			inRocketPunchFlight_ = true;
			momentumOnlyFlight_ = false;
			takeoffGraceTimer_ = k::GROUND_PUNCH_HOP_TAKEOFF_GRACE;
			didRocketLaunch = true;
			Emit(EventType::RocketPunchLaunch, strength);
		}

		if (isAirKickDash) {
			StartAirKickDash(AimDirection(input_.yaw, pitch), strength, vel_);
			return;
		}

		Event& e = EmitEvent(EventType::PunchThrown);
		e.strength = strength;
		e.value = lastChargeTime_;
		e.critical = isCriticalFist;
		e.flag = pitch > k::PLAYER_UPPERCUT_MIN_PITCH && strength >= k::PLAYER_UPPERCUT_MIN_CHARGE;
		e.direction = AimDirection(input_.yaw, pitch);
		e.kind = static_cast<int>(didRocketLaunch ? PunchKind::Rocket :
		                          isCriticalDash ? PunchKind::CriticalDash :
		                          strength <= k::TAP_PUNCH_STRENGTH + 0.001f && lastChargeTime_ == 0.0f ? PunchKind::Tap :
		                                                                                              PunchKind::Normal);

		// Ground-punch hop: a full-charge punch aimed at your feet.
		if (strength >= 1.0f && pitch <= k::GROUND_PARRY_LOOK_DOWN_PITCH && IsGroundedForAction(0.12f) && !didRocketLaunch) {
			const float speed = std::hypot(vel_.x, vel_.y);
			float newSpeed = speed * 1.06f + 1.0f;
			const float moveMagnitude = std::hypot(input_.moveX, input_.moveY);
			Vec3 dir = speed > 0.1f ? Vec3{ vel_.x / speed, vel_.y / speed, 0.0f } : forward;
			if (moveMagnitude > 0.01f) {
				const Vec3 right = YawRight(input_.yaw);
				dir = Vec3{ input_.moveX * right.x + input_.moveY * forward.x, input_.moveX * right.y + input_.moveY * forward.y, 0.0f }
				          .Normalized();
				newSpeed = std::max(newSpeed, walkSpeed * (k::CHARGE_SPRINT_MULTIPLIER + lastPunchSprintTimer_ / k::SPRINT_RAMP_TIME));
			} else if (speed <= 0.1f) {
				newSpeed = 0.0f;
			}
			vel_.x = dir.x * newSpeed;
			vel_.y = dir.y * newSpeed;
			vel_.z = std::max(vel_.z, k::GROUND_PUNCH_HOP_PUNCH_LAUNCH);
			StopLandingSlide();
			knockbackTimer_ = std::max(knockbackTimer_, k::GROUND_PUNCH_HOP_KNOCKBACK);
			BeginGroundPunchHop(newSpeed);
			GrantInstantPunchCharge();
			Emit(EventType::GroundPunchHop, strength);
		}
	}

	void Sim::ApplyWallPunchRecoil(float a_strength)
	{
		const Vec3 forward = YawForward(input_.yaw);
		const float deltaV = a_strength * k::WALL_PUNCH_RECOIL_IMPULSE / k::RAGDOLL_MASS;
		vel_.x -= forward.x * deltaV;
		vel_.y -= forward.y * deltaV;
		knockbackTimer_ = std::max(knockbackTimer_, 0.12f + 0.18f * a_strength);
	}

	void Sim::OnHitByAttack(float a_strength, const Vec3& a_impulseVelocity)
	{
		if (IsInvulnerable()) {
			return;
		}
		ClearNormalPunchCharge();
		ClearSelfParryInstantCharge();
		vel_ += a_impulseVelocity;
		knockbackTimer_ = std::max(knockbackTimer_, k::KNOCKBACK_DURATION * (0.25f + 0.75f * Clamp01(a_strength)));
		if (isGrabbing_ || isHoldingGrabbed_) {
			EndGrab(true);
		}
	}

	void Sim::StartCriticalDash(const Vec3& a_forward, float a_speed, float a_duration)
	{
		criticalDashTimer_ = std::max(criticalDashTimer_, a_duration);
		vel_.x = a_forward.x * a_speed;
		vel_.y = a_forward.y * a_speed;
	}

	// ------------------------------------------------------------------ railing

	bool Sim::StartOverchargeDash()
	{
		if (!CanReleaseOverchargeDash()) {
			return false;
		}
		const Vec3 dir = YawForward(input_.yaw);
		isOverchargeDashing_ = true;
		overchargeDashMegaBoostActive_ = false;
		overchargeDashMegaBoostInputTime_ = 0.0f;
		overchargeDashMegaBoostInputConsumed_ = false;
		overchargeDashImpactParryTimer_ = 0.0f;
		overchargeDashDir_ = dir;
		overchargeDashLaunchChargeTime_ = std::max(0.0f, chargeTime_);
		overchargeDashSpeed_ = RailingTravelSpeed(overchargeDashLaunchChargeTime_);
		vel_.x = dir.x * overchargeDashSpeed_;
		vel_.y = dir.y * overchargeDashSpeed_;
		vel_.z = Clamp(vel_.z, -2.0f, 2.0f);
		StopLandingSlide();
		Event& e = EmitEvent(EventType::RailingStart);
		e.value = overchargeDashSpeed_;
		e.direction = dir;
		e.strength = RailingTuningProgress(overchargeDashLaunchChargeTime_);
		return true;
	}

	void Sim::StopOverchargeDash(bool a_intoSlide)
	{
		if (!isOverchargeDashing_) {
			return;
		}
		isOverchargeDashing_ = false;
		overchargeDashMegaBoostActive_ = false;
		Emit(EventType::RailingStop);
		if (a_intoSlide) {
			if (grounded_) {
				if (!StartLandingSlideFromHorizontalVelocity(vel_.x, vel_.y)) {
					landingSlideCandidate_ = false;
				}
			} else {
				dashCancelMomentumActive_ = true;
			}
		}
	}

	void Sim::OnRailingHit(bool a_flatten)
	{
		if (!isOverchargeDashing_ || a_flatten) {
			return;
		}
		overchargeDashSpeed_ *= k::RAILING_PLAYER_HIT_MOMENTUM_RETENTION;
		vel_.x = overchargeDashDir_.x * overchargeDashSpeed_;
		vel_.y = overchargeDashDir_.y * overchargeDashSpeed_;
		StopOverchargeDash(true);
	}

	// ------------------------------------------------------------------ air kick

	float Sim::AirKickDashSpeedAtTime(float a_time) const
	{
		const float startSpeed = airKickDashInitialSpeed_ * k::AIR_KICK_DASH_START_SPEED_MULTIPLIER;
		const float maxSpeed = airKickDashInitialSpeed_ * k::AIR_KICK_DASH_MAX_SPEED_MULTIPLIER;
		if (a_time <= k::AIR_KICK_DASH_RAMP_TIME) {
			return startSpeed + (maxSpeed - startSpeed) * Smoothstep01(a_time / k::AIR_KICK_DASH_RAMP_TIME);
		}
		if (a_time <= k::AIR_KICK_DASH_COAST_TIME) {
			return maxSpeed;
		}
		if (a_time >= k::AIR_KICK_DASH_STOP_TIME) {
			return 0.0f;
		}
		const float decel = (a_time - k::AIR_KICK_DASH_COAST_TIME) / (k::AIR_KICK_DASH_STOP_TIME - k::AIR_KICK_DASH_COAST_TIME);
		return maxSpeed * (1.0f - decel);
	}

	float Sim::AirKickImpactScale() const
	{
		const float startSpeed = airKickDashInitialSpeed_ * k::AIR_KICK_DASH_START_SPEED_MULTIPLIER;
		const float maxSpeed = airKickDashInitialSpeed_ * k::AIR_KICK_DASH_MAX_SPEED_MULTIPLIER;
		const float speedProgress = Clamp01((airKickDashSpeed_ - startSpeed) / std::max(0.001f, maxSpeed - startSpeed));
		const float progress = std::max(Smoothstep01(airKickDashTimer_ / k::AIR_KICK_DASH_RAMP_TIME), speedProgress);
		return k::AIR_KICK_DASH_MIN_IMPACT_SCALE +
		       (k::AIR_KICK_DASH_MAX_IMPACT_SCALE - k::AIR_KICK_DASH_MIN_IMPACT_SCALE) * progress * progress +
		       k::AIR_KICK_DASH_MOMENTUM_IMPACT_BONUS * airKickDashMomentumScale_ * progress;
	}

	void Sim::StartAirKickDash(const Vec3& a_direction, float a_charge, const Vec3& a_startVelocity)
	{
		if (isLimp_ || isAirKickDashing_) {
			return;
		}
		const Vec3 dir = a_direction.Normalized();
		if (dir.Length() < 0.5f) {
			return;
		}
		const float alignedStartSpeed = std::max(0.0f, a_startVelocity.Dot(dir));
		const float totalStartSpeed = a_startVelocity.Length();
		const float momentumRange = std::max(0.001f, k::AIR_KICK_DASH_MOMENTUM_FULL_SPEED - k::AIR_KICK_DASH_MOMENTUM_MIN_SPEED);
		const float momentumScale = Smoothstep01((alignedStartSpeed - k::AIR_KICK_DASH_MOMENTUM_MIN_SPEED) / momentumRange);

		StopLandingSlide();
		isAirKickDashing_ = true;
		dashCancelMomentumActive_ = false;
		airKickDashCharge_ = a_charge;
		airKickDashTimer_ = 0.0f;
		airKickDashMomentumScale_ = momentumScale;
		const float impactInitialSpeed = std::max(k::AIR_KICK_DASH_BASE_SPEED + k::AIR_KICK_DASH_MOMENTUM_SPEED_BONUS * momentumScale,
			alignedStartSpeed / k::AIR_KICK_DASH_START_SPEED_MULTIPLIER);
		airKickDashInitialSpeed_ = std::max(impactInitialSpeed, totalStartSpeed / k::AIR_KICK_DASH_START_SPEED_MULTIPLIER);
		airKickDashSpeed_ = AirKickDashSpeedAtTime(0.0f);
		airKickDir_ = dir;
		airKickHomingValid_ = airKickLocked_;
		inRocketPunchFlight_ = false;
		isRocketPunchAscending_ = false;
		rocketPunchHorizontalBoostActive_ = false;
		vel_ = dir * airKickDashSpeed_;
		Event& e = EmitEvent(EventType::AirKickStart);
		e.strength = a_charge;
		e.direction = dir;
		e.flag = airKickLocked_;
	}

	void Sim::StopAirKickDash()
	{
		if (!isAirKickDashing_) {
			return;
		}
		isAirKickDashing_ = false;
		airKickHomingValid_ = false;
		airKickDashSpeed_ = 0.0f;
		Emit(EventType::AirKickStop);
	}

	void Sim::CancelAirKickDashPreservingMomentum()
	{
		StopAirKickDash();
		dashCancelMomentumActive_ = true;
	}

	void Sim::SetAirKickHomingTarget(bool a_valid, const Vec3& a_position)
	{
		if (!isAirKickDashing_) {
			return;
		}
		airKickHomingValid_ = airKickHomingValid_ && a_valid;
		airKickHomingTarget_ = a_position;
	}

	void Sim::AirKickWhiff(const Vec3& a_normal)
	{
		const float approach = std::max(0.0f, -vel_.Dot(a_normal));
		const float rampProgress = Smoothstep01(airKickDashTimer_ / k::AIR_KICK_DASH_RAMP_TIME);
		// A whiffed kick costs control (ragdoll plus hard knockdown), never health.
		const float damage = 0.0f;
		(void)approach;
		StopAirKickDash();
		vel_ = vel_ - a_normal * vel_.Dot(a_normal);
		vel_ = vel_ * 0.5f;
		Event& e = EmitEvent(EventType::AirKickWhiff);
		e.value = damage;
		e.direction = a_normal;
		EnterRagdoll(Lerp(0.85f, 1.4f, rampProgress), true, Lerp(1.2f, 2.0f, rampProgress));
		ragdollDamageSuppressed_ = true;
	}

	void Sim::OnAirKickHit()
	{
		if (!isAirKickDashing_) {
			return;
		}
		const float strength = airKickDashCharge_;
		const Vec3 dir = airKickDir_;
		const float currentVz = vel_.z;
		StopAirKickDash();
		const float horizontal = 4.5f + 3.0f * strength;
		const float dirH = std::hypot(dir.x, dir.y);
		if (dirH > 0.001f) {
			vel_.x = -dir.x / dirH * horizontal;
			vel_.y = -dir.y / dirH * horizontal;
		} else {
			vel_.x = vel_.y = 0.0f;
		}
		vel_.z = std::max(5.5f + 3.5f * strength, 0.15f * currentVz);
		takeoffGraceTimer_ = k::GROUND_PUNCH_HOP_TAKEOFF_GRACE;
		StartFlip();
		hasUsedRocketPunchInAir_ = false;
		GrantInstantPunchCharge();
		BeginMomentumFlight(horizontal);
	}

	// ------------------------------------------------------------------ dive bomb

	float Sim::DiveSpeedAtTime(float a_time) const
	{
		return UnboundedRampSpeedAtTime(a_time, rocketPunchInitialSpeed_ * k::ROCKET_PUNCH_DIVE_START_SPEED_MULTIPLIER,
			rocketPunchInitialSpeed_ * k::ROCKET_PUNCH_DIVE_MAX_SPEED_MULTIPLIER, k::ROCKET_PUNCH_DIVE_RAMP_TIME);
	}

	float Sim::DiveImpactIntensity(float a_impactZ, float a_impactSpeed) const
	{
		const float startSpeed = rocketPunchInitialSpeed_ * k::ROCKET_PUNCH_DIVE_START_SPEED_MULTIPLIER;
		const float maxSpeed = rocketPunchInitialSpeed_ * k::ROCKET_PUNCH_DIVE_MAX_SPEED_MULTIPLIER;
		const float speedProgress = Smoothstep01((a_impactSpeed - startSpeed) / std::max(0.001f, maxSpeed - startSpeed));
		const float heightProgress = Smoothstep01(std::max(0.0f, rocketPunchStartZ_ - a_impactZ) / 24.0f);
		const float baseCharge = Clamp(rocketPunchCharge_, 0.75f, 1.0f);
		return Clamp(baseCharge + speedProgress * 0.25f + heightProgress * 0.1f, 0.75f, k::ROCKET_PUNCH_DIVE_MAX_IMPACT_INTENSITY);
	}

	void Sim::FinishDive(const Vec3& a_point, bool a_obstacle)
	{
		const float intensity = DiveImpactIntensity(a_point.z, std::max(vel_.Length(), rocketPunchDiveSpeed_));
		isRocketPunchDiving_ = false;
		rocketPunchCharge_ = 0.0f;
		rocketPunchDiveTimer_ = 0.0f;
		rocketPunchDiveSpeed_ = 0.0f;
		rocketPunchHorizontalBoostActive_ = false;
		rocketPunchBoostVel_ = {};
		hasUsedRocketPunchInAir_ = false;
		vel_ = {};
		awaitingDiveResolution_ = true;
		Event& e = EmitEvent(a_obstacle ? EventType::DiveObstacle : EventType::DiveImpact);
		e.value = intensity;
		e.position = a_point;
	}

	void Sim::ResolveDiveImpact(bool a_hitAnyone)
	{
		if (!awaitingDiveResolution_) {
			return;
		}
		awaitingDiveResolution_ = false;
		if (!a_hitAnyone) {
			EnterRagdoll(k::DIVE_WHIFF_RAGDOLL_SECONDS, true);
		}
	}

	// ------------------------------------------------------------------ specials update

	void Sim::UpdateSpecialMoves(float a_dt)
	{
		if (isRocketPunchAscending_) {
			rocketPunchAscentTimer_ += a_dt;
			if (vel_.z <= 0.0f || rocketPunchAscentTimer_ >= k::ROCKET_PUNCH_ASCENT_MAX_TIME) {
				isRocketPunchAscending_ = false;
				rocketPunchAscentCharge_ = 0.0f;
			}
		}

		if (isRocketPunchDiving_) {
			rocketPunchDiveTimer_ += a_dt;
			rocketPunchDiveSpeed_ = DiveSpeedAtTime(rocketPunchDiveTimer_);
			const bool floorContact = env_.contact && env_.contactNormal.z > k::SURFACE_PARRY_FLOOR_MIN_NORMAL_UP;
			const bool hitGround = env_.groundSupport || floorContact;
			const bool hitObstacle = !hitGround && env_.contact && rocketPunchDiveTimer_ > 0.15f;
			if (hitGround || hitObstacle) {
				const Vec3 normal = hitGround ? (floorContact ? env_.contactNormal : env_.groundNormal) : env_.contactNormal;
				const Vec3 point = env_.contact ? env_.contactPoint : env_.position;
				if (!TrySurfaceParryRebound(normal, true)) {
					FinishDive(point, hitObstacle);
				}
			} else {
				if (rocketPunchHorizontalBoostActive_) {
					vel_.x = rocketPunchBoostVel_.x;
					vel_.y = rocketPunchBoostVel_.y;
				}
				vel_.z = -rocketPunchDiveSpeed_;
			}
		}

		if (isPiledriving_) {
			piledriverTimer_ += a_dt;
			const float speed = UnboundedRampSpeedAtTime(piledriverTimer_, k::PLAYER_GRAB_PILEDRIVER_TRAVEL_MIN_SPEED,
				k::PLAYER_GRAB_PILEDRIVER_TRAVEL_MAX_SPEED, k::PLAYER_GRAB_PILEDRIVER_TRAVEL_RAMP_TIME);
			const bool hitSurface = env_.groundSupport || env_.contact;
			if (hitSurface && piledriverTimer_ > 0.05f) {
				isPiledriving_ = false;
				vel_ = {};
				grabCooldown_ = k::PLAYER_GRAB_COOLDOWN;
				Event& e = EmitEvent(EventType::PiledriverImpact);
				e.value = std::min(1.15f, 0.55f + speed / 32.0f);
				e.position = env_.contact ? env_.contactPoint : env_.position;
			} else {
				Vec3 dir = AimDirection(input_.yaw, input_.pitch);
				if (dir.z > -0.35f) {
					dir.z = -0.35f;
					dir = dir.Normalized();
				}
				vel_ = dir * speed;
			}
		}

		if (isAirKickDashing_) {
			airKickDashTimer_ += a_dt;
			airKickDashSpeed_ = AirKickDashSpeedAtTime(airKickDashTimer_);

			if (airKickHomingValid_) {
				const Vec3 toTarget = (airKickHomingTarget_ - env_.position).Normalized();
				const float cosAngle = Clamp(toTarget.Dot(airKickDir_), -1.0f, 1.0f);
				const float angle = std::acos(cosAngle);
				if (angle > k::AIR_KICK_HOMING_MAX_OFF_AXIS) {
					airKickHomingValid_ = false;
				} else if (angle > 0.0001f) {
					const float t = std::min(1.0f, k::AIR_KICK_HOMING_TURN_RATE * a_dt / angle);
					airKickDir_ = (airKickDir_ * (1.0f - t) + toTarget * t).Normalized();
				}
			}

			bool handled = false;
			const bool groundLanding = env_.groundSupport && airKickDashTimer_ >= k::AIR_KICK_DASH_LANDING_WHIFF_ARM_TIME;
			if (env_.contact || groundLanding) {
				const Vec3 normal = env_.contact ? env_.contactNormal : env_.groundNormal;
				const float approach = -vel_.Dot(normal);
				bool whiff = false;
				if (normal.z <= k::AIR_KICK_DASH_WALL_WHIFF_MAX_NORMAL_UP) {
					whiff = approach >= k::AIR_KICK_DASH_WALL_WHIFF_MIN_APPROACH_SPEED;
				} else if (normal.z < k::AIR_KICK_DASH_LANDING_MIN_NORMAL_UP) {
					whiff = approach >= k::AIR_KICK_DASH_SLOPE_WHIFF_MIN_APPROACH_SPEED;
				} else {
					whiff = airKickDashTimer_ >= k::AIR_KICK_DASH_LANDING_WHIFF_ARM_TIME &&
					        approach >= k::AIR_KICK_DASH_LANDING_WHIFF_MIN_APPROACH_SPEED;
				}
				if (whiff) {
					if (!TrySurfaceParryRebound(normal, false)) {
						AirKickWhiff(normal);
					}
					handled = true;
				} else if (groundLanding && normal.z >= k::AIR_KICK_DASH_LANDING_MIN_NORMAL_UP) {
					// Touched down too gently to whiff: the dash simply ends.
					StopAirKickDash();
					handled = true;
				}
			}
			if (!handled && isAirKickDashing_) {
				if (airKickDashTimer_ > 10.0f || airKickDashSpeed_ <= 0.01f) {
					StopAirKickDash();
				} else if (airKickDashTimer_ <= k::AIR_KICK_DASH_COAST_TIME) {
					vel_ = airKickDir_ * airKickDashSpeed_;
				} else {
					vel_.x = airKickDir_.x * airKickDashSpeed_;
					vel_.y = airKickDir_.y * airKickDashSpeed_;
				}
			}
		}

		if (isOverchargeDashing_) {
			// Mega boost: hold parry alone for a second.
			if (input_.parrying && !input_.charging && !input_.grabbing && !overchargeDashMegaBoostInputConsumed_) {
				overchargeDashMegaBoostInputTime_ += a_dt;
				if (overchargeDashMegaBoostInputTime_ >= k::OVERCHARGE_DASH_MEGA_BOOST_HOLD) {
					overchargeDashMegaBoostInputConsumed_ = true;
					overchargeDashMegaBoostActive_ = true;
					overchargeDashSpeed_ *= 2.0f;
					Emit(EventType::RailingMegaBoost, 0.0f, overchargeDashSpeed_);
				}
			} else if (!input_.parrying) {
				overchargeDashMegaBoostInputTime_ = 0.0f;
			}
			if (input_.parrying && !previousParryInput_) {
				overchargeDashImpactParryTimer_ = k::SURFACE_PARRY_WINDOW;
			}

			if (!overchargeDashMegaBoostActive_) {
				const float currentAngle = std::atan2(overchargeDashDir_.x, overchargeDashDir_.y);
				const float delta = WrapAngle(input_.yaw - currentAngle);
				const float speedRange = std::max(0.001f, k::RAILING_MAX_TRAVEL_SPEED - k::RAILING_MIN_TRAVEL_SPEED);
				const float speedRatio = Clamp01((overchargeDashSpeed_ - k::RAILING_MIN_TRAVEL_SPEED) / speedRange);
				const float turnRate = Lerp(k::OVERCHARGE_DASH_MAX_TURN_RATE, k::OVERCHARGE_DASH_MIN_TURN_RATE, speedRatio);
				const float maxDelta = std::max(0.0f, turnRate * a_dt);
				const float nextAngle = currentAngle + Clamp(delta, -maxDelta, maxDelta);
				overchargeDashDir_ = { std::sin(nextAngle), std::cos(nextAngle), 0.0f };
			}

			const bool wall = env_.contact && env_.contactNormal.z <= 0.7f && overchargeDashDir_.Dot(env_.contactNormal) < -0.05f;
			if (wall) {
				Vec3 n{ env_.contactNormal.x, env_.contactNormal.y, 0.0f };
				n = n.Normalized();
				if (overchargeDashImpactParryTimer_ > 0.0f) {
					overchargeDashImpactParryTimer_ = 0.0f;
					overchargeDashDir_ = -overchargeDashDir_;
					overchargeDashSpeed_ *= 2.0f;
					overchargeDashMegaBoostActive_ = true;
					overchargeDashMegaBoostInputConsumed_ = true;
					parryCooldown_ = 0.0f;
					GrantInstantPunchCharge();
					Emit(EventType::ParrySuccess, 0.0f, 0.0f, false, static_cast<int>(ParryKind::Surface));
					Emit(EventType::RailingMegaBoost, 0.0f, overchargeDashSpeed_);
				} else {
					const bool lethal = overchargeDashMegaBoostActive_;
					const Vec3 d = overchargeDashDir_;
					const Vec3 reflected = (d - n * (2.0f * d.Dot(n))).Normalized();
					const float newSpeed = Clamp(overchargeDashSpeed_ * k::OVERCHARGE_DASH_BOUNCE_SPEED_SCALE,
						k::OVERCHARGE_DASH_BOUNCE_MIN_SPEED, k::OVERCHARGE_DASH_BOUNCE_MAX_SPEED);
					const float launchProgress = RailingTuningProgress(overchargeDashLaunchChargeTime_);
					StopOverchargeDash(false);
					vel_ = { reflected.x * newSpeed, reflected.y * newSpeed, 0.0f };
					knockbackTimer_ = std::max(knockbackTimer_, k::OVERCHARGE_DASH_BOUNCE_KNOCKBACK);
					Event& e = EmitEvent(EventType::RailingBounce);
					e.value = newSpeed;
					e.flag = lethal;
					e.strength = launchProgress;
					e.position = env_.contactPoint;
					e.direction = n;
				}
			}
			if (isOverchargeDashing_) {
				vel_.x = overchargeDashDir_.x * overchargeDashSpeed_;
				vel_.y = overchargeDashDir_.y * overchargeDashSpeed_;
				vel_.z = Clamp(vel_.z, -k::OVERCHARGE_DASH_MAX_VERTICAL, k::OVERCHARGE_DASH_MAX_VERTICAL);
			}
		}
	}

	// ------------------------------------------------------------------ parry

	bool Sim::CanStartStandingParry() const
	{
		return parryCooldown_ <= 0.0f && !isParrying_ && !isLimp_ && !isGrabbing_ && !isHoldingGrabbed_ &&
		       !isPiledriving_ && !isOverchargeDashing_;
	}

	bool Sim::CanStartSurfaceParry() const
	{
		if (parryCooldown_ > 0.0f || isParrying_ || isLimp_ || isGrabbing_ || isHoldingGrabbed_ || isPiledriving_ ||
		    isOverchargeDashing_) {
			return false;
		}
		if (isRocketPunchDiving_ || isAirKickDashing_) {
			return true;
		}
		return IsAirborneForFullChargePunch() || !IsGroundedForAction(0.12f) || std::abs(vel_.z) > 2.0f;
	}

	void Sim::MarkParrySuccessful(ParryKind a_kind)
	{
		if (isParrying_ && parryTimer_ > 0.0f) {
			parrySucceededThisWindow_ = true;
		}
		parryCooldown_ = 0.0f;
		parryEndlag_ = 0.0f;
		midairParryWindowTimer_ = 0.0f;
		GrantInstantPunchCharge();
		Emit(EventType::ParrySuccess, 0.0f, 0.0f, false, static_cast<int>(a_kind));
	}

	void Sim::BeginMomentumFlight(float a_horizontalSpeed)
	{
		inRocketPunchFlight_ = true;
		momentumOnlyFlight_ = true;
		rocketPunchLaunchSpeed_ = a_horizontalSpeed;
	}

	void Sim::BeginGroundPunchHop(float a_horizontalSpeed)
	{
		BeginMomentumFlight(a_horizontalSpeed);
		takeoffGraceTimer_ = k::GROUND_PUNCH_HOP_TAKEOFF_GRACE;
	}

	void Sim::StartFlip()
	{
		flipTimer_ = k::PLAYER_ACROBATIC_FLIP_DURATION;
		Emit(EventType::Flip);
	}

	bool Sim::TryApplyStandingGroundSelfParryBounce()
	{
		if (input_.pitch > k::GROUND_PARRY_LOOK_DOWN_PITCH || isRocketPunchDiving_ || isAirKickDashing_ || isLimp_) {
			return false;
		}
		const bool hasSupport = grounded_ || env_.groundSupport || env_.groundClearance <= k::GROUND_SUPPORT_CLEARANCE_EPSILON;
		if (!hasSupport || env_.groundNormal.z < k::SURFACE_PARRY_FLOOR_MIN_NORMAL_UP) {
			return false;
		}
		vel_.x *= k::SURFACE_PARRY_FLOOR_HORIZONTAL_SPEED_MULTIPLIER;
		vel_.y *= k::SURFACE_PARRY_FLOOR_HORIZONTAL_SPEED_MULTIPLIER;
		vel_.z = std::max(vel_.z, k::GROUND_PUNCH_HOP_LAUNCH_VELOCITY);
		const float horizontalSpeed = std::hypot(vel_.x, vel_.y);
		StopLandingSlide();
		hasUsedRocketPunchInAir_ = false;
		StartFlip();
		BeginGroundPunchHop(horizontalSpeed);
		MarkParrySuccessful(ParryKind::Ground);
		return true;
	}

	bool Sim::TrySurfaceParryRebound(const Vec3& a_normal, bool a_afterDive)
	{
		if (!isParrying_ || !parryIsSurface_ || parryTimer_ <= 0.0f) {
			return false;
		}
		const Vec3 incoming = a_afterDive ? Vec3{ vel_.x, vel_.y, -std::max(rocketPunchDiveSpeed_, -vel_.z) } : vel_;
		const float approach = -incoming.Dot(a_normal);
		if (approach < k::SURFACE_PARRY_MIN_APPROACH_SPEED) {
			return false;
		}
		const bool wasAirKick = isAirKickDashing_;
		const bool special = wasAirKick || a_afterDive;
		const bool isFloor = a_normal.z > k::SURFACE_PARRY_FLOOR_MIN_NORMAL_UP;
		Vec3 rebound{};
		if (isFloor) {
			rebound.x = incoming.x * k::SURFACE_PARRY_FLOOR_HORIZONTAL_SPEED_MULTIPLIER;
			rebound.y = incoming.y * k::SURFACE_PARRY_FLOOR_HORIZONTAL_SPEED_MULTIPLIER;
			rebound.z = k::SURFACE_PARRY_FLOOR_MIN_UPWARD_VELOCITY;
			if (a_afterDive) {
				const float drop = std::max(0.0f, rocketPunchStartZ_ - env_.position.z);
				rebound.z = std::max(rebound.z, 0.5f * std::sqrt(2.0f * std::abs(env_.gravity) * drop));
			}
		} else {
			const float multiplier = wasAirKick ? k::AIR_KICK_SURFACE_PARRY_REBOUND_MULTIPLIER : k::SURFACE_PARRY_REBOUND_MULTIPLIER;
			rebound = (incoming - a_normal * (2.0f * incoming.Dot(a_normal))) * multiplier;
			const float speed = rebound.Length();
			if (speed > k::SURFACE_PARRY_MAX_REBOUND_SPEED) {
				rebound = rebound * (k::SURFACE_PARRY_MAX_REBOUND_SPEED / speed);
			}
			if (wasAirKick) {
				const float h = std::hypot(rebound.x, rebound.y);
				if (h > k::AIR_KICK_SURFACE_PARRY_MAX_HORIZONTAL_SPEED) {
					const float s = k::AIR_KICK_SURFACE_PARRY_MAX_HORIZONTAL_SPEED / h;
					rebound.x *= s;
					rebound.y *= s;
				}
			}
			rebound.z = std::max(rebound.z, k::SURFACE_PARRY_MIN_UPWARD_VELOCITY);
			if (special) {
				rebound.z = std::min(rebound.z, k::SURFACE_PARRY_SPECIAL_WALL_MAX_UPWARD_VELOCITY);
			}
		}

		if (isAirKickDashing_) StopAirKickDash();
		if (isRocketPunchDiving_) {
			isRocketPunchDiving_ = false;
			rocketPunchDiveTimer_ = 0.0f;
			rocketPunchDiveSpeed_ = 0.0f;
			rocketPunchHorizontalBoostActive_ = false;
		}
		isRocketPunchAscending_ = false;
		StopLandingSlide();
		vel_ = rebound;
		knockbackTimer_ = std::max(knockbackTimer_, k::SURFACE_PARRY_KNOCKBACK);
		hasUsedRocketPunchInAir_ = false;
		takeoffGraceTimer_ = k::GROUND_PUNCH_HOP_TAKEOFF_GRACE;
		StartFlip();
		BeginMomentumFlight(std::hypot(rebound.x, rebound.y));
		MarkParrySuccessful(ParryKind::Surface);
		return true;
	}

	void Sim::UpdateParryInput(float a_dt)
	{
		const bool pressEdge = input_.parrying && !previousParryInput_;

		if (isParrying_) {
			// A generic wall / floor contact during a surface window rebounds.
			if (parryIsSurface_ && env_.contact && !isRocketPunchDiving_ && !isAirKickDashing_) {
				TrySurfaceParryRebound(env_.contactNormal, false);
			}
			parryTimer_ -= a_dt;
			if (parryTimer_ <= 0.0f) {
				isParrying_ = false;
				parryTimer_ = 0.0f;
				if (!parrySucceededThisWindow_ && !parryIsSurface_) {
					parryEndlag_ = k::PARRY_ENDLAG_DURATION;
					Emit(EventType::ParryEndlag);
				}
			}
		}

		if (!pressEdge || input_.grabbing || isGrabbing_ || isOverchargeDashing_) {
			return;
		}
		if (CanStartSurfaceParry()) {
			isParrying_ = true;
			parryIsSurface_ = true;
			parryTimer_ = k::SURFACE_PARRY_WINDOW;
			parrySucceededThisWindow_ = false;
			parryCooldown_ = k::PARRY_COOLDOWN_DURATION;
			parryEndlag_ = 0.0f;
			Emit(EventType::ParryStart, 0.0f, 0.0f, false, static_cast<int>(ParryKind::Surface));
		} else if (CanStartStandingParry()) {
			isParrying_ = true;
			parryIsSurface_ = false;
			parryTimer_ = k::PARRY_WINDOW;
			parrySucceededThisWindow_ = false;
			parryCooldown_ = k::PARRY_COOLDOWN_DURATION;
			Emit(EventType::ParryStart, 0.0f, 0.0f, false, static_cast<int>(ParryKind::Standing));
			if (input_.pitch <= k::GROUND_PARRY_LOOK_DOWN_PITCH) {
				if (!TryApplyStandingGroundSelfParryBounce()) {
					standingGroundSelfParryBufferTimer_ = k::STANDING_GROUND_SELF_PARRY_BUFFER;
				}
			}
		}
	}

	void Sim::OnRocketAscentParried(const Vec3& a_away)
	{
		isRocketPunchAscending_ = false;
		vel_ += a_away * (2500.0f / k::RAGDOLL_MASS);
	}

	// ------------------------------------------------------------------ grab

	void Sim::StartGrabLunge()
	{
		grabCritical_ = chargeTime_ >= k::CRITICAL_FIST_CHARGE_TIME;
		ClearNormalPunchCharge();
		isGrabbing_ = true;
		grabTimer_ = k::PLAYER_GRAB_SWIPE_DURATION;
		grabYaw_ = input_.yaw;
		grabStartedAirborne_ = IsAirborneForFullChargePunch();
		grabInputBufferTimer_ = 0.0f;
		isRocketPunchAscending_ = false;
		inRocketPunchFlight_ = false;
		StopLandingSlide();
		if (!grabStartedAirborne_) {
			vel_.z = std::max(vel_.z, 0.0f) + k::PLAYER_GRAB_LUNGE_GROUND_UP;
			takeoffGraceTimer_ = 0.15f;
		}
		Emit(EventType::GrabStart, 0.0f, 0.0f, grabStartedAirborne_);
	}

	void Sim::EndGrab(bool a_cooldown)
	{
		isGrabbing_ = false;
		isHoldingGrabbed_ = false;
		grabTimer_ = 0.0f;
		grabHoldTimer_ = 0.0f;
		if (a_cooldown) {
			grabCooldown_ = k::PLAYER_GRAB_COOLDOWN;
		}
	}

	void Sim::OnGrabCaught()
	{
		if (!isGrabbing_) {
			return;
		}
		isGrabbing_ = false;
		isHoldingGrabbed_ = true;
		grabHoldTimer_ = 0.0f;
		if (!grabStartedAirborne_) {
			vel_.x = vel_.y = 0.0f;
		}
	}

	void Sim::OnGrabBroken()
	{
		if (isGrabbing_ || isHoldingGrabbed_) {
			EndGrab(true);
		}
		isPiledriving_ = false;
	}

	void Sim::ThrowGrabbed()
	{
		const bool piledriver = grabStartedAirborne_ && input_.pitch <= -kPi / 2.0f + 0.1f && IsAirborneForFullChargePunch();
		isHoldingGrabbed_ = false;
		grabHoldTimer_ = 0.0f;
		throwTimer_ = k::PLAYER_GRAB_THROW_DURATION;
		grabCooldown_ = k::PLAYER_GRAB_COOLDOWN;
		Event& e = EmitEvent(EventType::GrabThrow);
		Vec3 dir = AimDirection(input_.yaw, input_.pitch);
		dir.z += 0.3f;
		e.direction = dir.Normalized();
		e.value = k::PLAYER_GRAB_THROW_FORCE * (grabCritical_ ? 3.0f : 1.0f);
		e.critical = grabCritical_;
		e.flag = piledriver;
		if (piledriver) {
			isPiledriving_ = true;
			piledriverTimer_ = 0.0f;
		}
	}

	void Sim::UpdateGrab(float a_dt)
	{
		const bool grabEdge = input_.grabbing && !previousGrabInput_;
		const bool parryEdgeAlone = input_.parrying && !previousParryInput_ && !input_.grabbing;
		if (grabEdge) {
			grabInputBufferTimer_ = k::PLAYER_GRAB_INPUT_BUFFER_DURATION;
		}

		if (isPiledriving_) {
			return;
		}
		if (isHoldingGrabbed_) {
			grabHoldTimer_ += a_dt;
			if (grounded_) {
				vel_.x = vel_.y = 0.0f;
			}
			if (grabHoldTimer_ >= k::PLAYER_GRAB_HOLD_DURATION) {
				ThrowGrabbed();
			}
			return;
		}
		if (!isGrabbing_ && grabInputBufferTimer_ > 0.0f && grabCooldown_ <= 0.0f && grabSuppressTimer_ <= 0.0f &&
		    !isParrying_ && parryEndlag_ <= 0.0f && !isOverchargeDashing_ && !isRocketPunchDiving_ &&
		    !isAirKickDashing_ && knockbackTimer_ <= 0.0f) {
			StartGrabLunge();
		}
		if (isGrabbing_) {
			if (parryEdgeAlone) {
				vel_.x *= 0.3f;
				vel_.y *= 0.3f;
				EndGrab(true);
				Emit(EventType::GrabCancel);
				return;
			}
			grabTimer_ -= a_dt;
			if (grabStartedAirborne_) {
				const Vec3 d = AimDirection(grabYaw_, Clamp(input_.pitch, -0.6f, 0.9f));
				vel_ = d * k::PLAYER_GRAB_LUNGE_SPEED;
			} else {
				const Vec3 f = YawForward(grabYaw_);
				vel_.x = f.x * k::PLAYER_GRAB_LUNGE_SPEED;
				vel_.y = f.y * k::PLAYER_GRAB_LUNGE_SPEED;
			}
			if (grabTimer_ <= 0.0f) {
				EndGrab(true);
				Emit(EventType::GrabMiss);
			}
			return;
		}

	}

	// ------------------------------------------------------------------ sim-owned ragdoll

	void Sim::EnterRagdoll(float a_duration, bool a_hardKnockdown, float a_hardKnockdownSeconds)
	{
		// A ragdoll entered after teching out of a hard knockdown, before landing, is hard again.
		const bool techedFromHardKnockdown = airTechFromHardKnockdown_ && !grounded_;
		ClearNormalPunchCharge();
		ClearSelfParryInstantCharge();
		pendingTapPunchCount_ = 0;
		if (isAirKickDashing_) StopAirKickDash();
		if (isOverchargeDashing_) StopOverchargeDash(false);
		if (isGrabbing_ || isHoldingGrabbed_) EndGrab(true);
		isPiledriving_ = false;
		isRocketPunchDiving_ = false;
		isRocketPunchAscending_ = false;
		inRocketPunchFlight_ = false;
		dashCancelMomentumActive_ = false;
		isParrying_ = false;
		parryTimer_ = 0.0f;
		StopLandingSlide();

		const bool hard = a_hardKnockdown || techedFromHardKnockdown;
		isLimp_ = true;
		limpTimer_ = a_duration;
		ragdollElapsed_ = 0.0f;
		airTechUsed_ = false;
		airTechInputLatched_ = false;
		midairParryUsedThisRagdoll_ = false;
		midairParryWindowTimer_ = 0.0f;
		hardKnockdownTimer_ = hard ? a_hardKnockdownSeconds : 0.0f;
		enteredHardKnockdown_ = hard;
		ragdollDamageSuppressed_ = false;
		Emit(EventType::Ragdoll, 0.0f, a_duration, hard);
	}

	void Sim::UpdateLimp(float a_dt)
	{
		ragdollElapsed_ += a_dt;
		limpTimer_ -= a_dt;

		const bool hardKnockdown = hardKnockdownTimer_ > 0.0f;

		// Ragdoll wall parry.
		if (input_.parrying && !previousParryInput_ && !hardKnockdown && !midairParryUsedThisRagdoll_) {
			midairParryWindowTimer_ = k::MIDAIR_PARRY_WINDOW;
			midairParryUsedThisRagdoll_ = true;
		}

		// Air tech: grab combo + a movement direction, or on release with none.
		auto executeAirTech = [this](bool a_hasDirection, float a_dirX, float a_dirY) {
			vel_ = vel_ * k::AIRTECH_MOMENTUM_RETENTION;
			if (a_hasDirection) {
				vel_.x += a_dirX * k::AIRTECH_DASH_SPEED;
				vel_.y += a_dirY * k::AIRTECH_DASH_SPEED;
			}
			vel_.z = std::max(k::AIRTECH_UPWARD_SPEED, vel_.z);
			isLimp_ = false;
			airTechUsed_ = true;
			airTechInputLatched_ = false;
			airTechFromHardKnockdown_ = enteredHardKnockdown_;
			airTechLaunchTimer_ = k::AIRTECH_LAUNCH_DURATION;
			airTechInvulnTimer_ = k::AIRTECH_INVINCIBILITY_DURATION;
			grabSuppressTimer_ = k::AIRTECH_GRAB_SUPPRESS_DURATION;
			takeoffGraceTimer_ = k::GROUND_PUNCH_HOP_TAKEOFF_GRACE;
			hardKnockdownTimer_ = 0.0f;
			StartFlip();
			Emit(EventType::AirTech);
			Emit(EventType::Recover);
		};
		if (!airTechUsed_ && !hardKnockdown) {
			if (input_.grabbing) {
				const Vec3 forward = YawForward(input_.yaw);
				const Vec3 right = YawRight(input_.yaw);
				const float moveX = input_.moveX * right.x + input_.moveY * forward.x;
				const float moveY = input_.moveX * right.y + input_.moveY * forward.y;
				const float magnitude = std::hypot(moveX, moveY);
				if (magnitude > 0.1f) {
					executeAirTech(true, moveX / magnitude, moveY / magnitude);
					return;
				}
				airTechInputLatched_ = true;
			} else if (airTechInputLatched_) {
				executeAirTech(false, 0.0f, 0.0f);
				return;
			}
		}

		// Ballistic body with simple contact response.
		const bool floorSupport = env_.groundSupport;
		if (env_.contact || floorSupport) {
			const Vec3 n = env_.contact ? env_.contactNormal : env_.groundNormal;
			const float vn = vel_.Dot(n);
			if (vn < 0.0f) {
				const float impactSpeed = -vn;
				const bool isWall = n.z <= 0.7f;
				if (midairParryWindowTimer_ > 0.0f && isWall && impactSpeed >= k::MIDAIR_PARRY_MIN_VELOCITY) {
					vel_ = vel_ - n * (2.0f * vn);
					isLimp_ = false;
					hardKnockdownTimer_ = 0.0f;
					midairParryInvulnTimer_ = std::max(midairParryInvulnTimer_, 0.12f);
					StartFlip();
					BeginMomentumFlight(std::hypot(vel_.x, vel_.y));
					MarkParrySuccessful(ParryKind::RagdollWall);
					Emit(EventType::Recover);
					return;
				}
				if (impactSpeed > k::COLLISION_DAMAGE_MIN_SPEED && collisionDamageCooldown_ <= 0.0f && airTechInvulnTimer_ <= 0.0f &&
				    !ragdollDamageSuppressed_) {
					collisionDamageCooldown_ = k::COLLISION_DAMAGE_COOLDOWN;
					Event& e = EmitEvent(EventType::RagdollImpact);
					e.value = std::pow(impactSpeed - k::COLLISION_DAMAGE_MIN_SPEED, 1.5f) * k::COLLISION_DAMAGE_SCALE;
					e.strength = impactSpeed;
					e.position = env_.contact ? env_.contactPoint : env_.position;
					e.direction = n;
				}
				vel_ = vel_ - n * (1.1f * vn);  // restitution 0.1
			}
		}
		if (floorSupport) {
			// friction 0.3 against the floor plus the ragdoll's linear damping
			const float speed = std::hypot(vel_.x, vel_.y);
			if (speed > 0.0001f) {
				const float decel = (0.3f * std::abs(env_.gravity) + 0.1f * speed) * a_dt;
				const float scale = std::max(0.0f, speed - decel) / speed;
				vel_.x *= scale;
				vel_.y *= scale;
			}
			if (vel_.z < 0.0f) vel_.z = 0.0f;
		} else {
			vel_.z += env_.gravity * a_dt;
		}

		const float speed = vel_.Length();
		if ((limpTimer_ <= 0.0f && hardKnockdownTimer_ <= 0.0f && speed < k::RAGDOLL_RECOVERY_SPEED) || ragdollElapsed_ > 12.0f) {
			isLimp_ = false;
			if (!grounded_) {
				vel_.x = Clamp(vel_.x, -12.0f, 12.0f);
				vel_.y = Clamp(vel_.y, -12.0f, 12.0f);
			} else {
				airTechFromHardKnockdown_ = false;
			}
			Emit(EventType::Recover);
		}
	}
}
