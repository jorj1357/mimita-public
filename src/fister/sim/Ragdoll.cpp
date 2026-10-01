#include "Ragdoll.h"

namespace fister
{
	namespace
	{
		constexpr float kStep = 1.0f / 120.0f;
		constexpr int kIterations = 6;
		constexpr float kPointRadius = 0.18f;  // about half a limb's thickness
		constexpr float kTorsoRadius = 0.25f;  // half the torso's depth

		// Avatar-space rest pose (x: -left/+right mesh side, y: up, z: forward), origin one unit above the feet.
		struct Rest
		{
			float x, y, z;
		};
		constexpr Rest kRest[Ragdoll::kPointCount] = {
			{ 0.0f, 0.0f, 0.0f },    // pelvis (torso bottom centre)
			{ 0.0f, 1.0f, 0.0f },    // neck (torso top centre)
			{ 0.0f, 1.6f, 0.0f },    // head top
			{ -0.7f, 1.0f, 0.0f },   // shoulders: arm pivots
			{ 0.7f, 1.0f, 0.0f },
			{ -0.7f, 0.0f, 0.0f },   // hands
			{ 0.7f, 0.0f, 0.0f },
			{ -0.25f, 0.0f, 0.0f },  // hips: leg pivots
			{ 0.25f, 0.0f, 0.0f },
			{ -0.25f, -1.0f, 0.0f }, // feet
			{ 0.25f, -1.0f, 0.0f },
		};

		[[nodiscard]] float RestDistance(int a, int b)
		{
			const float dx = kRest[a].x - kRest[b].x, dy = kRest[a].y - kRest[b].y, dz = kRest[a].z - kRest[b].z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		[[nodiscard]] bool IsTorsoPoint(int a_point)
		{
			return a_point == Ragdoll::kPelvis || a_point == Ragdoll::kNeck || a_point == Ragdoll::kShoulderL ||
			       a_point == Ragdoll::kShoulderR || a_point == Ragdoll::kHipL || a_point == Ragdoll::kHipR;
		}
	}

	void Ragdoll::Start(const Vec3& a_centre, float a_yaw, const Vec3& a_velocity)
	{
		const Vec3 forward = YawForward(a_yaw);
		const Vec3 right = YawRight(a_yaw);
		// Avatar +X (the mesh's "right" side) is the body's anatomical left, i.e. -right in the world.
		const Vec3 origin{ a_centre.x, a_centre.y, a_centre.z + 0.2f };  // centre is 0.8 above the feet, the avatar origin 1.0
		const float speed = a_velocity.Length();
		// Tumble forward over the travel direction, faster for harder hits.
		const float spin = std::min(9.0f, 2.5f + speed * 0.25f);
		const Vec3 travel = speed > 0.1f ? a_velocity * (1.0f / speed) : forward;
		for (int i = 0; i < kPointCount; ++i) {
			const Rest& r = kRest[i];
			pos_[i] = { origin.x - right.x * r.x + forward.x * r.z, origin.y - right.y * r.x + forward.y * r.z, origin.z + r.y };
			// Points above the torso centre lead, points below trail: a rotation about the horizontal axis
			// perpendicular to the travel direction.
			const float arm = r.y - 0.5f;
			const Vec3 tumble{ travel.x * spin * arm, travel.y * spin * arm, 0.0f };
			const Vec3 velocity = a_velocity + tumble;
			prev_[i] = pos_[i] - velocity * kStep;
		}
		lastCentre_ = a_centre;
		accumulator_ = 0.0f;
		active_ = true;
	}

	void Ragdoll::Step(float a_dt, const Surroundings& a_surroundings)
	{
		if (!active_) {
			return;
		}
		accumulator_ += std::min(a_dt, 0.1f);
		int steps = static_cast<int>(accumulator_ / kStep);
		if (steps <= 0) {
			return;
		}
		accumulator_ -= static_cast<float>(steps) * kStep;
		// The torso rides with the owning body; spread this frame's travel over the sub-steps.
		const Vec3 travel = a_surroundings.centre - lastCentre_;
		lastCentre_ = a_surroundings.centre;
		const Vec3 follow = travel * (1.0f / static_cast<float>(steps));
		for (int i = 0; i < steps; ++i) {
			SubStep(kStep, a_surroundings, follow);
		}
	}

