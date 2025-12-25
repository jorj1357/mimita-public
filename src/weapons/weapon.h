#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "projectile.h"

struct Weapon {
    float cooldown = 0.3f;
    float timer = 0.0f;

    void update(float dt);
    void shoot(std::vector<Projectile>& projectiles, const glm::vec3& origin, const glm::vec3& direction);
};
