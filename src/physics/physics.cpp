/**
 * dec 12 2025 i dont get this so make it super super dumb
 * and add bit by bit only the pieces i can comfortably explain jus 
 * offthe ttop of my head
 * like 
 * yeah gravity pulls you down every frame dt = last frame i think
 * yeah move speed is your move speed
 * idgaf about what 30.0f is or 30f or what the f is 
 * or what voidi means or what stdio i dont want definitions i want functionality
 * if i can explain what it DOES off top my head then i get it and i can add it
 * if i dont get it dont add it
 * and that sounds slow but it avoids bloat that makes sutpid bullshit happen later 
 * and file size dont matter line count dont matter, its just do i understand it or not
 */

#include "physics.h"
#include "physics/config.h"
#include "../camera.h"
#include <glm/glm.hpp>

// ---------------- world constants ----------------
// lives in config.h never hardcode values in a file again 

// ---------------- struct for players start ----------------

struct Capsule {
    glm::vec3 a; // bottom sphere center
    glm::vec3 b; // top sphere center
    float r;
};

static Capsule playerCapsule(const Player& p)
{
    Capsule c;
    c.r = PLAYER_RADIUS;
    c.a = p.pos + glm::vec3(0.0f, c.r, 0.0f);
    c.b = p.pos + glm::vec3(0.0f, (PLAYER_HEIGHT - c.r) * 2, 0.0f);
    return c;
}

// ---------------- struct for players end ----------------

// ---------------- helper functions start ----------------

static glm::vec3 closestPointOnTriangle(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
{
    // edges
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return a + ab * (d1 / (d1 - d3));

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return a + ac * (d2 / (d2 - d6));

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

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

static void collideCapsuleTriangle(
    Player& p,
    Capsule& cap,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
{
    // closest point on triangle to capsule line
    glm::vec3 triPoint = closestPointOnTriangle(
        closestPointOnSegment((a + b + c) / 3.0f, cap.a, cap.b),
        a, b, c
    );

    // closest point on capsule line to triangle
    glm::vec3 capPoint = closestPointOnSegment(triPoint, cap.a, cap.b);

    glm::vec3 delta = capPoint - triPoint;
    float dist = glm::length(delta);

    if (dist < cap.r && dist > 0.0001f)
    {
        glm::vec3 normal = delta / dist;
        float howmuchinside = cap.r - dist;

        // only add bias if we're INSIDE A BLOCK. A BLOCK NOTHING ELSE (phasing case)
        float bias = (howmuchinside > 0.05f) ? 0.005f : 0.0f;

        float push = howmuchinside + bias;

        // push player out
        p.pos += normal * push;

        // ground detection
        if (normal.y > 0.5f)
        {
            p.onGround = true;
            if (p.vel.y < 0.0f)
                p.vel.y = 0.0f;
        }
    }
}


// ---------------- helper functions end ----------------

// ---------------- main update ----------------
void updatePhysics(
    Player& p,
    const Mesh& world,
    GLFWwindow* win,
    float dt,
    const Camera& cam)
{

    // ---- debug teleports ----
    if (glfwGetKey(win, GLFW_KEY_T) == GLFW_PRESS)
    {
        p.pos.y += 1.0f;
        p.vel = glm::vec3(0.0f);
    }

    if (glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS)
    {
        glm::vec3 dir = cam.front;
        dir.y = 0.0f;
        if (glm::length(dir) > 0.0001f)
            dir = glm::normalize(dir);

        p.pos += dir * 1.0f;
        p.vel = glm::vec3(0.0f);
    }

    // movement input
    glm::vec3 wish(0.0f);

    glm::vec3 forward = cam.front;
    forward.y = 0.0f;
    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);

    glm::vec3 right = glm::cross(forward, glm::vec3(0,1,0));

    if (glfwGetKey(win, GLFW_KEY_W)) wish += forward;
    if (glfwGetKey(win, GLFW_KEY_S)) wish -= forward;
    if (glfwGetKey(win, GLFW_KEY_A)) wish -= right;
    if (glfwGetKey(win, GLFW_KEY_D)) wish += right;

    if (glm::length(wish) > 0.0001f)
        wish = glm::normalize(wish);

    glm::vec3 delta = wish * PHYS.moveSpeed * dt;

    // horizontal move
    p.pos += glm::vec3(delta.x, 0.0f, delta.z);

    // gravity
    p.vel.y += PHYS.gravity * dt;
    p.pos.y += p.vel.y * dt;

    // collisions
    p.onGround = false;
    Capsule cap = playerCapsule(p);

    for (size_t i = 0; i + 2 < world.verts.size(); i += 3)
    {
        collideCapsuleTriangle(
            p,
            cap,
            world.verts[i+0].pos,
            world.verts[i+1].pos,
            world.verts[i+2].pos
        );
    }

    // jump
    if (glfwGetKey(win, GLFW_KEY_SPACE) && p.onGround)
    {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }
}
