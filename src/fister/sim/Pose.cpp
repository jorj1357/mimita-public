#include "Pose.h"

namespace fister
{
	namespace
	{
		constexpr float kTwoPi = kPi * 2.0f;

		constexpr Tint MakeTint(unsigned a_hex, float a_emissive)
		{
			return Tint{ static_cast<float>((a_hex >> 16) & 0xFF) / 255.0f, static_cast<float>((a_hex >> 8) & 0xFF) / 255.0f,
				static_cast<float>(a_hex & 0xFF) / 255.0f, a_emissive };
		}

		void TintAll(Pose& a_pose, const Tint& a_tint)
		{
			for (auto& t : a_pose.tints) t = a_tint;
		}
	}

	void PoseAnimator::Reset()
	{
		*this = PoseAnimator{};
	}

	void PoseAnimator::TriggerPunch(float a_strength, float a_pitch)
	{
		punchTimer_ = 1.0f;
		punchStrength_ = a_strength;
		punchPitch_ = a_pitch;
	}

	void PoseAnimator::ResetParts(Pose& a_pose) const
	{
		a_pose.parts[kTorso].position = { 0.0f, 0.5f, 0.0f };
		a_pose.parts[kHead].position = { 0.0f, 1.3f, 0.0f };
		a_pose.parts[kLeftArm].position = { -0.7f, 1.0f, 0.0f };
		a_pose.parts[kRightArm].position = { 0.7f, 1.0f, 0.0f };
		a_pose.parts[kLeftLeg].position = { -0.25f, 0.0f, 0.0f };
		a_pose.parts[kRightLeg].position = { 0.25f, 0.0f, 0.0f };
	}

	void PoseAnimator::UpdateGait(float a_dt, float a_speed, float a_forward, float a_sideways)
	{
		constexpr float RESPONSE = 14.0f;
		const float step = Clamp(a_dt, 0.0f, 0.1f);
		const float safeSpeed = std::max(0.0f, a_speed);
		const float speedRatio = std::min(2.0f, safeSpeed / k::BASE_WALK_SPEED);
		const float moving = Clamp01((safeSpeed - 0.12f) / 0.8f);
		const float targetCadence = moving > 0.0f ? kTwoPi * (0.9f + speedRatio * 0.85f) : 0.0f;
		const float stride = moving * std::min(1.05f, 0.35f + speedRatio * 0.25f);
		const float targetForward = stride * Clamp(a_forward, -1.0f, 1.0f);
		const float targetSide = stride * 0.65f * Clamp(a_sideways, -1.0f, 1.0f);
		const float decay = std::exp(-RESPONSE * step);

		phase_ += targetCadence * step + (cadence_ - targetCadence) * (1.0f - decay) / RESPONSE;
		cadence_ = targetCadence + (cadence_ - targetCadence) * decay;
		forwardStride_ = targetForward + (forwardStride_ - targetForward) * decay;
		sideStride_ = targetSide + (sideStride_ - targetSide) * decay;
		const float swing = std::sin(phase_);
		swingX_ = swing * forwardStride_;
		swingZ_ = swing * sideStride_;
		if (moving == 0.0f && std::abs(forwardStride_) + std::abs(sideStride_) < 0.001f) {
			phase_ = cadence_ = forwardStride_ = sideStride_ = swingX_ = swingZ_ = 0.0f;
		}
	}

