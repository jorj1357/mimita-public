// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-triangle.cpp
// dec 16 2025
/**
 * purpose
 * this has all the SB that we dont want in the phsics cpp file 
 * so phsics just calls the functions 
 * and its all nice and readable 
 */

#include "collision-capsule-triangle.h"
#include "physics/config.h"
#include <glm/glm.hpp>

static glm::vec3 closestPointOnSegment(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + ab * t;
}

glm::vec3 collideCapsuleTriangleMove(
    const Capsule& cap,
    const glm::vec3& move,
    const Triangle& tri,
    bool& onGround)
{
    glm::vec3 mid = (tri.a + tri.b + tri.c) / 3.0f;
    glm::vec3 capPoint = closestPointOnSegment(mid, cap.a, cap.b);
    glm::vec3 triPoint = mid;

    glm::vec3 delta = capPoint - triPoint;
    float dist = glm::length(delta);

    if (dist >= cap.r || dist < 0.00001f)
        return move;

    glm::vec3 normal = delta / dist;
    float into = glm::dot(move, normal);

    glm::vec3 out = move;
    if (into < 0.0f)
        out -= normal * into;

    if (normal.y > MAX_SLOPE_ANGLE)
        onGround = true;

    return out;
}
