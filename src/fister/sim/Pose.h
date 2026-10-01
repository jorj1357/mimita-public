#pragma once

#include "FisterSim.h"
#include "Ragdoll.h"

// Port of the procedural poses in packages/fister-client/src/graphics/PlayerMesh.ts and
// WalkingGait.ts. Output is in Fister avatar space exactly as the original writes it:
// Y up, the avatar faces +Z, mesh "right" parts sit at +X, rotations are Euler XYZ radians
// about each part's pivot, positions are in avatarVisualRoot.
namespace fister
{
	enum Part : int { kTorso = 0, kHead, kLeftArm, kRightArm, kLeftLeg, kRightLeg, kPartCount };

	struct PartPose
	{
		Vec3 position{};
		Vec3 rotation{};
		float scale{ 1.0f };
		// When set, `matrix` (columns are the part's X, Y, Z axes in avatar space) replaces `rotation`.
		bool hasMatrix{ false };
		float matrix[3][3]{};
	};

	struct Tint
	{
		float r{ 0.0f };
		float g{ 0.0f };
		float b{ 0.0f };
		float emissive{ 0.0f };  // 0 = untinted
		float blend{ 1.0f };     // how far the part's own colour is replaced by the tint (charging arm: the charge)
	};

	struct Pose
	{
		PartPose parts[kPartCount]{};
		Tint tints[kPartCount]{};
		// Root (the `mesh` group): Euler XYZ applied after the actor's own yaw.
		Vec3 rootRotation{};
		bool rootOverridesYaw{ false };  // air kick: rootRotation.y is an absolute heading
		float flipAngle{ 0.0f };         // about avatarFlipPivot (0.75 above the root origin)
		bool shirtHidden{ false };
	};

	struct PoseInput
	{
		float dt{ 0.0f };
		float time{ 0.0f };
		float yaw{ 0.0f };
		float pitch{ 0.0f };
		Vec3 velocity{};
		// World placement, for the ragdoll.
		Vec3 position{};  // body centre
		bool forceRagdoll{ false };  // the host engine has the player ragdolled or dead: go limp as well
		bool hasGround{ false };
		float groundZ{ 0.0f };
		bool hasWall{ false };
		Vec3 wallPoint{};
		Vec3 wallNormal{};
	};

	class PoseAnimator
	{
	public:
		void Reset();
		void TriggerPunch(float a_strength, float a_pitch);
		void TriggerChargeReady() { chargeReadyFlash_ = 0.3f; }
		void TriggerParryReady() { parryReadyFlash_ = 0.4f; }
		// White "impact frame" held on the avatar for 35-90 ms (the original's stand-in for hit-stop).
		void TriggerImpactFrame(float a_seconds)
		{
			impactFrameDuration_ = Clamp(a_seconds, 0.035f, 0.09f);
			impactFrameTimer_ = impactFrameDuration_;
		}
		// Red body flash for 10 frames when health drops by 2 or more.
		void TriggerDamageFlash() { damageFlashFrames_ = 10.0f; }
		[[nodiscard]] Pose Update(const Sim& a_sim, const PoseInput& a_input);
		[[nodiscard]] float GaitPhase() const { return phase_; }

	private:
		[[nodiscard]] Pose Build(const Sim& a_sim, const PoseInput& a_input);
		void ResetParts(Pose& a_pose) const;
		void UpdateGait(float a_dt, float a_speed, float a_forward, float a_sideways);
		void ApplyGroundMovement(Pose& a_pose, const Sim& a_sim, const PoseInput& a_input, bool a_charging);

		// WalkingGait
		float phase_{ 0.0f };
		float cadence_{ 0.0f };
		float forwardStride_{ 0.0f };
		float sideStride_{ 0.0f };
		float swingX_{ 0.0f };
		float swingZ_{ 0.0f };

		float punchTimer_{ 0.0f };
		float punchStrength_{ 0.0f };
		float punchPitch_{ 0.0f };
		float chargeReadyFlash_{ 0.0f };
		float parryReadyFlash_{ 0.0f };
		float lastAirKickYaw_{ 0.0f };
		float lastParryCooldown_{ 0.0f };
		float impactFrameTimer_{ 0.0f };
		float impactFrameDuration_{ 0.0f };
		float damageFlashFrames_{ 0.0f };
		Ragdoll ragdoll_;
	};
}
