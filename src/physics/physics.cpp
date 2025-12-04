// C:\important\go away v5\s\mimita-v5\src\physics\physics.cpp

// dec 3 2025 todo 
// maybe split this into smaller files so this isnt just a massive 500 line file but whatever 

#define NOMINMAX
#include "physics/config.h"
#include "physics.h"
#include "../camera.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <iostream>
#include <Windows.h>

// dec 3 2025 idk where put this
// prob should go in a centralized player config file
// MAKE THIS LIKE 89 DEGREES LATER after testing done 
// dec 4 2025 default
// const float MAX_WALKABLE_ANGLE = 50.0f;  // degrees
// 70 deg = default + 20
const float MAX_WALKABLE_ANGLE = 70.0f;  // degrees
const float MAX_WALKABLE_DOT = cos(glm::radians(MAX_WALKABLE_ANGLE));
// dec 3 2025 this rl tall maybe dont 
const float STEP_HEIGHT = 0.5f;

const float FLOOR_EPS = 0.001f;
const float WALL_EPS  = 0.0002f;

// dec 4 2025 idk where put this
static void slideAgainstWall(Player& p, const glm::vec3& n)
{
    float into = glm::dot(p.vel, n);
    if (into < 0.0f)
        p.vel -= n * into;  // remove component pushing into wall
}

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

// this goes above updatephysics but also is industry standard triangle colliisons ehh idk
// Möller–Trumbore ray-triangle intersection
// THIS MUST GO ABOVE TRIANGLENORMALAT
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

// dec 3 2025 todo need make this better , this for collisions 
static glm::vec3 findTriangleNormalAt(const Mesh& mesh, float x, float z)
{
    Ray r;
    r.origin = glm::vec3(x, 9999.0f, z);
    r.dir    = glm::vec3(0, -1, 0);

    float closest = FLT_MAX;
    glm::vec3 bestNormal = glm::vec3(0,1,0);

    for (size_t i = 0; i < mesh.verts.size(); i += 3)
    {
        const glm::vec3& v0 = mesh.verts[i+0].pos;
        const glm::vec3& v1 = mesh.verts[i+1].pos;
        const glm::vec3& v2 = mesh.verts[i+2].pos;

        float t;
        if (raycastTriangle(r, v0, v1, v2, t))
        {
            if (t < closest)
            {
                closest = t;
                bestNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            }
        }
    }

    return bestNormal;
}

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

// dec 3 2025 todo sweep to make it good i think 
struct SweepResult {
    bool hit;
    float t;            // fraction of movement (0..1)
    glm::vec3 hitPoint;
    glm::vec3 normal;
};

// Sweep a moving point against a triangle
static SweepResult sweepPointTriangle(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2)
{
    SweepResult out;
    out.hit = false;
    out.t = 1.0f;

    glm::vec3 dir = end - start;

    // Backface cull? No, we want both sides for ground
    glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    float denom = glm::dot(n, dir);
    if (fabs(denom) < 1e-6f) return out; // no intersection

    float t = glm::dot(v0 - start, n) / denom;
    if (t < 0.0f || t > 1.0f) return out;

    glm::vec3 p = start + dir * t;

    // Barycentric test
    glm::vec3 c;

    glm::vec3 edge0 = v1 - v0;
    glm::vec3 vp0 = p - v0;
    c = glm::cross(edge0, vp0);
    if (glm::dot(n, c) < 0) return out;

    glm::vec3 edge1 = v2 - v1;
    glm::vec3 vp1 = p - v1;
    c = glm::cross(edge1, vp1);
    if (glm::dot(n, c) < 0) return out;

    glm::vec3 edge2 = v0 - v2;
    glm::vec3 vp2 = p - v2;
    c = glm::cross(edge2, vp2);
    if (glm::dot(n, c) < 0) return out;

    out.hit = true;
    out.t = t;
    out.hitPoint = p;
    out.normal = n;
    return out;
}

