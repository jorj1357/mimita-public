// 2026-10-27
/* purpose
* Generate and draw one smooth textured cylinder mesh. Shared by vehicle
* wheels and future procedural cylindrical props.
* Does NOT own vehicle state or gameplay.
*/
#pragma once

#include <glm/glm.hpp>

class Camera;

struct TexturedCylinderMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertexCount = 0;
};

const TexturedCylinderMesh& getTexturedCylinderMesh(float radius,
                                                    float length,
                                                    int segments);

void drawTexturedCylinder(const Camera& camera, const glm::mat4& model,
                          const TexturedCylinderMesh& mesh,
                          unsigned int texture, const glm::vec3& tint,
                          float alpha = 1.0f);
