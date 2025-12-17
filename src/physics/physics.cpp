// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics.cpp
// dec 16 2025 
/**
 * purpose
 * phsics just like main cpp
 * just calls other files functions
 * no mess in here 
 * config values all defined in config.h 
 * and when we have a bunch of values thatll get split too but u know whaever 
 */

// physics.cpp
#include "physics.h"
#include "physics/config.h"
#include "collision-capsule-triangle.h"
#include "world/world.h"              // <-- your World type
#include "../camera.h"
#include <glm/glm.hpp>
#include <vector>

// you need access to your built world somewhere.
// simplest: pass it into updatePhysics (recommended)
// OR use extern World gWorld; if you want global.

static Capsule playerCapsule(const Player& p)
{
    Capsule c;
    c.r = PLAYER_RADIUS;
    c.a = p.pos + glm::vec3(0.0f, c.r, 0.0f);
    c.b = p.pos + glm::vec3(0.0f, PLAYER_HEIGHT - c.r, 0.0f);
    return c;
}

void updatePhysics(
    Player& p,
    const World& world,          // <-- CHANGE: Mesh -> World
    GLFWwindow* win,
    float dt,
    const Camera& cam)
{
    dt = glm::min(dt, 0.033f);

    // 1) build desired move from input (this IS your "desiredMove")
    glm::vec3 wish(0.0f);

    glm::vec3 forward = cam.front;
    forward.y = 0.0f;
    if (glm::length(forward) > 0.0001f) forward = glm::normalize(forward);

    glm::vec3 right = glm::cross(forward, glm::vec3(0,1,0));

    if (glfwGetKey(win, GLFW_KEY_W)) wish += forward;
    if (glfwGetKey(win, GLFW_KEY_S)) wish -= forward;
    if (glfwGetKey(win, GLFW_KEY_A)) wish -= right;
    if (glfwGetKey(win, GLFW_KEY_D)) wish += right;

    if (glm::length(wish) > 0.0001f) wish = glm::normalize(wish);

    glm::vec3 move = wish * PHYS.moveSpeed * dt;   // <-- THIS is desiredMove

    // 2) gravity (your existing code)
    p.vel.y += PHYS.gravity * dt;
    p.pos.y += p.vel.y * dt;

    // 3) query nearby triangles (chunking)
    std::vector<Triangle> nearby;
    nearby.reserve(2048); // optional, reduces allocations
    world.getNearbyTriangles(p.pos, nearby);

    // 4) multipass slide
    Capsule cap = playerCapsule(p);
    p.onGround = false;

    for (int pass = 0; pass < 3; pass++)
    {
        for (const Triangle& t : nearby)
        {
            move = collideCapsuleTriangleMove(cap, move, t, p.onGround);
        }
    }

    // 5) apply final move once
    p.pos += glm::vec3(move.x, 0.0f, move.z);

    // 6) jump (your existing code)
    if (glfwGetKey(win, GLFW_KEY_SPACE) && p.onGround)
    {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }
}
