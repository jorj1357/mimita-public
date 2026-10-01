#pragma once

#include <algorithm>
#include <cmath>

// Engine-independent maths for the Fister simulation port.
//
// Frame: Skyrim axes (X east, Y north, Z up) in Fister units ("metres" as the
// TypeScript source treats them). Yaw is Skyrim yaw: forward = (sin yaw, cos yaw, 0),
// right = (cos yaw, -sin yaw, 0). Pitch is positive looking up.
namespace fister
{
	inline constexpr float kPi = 3.14159265358979323846f;

	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };

		constexpr Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
		constexpr Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
		constexpr Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
		constexpr Vec3 operator-() const { return { -x, -y, -z }; }
		Vec3& operator+=(const Vec3& o)
		{
			x += o.x;
			y += o.y;
			z += o.z;
			return *this;
		}
		[[nodiscard]] constexpr float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
		[[nodiscard]] float Length() const { return std::sqrt(x * x + y * y + z * z); }
		[[nodiscard]] float HorizontalLength() const { return std::sqrt(x * x + y * y); }
		[[nodiscard]] Vec3 Normalized() const
		{
			const float len = Length();
			return len > 1e-6f ? Vec3{ x / len, y / len, z / len } : Vec3{};
		}
	};

	[[nodiscard]] inline float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
	[[nodiscard]] inline float Clamp01(float v) { return Clamp(v, 0.0f, 1.0f); }
	[[nodiscard]] inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
	[[nodiscard]] inline float Smoothstep01(float v)
	{
		const float p = Clamp01(v);
		return p * p * (3.0f - 2.0f * p);
	}

	[[nodiscard]] inline Vec3 YawForward(float yaw) { return { std::sin(yaw), std::cos(yaw), 0.0f }; }
	[[nodiscard]] inline Vec3 YawRight(float yaw) { return { std::cos(yaw), -std::sin(yaw), 0.0f }; }
	[[nodiscard]] inline Vec3 AimDirection(float yaw, float pitch)
	{
		const float c = std::cos(pitch);
		return { std::sin(yaw) * c, std::cos(yaw) * c, std::sin(pitch) };
	}
	[[nodiscard]] inline float WrapAngle(float a) { return std::atan2(std::sin(a), std::cos(a)); }
}