	void PoseAnimator::ApplyGroundMovement(Pose& a_pose, const Sim& a_sim, const PoseInput& a_input, bool a_charging)
	{
		auto& p = a_pose.parts;
		const float speed = std::hypot(a_input.velocity.x, a_input.velocity.y);
		const float inverseSpeed = 1.0f / std::max(0.001f, speed);
		const Vec3 f = YawForward(a_input.yaw);
		const Vec3 r = YawRight(a_input.yaw);
		const float forward = (a_input.velocity.x * f.x + a_input.velocity.y * f.y) * inverseSpeed;
		const float sideways = (a_input.velocity.x * r.x + a_input.velocity.y * r.y) * inverseSpeed;
		UpdateGait(a_input.dt, a_sim.IsSliding() ? 0.0f : speed, forward, sideways);
		if (a_sim.IsSliding() && speed > 0.1f) {
			p[kTorso].position.y = 0.42f;
			p[kTorso].rotation.x = -forward * 0.2f;
			p[kTorso].rotation.z = sideways * 0.14f;
			p[kHead].position.y = 1.22f;
			p[kHead].rotation.x = forward * 0.08f;
			p[kHead].rotation.z = sideways * 0.08f;
			p[kLeftArm].rotation.x = 0.35f * forward - 0.16f * sideways;
			p[kLeftArm].rotation.z = 0.22f + sideways * 0.12f;
			p[kRightArm].rotation.x += 0.35f * forward + 0.16f * sideways;
			p[kRightArm].rotation.z += -0.22f + sideways * 0.12f;
			p[kLeftLeg].rotation = { -0.28f * forward, 0.0f, 0.08f + sideways * 0.08f };
			p[kRightLeg].rotation = { -0.12f * forward, 0.0f, -0.08f + sideways * 0.08f };
		} else {
			p[kLeftLeg].rotation = { swingX_, 0.0f, swingZ_ };
			p[kRightLeg].rotation = { -swingX_, 0.0f, swingZ_ };
			p[kLeftLeg].position.y = std::max(0.0f, swingZ_) * 0.2f;
			p[kRightLeg].position.y = std::max(0.0f, -swingZ_) * 0.2f;
			if (!a_charging) {
				p[kLeftArm].rotation.x = -swingX_ * 0.625f;
				p[kRightArm].rotation.x = swingX_ * 0.625f;
			}
		}
	}

