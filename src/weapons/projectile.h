#pragma once
#include <glm/glm.hpp>
#include "renderer/renderer.h"

struct Projectile {
    glm::vec3 pos;
    glm::vec3 vel;
    float life = 3.0f;

    void update(float dt);
    void draw(Renderer& renderer, const glm::mat4& view, const glm::mat4& proj);
};
