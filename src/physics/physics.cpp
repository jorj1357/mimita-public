// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics.cpp
/**
 * file purpose
 * this controls physics for the whole game
 * air accel and collisions and step up and velocity too i think idk
 * but i want to split it into multiple tiny files eventually just later 
 * 
 * eventually do this
 * move()
sweep()
resolveFloor()
resolveSlope()
resolveWall()
stepUp()
just functions for other files to call 
 */

// ------------------ PHYSICS.CPP (CLEAN Source VERSION) --------------------

#define NOMINMAX
#include "physics/config.h"
#include "physics.h"
#include "../camera.h"

#include <glm/glm.hpp>
#include <algorithm>

// ---------------- CONFIG ----------------

const float MAX_WALKABLE_ANGLE = 70.0f;
const float MAX_WALKABLE_DOT   = cos(glm::radians(MAX_WALKABLE_ANGLE));
const float STEP_HEIGHT        = 0.5f;
const float FLOOR_EPS          = 0.001f;
const float WALL_EPS           = 0.0002f;

// ---------------- BASIC HELPERS ----------------

struct Ray {
    glm::vec3 o, d;
};

struct SweepResult {
    bool hit = false;
    float t = 1.0f;
    glm::vec3 normal = {0,1,0};
};

// point-triangle sweep (Möller-Trumbore)
static bool rayTri(const Ray& r,
                   const glm::vec3& v0,
                   const glm::vec3& v1,
                   const glm::vec3& v2,
                   float& tOut)
{
    const float EPS = 1e-6f;
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 p  = glm::cross(r.d, e2);
    float det = glm::dot(e1, p);
    if (fabs(det) < EPS) return false;

    float inv = 1.0f / det;
    glm::vec3 t = r.o - v0;
    float u = glm::dot(t, p) * inv;
    if (u < 0 || u > 1) return false;

    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(r.d, q) * inv;
    if (v < 0 || u + v > 1) return false;

    float dist = glm::dot(e2, q) * inv;
    if (dist > EPS) {
        tOut = dist;
        return true;
    }

    return false;
}

static SweepResult sweepPointTri(const glm::vec3& a, const glm::vec3& b,
                                 const glm::vec3& v0,
                                 const glm::vec3& v1,
                                 const glm::vec3& v2)
{
    SweepResult r;
    glm::vec3 dir = b - a;
    Ray ray{a, dir};

    float t;
    if (!rayTri(ray, v0, v1, v2, t)) return r;
    if (t < 0.0f || t > 1.0f) return r;

    glm::vec3 p = a + dir * t;

    glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    if (n.y < 0) n = -n; // <-- Force normals to face upward

    // barycentric manual test
    if (glm::dot(n, glm::cross(v1 - v0, p - v0)) < 0) return r;
    if (glm::dot(n, glm::cross(v2 - v1, p - v1)) < 0) return r;
    if (glm::dot(n, glm::cross(v0 - v2, p - v2)) < 0) return r;

    r.hit = true;
    r.t = t;
    r.normal = n;
    return r;
}

// box sweep (8 corner tests)
static SweepResult sweepBoxMesh(const Mesh& mesh,
                                const glm::vec3& cs,
                                const glm::vec3& ce,
                                const glm::vec3& half)
{
    SweepResult best;

    glm::vec3 offs[8] = {
        {+half.x,+half.y,+half.z}, {+half.x,+half.y,-half.z},
        {+half.x,-half.y,+half.z}, {+half.x,-half.y,-half.z},
        {-half.x,+half.y,+half.z}, {-half.x,+half.y,-half.z},
        {-half.x,-half.y,+half.z}, {-half.x,-half.y,-half.z},
    };

    for (int c = 0; c < 8; c++)
    {
        glm::vec3 a = cs + offs[c];
        glm::vec3 b = ce + offs[c];

        for (size_t i = 0; i < mesh.verts.size(); i += 3)
        {
            auto& v0 = mesh.verts[i+0].pos;
            auto& v1 = mesh.verts[i+1].pos;
            auto& v2 = mesh.verts[i+2].pos;

            SweepResult hit = sweepPointTri(a, b, v0, v1, v2);
            if (hit.hit && hit.t < best.t) {
                best = hit;
                if (best.t < 0.0001f) return best;
            }
        }
    }

    return best;
}

// ---------------- MOVEMENT HELPERS ----------------

static void slideWall(Player& p, const glm::vec3& n)
{
    float into = glm::dot(p.vel, n);
    if (into < 0)
        p.vel -= n * into;
}

