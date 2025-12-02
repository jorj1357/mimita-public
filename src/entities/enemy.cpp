#include "enemy.h"
#include <cstdlib>
#include <ctime>
#include <glm/gtc/matrix_transform.hpp>

void Enemy::update(float dt) {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }

    // Change direction every 2–3 seconds
    directionTimer -= dt;
    if (directionTimer <= 0.0f) {
        directionTimer = 2.0f + (rand() % 100) / 50.0f; // 2–4 sec
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        targetDir = glm::normalize(glm::vec3(cos(angle), 0, sin(angle)));
    }

    // Move in direction
    glm::vec3 horizVel = targetDir * moveSpeed;
    vel.x = horizVel.x;
    vel.z = horizVel.z;

    // Jump randomly
    jumpTimer -= dt;
    if (onGround && jumpTimer <= 0.0f) {
        jumpTimer = 2.0f + (rand() % 200) / 100.0f; // every 2–4 sec
        vel.y = jumpStrength;
        onGround = false;
    }

    vel.y += gravity * dt;
    pos += vel * dt;

    // todo nov 6 2025 this too simpler
    if (pos.y < 0.5f) {
        pos.y = 0.5f; // TEMP
    }
}

void Enemy::draw(Renderer& renderer, const glm::mat4& view, const glm::mat4& proj) {
    /*
    todo nov 6 2025
    remove phase out
    drawCube
    drawCubeColored
    remove these
    */
    renderer.drawCubeColored(pos, glm::vec3(1.0f, 0.2f, 0.2f), view, proj);
}
