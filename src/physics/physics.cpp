// C:\important\go away v5\s\mimita-v5\src\physics\physics.cpp

#define NOMINMAX
#include "physics/config.h"
#include "physics.h"
#include "../camera.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <iostream>
#include <Windows.h>

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
    p.vel.x = dir.x * PHYS.moveSpeed;
    p.vel.z = dir.z * PHYS.moveSpeed;
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
    float groundY = p.halfExtents().y;
    if (p.pos.y < groundY) {
        p.pos.y = groundY;
        p.vel.y = 0;
        p.onGround = true;
    } else {
        p.onGround = false;
    }

    // respawn check (optional)
    if (p.pos.y < PHYS.deathHeight) p.reset();
}
