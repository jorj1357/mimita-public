#include "render/render-world.h"

#include <cstddef>
#include <cstdint>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"
#include "debug/gl-debug.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace {

GLuint gSkyVao = 0;
GLuint gSkyVbo = 0;
size_t gSkyBuiltVertCount = 0;
size_t gSkyBuiltBatchCount = 0;
uint64_t gSkyBuiltRevision = 0;

} // anonymous namespace

void setUniforms(GLuint shader, const glm::vec3& cameraPos);

void uploadSkyIfNeeded(const World& world)
{
    const Mesh& mesh = world.skyMesh;
    if (mesh.verts.empty()) return;

    if (mesh.verts.size() == gSkyBuiltVertCount &&
        mesh.batches.size() == gSkyBuiltBatchCount &&
        world.renderRevision == gSkyBuiltRevision)
        return;

    if (!gSkyVao) {
        glGenVertexArrays(1, &gSkyVao);
        glGenBuffers(1, &gSkyVbo);
    }

    glBindVertexArray(gSkyVao);
    glBindBuffer(GL_ARRAY_BUFFER, gSkyVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh.verts.size() * sizeof(Vertex),
                 mesh.verts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    gSkyBuiltVertCount = mesh.verts.size();
    gSkyBuiltBatchCount = mesh.batches.size();
    gSkyBuiltRevision = world.renderRevision;
}

void renderSky(const World& world, const Camera& cam)
{
    if (!gRenderer || !gRenderer->shaderProgram || world.skyMesh.verts.empty())
        return;

    auto& mesh = world.skyMesh;
    uploadSkyIfNeeded(world);

    GLuint shader = gRenderer->shaderProgram;
    glUseProgram(shader);

    glDisable(GL_DEPTH_TEST);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), cam.pos);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &model[0][0]);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &proj[0][0]);

    setUniforms(shader);

    glBindVertexArray(gSkyVao);
    for (const auto& batch : mesh.batches)
    {
        GLuint tex = batch.texture ? batch.texture : gTextures.get("default");
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
    }
    glBindVertexArray(0);
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
}
