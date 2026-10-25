// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics-types.h
// dec 16 2025
/**
 * purpose
 * stop having phsics cpp be so full of shish
 */

/**
 * feb 10 2026
 * i know its a small ridiculous tiniest whi is this here at all file
 * idc 
 * its not 1 billion trillion lines and thats what i like 
 * if its short then cool
 */

#pragma once
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>

// Absolute sanity limit for one world-space coordinate. The chunk broadphase
// works for any finite position; this only rejects absurd/garbage values before
// they reach integer chunk-coordinate casts, and must stay large enough not to
// clip legitimate large maps (bhop1's spawn is near x = -21855).
inline constexpr float kMaxWorldExtent = 1.0e7f;
inline constexpr float kMaxWorldAABBSize = kMaxWorldExtent * 2.0f;

struct Capsule
{
    glm::vec3 a;
    glm::vec3 b;
    float r;
};

struct TransformNode
{
    std::string name;
    int parent = -1;
    std::vector<int> children;
    glm::mat4 localTransform{1.0f};
    glm::mat4 worldTransform{1.0f};
};

struct Force
{
    glm::vec3 vector{0.0f};
    glm::vec3 point{0.0f};
    float strength = 0.0f;
};

struct CollisionTriangle
{
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 c{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

struct SweepHit
{
    bool hit = false;
    float time = 1.0f;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    int triangleIndex = -1;
    std::string colliderName;
};

struct Contact
{
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
};

struct Collider
{
    std::string name;
    std::vector<CollisionTriangle> triangles;
    std::vector<glm::vec3> samplePoints;
    glm::vec3 localMin{0.0f};
    glm::vec3 localMax{0.0f};
};

struct SpringState
{
    glm::vec3 value{0.0f};
    glm::vec3 velocity{0.0f};
};

struct ProceduralPose
{
    glm::vec3 translation{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 scale{1.0f};
};

struct BodyPart
{
    std::string name;
    int nodeIndex = -1;
    Collider collider;
    ProceduralPose pose;
    SpringState translationSpring;
    SpringState rotationSpring;
};

struct PhysicsBody
{
    TransformNode transform;
    std::vector<Collider> colliders;
    std::vector<Force> forces;
    glm::vec3 velocity{0.0f};
    float mass = 1.0f;
};

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

// Conservative triangle-oriented prism used only for broadphase rejection.
// The original CollisionTriangle remains the authoritative collision shape.
struct CollisionTriangleOBB
{
    glm::vec3 origin{0.0f};
    glm::vec3 axisU{1.0f, 0.0f, 0.0f};
    glm::vec3 axisV{0.0f, 1.0f, 0.0f};
    glm::vec3 axisN{0.0f, 0.0f, 1.0f};
    glm::vec3 localMin{0.0f};
    glm::vec3 localMax{0.0f};
    bool valid = false;
};

struct CollisionMeshCache
{
    std::vector<CollisionTriangle> triangles;
    // Optional parallel source-solid identity. A nonzero value groups triangles
    // that came from one authored solid so static hidden-face removal can avoid
    // comparing faces belonging to the same object. Zero means unknown.
    std::vector<uint32_t> solidIds;
    // Cached per-triangle AABB, parallel to `triangles`. Built once when the
    // world collision mesh/chunks are built so the actor narrowphase never
    // recomputes a world triangle's bounds inside a per-pair loop.
    std::vector<AABB> triangleAABBs;
    std::vector<CollisionTriangleOBB> triangleOBBs;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    bool empty() const { return triangles.empty(); }
    void clear()
    {
        triangles.clear();
        solidIds.clear();
        triangleAABBs.clear();
        triangleOBBs.clear();
        boundsMin = glm::vec3(0.0f);
        boundsMax = glm::vec3(0.0f);
    }
};
