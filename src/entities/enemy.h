#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "renderer/renderer.h"

struct Enemy {
    glm::vec3 pos{5.0f, 0.5f, 5.0f};
    glm::vec3 vel{0.0f};
    float moveSpeed = 4.0f;
    float jumpStrength = 6.0f;
    float gravity = -9.81f;
    bool onGround = true;
    float jumpTimer = 0.0f;
    float directionTimer = 0.0f;
    glm::vec3 targetDir{0.0f};

    void update(float dt);
    void draw(Renderer& renderer, const glm::mat4& view, const glm::mat4& proj);
};
