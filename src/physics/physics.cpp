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
#include "physics-debug-movement.h"
#include "physics.h"
#include "physics/config.h"
#include "collision-capsule-triangle.h"
#include "world/world.h"              // <-- your World type (idk waht this is dec 172205)
#include "../camera.h"
#include "glm/glm.hpp"
#include <vector>
#include <cstdio>

// do we really define this here or no 
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
    const World& world,
    GLFWwindow* win,
    float dt,
    const Camera& cam)
{

    static float debugTimer = 0.0f;
    debugTimer += dt;

  

    dt = glm::min(dt, 0.033f);

    // ----------------------------
    // INPUT → DESIRED MOTION
    // ----------------------------
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

    glm::vec3 move = wish * PHYS.moveSpeed * dt;

    // ----------------------------
    // GRAVITY (VELOCITY ONLY)
    // ----------------------------
    p.vel.y += PHYS.gravity * dt;
    p.vel.y = glm::max(p.vel.y, -MAX_FALL_SPEED);
    move.y = p.vel.y * dt;

    // no idea where to put debugmovement
    applyDebugMovement(p, win, cam, dt);

    // ----------------------------
    // COLLECT NEARBY TRIANGLES
    // ----------------------------
    std::vector<Triangle> nearby;
    world.getNearbyTriangles(p.pos, nearby);

    // ----------------------------
    // COLLISION 
    // ----------------------------
    Capsule cap0 = playerCapsule(p);
    glm::vec3 resolvedMove = move;
    p.onGround = false;

    for (int pass = 0; pass < 3; pass++) {
        bool hitThisPass = false;
        for (const Triangle& t : nearby) {
            glm::vec3 newMove = collideCapsuleTriangleMove(cap0, resolvedMove, t, p.onGround);
            if (glm::length2(newMove - resolvedMove) > 1e-10f) hitThisPass = true;
            resolvedMove = newMove;
        }
        if (!hitThisPass) break; // stop early if nothing changes
    }

    p.pos += resolvedMove;

    // ----------------------------
    // GROUND PREVENTION FALLBACK
    // ----------------------------
    // dec 17 2025 todo this is cheating 
    // i dont want to just snap to the ground 
    // just work with the blocks we have imported from blender 
    if (!p.onGround && move.y < 0.0f)
    {
        float highestGround = -1e9f;
        bool foundGround = false;

        for (const Triangle& t : nearby)
        {
            if (nearby.empty())
            {
                printf("[WARN] NO TRIANGLES NEAR PLAYER\n");
                continue;
            }
            // simple flat-ground assumption (temporary)
            float y = (t.a.y + t.b.y + t.c.y) / 3.0f;
            if (y <= p.pos.y && y > highestGround)
            {
                highestGround = y;
                foundGround = true;
            }
        }

        if (foundGround)
        {
            float feetY = p.pos.y;
            float nextFeetY = feetY + move.y;

            if (nextFeetY < highestGround)
            {
                move.y = 0.0f;
                p.vel.y = 0.0f;
                p.onGround = true;
            }
        }
    }

    // ----------------------------
    // APPLY MOTION
    // ----------------------------
    p.pos += move;

    // ----------------------------
    // JUMP
    // ----------------------------
    if (glfwGetKey(win, GLFW_KEY_SPACE) && p.onGround)
    {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }

    static float dbg = 0;
    dbg += dt;
    if (dbg > 0.5f)
    {
        dbg = 0;
        printf(
            "[PHYS DEBUG 1] pos(%.2f %.2f %.2f) velY=%.2f ground=%d tris=%zu\n",
            p.pos.x, p.pos.y, p.pos.z,
            p.vel.y,
            p.onGround,
            nearby.size()
        );
    }

    if (debugTimer >= 0.5f)
    {
        debugTimer = 0.0f;
        printf(
            "[PHYS DEBUG 2] pos(%.2f %.2f %.2f) velY=%.2f move(%.2f %.2f %.2f) ground=%d\n",
            p.pos.x, p.pos.y, p.pos.z,
            (double)p.vel.y,
            (double)move.x, (double)move.y, (double)move.z,
            p.onGround ? 1 : 0
        );
    }

    static int lastTris = -1;
    if ((int)nearby.size() != lastTris) {
        lastTris = (int)nearby.size();
        printf("[PHYS] nearby tris=%d pos(%.2f %.2f %.2f)\n",
            lastTris, (double)p.pos.x, (double)p.pos.y, (double)p.pos.z);
    }

}
