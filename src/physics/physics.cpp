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

// dec 19 2025 we are z up not y up

#include <glm/gtc/matrix_transform.hpp>
#include "glm/glm.hpp"
#include <vector>
#include <cstdio>
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

// dec 19 2025 z is up not y
static Capsule playerCapsule(const Player& p)
{
    Capsule c;
    c.r = PLAYER_RADIUS;
    c.a = p.pos + glm::vec3(0.0f, 0.0f, c.r);
    c.b = p.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT - c.r);
    return c;
}

void updatePhysics(
    Player& p,
    const World& world,
    GLFWwindow* win,
    float dt,
    const Camera& cam)
{

    // prevent phisics calc from crashing everthing and killing us 
    dt = glm::min(dt, 0.033f);

    // DEC 18 2025 THIS WORKS IT STAY HERE 
    applyDebugMovement(p, win, cam, dt);

    // ----------------------------
    // INPUT → DESIRED MOTION
    // ----------------------------
    glm::vec3 wish(0.0f);

    // z is up, flatten for z 
    glm::vec3 forward = cam.front;
    forward.z = 0.0f;
    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);

    // again z is up not y dec 19 2025 
    glm::vec3 right = glm::cross(forward, glm::vec3(0,0,1));

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
    // z is up now, dec 19 2025 
    p.vel.z += PHYS.gravity * dt;
    p.vel.z = glm::max(p.vel.z, -MAX_FALL_SPEED);
    move.z = p.vel.z * dt;

    // ----------------------------
    // COLLECT NEARBY Parts... json.. not triangles dec 18 2025
    // ----------------------------
    std::vector<Block*> nearbyBlocks;
    std::vector<Sphere*> nearbySpheres;

    world.getNearby(p.pos, nearbyBlocks, nearbySpheres);

    // ----------------------------
    // COLLISION 
    // ----------------------------
    Capsule baseCap = playerCapsule(p);

    p.groundGrace -= dt;

    p.onGround = false; // start false for this frame

    // sweeping 
    float moveLen = glm::length(move);
    int steps = glm::max(1, (int)ceil(moveLen / PLAYER_RADIUS));

    // 3 passes best 
    glm::vec3 remaining = move;
    glm::vec3 totalMove(0.0f);

    // collision loop

    for (int step = 0; step < steps; step++) {
        glm::vec3 stepMove = remaining / float(steps - step);

        Capsule cap = baseCap;
        cap.a += totalMove;
        cap.b += totalMove;

        for (int pass = 0; pass < 3; pass++) {
            for (Block* b : nearbyBlocks) {
                glm::vec3 correction = collideCapsuleOBBMove(
                    cap,
                    stepMove,
                    b->pos,
                    // test no size mult
                    // b->size * BLOCK_PHYS_MULT,
                    b->size,
                    b->rot,
                    p.onGround
                );

                stepMove = correction;
            }
        }

        totalMove += stepMove;
        remaining -= stepMove;

        if (p.onGround && p.vel.z < 0.0f) {
            p.groundGrace = COLLISIONS_GRACE_PERIOD;
            p.vel.z = 0.0f;
            remaining.z = 0.0f;
        }
    }

    p.pos += totalMove;

    // ----------------------------
    // JUMP
    // ----------------------------
    // todo allow holding space to make auto hop 
    static bool lastSpace = false;
    bool spaceNow = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;

    // z is up now dec 19 2025 
    if (spaceNow && !lastSpace && p.onGround) {
        p.vel.z = PHYS.jumpStrength;
        p.onGround = false;
    }

    lastSpace = spaceNow;
}
