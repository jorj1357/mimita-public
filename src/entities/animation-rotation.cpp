// 2026-10-17
/* purpose
* Implements animation rotation encoding conversion and shortest-arc SLERP.
* See animation-rotation.h for ownership boundaries.
*/

#include "animation-rotation.h"

#include <algorithm>
#include <cmath>

namespace AnimRotation {

glm::quat eulerXyzDegreesToQuat(const glm::vec3& degrees)
{
    const glm::quat qx = glm::angleAxis(glm::radians(degrees.x), glm::vec3(1, 0, 0));
    const glm::quat qy = glm::angleAxis(glm::radians(degrees.y), glm::vec3(0, 1, 0));
    const glm::quat qz = glm::angleAxis(glm::radians(degrees.z), glm::vec3(0, 0, 1));
    return safeNormalize(qx * qy * qz);
}

bool isFinite(const glm::quat& q)
{
    return std::isfinite(q.w) && std::isfinite(q.x) &&
           std::isfinite(q.y) && std::isfinite(q.z);
}

glm::quat safeNormalize(const glm::quat& q)
{
    if (!isFinite(q))
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const float lengthSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (lengthSq <= 1e-12f)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return glm::normalize(q);
}

glm::vec3 eulerXyzDegreesFromQuat(const glm::quat& q)
{
    const glm::mat3 r = glm::mat3_cast(safeNormalize(q));
    // glm is column-major: r[column][row], so R[row][col] == r[col][row].
    const float R02 = r[2][0];
    const float R12 = r[2][1];
    const float R22 = r[2][2];
    const float R01 = r[1][0];
    const float R00 = r[0][0];
    const float R10 = r[0][1];
    const float R11 = r[1][1];

    const float y = std::asin(std::clamp(R02, -1.0f, 1.0f));
    float x;
    float z;
    if (std::abs(std::cos(y)) > 1e-6f) {
        x = std::atan2(-R12, R22);
        z = std::atan2(-R01, R00);
    } else {
        z = 0.0f;
        x = std::atan2((std::sin(y) < 0.0f ? -1.0f : 1.0f) * R10, R11);
    }

    auto wrap = [](float degrees) {
        degrees = std::fmod(degrees + 180.0f, 360.0f);
        if (degrees < 0.0f) degrees += 360.0f;
        return degrees - 180.0f;
    };
    return glm::vec3(wrap(glm::degrees(x)), wrap(glm::degrees(y)), wrap(glm::degrees(z)));
}

glm::quat slerpShortest(const glm::quat& a, const glm::quat& b, float t)
{
    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    glm::quat qa = safeNormalize(a);
    glm::quat qb = safeNormalize(b);
    float dot = qa.w * qb.w + qa.x * qb.x + qa.y * qb.y + qa.z * qb.z;
    if (dot < 0.0f) {
        qb = glm::quat(-qb.w, -qb.x, -qb.y, -qb.z);
        dot = -dot;
    }
    dot = std::clamp(dot, -1.0f, 1.0f);
    if (dot > 0.9995f)
        return safeNormalize(glm::quat(
            qa.w + clampedT * (qb.w - qa.w),
            qa.x + clampedT * (qb.x - qa.x),
            qa.y + clampedT * (qb.y - qa.y),
            qa.z + clampedT * (qb.z - qa.z)));

    const float theta0 = std::acos(dot);
    const float theta = theta0 * clampedT;
    const float sinTheta = std::sin(theta);
    const float sinTheta0 = std::sin(theta0);
    const float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    const float s1 = sinTheta / sinTheta0;
    return safeNormalize(glm::quat(
        s0 * qa.w + s1 * qb.w,
        s0 * qa.x + s1 * qb.x,
        s0 * qa.y + s1 * qb.y,
        s0 * qa.z + s1 * qb.z));
}

float samplePhaseClipTick(int phaseTick, int phaseTicks, int clipDurationTicks)
{
    if (clipDurationTicks <= 1 || phaseTicks <= 1)
        return 0.0f;
    const float progress =
        std::clamp((float)phaseTick / (float)(phaseTicks - 1), 0.0f, 1.0f);
    return progress * (float)(clipDurationTicks - 1);
}

} // namespace AnimRotation
