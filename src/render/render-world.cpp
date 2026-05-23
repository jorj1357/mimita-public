// C:\important\mimita-priv-v8\src\render\render-world.cpp
//
// World render pass.
//
// The current map path is GLB-first:
// - loadWorldFromGLB() fills world.mesh.verts and world.mesh.batches.
// - Each Vertex contains position, normal, and UV.
// - Each batch points at one material texture, so the renderer binds the texture
//   once and draws that range of triangles.
//
// This keeps the render path simple and inspectable. It is not a scene graph renderer;
// Blender node transforms are baked into vertices by the loader.

#include "render-world.h"

#include <cstdio>
#include <vector>

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

// TWEAKABLE STYLIZED LIGHTING VARIABLES
//
// These are global on purpose. MiMITA's renderer should be easy to tune while
// looking at the game, not hidden behind a large material system.

// Directional light direction. Points from light toward the world.
// Angled light makes floors, walls, and ramps read differently during movement.
glm::vec3 gLightDir = glm::normalize(glm::vec3(-0.45f, -0.35f, -0.85f));

// Higher = brighter dark areas.
// Lower = moodier shadows, but can hurt fast movement readability.
float gAmbientStrength = 0.34f;

// Higher = stronger separation between surfaces facing/away from light.
float gDiffuseStrength = 0.82f;

// Higher = darker silhouettes and grazing angles.
float gEdgeDarkness = 0.28f;

// Higher = wider edge darkening band.
float gEdgeWidth = 1.8f;

// Higher = stronger fake cavity/contact darkness.
float gAODarkness = 0.22f;

// Higher = sharper fake AO transition.
float gAOContrast = 1.2f;

// Higher = punchier texture colors.
float gTextureContrast = 1.12f;

// Higher = globally brighter texture sampling before lighting.
float gTextureBrightness = 1.0f;

GLuint gVao = 0;
GLuint gVbo = 0;
size_t gBuiltVertCount = (size_t)-1;
size_t gBuiltBatchCount = (size_t)-1;

void uploadMeshIfNeeded(const Mesh& mesh)
{
    if (gVao && gBuiltVertCount == mesh.verts.size() && gBuiltBatchCount == mesh.batches.size())
        return;

    printf("[RENDER] uploading GLB world mesh verts=%zu triangles=%zu batches=%zu\n",
           mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());

    if (!gVao) glGenVertexArrays(1, &gVao);
    if (!gVbo) glGenBuffers(1, &gVbo);

    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW);

    // location 0 = position. This is the 3D point.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    // location 1 = UV. UVs tell the shader where to sample the material texture.
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);

    // location 2 = normal. Normals drive the stylized lighting and AO.
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    gBuiltVertCount = mesh.verts.size();
    gBuiltBatchCount = mesh.batches.size();
}

void setUniforms(GLuint shader)
{
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glUniform1i(glGetUniformLocation(shader, "uDebugView"), DebugVis::shaderDebugView());

    glUniform3fv(glGetUniformLocation(shader, "uLightDir"), 1, &gLightDir.x);
    glUniform1f(glGetUniformLocation(shader, "uAmbientStrength"), gAmbientStrength);
    glUniform1f(glGetUniformLocation(shader, "uDiffuseStrength"), gDiffuseStrength);
    glUniform1f(glGetUniformLocation(shader, "uEdgeDarkness"), gEdgeDarkness);
    glUniform1f(glGetUniformLocation(shader, "uEdgeWidth"), gEdgeWidth);
    glUniform1f(glGetUniformLocation(shader, "uAODarkness"), gAODarkness);
    glUniform1f(glGetUniformLocation(shader, "uAOContrast"), gAOContrast);
    glUniform1f(glGetUniformLocation(shader, "uTextureContrast"), gTextureContrast);
    glUniform1f(glGetUniformLocation(shader, "uTextureBrightness"), gTextureBrightness);
}

}

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

    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &proj[0][0]);
    setUniforms(shader);

    glBindVertexArray(gVao);
    glActiveTexture(GL_TEXTURE0);

    if (!world.mesh.batches.empty())
    {
        for (const Mesh::Batch& batch : world.mesh.batches)
        {
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
}