	Pose PoseAnimator::Build(const Sim& a_sim, const PoseInput& a_input)
	{
		Pose pose{};
		ResetParts(pose);
		auto& p = pose.parts;
		const float t = a_input.time;
		const float frames = a_input.dt * 60.0f;

		chargeReadyFlash_ = std::max(0.0f, chargeReadyFlash_ - a_input.dt);
		parryReadyFlash_ = std::max(0.0f, parryReadyFlash_ - a_input.dt);
		if (lastParryCooldown_ > 0.0f && a_sim.ParryCooldown() <= 0.0f) {
			parryReadyFlash_ = 0.4f;
		}
		lastParryCooldown_ = a_sim.ParryCooldown();

		// Acrobatic flip: one full front flip in 0.5 s.
		if (a_sim.FlipTimer() > 0.0f) {
			const float progress = 1.0f - std::min(1.0f, a_sim.FlipTimer() / k::PLAYER_ACROBATIC_FLIP_DURATION);
			pose.flipAngle = progress * k::PLAYER_ACROBATIC_FLIP_DURATION * 2.0f * kTwoPi;
		}

		if (a_sim.IsLimp() || a_input.forceRagdoll) {
			// Six loose parts, as the original's physics ragdoll: simulate the joints and place each box on them.
			punchTimer_ = 0.0f;
			if (!ragdoll_.IsActive()) {
				ragdoll_.Start(a_input.position, a_input.yaw, a_input.velocity);
			}
			Ragdoll::Surroundings surroundings;
			surroundings.centre = a_input.position;
			surroundings.airborne = !a_sim.IsGrounded();
			surroundings.hasGround = a_input.hasGround;
			surroundings.groundZ = a_input.groundZ;
			surroundings.hasWall = a_input.hasWall;
			surroundings.wallPoint = a_input.wallPoint;
			surroundings.wallNormal = a_input.wallNormal;
			ragdoll_.Step(a_input.dt, surroundings);

			const Vec3 forward = YawForward(a_input.yaw);
			const Vec3 right = YawRight(a_input.yaw);
			const Vec3 origin{ a_input.position.x, a_input.position.y, a_input.position.z + 0.2f };
			// World -> avatar space (x toward the mesh's +X side, y up, z forward).
			auto toLocalDirection = [&](const Vec3& d) { return Vec3{ -d.Dot(right), d.z, d.Dot(forward) }; };
			auto local = [&](Ragdoll::Point a_point) { return toLocalDirection(ragdoll_.Position(a_point) - origin); };
			auto cross = [](const Vec3& a, const Vec3& b) {
				return Vec3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
			};
			auto setFrame = [&](PartPose& a_part, const Vec3& a_position, const Vec3& a_up, const Vec3& a_sideHint) {
				const Vec3 y = a_up.Normalized();
				Vec3 x = a_sideHint - y * a_sideHint.Dot(y);
				if (x.Length() < 1e-3f) {
					x = std::abs(y.x) < 0.9f ? Vec3{ 1.0f, 0.0f, 0.0f } : Vec3{ 0.0f, 0.0f, 1.0f };
					x = x - y * x.Dot(y);
				}
				x = x.Normalized();
				const Vec3 z = cross(x, y);
				a_part.position = a_position;
				a_part.hasMatrix = true;
				a_part.matrix[0][0] = x.x; a_part.matrix[1][0] = x.y; a_part.matrix[2][0] = x.z;
				a_part.matrix[0][1] = y.x; a_part.matrix[1][1] = y.y; a_part.matrix[2][1] = y.z;
				a_part.matrix[0][2] = z.x; a_part.matrix[1][2] = z.y; a_part.matrix[2][2] = z.z;
			};
			const Vec3 pelvis = local(Ragdoll::kPelvis), neck = local(Ragdoll::kNeck);
			const Vec3 shoulderL = local(Ragdoll::kShoulderL), shoulderR = local(Ragdoll::kShoulderR);
			const Vec3 side = shoulderR - shoulderL;
			setFrame(p[kTorso], (pelvis + neck) * 0.5f, neck - pelvis, side);
			const Vec3 headUp = (local(Ragdoll::kHeadTop) - neck).Normalized();
			setFrame(p[kHead], neck + headUp * 0.3f, headUp, side);
			setFrame(p[kLeftArm], shoulderL, shoulderL - local(Ragdoll::kHandL), side);
			setFrame(p[kRightArm], shoulderR, shoulderR - local(Ragdoll::kHandR), side);
			const Vec3 hipL = local(Ragdoll::kHipL), hipR = local(Ragdoll::kHipR);
			setFrame(p[kLeftLeg], hipL, hipL - local(Ragdoll::kFootL), side);
			setFrame(p[kRightLeg], hipR, hipR - local(Ragdoll::kFootR), side);
			if (a_sim.IsLimp() && a_sim.IsHardKnockdown()) {
				TintAll(pose, MakeTint(0xff6600, 1.0f));
			}
			return pose;
		}
		ragdoll_.Stop();

		if (a_sim.IsRocketPunchDiving()) {
			punchTimer_ = 0.0f;
			p[kTorso].position.y = 0.5f;
			p[kTorso].rotation.x = kPi + 0.15f;
			p[kHead].position = { 0.0f, -0.3f, 0.15f };
			p[kHead].rotation.x = kPi + 0.2f;
			p[kRightArm].position = { 0.15f, -1.0f, 0.2f };
			p[kRightArm].rotation.x = kPi + 0.1f;
			p[kLeftArm].position = { -0.4f, 0.6f, -0.3f };
			p[kLeftArm].rotation = { -0.3f, 0.0f, 0.2f };
			p[kLeftLeg].position = { -0.25f, 1.8f, 0.0f };
			p[kLeftLeg].rotation = { -0.15f, 0.0f, -0.15f };
			p[kRightLeg].position = { 0.25f, 1.8f, 0.0f };
			p[kRightLeg].rotation = { -0.15f, 0.0f, 0.15f };
			TintAll(pose, MakeTint(0xff0000, 2.0f));
			return pose;
		}

		if (a_sim.IsPiledriving()) {
			punchTimer_ = 0.0f;
			p[kTorso].position.y = 0.45f;
			p[kTorso].rotation.x = 0.9f + 0.45f;
			p[kHead].position = { 0.0f, 1.05f, 0.18f };
			p[kHead].rotation.x = 0.45f;
			p[kRightArm].position = { 0.35f, 0.35f, 0.7f };
			p[kRightArm].rotation = { -2.35f, 0.0f, -0.15f };
			p[kLeftArm].position = { -0.35f, 0.35f, 0.7f };
			p[kLeftArm].rotation = { -2.35f, 0.0f, 0.15f };
			p[kLeftLeg].rotation.x = 0.45f;
			p[kRightLeg].rotation.x = 0.45f;
			TintAll(pose, MakeTint(0xff0000, 1.8f));
			return pose;
		}

		if (a_sim.IsAirKickDashing()) {
			punchTimer_ = 0.0f;
			const Vec3 d = a_sim.AirKickDirection();
			const float horizontal = std::hypot(d.x, d.y);
			if (horizontal > 0.0001f) {
				lastAirKickYaw_ = std::atan2(d.x, d.y);
			}
			pose.rootOverridesYaw = true;
			pose.rootRotation = { -std::atan2(d.z, horizontal), lastAirKickYaw_, 0.0f };
			p[kTorso].position = { 0.0f, 0.32f, 0.2f };
			p[kTorso].rotation = { -0.42f, 0.0f, -0.08f };
			p[kHead].position = { 0.0f, 0.92f, 0.3f };
			p[kHead].rotation = { 0.24f, 0.0f, 0.04f };
			p[kRightArm].position = { 0.48f, 0.76f, 0.16f };
			p[kRightArm].rotation = { 0.18f, 0.0f, -0.62f };
			p[kLeftArm].position = { -0.46f, 0.8f, 0.08f };
			p[kLeftArm].rotation = { -0.04f, 0.0f, 0.42f };
			p[kRightLeg].position = { 0.22f, -0.16f, 0.62f };
			p[kRightLeg].rotation = { -1.55f, 0.0f, -0.08f };
			p[kLeftLeg].position = { -0.22f, 0.14f, -0.18f };
			p[kLeftLeg].rotation = { 0.78f, 0.0f, 0.18f };
			TintAll(pose, MakeTint(0xffb347, 1.7f));
			return pose;
		}
		lastAirKickYaw_ = a_input.yaw;

		if (a_sim.IsOverchargeDashing()) {
			punchTimer_ = 0.0f;
			const float dashSpeed = std::hypot(a_input.velocity.x, a_input.velocity.y);
			const float spin = t * (30.0f + std::min(36.0f, dashSpeed * 0.16f));
			const float pulse = 0.65f + std::abs(std::sin(t * 18.0f)) * 0.55f;
			TintAll(pose, Tint{ 1.0f, 0.08f + pulse * 0.18f, 0.02f, 2.0f + pulse * 1.4f });
			p[kTorso].position = { 0.0f, 0.42f, -0.08f };
			p[kTorso].rotation = { -0.18f, 0.0f, std::sin(spin * 0.25f) * 0.05f };
			p[kHead].position = { 0.0f, 1.18f, 0.05f };
			p[kHead].rotation = { 0.12f, 0.0f, -std::sin(spin * 0.25f) * 0.04f };
			p[kRightArm].position = { 0.68f, 0.99f, -0.08f };
			p[kRightArm].rotation = { 1.52f, 0.08f, 0.42f };
			p[kLeftArm].position = { -0.68f, 0.99f, -0.08f };
			p[kLeftArm].rotation = { 1.52f, -0.08f, -0.42f };
			p[kRightLeg].position = { 0.28f, -0.08f, 0.02f };
			p[kRightLeg].rotation = { spin, 0.0f, -0.1f };
			p[kLeftLeg].position = { -0.28f, -0.08f, 0.02f };
			p[kLeftLeg].rotation = { spin + kPi, 0.0f, 0.1f };
			return pose;
		}

		if (a_sim.IsHoldingGrabbed()) {
			punchTimer_ = 0.0f;
			p[kRightArm].position = { 0.5f, 1.3f, 0.2f };
			p[kRightArm].rotation = { -0.85f * kPi, 0.0f, -0.2f };
			p[kLeftArm].position = { -0.5f, 1.2f, 0.2f };
			p[kLeftArm].rotation = { -0.8f * kPi, 0.0f, 0.2f };
			p[kTorso].rotation.x = -0.1f;
			p[kHead].rotation.x = -0.2f;
			pose.tints[kLeftArm] = pose.tints[kRightArm] = MakeTint(0xff6600, 1.5f);
			return pose;
		}

		if (a_sim.IsThrowing()) {
			punchTimer_ = 0.0f;
			const float tp = a_sim.ThrowProgress();
			const float armRot = -0.85f * kPi + tp * kPi;
			p[kRightArm].position = { 0.5f, 1.3f - 0.5f * tp, 0.2f + 0.8f * tp };
			p[kRightArm].rotation = { armRot, 0.0f, -0.2f };
			p[kLeftArm].position = { -0.5f, 1.2f - 0.5f * tp, 0.2f + 0.8f * tp };
			p[kLeftArm].rotation = { armRot, 0.0f, 0.2f };
			p[kTorso].rotation.x = 0.4f * tp;
			pose.tints[kLeftArm] = pose.tints[kRightArm] = MakeTint(0xff2200, 2.0f);
			return pose;
		}

		if (a_sim.IsGrabbing()) {
			punchTimer_ = 0.0f;
			const float progress = a_sim.GrabProgress();
			const float grabExtend = std::sin(progress * kPi) * 1.2f;
			p[kRightArm].position = { 0.4f, 1.0f, 0.5f + grabExtend };
			p[kRightArm].rotation = { -kPi / 2.0f - 0.3f, 0.0f, -0.2f };
			p[kLeftArm].position = { -0.4f, 1.0f, 0.5f + grabExtend };
			p[kLeftArm].rotation = { -kPi / 2.0f - 0.3f, 0.0f, 0.2f };
			p[kTorso].rotation.x = progress * 0.3f;
			TintAll(pose, MakeTint(0xffff00, 1.0f));
			return pose;
		}

		// ---- normal stance: idle / walk / slide, with charge and punch layered on the right arm ----
		const bool charging = a_sim.Charge() > 0.0f && punchTimer_ <= 0.0f;
		const bool railingCharge = a_sim.IsOverchargeDashCharging() && a_sim.Charge() > 0.0f;

		if (punchTimer_ > 0.0f) {
			const float strength = punchStrength_;
			float extension = 0.0f;
			if (punchTimer_ > 0.5f) {
				const float thrust = (1.0f - punchTimer_) / 0.5f;  // 0 -> 1
				extension = Lerp(-0.5f * strength, 1.5f, thrust);
			} else {
				extension = 1.5f * (punchTimer_ / 0.5f);
			}
			p[kRightArm].position = { 0.7f, 1.0f, extension };
			p[kRightArm].rotation.x = -kPi / 2.0f - punchPitch_;
			pose.tints[kRightArm] = Tint{ 1.0f, 0.0f, 0.0f, 1.5f * strength, strength };
			punchTimer_ = std::max(0.0f, punchTimer_ - (0.03f + strength * 0.05f) * frames);
		} else if (charging && !railingCharge) {
			const float charge = a_sim.Charge();
			p[kRightArm].position = { 0.7f, 1.0f, -charge * 0.5f };
			p[kRightArm].rotation = { -kPi / 2.0f - a_input.pitch, 0.0f, 0.0f };
			pose.tints[kRightArm] = Tint{ 1.0f, 0.0f, 0.0f, charge * 1.5f, charge };
			const float chargeTime = a_sim.ChargeTime();
			if (chargeTime >= k::OVERCHARGE_WARNING_TIME) {
				const float blink = std::sin(t * kTwoPi * 6.0f) > 0.0f ? 3.0f : 0.6f;
				TintAll(pose, Tint{ 1.0f, 0.0f, 0.0f, blink });
			} else if (chargeTime >= k::CRITICAL_FIST_CHARGE_TIME) {
				const float blink = std::sin(t * kTwoPi * 4.0f) > 0.0f ? 2.2f : 0.8f;
				pose.tints[kRightArm] = Tint{ 1.0f, 0.0f, 0.0f, blink };
			} else if (chargeTime > k::MAX_CHARGE_TIME + 0.5f) {
				const float pulse = 0.5f + 0.5f * std::sin(t * 8.0f);
				pose.tints[kRightArm] = Tint{ 1.0f, 0.35f * (1.0f - pulse), 0.0f, 1.5f + pulse };
			}
		}

		if (railingCharge) {
			const float chargeTime = std::max(0.0f, a_sim.ChargeTime());
			const float ramp = Clamp01((chargeTime - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME) /
			                           (k::OVERCHARGE_DASH_MAX_CHARGE_TIME - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME));
			const float poseRamp = ramp * ramp * (3.0f - 2.0f * ramp);
			const float motionRamp = std::pow(ramp, 2.45f);
			const bool ready = chargeTime >= k::OVERCHARGE_DASH_READY_TIME;
			const float baseSpinSpeed = 1.35f + motionRamp * 34.0f;
			const float spin = t * (ready ? baseSpinSpeed * 1.35f : baseSpinSpeed);
			const float legSwing = 0.07f + poseRamp * 0.98f;
			const float armSwing = 0.28f + poseRamp * 1.28f;
			p[kTorso].position.y = 0.42f;
			p[kTorso].rotation.x = -0.08f - poseRamp * 0.12f;
			p[kHead].position.y = 1.22f;
			p[kRightArm].position = { 0.68f, 0.99f, -0.02f - poseRamp * 0.08f };
			p[kLeftArm].position = { -0.68f, 0.99f, -0.02f - poseRamp * 0.08f };
			p[kRightLeg].position = { 0.28f, -0.08f, 0.02f };
			p[kLeftLeg].position = { -0.28f, -0.08f, 0.02f };
			if (ready) {
				p[kRightLeg].rotation.x = spin;
				p[kLeftLeg].rotation.x = spin + kPi;
				p[kRightArm].rotation = { std::sin(spin + kPi) * 1.55f, 0.08f, 0.42f };
				p[kLeftArm].rotation = { std::sin(spin) * 1.55f, -0.08f, -0.42f };
			} else {
				const float pump = std::sin(spin) * legSwing;
				p[kRightLeg].rotation.x = pump;
				p[kLeftLeg].rotation.x = -pump;
				p[kRightArm].rotation = { std::sin(spin + kPi) * armSwing, 0.08f, 0.28f + poseRamp * 0.14f };
				p[kLeftArm].rotation = { std::sin(spin) * armSwing, -0.08f, -0.28f - poseRamp * 0.14f };
			}
			p[kRightLeg].rotation.z = -0.08f;
			p[kLeftLeg].rotation.z = 0.08f;
			const float pulse = 0.5f + 0.5f * std::sin(t * (6.0f + 18.0f * ramp));
			TintAll(pose, Tint{ 1.0f, 0.25f * (1.0f - ramp), 0.02f, (0.4f + 1.6f * ramp) * (0.6f + 0.4f * pulse) });
		} else {
			ApplyGroundMovement(pose, a_sim, a_input, charging || punchTimer_ > 0.0f);
		}

		// Flashes and state tints layered last.
		if (chargeReadyFlash_ > 0.0f) {
			pose.tints[kRightArm] = MakeTint(0xffff44, 3.0f * (chargeReadyFlash_ / 0.3f));
			p[kRightArm].scale = 1.0f + 0.15f * (chargeReadyFlash_ / 0.3f);
		} else if (a_sim.InstantChargeReady()) {
			// Instant-charge lightning: the arm flickers between gold and white.
			const float mix = 0.45f * std::abs(std::sin(18.0f * t));
			pose.tints[kRightArm] = Tint{ 1.0f, Lerp(0.847f, 1.0f, mix), Lerp(0.302f, 1.0f, mix),
				1.9f + 1.8f * (0.72f + 0.42f * std::abs(std::sin(12.5f * t))) };
		}
		if (parryReadyFlash_ > 0.0f) {
			pose.tints[kLeftArm] = MakeTint(0x3333ff, 3.0f * (parryReadyFlash_ / 0.4f));
			p[kLeftArm].scale = 1.0f + 0.15f * (parryReadyFlash_ / 0.4f);
		}
		if (a_sim.IsParrying()) {
			TintAll(pose, MakeTint(0x3333ff, 1.5f));
			pose.shirtHidden = true;
		}
		return pose;
	}

