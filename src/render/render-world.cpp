// C:\important\mimita-priv-v8\src\render\render-world.cpp

#include "render-world.h"

#include <cstdio>
#include <cstddef>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace {

// Easy lighting tuning.
// Shader expects these as uniforms.
glm::vec3 gLightDir = glm::normalize(glm::vec3(-0.35f, -0.10f, -1.0f));

float gAmbientStrength   = 0.72f;
float gDiffuseStrength   = 0.45f;

float gEdgeDarkness      = 0.10f;
float gEdgeWidth         = 1.20f;

float gAODarkness        = 0.05f;
float gAOContrast        = 0.80f;

float gTextureContrast   = 1.02f;
float gTextureBrightness = 1.35f;

GLuint gVao = 0;
GLuint gVbo = 0;

size_t gBuiltVertCount = (size_t)-1;
size_t gBuiltBatchCount = (size_t)-1;

void checkGl(const char* where)
{
    GLenum err = glGetError();
    while (err != GL_NO_ERROR)
    {
        printf("[OPENGL ERROR] %s err=0x%X\n", where, err);
        err = glGetError();
    }
}

GLint uniformLoc(GLuint shader, const char* name)
{
    return glGetUniformLocation(shader, name);
}

void setMat4(GLuint shader, const char* name, const glm::mat4& m)
{
    GLint loc = uniformLoc(shader, name);
    if (loc != -1)
        glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}

void setInt(GLuint shader, const char* name, int v)
{
    GLint loc = uniformLoc(shader, name);
    if (loc != -1)
        glUniform1i(loc, v);
}

void setFloat(GLuint shader, const char* name, float v)
{
    GLint loc = uniformLoc(shader, name);
    if (loc != -1)
        glUniform1f(loc, v);
}

void setVec3(GLuint shader, const char* name, const glm::vec3& v)
{
    GLint loc = uniformLoc(shader, name);
    if (loc != -1)
        glUniform3fv(loc, 1, &v.x);
}

bool batchLooksValid(const Mesh& mesh, const Mesh::Batch& batch)
{
    if (batch.count == 0)
        return false;

    if (batch.first >= mesh.verts.size())
        return false;

    if (batch.first + batch.count > mesh.verts.size())
        return false;

    return true;
}

void uploadMeshIfNeeded(const Mesh& mesh)
{
    if (gVao && gBuiltVertCount == mesh.verts.size() && gBuiltBatchCount == mesh.batches.size())
        return;

    printf("[RENDER] uploading GLB world mesh verts=%zu triangles=%zu batches=%zu\n",
           mesh.verts.size(),
           mesh.verts.size() / 3,
           mesh.batches.size());

    if (!gVao)
        glGenVertexArrays(1, &gVao);

    if (!gVbo)
        glGenBuffers(1, &gVbo);

    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        (GLsizeiptr)(mesh.verts.size() * sizeof(Vertex)),
        mesh.verts.data(),
        GL_STATIC_DRAW
    );

    // Must match shaders/basic.vert:
    // layout(location = 0) in vec3 aPos;
    // layout(location = 1) in vec2 aUV;
    // layout(location = 2) in vec3 aNormal;

    // position
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, pos)
    );
    glEnableVertexAttribArray(0);

    // UV
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, uv)
    );
    glEnableVertexAttribArray(1);

    // normal
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, normal)
    );
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    gBuiltVertCount = mesh.verts.size();
    gBuiltBatchCount = mesh.batches.size();

    checkGl("uploadMeshIfNeeded");
}

void setUniforms(GLuint shader)
{
    setInt(shader, "uUseColor", 0);
    setInt(shader, "uTex", 0);
    setInt(shader, "uDebugView", DebugVis::shaderDebugView());

    setVec3(shader, "uLightDir", gLightDir);
    setFloat(shader, "uAmbientStrength", gAmbientStrength);
    setFloat(shader, "uDiffuseStrength", gDiffuseStrength);
    setFloat(shader, "uEdgeDarkness", gEdgeDarkness);
    setFloat(shader, "uEdgeWidth", gEdgeWidth);
    setFloat(shader, "uAODarkness", gAODarkness);
    setFloat(shader, "uAOContrast", gAOContrast);
    setFloat(shader, "uTextureContrast", gTextureContrast);
    setFloat(shader, "uTextureBrightness", gTextureBrightness);
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

    uploadMeshIfNeeded(world.mesh);

    GLuint shader = gRenderer->shaderProgram;
    glUseProgram(shader);

    glm::mat4 model(1.0f);
    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);

    setMat4(shader, "model", model);
    setMat4(shader, "view", view);
    setMat4(shader, "projection", proj);
    setUniforms(shader);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(gVao);

    if (!world.mesh.batches.empty())
    {
        for (const Mesh::Batch& batch : world.mesh.batches)
        {
            if (!batchLooksValid(world.mesh, batch))
            {
                printf("[RENDER WARNING] bad batch skipped first=%zu count=%zu verts=%zu\n",
                       batch.first,
                       batch.count,
                       world.mesh.verts.size());
                continue;
            }

            GLuint tex = batch.texture ? batch.texture : gTextures.get("default");
            glBindTexture(GL_TEXTURE_2D, tex);

            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gTextures.get("default"));
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)world.mesh.verts.size());
    }

    glBindVertexArray(0);

    checkGl("renderWorld");
}