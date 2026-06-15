#include "render-world.h"

#include <cstdio>
#include <cstddef>
#include <limits>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "debug/gl-debug.h"
#include "renderer/renderer.h"
#include "map/map_common.h"
#include "world/world.h"
#include "world/texture-store.h"
#include "render/lighting-config.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "devtools/terminal.h"
#include "gui/font-stuff/font-loader.h"
#include "debug/debug-diag.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace {

GLuint gVao = 0;
GLuint gVbo = 0;

size_t gBuiltVertCount = (size_t)-1;
size_t gBuiltBatchCount = (size_t)-1;
std::uint64_t gBuiltRevision = (std::uint64_t)-1;
bool gSolidRedDebug = false;

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

void setUniforms(GLuint shader)
{
    const auto& cfg = LightingConfig::instance();
    const auto& scfg = ShadowConfig::instance();

    setInt(shader, "uUseColor", gSolidRedDebug ? 1 : 0);
    if (gSolidRedDebug)
        glUniform4f(uniformLoc(shader, "uColor"), 1.0f, 0.0f, 0.0f, 1.0f);
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

    // Shadow uniforms
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
        // Restore active texture unit to 0 after bindShadowMap changed it to 1
        glActiveTexture(GL_TEXTURE0);
    }
}

} // namespace

void setWorldSolidRedDebug(bool enabled) { gSolidRedDebug = enabled; }
bool worldSolidRedDebug() { return gSolidRedDebug; }

bool gWorldTextureDebug = false;

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

    const Mesh& mesh = world.mesh;
    size_t drawCalls = 0;
    const bool trace = diagRenderTraceSampling();
    if (trace) {
        GLint polygonMode[2] = {};
        glGetIntegerv(GL_POLYGON_MODE, polygonMode);
        const auto& cfg = LightingConfig::instance();
        printf("[WORLD] batches=%zu vertices=%zu depth=%d cull=%d blend=%d polygonMode=0x%x/0x%x\n",
               mesh.batches.size(), mesh.verts.size(),
               (int)glIsEnabled(GL_DEPTH_TEST), (int)glIsEnabled(GL_CULL_FACE),
               (int)glIsEnabled(GL_BLEND), polygonMode[0], polygonMode[1]);
        printf("[WORLD] uLightDir=(%.3f,%.3f,%.3f) uAmbientStrength=%.3f "
               "uDiffuseStrength=%.3f uTextureBrightness=%.3f "
               "uTextureContrast=%.3f uDebugView=%d\n",
               cfg.lightDir().x, cfg.lightDir().y, cfg.lightDir().z,
               cfg.ambientStrength(), cfg.diffuseStrength(),
               cfg.textureBrightness(), cfg.textureContrast(),
               DebugVis::shaderDebugView());
        const char* uniformNames[] = {
            "uLightDir", "uAmbientStrength", "uDiffuseStrength",
            "uTextureBrightness", "uTextureContrast", "uDebugView"
        };
        for (const char* name : uniformNames)
            printf("[WORLD] uniform %s location=%d\n", name, uniformLoc(shader, name));
    }

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

    glBindVertexArray(0);
    MIMITA_GL_CALL(glUseProgram(0));

    MIMITA_GL_CHECK("renderWorld");
}

void registerWorldTextureCommands()
{
    Terminal::instance().registerCommand({
        "world_texture_debug", "Toggle per-batch texture tracing", "world_texture_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gWorldTextureDebug = !gWorldTextureDebug;
            } else {
                gWorldTextureDebug = args[0] != "0";
            }
            printf("[WORLD TEX] debug=%d\n", (int)gWorldTextureDebug);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "world_texture_list", "Print all loaded textures including font/UI", "world_texture_list",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            t.addLog("=== TEXTURES (TextureStore) ===");
            size_t count = gTextures.map.size();
            char buf[128];
            snprintf(buf, sizeof(buf), "World textures: %zu", count);
            t.addLog(buf);
            for (const auto& pair : gTextures.map) {
                snprintf(buf, sizeof(buf), "  ID %u  %s", pair.second, pair.first.c_str());
                t.addLog(buf);
            }
            t.addLog("--- Font Atlases ---");
            snprintf(buf, sizeof(buf), "  gFontTex=%u pages=%d", gFontTex, (int)gFontPageCount);
            t.addLog(buf);
            for (int i = 0; i < (int)gFontPageCount && i < 8; i++) {
                snprintf(buf, sizeof(buf), "  gFontPages[%d]=%u", i, gFontPages[i]);
                t.addLog(buf);
            }
        },
        "2026-06-15", CommandCategory::Debug
    });
}