// dec 3 2025 more sweeping todo 
// Sweep all 8 corners of the box as points
static SweepResult sweepBoxMesh(
    const Mesh& mesh,
    const glm::vec3& startCenter,
    const glm::vec3& endCenter,
    const glm::vec3& half)
{
    SweepResult best;
    best.hit = false;
    best.t = 1.0f;

    // 8 corner offsets
    glm::vec3 offs[8] = {
        {+half.x, +half.y, +half.z},
        {+half.x, +half.y, -half.z},
        {+half.x, -half.y, +half.z},
        {+half.x, -half.y, -half.z},
        {-half.x, +half.y, +half.z},
        {-half.x, +half.y, -half.z},
        {-half.x, -half.y, +half.z},
        {-half.x, -half.y, -half.z}
    };

    for (int c = 0; c < 8; c++)
    {
        glm::vec3 start = startCenter + offs[c];
        glm::vec3 end   = endCenter   + offs[c];

        for (size_t i = 0; i < mesh.verts.size(); i += 3)
        {
            const glm::vec3& v0 = mesh.verts[i+0].pos;
            const glm::vec3& v1 = mesh.verts[i+1].pos;
            const glm::vec3& v2 = mesh.verts[i+2].pos;

            glm::vec3 triNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            /**
                Why 0.6?
                cos(≈53 degrees) ≈ 0.6
                So any slope less steep than 53° is treated as “the ground”
                You still collide with walls
                You still collide with steep slopes
                You do NOT collide sideways with the floor
                SO LATER WE NEED TO NOT HAVE IT BE HARD CODED 0.6 NEED STEEPER ALLOWED ANGLE
             */

            SweepResult hit = sweepPointTriangle(start, end, v0, v1, v2);

            if (hit.hit && hit.t < best.t)
            {
                best = hit;

                // Very slight bias: stop checking others if extremely close
                if (best.t < 0.0001f)
                    return best;
            }
        }
    }

    return best;
}

