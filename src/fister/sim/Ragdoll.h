#pragma once

#include "FisterMath.h"

// Loose-limbed ragdoll for the six-box Fister body (head, torso, two arms, two legs), standing in
// for shared/gameplay/RagdollBody.ts. Eleven verlet points joined by length constraints reproduce
// the same joints: neck, shoulders at the torso's top corners, hips at its bottom corners.
// World space, Fister units, Skyrim axes (Z up).
namespace fister
{
	class Ragdoll
	{
	public:
		enum Point : int
		{
			kPelvis = 0,
			kNeck,
			kHeadTop,
			kShoulderL,  // mesh "leftArm" side (-X in avatar space)
			kShoulderR,
			kHandL,
			kHandR,
			kHipL,
			kHipR,
			kFootL,
			kFootR,
			kPointCount
		};

		// a_centre is the body (capsule) centre; a_yaw the facing the body starts from.
		void Start(const Vec3& a_centre, float a_yaw, const Vec3& a_velocity);
		void Stop() { active_ = false; }
		[[nodiscard]] bool IsActive() const { return active_; }

		struct Surroundings
		{
			Vec3 centre{};      // where the owning body is this frame
			bool airborne{ true };
			bool hasGround{ false };
			float groundZ{ 0.0f };
			bool hasWall{ false };
			Vec3 wallPoint{};
			Vec3 wallNormal{};
			float gravity{ -9.82f };
		};
		void Step(float a_dt, const Surroundings& a_surroundings);

		[[nodiscard]] const Vec3& Position(Point a_point) const { return pos_[a_point]; }

	private:
		void SubStep(float a_dt, const Surroundings& a_surroundings, const Vec3& a_follow);

		struct Link
		{
			int a;
			int b;
			float rest;
			bool minimumOnly;  // only pushes apart (a joint limit), never pulls together
		};

		bool active_{ false };
		Vec3 pos_[kPointCount]{};
		Vec3 prev_[kPointCount]{};
		Vec3 lastCentre_{};
		float accumulator_{ 0.0f };
	};
}
