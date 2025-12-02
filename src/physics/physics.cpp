// C:\important\go away v5\s\mimita-v5\src\physics\physics.cpp

#define NOMINMAX
#include "physics/config.h"
#include "physics.h"
#include "../camera.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <iostream>
#include <Windows.h>

// dec 2 2025 todo no idea whre put this 
static glm::vec3 getBottomCorner(const OBB& obb, float sx, float sz)
{
    // sx and sz are either -1 or +1
    glm::vec3 local = glm::vec3(
        sx * obb.halfSize.x,
        -obb.halfSize.y,
        sz * obb.halfSize.z
    );

    glm::vec4 world = obb.orientation * glm::vec4(local, 1.0f);
    return obb.center + glm::vec3(world);
}

static float sampleHeightAt(const Mesh& map, float x, float z)
{
    float ground = -9999.0f;

    // simple plane: find maximum triangle Y at that XZ
    for (const auto& v : map.verts)
    {
        if (fabs(v.pos.x - x) < 0.1f && fabs(v.pos.z - z) < 0.1f)
        {
            ground = std::max(ground, v.pos.y);
        }
    }

    return ground;
}

// idk where put this dec 2 2025 todo
struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;
};

// dec 2 2025 dont hardcode ground friction here put it in a physics config or smth
static float groundFriction = 6.0f; // tweakable

void applyGroundFriction(Player& p, float dt)
{
    float speed = glm::length(glm::vec2(p.vel.x, p.vel.z));
    if (speed <= 0.0001f) return;

    float drop = speed * groundFriction * dt;

    float newSpeed = speed - drop;
    if (newSpeed < 0) newSpeed = 0;

    p.vel.x *= newSpeed / speed;
    p.vel.z *= newSpeed / speed;
}

// dec 2 2025 where putthis idk 
static void applySurfing(Player& p, const glm::vec3& normal)
{
    float into = glm::dot(p.vel, normal);
    if (into < 0.0f)
        p.vel -= normal * into;  // remove velocity pushing into slope
}

float intersectPlane(const glm::vec3& p0, const glm::vec3& n, const Ray& r) {
    float denom = glm::dot(n, r.dir);
    if (fabs(denom) < 1e-6f) return -1.0f;
    float t = glm::dot(p0 - r.origin, n) / denom;
    return (t >= 0.0f ? t : -1.0f);
}

// this goes above updatephysics but also is industry standard triangle colliisons ehh idk
// Möller–Trumbore ray-triangle intersection
static bool raycastTriangle(const Ray& r,
                            const glm::vec3& v0,
                            const glm::vec3& v1,
                            const glm::vec3& v2,
                            float& tOut)
{
    const float EPS = 0.000001f;
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;

    glm::vec3 p = glm::cross(r.dir, e2);
    float det = glm::dot(e1, p);
    if (fabs(det) < EPS) return false;

    float invDet = 1.0f / det;
    glm::vec3 t = r.origin - v0;
    float u = glm::dot(t, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(r.dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    float dist = glm::dot(e2, q) * invDet;
    if (dist > EPS)
    {
        tOut = dist;
        return true;
    }

    return false;
}

// dec 2 2025 todo stop hardcoding 17 difrent rays in 8 difrnet files
static float raycastMeshDown(const Mesh& mesh, const glm::vec3& origin)
{
    Ray r;
    r.origin = origin;
    r.dir = glm::vec3(0, -1, 0);

    float closest = FLT_MAX;

    // mesh.verts is list of vertices; every 3 = triangle
    for (size_t i = 0; i < mesh.verts.size(); i += 3)
    {
        const glm::vec3& v0 = mesh.verts[i+0].pos;
        const glm::vec3& v1 = mesh.verts[i+1].pos;
        const glm::vec3& v2 = mesh.verts[i+2].pos;

        float t;
        if (raycastTriangle(r, v0, v1, v2, t))
        {
            if (t < closest) closest = t;
        }
    }

    if (closest == FLT_MAX)
        return -9999.0f; // no hit

    // world y of intersection
    return origin.y - closest;
}

// air strafe surely itll work first go dec 2 2025
static void applyAirStrafe(Player& p, const glm::vec3& wishDir, float dt)
{
    // Source-like accel
    const float airAccel = 140.0f;  
    const float maxAirSpeed = 30.0f;

    float wishSpeed = maxAirSpeed;
    float currentSpeed = glm::dot(p.vel, wishDir);

    float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed <= 0) return;

    float accel = airAccel * dt;
    if (accel > addSpeed) accel = addSpeed;

    p.vel += wishDir * accel;
}

void updatePhysics(Player& p, const Mesh& world, GLFWwindow* w, float dt, const Camera& cam) {
    // derive camera-relative forward/right, ignoring vertical pitch
    glm::vec3 forward = glm::normalize(glm::vec3(cam.front.x, 0, cam.front.z));
    glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    /*
    this is how characters move in the entire game
    for now nov 6 2025 todo
    */
    glm::vec3 dir(0.0f);
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) dir += forward;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) dir -= forward;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) dir -= right;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) dir += right;
    if (glm::length(dir) > 0) dir = glm::normalize(dir);

    // movement and gravity from config
    // dec 2 2025 includes air strafe maybe
    if (p.onGround)
    {
        // Standard grounded movement
        p.vel.x = dir.x * PHYS.moveSpeed;
        p.vel.z = dir.z * PHYS.moveSpeed;
    }
    else
    {
        // AIR MOVEMENT
        if (glm::length(dir) > 0)
            applyAirStrafe(p, dir, dt);
    }
    p.vel.y += PHYS.gravity * dt;

    // jump
    if (p.onGround && glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }

    // apply velocity
    p.pos += p.vel * dt;

    /*
    todo nov 6 2025
    need much better collisions
    to handle cylinders
    spheres
    triangles
    rectangles
    etc etc etc
    i want it like tf2
    tf2 is like you walk into a wall and you instant stop,
    you dont bounce or squish like in roblox
    but also like roblox
    in roblox if you hold jump while ur character is squished between two parts
    you can fling out
    useful and fun mechanic
    i dont want to code that in, i want it to be a result
    of our collisions
    */

    // --- TRUE TRIANGLE GROUNDING (mesh raycast) ---
    OBB obb = p.getOBB();

    glm::vec3 c1 = getBottomCorner(obb, -1, -1);
    glm::vec3 c2 = getBottomCorner(obb, +1, -1);
    glm::vec3 c3 = getBottomCorner(obb, -1, +1);
    glm::vec3 c4 = getBottomCorner(obb, +1, +1);
    glm::vec3 corners[4] = { c1, c2, c3, c4 };

    bool grounded = false;
    float bestGround = -9999.0f;

    for (int i = 0; i < 4; i++)
    {
        float y = raycastMeshDown(world, corners[i]);
        if (y > -9000.0f) // valid hit
        {
            // corner is on or below ground
            if (corners[i].y <= y + 0.05f)
            {
                grounded = true;
                bestGround = std::max(bestGround, y);
            }
        }
    }

    if (grounded)
    {
        // find triangle normal too
        glm::vec3 triNormal = findTriangleNormalAt(world, bestCornerX, bestCornerZ);

        // surfing
        applySurfing(p, triNormal);

        p.pos.y = bestGround + obb.halfSize.y;
        p.vel.y = 0;
        p.onGround = true;
    }
    else
    {
        p.onGround = false;
    }

    // respawn check (optional)
    if (p.pos.y < PHYS.deathHeight) p.reset();
}