void updatePhysics(Player& p, const Mesh& world, GLFWwindow* w, float dt, const Camera& cam) {
    // derive camera-relative forward/right, ignoring vertical pitch
    glm::vec3 forward = glm::normalize(glm::vec3(cam.front.x, 0, cam.front.z));
    glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));


    /*
    this is how characters move in the entire game
    for now nov 6 2025 todo
    dec 3 2025 todo maybe this goes into another file like player constants edit?
    walkspeed and hp and jump height and gravity and game speed etc
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
        p.vel.x = dir.x * PHYS.moveSpeed;
        p.vel.z = dir.z * PHYS.moveSpeed;
    }
    else
    {
        if (glm::length(dir) > 0.0f)
            applyAirStrafe(p, dir, dt);
    }
    p.vel.y += PHYS.gravity * dt;

    // jump
    // put this ater     p.vel.y += PHYS.gravity * dt; ? or no?
    if (p.onGround && glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS) {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }

    // apply velocity
    // p.pos += p.vel * dt;

    // apply velocity BETTER with SWEEPING dec 3 2025 todo

    glm::vec3 start = p.pos;
    glm::vec3 end = p.pos + p.vel * dt;
    // DONT DO CAPSULE WE'RE DOING a box. dec 3 2025 
    float rad = 0.5f * p.hitboxSize.x; // approximate capsule radius

    glm::vec3 half = p.hitboxSize * 0.5f;

    glm::vec3 centerStart = p.pos + glm::vec3(0, half.y, 0);
    glm::vec3 centerEnd   = centerStart + p.vel * dt;

    // Try stepping up before normal movement
    // Only attempt step-up if actually moving
    float horizSpeed = glm::length(glm::vec2(p.vel.x, p.vel.z));
    bool wantsStep = horizSpeed > 0.001f;

    if (wantsStep)
    {
        SweepResult normalCheck = sweepBoxMesh(world, centerStart, centerEnd, half);

        if (normalCheck.hit && normalCheck.normal.y >= MAX_WALKABLE_DOT)
        {
            // ===== WALKABLE SLOPE OR SMALL LEDGE =====
            glm::vec3 stepPos = p.pos;
            stepPos.y += STEP_HEIGHT;

            glm::vec3 stepCenterStart = stepPos + glm::vec3(0, half.y, 0);
            glm::vec3 stepCenterEnd   = stepCenterStart + p.vel * dt;

            SweepResult stepCheck = sweepBoxMesh(world, stepCenterStart, stepCenterEnd, half);

            if (!stepCheck.hit)
            {
                p.pos = stepPos + p.vel * dt;
                return;
            }

            // adaptive climb
            const float MAX_CLIMB = p.hitboxSize.y * 1.5f;
            const float STEP = 0.1f;

            for (float up = STEP_HEIGHT; up < MAX_CLIMB; up += STEP)
            {
                glm::vec3 climbPos = p.pos;
                climbPos.y += up;

                glm::vec3 climbStart = climbPos + glm::vec3(0, half.y, 0);
                glm::vec3 climbEnd   = climbStart + p.vel * dt;

                SweepResult climbCheck = sweepBoxMesh(world, climbStart, climbEnd, half);

                if (!climbCheck.hit)
                {
                    p.pos = climbPos + p.vel * dt;
                    return;
                }
            }
        }
    }

    // USE CENTER, not FEET
    SweepResult result = sweepBoxMesh(world, centerStart, centerEnd, half);

    if (result.hit)
    {
        // Before handling walkable/steep/wall:
        if (result.t < 0.001f)
        {
            p.vel = glm::vec3(0.0f);
        }

        glm::vec3 newCenter = centerStart + (centerEnd - centerStart) * result.t;
        float dotUp = result.normal.y;

        // --- walkable floor ---
        if (dotUp >= MAX_WALKABLE_DOT)
        {
            p.onGround = true;

            float into = glm::dot(p.vel, result.normal);
            if (into < 0.0f)
                p.vel -= result.normal * into;

            if (p.vel.y < 0) p.vel.y = 0;

            p.pos = newCenter - glm::vec3(0, half.y, 0) + result.normal * FLOOR_EPS;
            return;
        }

        // --- steep slope ---
        if (dotUp > 0.0f && dotUp < MAX_WALKABLE_DOT)
        {
            p.onGround = false;

            float into = glm::dot(p.vel, result.normal);
            if (into < 0.0f)
                p.vel -= result.normal * into;

            p.pos = newCenter - glm::vec3(0, half.y, 0) + result.normal * FLOOR_EPS;
            return;
        }

        // --- WALL ---
        p.onGround = false;
        slideAgainstWall(p, result.normal);
        p.pos = newCenter - glm::vec3(0, half.y, 0) + result.normal * WALL_EPS;
        return;
    }
    else
    {
        glm::vec3 newCenter = centerEnd;
        p.pos = newCenter - glm::vec3(0, half.y, 0);
    }

    // // --- UNIVERSAL UN-STUCK PASS ---
    // if (p.onGround && p.vel.y <= 0.0f)   // AND you're moving down OR not moving up
    // {
    //     glm::vec3 probeOrigin = p.pos + glm::vec3(0, half.y + 2.0f, 0);
    //     float surfaceY = raycastMeshDown(world, probeOrigin);

    //     float playerFeet = p.pos.y;
    //     float penetration = surfaceY - playerFeet;

    //     // i think this relates to the snap position being like 0.02f
    //     // if this is bigger than the snap position tolerance i think its ok?
    //     if (fabs(penetration) < 0.05f) return;   // we're basically correct height

    //     if (penetration > 0.001f && penetration < 2.0f)
    //     {
    //         glm::vec3 underN = findTriangleNormalAt(world, p.pos.x, p.pos.z);

    //         if (underN.y > MAX_WALKABLE_DOT)   // only snap on walkable
    //         {
    //             p.pos.y = surfaceY + 0.001f;
    //             p.vel.y = 0;
    //             p.onGround = true;
    //         }
    //     }
    // }
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
}
