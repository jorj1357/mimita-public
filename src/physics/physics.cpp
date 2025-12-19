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
#include "physics-types.h"
#include "physics/collision-capsule-aabb.h"
#include "physics/collision-capsule-obb.h"
#include "physics/config.h"
// #include "collision-capsule-triangle.h"
#include "world/world.h"
#include "world/world-mesh.h"
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

    // prevent phisics calc from crashing everthing and killing us 
    dt = glm::min(dt, 0.033f);

    // DEC 18 2025 THIS WORKS IT STAY HERE 
    applyDebugMovement(p, win, cam, dt);

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

    // ----------------------------
    // COLLECT NEARBY Parts... json.. not triangles dec 18 2025
    // ----------------------------
    std::vector<Block*> nearbyBlocks;
    std::vector<Sphere*> nearbySpheres;

    world.getNearby(p.pos, nearbyBlocks, nearbySpheres);

    // ----------------------------
    // COLLISION 
    // ----------------------------
    // bool wasOnGround = p.onGround;
    Capsule cap0 = playerCapsule(p);
    glm::vec3 resolvedMove = move;
    p.onGround = false; // start false for this frame

    // 3 passes best 
    for (int pass = 0; pass < 3; pass++) {
        for (Block* b : nearbyBlocks) {

            glm::vec3 boxCenter = toYUp(b->pos);
            glm::vec3 boxSize   = glm::vec3(
                b->size.x,
                b->size.z,
                b->size.y
            ) * BLOCK_PHYS_MULT;

            glm::mat3 rot =
                glm::mat3(
                    glm::rotate(glm::mat4(1.0f), glm::radians(b->rot.z), glm::vec3(0,0,1)) *
                    glm::rotate(glm::mat4(1.0f), glm::radians(b->rot.y), glm::vec3(0,1,0)) *
                    glm::rotate(glm::mat4(1.0f), glm::radians(b->rot.x), glm::vec3(1,0,0))
                );

            glm::vec3 newMove = collideCapsuleOBBMove(
                cap0,
                resolvedMove,
                boxCenter,
                boxSize * 0.5f,
                rot,
                p.onGround
            );

            static FILE* f = nullptr;
            if (!f) {
                f = fopen("run.log", "w");
            }

            fprintf(
                f,
                "BLOCK pos engine: %.2f %.2f %.2f  size: %.2f %.2f %.2f\n",
                boxCenter.x, boxCenter.y, boxCenter.z,
                boxSize.x, boxSize.y, boxSize.z
            );
            fprintf(
                f,
                "CAP a: %.2f %.2f %.2f  b: %.2f %.2f %.2f\n",
                cap0.a.x, cap0.a.y, cap0.a.z,
                cap0.b.x, cap0.b.y, cap0.b.z
            );

            fflush(f);

            resolvedMove = newMove;
        }
    }

    p.pos += resolvedMove;

    // ----------------------------
    // JUMP
    // ----------------------------
    static bool lastSpace = false;
    bool spaceNow = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;

    if (spaceNow && !lastSpace && p.onGround) {
        p.vel.y = PHYS.jumpStrength;
        p.onGround = false;
    }

    lastSpace = spaceNow;
}
