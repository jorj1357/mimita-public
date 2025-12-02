#include "projectile.h"

void Projectile::update(float dt) {
    pos += vel * dt;
    life -= dt;
}

/*
todo nov 6 2025
remove phase out
    drawCube
    drawCubeColored
remove these
*/
void Projectile::draw(Renderer& renderer, const glm::mat4& view, const glm::mat4& proj) {
    renderer.drawCubeColored(pos, glm::vec3(1, 1, 0.3f), view, proj);
}
