#pragma once

#include <glm/glm.hpp>

struct Capsule;
struct Contact;
struct SweepHit;
class CollisionTriangle;

bool sweepSphereEdge(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 edgeA,
    glm::vec3 edgeB,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

bool capsuleTriangleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const CollisionTriangle& tri,
    int triIndex,
    SweepHit& out
);

bool capsuleTriangleContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    int triIndex,
    Contact& out
);

glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c);

bool pointInTriangle(glm::vec3 p, const CollisionTriangle& tri);

const char* triangleFeatureLabel(const CollisionTriangle& tri, const glm::vec3& point);
