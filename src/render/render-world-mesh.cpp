#include "render-world.h"

#include <cstddef>
#include <cmath>
#include <limits>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "renderer/renderer.h"
#include "map/map_common.h"
#include "world/world.h"
#include "render/lighting-config.h"
#include "render/dynamic-light.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "debug/debug-diag.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

GLuint gVao = 0;
GLuint gVbo = 0;

size_t gBuiltVertCount = (size_t)-1;
size_t gBuiltBatchCount = (size_t)-1;
std::uint64_t gBuiltRevision = (std::uint64_t)-1;

namespace {

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

} // anonymous namespace

void uploadMeshIfNeeded(const World& world)
{
    const Mesh& mesh = world.mesh;
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

    gBuiltVertCount = mesh.verts.size();
    gBuiltBatchCount = mesh.batches.size();
    gBuiltRevision = world.renderRevision;

    MIMITA_GL_CHECK("uploadMeshIfNeeded complete");
}

static bool gTexBreatheEnabled = false;
static float gTexBreatheOriginalTime = 0.0f;
static float gTexBreatheElapsed = 0.0f;
static bool gTexBreatheRestoring = false;
static float gTexBreatheRestoreElapsed = 0.0f;
static constexpr float TEXBREATHE_RESTORE_DURATION = 1.0f;

void setTexBreatheEnabled(bool enabled)
{
    if (enabled == gTexBreatheEnabled) return;
    if (enabled && !gTexBreatheEnabled) {
        gTexBreatheEnabled = true;
        gTexBreatheElapsed = 0.0f;
        gTexBreatheRestoring = false;
        Debug::log(Debug::Category::General, "[TEXBREATHE] enabled=1\n");
    } else if (!enabled && gTexBreatheEnabled) {
        gTexBreatheEnabled = false;
        gTexBreatheRestoring = true;
        gTexBreatheRestoreElapsed = 0.0f;
        Debug::log(Debug::Category::General, "[TEXBREATHE] disabled restoringOriginalValues\n");
    }
}

bool texBreatheEnabled() { return gTexBreatheEnabled || gTexBreatheRestoring; }

