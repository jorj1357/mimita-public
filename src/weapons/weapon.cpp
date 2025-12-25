// C:\important\go away v5\s\mimita-v5\src\weapons\weapon.cpp

#include "weapon.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

void Weapon::update(float dt) {
    if (timer > 0) timer -= dt;
}

void Weapon::shoot(std::vector<Projectile>& projectiles, const glm::vec3& origin, const glm::vec3& dir) {
    if (timer > 0) return;
    Projectile p;
    p.pos = origin + dir * 1.0f;
    p.vel = dir * 20.0f;
    projectiles.push_back(p);
    timer = cooldown;
}
