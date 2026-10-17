// 2026-10-17
/* purpose
* Single owner for animation rotation encoding conversion and interpolation.
* Accepts version-2 Euler XYZ degrees or WXYZ quaternions and the legacy
* version-1 Euler array, and normalizes all of them to one canonical quaternion.
* Provides shortest-arc SLERP and the Melee phase-clip sample mapping.
* Does NOT own clip parsing, pose application, rendering, or replication.
* Does NOT guess a Blender basis conversion; that is a separate importer step.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace AnimRotation {

// Euler XYZ degrees composed as Rx * Ry * Rz (equals qx * qy * qz), per
// docs/specs/animations/animations.md. Returns a normalized quaternion.
glm::quat eulerXyzDegreesToQuat(const glm::vec3& degrees);

// Inverse extraction for authoring/interchange. XYZ order, angles wrapped to
// [-180, 180). Never accumulate the result frame to frame.
glm::vec3 eulerXyzDegreesFromQuat(const glm::quat& q);

// True when all four components are finite.
bool isFinite(const glm::quat& q);

// Normalizes; returns identity for zero-length or non-finite input.
glm::quat safeNormalize(const glm::quat& q);

// Shortest-arc spherical interpolation. Negates the endpoint whose dot product
// is negative so the interpolation takes the short path.
glm::quat slerpShortest(const glm::quat& a, const glm::quat& b, float t);

// Melee phase-clip sampling. `phaseTick` is zero-based within an N-tick phase;
// an N-tick phase evaluates samples 0..N-1 so a one-tick phase holds its start
// pose and the final tick reaches the authored end pose.
float samplePhaseClipTick(int phaseTick, int phaseTicks, int clipDurationTicks);

} // namespace AnimRotation