void setUniforms(GLuint shader, const glm::vec3& cameraPos)
{
    const auto& cfg = LightingConfig::instance();
    const auto& scfg = ShadowConfig::instance();

    setInt(shader, "uUseColor", worldSolidRedDebug() ? 1 : 0);
    if (worldSolidRedDebug())
        glUniform4f(uniformLoc(shader, "uColor"), 1.0f, 0.0f, 1.0f, 1.0f);
    double now = glfwGetTime();
    if (gTexBreatheRestoring) {
        gTexBreatheRestoreElapsed += 0.016f; // approximate dt
        float t = std::clamp(gTexBreatheRestoreElapsed / TEXBREATHE_RESTORE_DURATION, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);
        setFloat(shader, "uTexBreatheTime", gTexBreatheElapsed * (1.0f - t));
        if (gTexBreatheRestoreElapsed >= TEXBREATHE_RESTORE_DURATION) {
            gTexBreatheRestoring = false;
            Debug::log(Debug::Category::General, "[TEXBREATHE] restoreComplete\n");
        }
    }
    if (gTexBreatheEnabled) {
        gTexBreatheElapsed += 0.016f;
        setFloat(shader, "uTexBreatheTime", gTexBreatheElapsed);
    }
    setInt(shader, "uTexBreatheEnabled", (gTexBreatheEnabled || gTexBreatheRestoring) ? 1 : 0);
    setFloat(shader, "uTime", (float)now);
    setInt(shader, "uTex", 0);
    setInt(shader, "uDebugView", DebugVis::shaderDebugView());

    setVec3(shader, "uTint", glm::vec3(1.0f));
    setVec3(shader, "uLightDir", cfg.lightDir());
    setFloat(shader, "uAmbientStrength", cfg.ambientStrength());
    setFloat(shader, "uDiffuseStrength", cfg.diffuseStrength());
    setFloat(shader, "uEdgeDarkness", cfg.edgeDarkness());
    setFloat(shader, "uEdgeWidth", cfg.edgeWidth());
    setFloat(shader, "uAODarkness", cfg.aoDarkness());
    setFloat(shader, "uAOContrast", cfg.aoContrast());
    setFloat(shader, "uTextureContrast", cfg.textureContrast());
    setFloat(shader, "uTextureBrightness", cfg.textureBrightness());

    // Dynamic point lights
    const auto& dlmgr = DynamicLightManager::instance();
    DynamicLightManager::SubmitResult dlights = dlmgr.submitToShader(cameraPos, DynamicLightManager::MAX_SUBMIT);
    setInt(shader, "uDynamicLightCount", dlights.count);
    if (dlights.count > 0) {
        Debug::logThrottled(Debug::Category::Render, "dlight-upload", 1.0f,
            "[DYNAMIC LIGHT] upload count=%d\n", dlights.count);
    }
    for (int i = 0; i < dlights.count; ++i) {
        char posName[32], colName[32], radName[32], intName[32];
        snprintf(posName, sizeof(posName), "uDynamicLightPos[%d]", i);
        snprintf(colName, sizeof(colName), "uDynamicLightColor[%d]", i);
        snprintf(radName, sizeof(radName), "uDynamicLightRadius[%d]", i);
        snprintf(intName, sizeof(intName), "uDynamicLightIntensity[%d]", i);
        setVec3(shader, posName, dlights.lights[i].position);
        setVec3(shader, colName, dlights.lights[i].color);
        setFloat(shader, radName, dlights.lights[i].radius);
        setFloat(shader, intName, dlights.lights[i].intensity);
    }

    bool shadowsEnabled = scfg.enabled() && shadowDepthTex() != 0;
    setInt(shader, "uShadowsEnabled", shadowsEnabled ? 1 : 0);
    if (shadowsEnabled) {
        bindShadowMap(1);
        setInt(shader, "uShadowMap", 1);
        setMat4(shader, "uShadowMatrix", shadowMatrix());
        setFloat(shader, "uShadowDarkness", scfg.shadowDarkness());
        setFloat(shader, "uShadowBias", scfg.shadowBias());
        setFloat(shader, "uShadowSoftness", scfg.shadowSoftness());
        glm::vec3 tint = scfg.shadowTint();
        setVec3(shader, "uShadowTint", tint);
        glActiveTexture(GL_TEXTURE0);
    }
}

bool gWorldTextureDebug = false;
bool gRenderBackfaces = true;
bool gSolidRedDebug = false;

void setWorldSolidRedDebug(bool enabled) { gSolidRedDebug = enabled; }
bool worldSolidRedDebug() { return gSolidRedDebug; }

void renderWorldMeshBatches(const World& world, const Camera& cam)
{
    if (gRenderBackfaces)
        glDisable(GL_CULL_FACE);
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    GLuint shader = gRenderer->shaderProgram;
    MIMITA_GL_CALL(glUseProgram(shader));

    glm::mat4 model(1.0f);
    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);

    setMat4(shader, "model", model);
    setMat4(shader, "view", view);
    setMat4(shader, "projection", proj);

    setUniforms(shader, cam.pos);

    glBindVertexArray(gVao);

    const Mesh& mesh = world.mesh;
    size_t drawCalls = 0;

    for (const auto& batch : mesh.batches)
    {
        GLuint tex = batch.texture ? batch.texture : gTextures.get("default");
        if (gWorldTextureDebug)
        {
            printf("[WORLD TEX] batch first=%zu count=%zu texture=%u material=%s\n",
                   batch.first, batch.count, tex, batch.materialName.c_str());
        }
        MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));

        MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
        ++drawCalls;
    }
    diagRenderWorldCounts(mesh.batches.size(), mesh.verts.size(), drawCalls);
}