	void Ragdoll::SubStep(float a_dt, const Surroundings& a_surroundings, const Vec3& a_follow)
	{
		static const Link links[] = {
			// torso: a rigid quad with its spine and both diagonals
			{ kPelvis, kNeck, RestDistance(kPelvis, kNeck), false },
			{ kNeck, kShoulderL, RestDistance(kNeck, kShoulderL), false },
			{ kNeck, kShoulderR, RestDistance(kNeck, kShoulderR), false },
			{ kShoulderL, kShoulderR, RestDistance(kShoulderL, kShoulderR), false },
			{ kPelvis, kHipL, RestDistance(kPelvis, kHipL), false },
			{ kPelvis, kHipR, RestDistance(kPelvis, kHipR), false },
			{ kHipL, kHipR, RestDistance(kHipL, kHipR), false },
			{ kShoulderL, kHipL, RestDistance(kShoulderL, kHipL), false },
			{ kShoulderR, kHipR, RestDistance(kShoulderR, kHipR), false },
			{ kShoulderL, kHipR, RestDistance(kShoulderL, kHipR), false },
			{ kShoulderR, kHipL, RestDistance(kShoulderR, kHipL), false },
			{ kPelvis, kShoulderL, RestDistance(kPelvis, kShoulderL), false },
			{ kPelvis, kShoulderR, RestDistance(kPelvis, kShoulderR), false },
			{ kNeck, kHipL, RestDistance(kNeck, kHipL), false },
			{ kNeck, kHipR, RestDistance(kNeck, kHipR), false },
			// head, arms, legs
			{ kNeck, kHeadTop, RestDistance(kNeck, kHeadTop), false },
			{ kShoulderL, kHandL, 1.0f, false },
			{ kShoulderR, kHandR, 1.0f, false },
			{ kHipL, kFootL, 1.0f, false },
			{ kHipR, kFootR, 1.0f, false },
			// joint limits: neck cone 40 deg, shoulder cone 75 deg, hip cone 60 deg (RagdollBody.ts createJoints),
			// expressed as the closest a limb end may come to a reference point on the torso
			{ kHeadTop, kShoulderL, 0.62f, true },
			{ kHeadTop, kShoulderR, 0.62f, true },
			{ kHeadTop, kPelvis, 1.45f, true },
			{ kHandL, kShoulderR, 0.95f, true },
			{ kHandR, kShoulderL, 0.95f, true },
			{ kHandL, kNeck, 0.75f, true },
			{ kHandR, kNeck, 0.75f, true },
			{ kFootL, kNeck, 1.75f, true },
			{ kFootR, kNeck, 1.75f, true },
			{ kFootL, kFootR, 0.4f, true },
			{ kFootL, kHipR, 0.85f, true },
			{ kFootR, kHipL, 0.85f, true },
		};

		// Integrate. Linear damping 0.1 as on the original bodies.
		const float damping = std::max(0.0f, 1.0f - 0.1f * a_dt);
		for (int i = 0; i < kPointCount; ++i) {
			Vec3 velocity = (pos_[i] - prev_[i]) * damping;
			prev_[i] = pos_[i];
			velocity.z += a_surroundings.gravity * a_dt * a_dt;
			pos_[i] += velocity;
		}

		// The points fly on their own momentum, like the owning body. Drift between the two is removed by
		// sliding the whole ragdoll (never part of it), which leaves its pose and spin untouched: holding
		// only the torso in place would prop the figure upright on its legs.
		(void)a_follow;
		{
			const Vec3 torsoCentre = (pos_[kPelvis] + pos_[kNeck]) * 0.5f;
			Vec3 error = a_surroundings.centre - torsoCentre;
			if (!a_surroundings.airborne) {
				error.z = 0.0f;
			}
			const Vec3 correction = error * 0.12f;
			for (int i = 0; i < kPointCount; ++i) {
				pos_[i] += correction;
				prev_[i] += correction;
			}
		}

		bool onFloor[kPointCount]{};
		for (int iteration = 0; iteration < kIterations; ++iteration) {
			for (const Link& link : links) {
				Vec3 delta = pos_[link.b] - pos_[link.a];
				const float length = delta.Length();
				if (length < 1e-5f || (link.minimumOnly && length >= link.rest)) {
					continue;
				}
				const float difference = (length - link.rest) / length;
				// Torso points are heavier than limb ends (30 kg torso against 5-12 kg limbs).
				const float weightA = IsTorsoPoint(link.a) ? 1.0f : 3.0f;
				const float weightB = IsTorsoPoint(link.b) ? 1.0f : 3.0f;
				const float total = weightA + weightB;
				pos_[link.a] += delta * (difference * weightA / total);
				pos_[link.b] = pos_[link.b] - delta * (difference * weightB / total);
			}

			// World contacts: friction 0.3, restitution 0.1.
			for (int i = 0; i < kPointCount; ++i) {
				const float radius = IsTorsoPoint(i) ? kTorsoRadius : kPointRadius;
				if (a_surroundings.hasGround) {
					const float floor = a_surroundings.groundZ + radius;
					if (pos_[i].z < floor) {
						pos_[i].z = floor;
						onFloor[i] = true;
					}
				}
				if (a_surroundings.hasWall) {
					const float depth = (pos_[i] - a_surroundings.wallPoint).Dot(a_surroundings.wallNormal) - radius;
					if (depth < 0.0f) {
						pos_[i] = pos_[i] - a_surroundings.wallNormal * depth;
						const Vec3 velocity = pos_[i] - prev_[i];
						const float normal = velocity.Dot(a_surroundings.wallNormal);
						if (normal < 0.0f) {
							prev_[i] = pos_[i] - (velocity - a_surroundings.wallNormal * (1.1f * normal));
						}
					}
				}
			}
		}

		// Floor response once per step: no bounce to speak of (restitution 0.1) and sliding friction 0.3,
		// so feet and hands skid instead of sticking.
		const float frictionStep = 0.3f * std::abs(a_surroundings.gravity) * a_dt * a_dt;
		for (int i = 0; i < kPointCount; ++i) {
			if (!onFloor[i]) {
				continue;
			}
			Vec3 velocity = pos_[i] - prev_[i];
			if (velocity.z < 0.0f) {
				velocity.z *= -0.1f;
			}
			const float slide = std::hypot(velocity.x, velocity.y);
			const float keep = slide > 1e-6f ? std::max(0.0f, slide - frictionStep) / slide : 0.0f;
			prev_[i] = { pos_[i].x - velocity.x * keep, pos_[i].y - velocity.y * keep, pos_[i].z - velocity.z };
		}
	}
}