static void applyAirStrafe(Player& p, const glm::vec3& wish, float dt)
{
    const float accel = 140.0f;
    const float maxAir = 30.0f;

    float wishSpeed = maxAir;
    float cur = glm::dot(p.vel, wish);
    float add = wishSpeed - cur;
    if (add <= 0) return;

    float a = accel * dt;
    if (a > add) a = add;
    p.vel += wish * a;
}

static void applyGroundFriction(Player& p, float dt)
{
    float sp = glm::length(glm::vec2(p.vel.x, p.vel.z));
    if (sp < 0.0001f) return;

    float drop = sp * 6.0f * dt;
    float ns = std::max(sp - drop, 0.0f);
    float scale = (sp > 0) ? ns / sp : 0;

    p.vel.x *= scale;
    p.vel.z *= scale;
}

// ---------------- MAIN UPDATE ----------------

void updatePhysics(Player& p, const Mesh& world, GLFWwindow* w, float dt, const Camera& cam)
{
    glm::vec3 f = glm::normalize(glm::vec3(cam.front.x, 0, cam.front.z));
    glm::vec3 r = glm::cross(f, glm::vec3(0,1,0));

    glm::vec3 dir(0);
    if (glfwGetKey(w, GLFW_KEY_W)) dir += f;
    if (glfwGetKey(w, GLFW_KEY_S)) dir -= f;
    if (glfwGetKey(w, GLFW_KEY_A)) dir -= r;
    if (glfwGetKey(w, GLFW_KEY_D)) dir += r;
    if (glm::length(dir) > 0) dir = glm::normalize(dir);

    // ground or air move
    if (p.onGround) {
        p.vel.x = dir.x * PHYS.moveSpeed;
        p.vel.z = dir.z * PHYS.moveSpeed;
    } else if (glm::length(dir) > 0) {
        applyAirStrafe(p, dir, dt);
    }

    // gravity
    p.vel.y += PHYS.gravity * dt;

    // jump
    if (p.onGround && glfwGetKey(w, GLFW_KEY_SPACE)) {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }

    glm::vec3 half = p.hitboxSize * 0.5f;
    glm::vec3 cs   = p.pos + glm::vec3(0, half.y, 0);
    glm::vec3 ce   = cs + p.vel * dt;

    float horiz = glm::length(glm::vec2(p.vel.x, p.vel.z));
    bool wantsStep = horiz > 0.001f;

    // ---------------- STEP-UP ATTEMPT (minimal version) ----------------
    if (wantsStep)
    {
        SweepResult nc = sweepBoxMesh(world, cs, ce, half);

        if (nc.hit && nc.normal.y >= MAX_WALKABLE_DOT)
        {
            glm::vec3 stepPos = p.pos; stepPos.y += STEP_HEIGHT;
            glm::vec3 scs = stepPos + glm::vec3(0, half.y, 0);
            glm::vec3 sce = scs + p.vel * dt;

            if (!sweepBoxMesh(world, scs, sce, half).hit) {
                p.pos = stepPos + p.vel * dt;
                return;
            }
        }
    }

    // ---------------- BOX SWEEP ----------------
    SweepResult hit = sweepBoxMesh(world, cs, ce, half);

    if (hit.hit)
    {
        if (hit.t < 0.001f)
        {
            p.vel = glm::vec3(0);
        }

        float up = hit.normal.y;   // <-- now visible below

        glm::vec3 newC = cs + (ce - cs) * hit.t;
        glm::vec3 push = hit.normal * 0.01f;
        p.pos = newC - glm::vec3(0, half.y, 0) + push;

        // --- floor ---
        if (up >= MAX_WALKABLE_DOT)
        {
            p.onGround = true;

            float into = glm::dot(p.vel, hit.normal);
            if (into < 0) p.vel -= hit.normal * into;
            if (p.vel.y < 0) p.vel.y = 0;

            p.pos = newC - glm::vec3(0, half.y, 0) + hit.normal * FLOOR_EPS;
            applyGroundFriction(p, dt);
            return;
        }

        // --- steep slope ---
        if (up > 0.05f && up < MAX_WALKABLE_DOT)
        {
            p.onGround = false;
            slideWall(p, hit.normal);
            p.pos = newC - glm::vec3(0, half.y, 0) + hit.normal * FLOOR_EPS;
            return;
        }

        // --- wall ---
        p.onGround = false;
        slideWall(p, hit.normal);
        p.pos = newC - glm::vec3(0, half.y, 0) + hit.normal * WALL_EPS;
        return;
    }

    // no collision
    glm::vec3 newC = ce;
    p.pos = newC - glm::vec3(0, half.y, 0);
}

