#include "render-world.h"

#include <cstdio>
#include <cstddef>
#include <limits>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "debug/gl-debug.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "world/texture-store.h"
#include "render/lighting-config.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace {

GLuint gVao = 0;
GLuint gVbo = 0;

size_t gBuiltVertCount = (size_t)-1;
size_t gBuiltBatchCount = (size_t)-1;
std::uint64_t gBuiltRevision = (std::uint64_t)-1;

GLint uniformLoc(GLuint shader, const char* name)
{
    return glGetUniformLocation(shader, name);
}

void setInt(GLuint shader, const char* name, int v)
{
    glUniform1i(uniformLoc(shader, name), v);
}

void setFloat(GLuint shader, const char* name, float v)
{
    glUniform1f(uniformLoc(shader, name), v);
}

void setVec3(GLuint shader, const char* name, glm::vec3 v)
{
    glUniform3f(uniformLoc(shader, name), v.x, v.y, v.z);
}

void setMat4(GLuint shader, const char* name, const glm::mat4& m)
{
    glUniformMatrix4fv(uniformLoc(shader, name), 1, GL_FALSE, &m[0][0]);
}

struct MeshVertex
{
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

void uploadMeshIfNeeded(const World& world)
{
    const WorldMesh& mesh = world.mesh;
    if (mesh.verts.empty()) return;

    if (mesh.verts.size() == gBuiltVertCount &&
        mesh.batches.size() == gBuiltBatchCount &&
        world.renderRevision == gBuiltRevision)
        return;

    if (!gVao) {
        glGenVertexArrays(1, &gVao);
        glGenBuffers(1, &gVbo);
    }

    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh.verts.size() * sizeof(MeshVertex),
                 mesh.verts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, nx));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    gBuiltVertCount = mesh.verts.size();
    gBuiltBatchCount = mesh.batches.size();
    gBuiltRevision = world.renderRevision;

    MIMITA_GL_CHECK("uploadMeshIfNeeded complete");
}

void setUniforms(GLuint shader)
{
    const auto& cfg = LightingConfig::instance();

    setInt(shader, "uUseColor", 0);
    setInt(shader, "uTex", 0);
    setInt(shader, "uDebugView", DebugVis::shaderDebugView());

    setVec3(shader, "uLightDir", cfg.lightDir());
    setFloat(shader, "uAmbientStrength", cfg.ambientStrength());
    setFloat(shader, "uDiffuseStrength", cfg.diffuseStrength());
    setFloat(shader, "uEdgeDarkness", cfg.edgeDarkness());
    setFloat(shader, "uEdgeWidth", cfg.edgeWidth());
    setFloat(shader, "uAODarkness", cfg.aoDarkness());
    setFloat(shader, "uAOContrast", cfg.aoContrast());
    setFloat(shader, "uTextureContrast", cfg.textureContrast());
    setFloat(shader, "uTextureBrightness", cfg.textureBrightness());
}

} // namespace

void renderWorld(const World& world, const Camera& cam)
{
    if (!gRenderer || !gRenderer->shaderProgram)
    {
        printf("[RENDER WARNING] renderWorld skipped: renderer/shader missing\n");
        return;
    }

    if (world.mesh.verts.empty())
    {
        static bool printed = false;
        if (!printed)
        {
            printf("[RENDER WARNING] world mesh empty. GLB did not load or has no triangles.\n");
            printed = true;
        }
        return;
    }

    MIMITA_GL_CLEAR_STAGE("renderWorld");
    uploadMeshIfNeeded(world);

    GLuint shader = gRenderer->shaderProgram;
    MIMITA_GL_CALL(glUseProgram(shader));

    glm::mat4 model(1.0f);
    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);

    setMat4(shader, "model", model);
    setMat4(shader, "view", view);
    setMat4(shader, "projection", proj);

    setUniforms(shader);

    glBindVertexArray(gVao);

    const WorldMesh& mesh = world.mesh;
    GLuint texId = gTextures.getTexture();

    for (const auto& batch : mesh.batches)
    {
        if (texId) {
            MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, texId));
        }

        MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.start, (GLsizei)batch.count));
    }

    glBindVertexArray(0);
    MIMITA_GL_CALL(glUseProgram(0));

    MIMITA_GL_CHECK("renderWorld");
}
