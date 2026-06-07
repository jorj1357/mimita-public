#include "npc-navigation.h"
#include "npc.h"

#include <glm/gtc/constants.hpp>

#include "world/world.h"

namespace {

float random01(unsigned int& state)
{
    state = state * 1664525u + 1013904223u;
    return (float)((state >> 8) & 0x00ffffffu) / (float)0x01000000u;
}

// Simple line-vs-triangle intersection
bool rayTriangleIntersect(glm::vec3 origin, glm::vec3 dir, const CollisionTriangle& tri, float maxT, float& outT)
{
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(dir, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.0001f) return false;
    float invDet = 1.0f / det;
    glm::vec3 tVec = origin - tri.a;
    float u = glm::dot(tVec, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(tVec, e1);
    float v = glm::dot(dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    outT = glm::dot(e2, q) * invDet;
    return outT > 0.01f && outT < maxT;
}

} // anonymous namespace

glm::vec3 NpcNavigation::wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world)
{
    if (glm::length(desiredDir) < 0.001f)
        return desiredDir;

    desiredDir.z = 0.0f;
    float len = glm::length(desiredDir);
    if (len < 0.001f) return desiredDir;
    desiredDir /= len;

    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f; // cast from mid-body height

    float checkDist = 1.5f;
    float stepAngle = glm::pi<float>() / 6.0f; // 30 degrees

    // Check if desired direction is blocked
    for (const auto& tri : world.collisionMesh.triangles)
    {
        float t;
        if (rayTriangleIntersect(origin, desiredDir, tri, checkDist, t))
        {
            // Blocked! Try angling left and right
            for (int side = 0; side < 6; ++side)
            {
                float angle = stepAngle * (side + 1);
                for (float sign : {1.0f, -1.0f})
                {
                    glm::vec3 altDir = desiredDir;
                    float c = std::cos(angle * sign);
                    float s = std::sin(angle * sign);
                    altDir = {altDir.x * c - altDir.y * s, altDir.x * s + altDir.y * c, 0.0f};

                    bool blocked = false;
                    for (const auto& checkTri : world.collisionMesh.triangles)
                    {
                        if (rayTriangleIntersect(origin, altDir, checkTri, checkDist, t))
                        { blocked = true; break; }
                    }
                    if (!blocked)
                        return altDir;
                }
            }
            // All directions blocked, try perpendicular
            glm::vec3 perp{-desiredDir.y, desiredDir.x, 0.0f};
            bool blocked = false;
            for (const auto& checkTri : world.collisionMesh.triangles)
            {
                if (rayTriangleIntersect(origin, perp, checkTri, checkDist, t))
                { blocked = true; break; }
            }
            if (!blocked)
                return perp;
            return -perp;
        }
    }

    return desiredDir;
}

bool NpcNavigation::isStuck(const Npc& npc)
{
    // NPC is stuck if it's trying to move but barely moved
    bool tryingMove = glm::length(npc.lastMoveInput) > 0.1f;
    glm::vec3 moved = npc.body.pos - npc.previousPosition;
    float moveSpeed = glm::length(moved);
    bool groundedAndSlow = npc.body.onGround && moveSpeed < 0.02f;
    return tryingMove && groundedAndSlow;
}

glm::vec3 NpcNavigation::unstuckDirection(const Npc& npc, unsigned int& rng, const World& world)
{
    // Try several random directions, pick the one with the most open space
    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f;

    struct DirScore {
        glm::vec3 dir;
        float maxDist;
    };
    DirScore best{{1.0f, 0.0f, 0.0f}, 0.0f};

    for (int i = 0; i < 8; ++i)
    {
        float angle = random01(rng) * glm::two_pi<float>();
        glm::vec3 testDir{std::cos(angle), std::sin(angle), 0.0f};

        float closest = 10.0f;
        for (const auto& tri : world.collisionMesh.triangles)
        {
            float t;
            if (rayTriangleIntersect(origin, testDir, tri, 10.0f, t))
                closest = std::min(closest, t);
        }
        if (closest > best.maxDist)
        {
            best.dir = testDir;
            best.maxDist = closest;
        }
    }

    return best.dir;
}
