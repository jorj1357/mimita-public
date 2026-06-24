#include "render/render-world.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "world/world.h"

extern GLuint gVao;

void uploadMeshIfNeeded(const World& world);

void renderWorldDepth(const World& world, GLuint shadowShader, const glm::mat4& lightMVP)
{
    if (world.mesh.verts.empty()) return;
    uploadMeshIfNeeded(world);

    glUseProgram(shadowShader);
    if (shadowShader) {
        GLint loc = glGetUniformLocation(shadowShader, "uLightMVP");
        if (loc >= 0)
            glUniformMatrix4fv(loc, 1, GL_FALSE, &lightMVP[0][0]);
    }

    glBindVertexArray(gVao);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (const auto& batch : world.mesh.batches) {
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}
