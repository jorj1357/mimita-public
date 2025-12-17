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

// ---------------- helper functions start ----------------

glm::vec3 playerHalfExtents()
{
    return glm::vec3(
        PLAYER_WIDTH * 0.5f,
        PLAYER_HEIGHT * 0.5f,
        PLAYER_DEPTH * 0.5f
    );
}

static bool aabbOverlapsTriangle(
    const glm::vec3& boxCenter,
    const glm::vec3& half,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
{
    // Triangle AABB
    glm::vec3 tmin = glm::min(a, glm::min(b, c));
    glm::vec3 tmax = glm::max(a, glm::max(b, c));

    // Box AABB
    glm::vec3 bmin = boxCenter - half;
    glm::vec3 bmax = boxCenter + half;

    // AABB vs AABB overlap
    if (bmax.x < tmin.x || bmin.x > tmax.x) return false;
    if (bmax.y < tmin.y || bmin.y > tmax.y) return false;
    if (bmax.z < tmin.z || bmin.z > tmax.z) return false;

    return true;
}

static bool playerBoxHitsWorld(const Player& p, const Mesh& world)
{
    glm::vec3 half = playerHalfExtents();
    glm::vec3 center = p.pos + glm::vec3(0.0f, half.y, 0.0f);

    for (size_t i = 0; i + 2 < world.verts.size(); i += 3)
    {
        glm::vec3 a = world.verts[i + 0].pos;
        glm::vec3 b = world.verts[i + 1].pos;
        glm::vec3 c = world.verts[i + 2].pos;

        if (aabbOverlapsTriangle(center, half, a, b, c))
            return true;
    }
    return false;
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

    // debug stuff so i get not stuck
    // teleport up
    if (glfwGetKey(win, GLFW_KEY_T) == GLFW_PRESS)
    {
        p.pos.y += 1.0f;
        p.vel = glm::vec3(0.0f);
    }

    // teleport forward (where player is facing)
    if (glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS)
    {
        glm::vec3 dir = cam.front;
        dir.y = 0.0f; // stay horizontal
        if (glm::length(dir) > 0.0001f)
            dir = glm::normalize(dir);

        p.pos += dir * 1.0f;
        p.vel = glm::vec3(0.0f);
    }

    // ---- movement input ----
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

    // ---- CS-style sliding ----

    // X axis
    p.pos.x += delta.x;
    if (playerBoxHitsWorld(p, world))
        p.pos.x -= delta.x;

    // Z axis
    p.pos.z += delta.z;
    if (playerBoxHitsWorld(p, world))
        p.pos.z -= delta.z;

    // ---- gravity ----
    p.vel.y += PHYS.gravity * dt;
    float dy = p.vel.y * dt;

    p.pos.y += dy;
    if (playerBoxHitsWorld(p, world))
    {
        p.pos.y -= dy;

        if (p.vel.y < 0.0f)
            p.onGround = true;

        p.vel.y = 0.0f;
    }
    else
    {
        p.onGround = false;
    }

    // jump i think 
    if (glfwGetKey(win, GLFW_KEY_SPACE) && p.onGround)
    {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }


    // ---- collisions ----
    // p.onGround = false;

    // for (int pass = 0; pass < 2; ++pass)
    // {
    //     // do we do this twice i dont know 
    //     glm::vec3 feetSphereCenter = p.pos + glm::vec3(0.0f, PLAYER_RADIUS, 0.0f);
    //     glm::vec3 midSphereCenter    = p.pos + glm::vec3(0.0f, PLAYER_HEIGHT * 0.5f, 0.0f);
    //     // matbe dont subtract the radius? hmm 
    //     glm::vec3 headSphereCenter    = p.pos + glm::vec3(0.0f, PLAYER_HEIGHT, 0.0f);

    //     for (size_t i = 0; i + 2 < world.verts.size(); i += 3)
    //     {
    //         glm::vec3 a = world.verts[i + 0].pos;
    //         glm::vec3 b = world.verts[i + 1].pos;
    //         glm::vec3 c = world.verts[i + 2].pos;

    //         collideSphere(p, feetSphereCenter, PLAYER_RADIUS, a, b, c);
    //         collideSphere(p, midSphereCenter,  PLAYER_RADIUS, a, b, c);
    //         collideSphere(p, headSphereCenter, PLAYER_RADIUS, a, b, c);
    //     }
    // }
}