	Pose PoseAnimator::Update(const Sim& a_sim, const PoseInput& a_input)
	{
		Pose pose = Build(a_sim, a_input);
		const float t = a_input.time;
		constexpr float kTwoPiLocal = kPi * 2.0f;

		// Whole-body state tints, lowest priority first (PlayerMesh.ts; spec section 4.23).
		if (!a_sim.IsLimp() && !a_input.forceRagdoll) {
			const float chargeTime = a_sim.ChargeTime();
			const bool railingCharge = a_sim.IsOverchargeDashCharging() && a_sim.Charge() > 0.0f;
			if (a_sim.Charge() > 0.0f && !railingCharge) {
				if (chargeTime >= k::CRITICAL_FIST_CHARGE_TIME && std::sin(t * 4.0f * kTwoPiLocal) > 0.0f) {
					TintAll(pose, MakeTint(0xff0000, 1.8f));  // critical fist ready: 4 Hz blink
				}
				if (chargeTime >= k::OVERCHARGE_WARNING_TIME && std::sin(t * 10.5f * kTwoPiLocal) > 0.0f) {
					TintAll(pose, MakeTint(0xff0000, 3.0f));  // about to self-destruct: 10.5 Hz blink
				}
			}
			if (railingCharge) {
				const float q = std::abs(std::sin(18.0f * t));
				if (chargeTime >= k::OVERCHARGE_DASH_READY_TIME) {
					TintAll(pose, Tint{ 1.0f, 0.24f + 0.14f * q, 0.04f, 0.95f + 0.55f * q });
				} else {
					const float progress = Clamp01((chargeTime - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME) /
												   (k::OVERCHARGE_DASH_MAX_CHARGE_TIME - k::OVERCHARGE_DASH_RELEASE_MIN_CHARGE_TIME));
					const float cr = std::pow(progress, 2.35f);
					TintAll(pose, Tint{ 1.0f, 0.42f - 0.14f * cr, 0.10f - 0.05f * cr,
						0.8f * cr + std::abs(std::sin(9.0f * t)) * (0.02f + 0.13f * cr) + 0.001f });
				}
			}
		}
		if (damageFlashFrames_ > 0.0f) {
			damageFlashFrames_ -= a_input.dt * 60.0f;
			TintAll(pose, MakeTint(0xff3333, 1.0f));
		}
		if (impactFrameTimer_ > 0.0f) {
			const float progress = 1.0f - impactFrameTimer_ / std::max(1e-4f, impactFrameDuration_);
			impactFrameTimer_ -= a_input.dt;
			TintAll(pose, MakeTint(0xffffff, 1.9f - 0.65f * progress));
			pose.flipAngle = 0.0f;
		}
		if (a_sim.IsParrying()) {
			TintAll(pose, MakeTint(0x3333ff, 1.5f));
			pose.shirtHidden = true;
		}
		return pose;
	}
}
