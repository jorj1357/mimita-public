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
#include "map/map_loader.h"
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

static GLuint gSkyVao = 0;
static GLuint gSkyVbo = 0;
static size_t gSkyBuiltVertCount = 0;
static size_t gSkyBuiltBatchCount = 0;
static uint64_t gSkyBuiltRevision = 0;

static void uploadSkyIfNeeded(const World& world)
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

// Backface rendering control.
// True  = disable backface culling (interior surfaces visible).
// False = enable backface culling (interior surfaces hidden).
bool gRenderBackfaces = true;

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

void renderSky(const World& world, const Camera& cam)
{
    if (!gRenderer || !gRenderer->shaderProgram || world.skyMesh.verts.empty())
        return;

    auto& mesh = world.skyMesh;
    uploadSkyIfNeeded(world);

    GLuint shader = gRenderer->shaderProgram;
    glUseProgram(shader);

    // Depth disabled: sky renders behind everything
    glDisable(GL_DEPTH_TEST);

    // Model matrix: follow camera position (sky is infinitely distant)
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

    // Manage backface culling so interior surfaces (e.g. holes cut
    // into geometry) are visible.  Default: culling OFF.
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

    setUniforms(shader);

    glBindVertexArray(gVao);

    const Mesh& mesh = world.mesh;
    size_t drawCalls = 0;
    const bool trace = diagRenderTraceSampling();
    if (trace) {
        GLint polygonMode[2] = {};
        glGetIntegerv(GL_POLYGON_MODE, polygonMode);
        GLint cullFaceMode = GL_BACK;
        glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
        const char* cullFaceName =
            cullFaceMode == GL_FRONT ? "GL_FRONT" :
            cullFaceMode == GL_BACK  ? "GL_BACK"  : "GL_FRONT_AND_BACK";
        const auto& cfg = LightingConfig::instance();
        printf("[WORLD] batches=%zu vertices=%zu depth=%d cull=%d cullMode=%s blend=%d polygonMode=0x%x/0x%x\n",
               mesh.batches.size(), mesh.verts.size(),
               (int)glIsEnabled(GL_DEPTH_TEST), (int)glIsEnabled(GL_CULL_FACE),
               cullFaceName,
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

    // Debug: visualize face normals (green = front face, red = back face)
    if (DebugVis::normals())
    {
        const std::vector<Vertex>& verts = mesh.verts;
        for (size_t i = 0; i + 2 < verts.size(); i += 3)
        {
            const glm::vec3& a = verts[i].pos;
            const glm::vec3& b = verts[i + 1].pos;
            const glm::vec3& c = verts[i + 2].pos;

            glm::vec3 centroid = (a + b + c) / 3.0f;
            glm::vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
            glm::vec3 viewDir = glm::normalize(centroid - cam.pos);

            // Approximate front/back: if normal points toward camera, face is visible (front).
            bool towardCamera = glm::dot(faceNormal, viewDir) < 0.0f;
            glm::vec4 color = towardCamera
                ? glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)   // green = front
                : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);   // red   = back

            DebugVis::drawLine(cam, centroid, centroid + faceNormal * 0.4f, color);
        }
    }

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

    Terminal::instance().registerCommand({
        "glb_materials", "Output all GLB material info from last loaded map", "glb_materials",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            dumpGLBMaterials(t);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "glb_textures", "Output all GLB texture/image info from last loaded map", "glb_textures",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            dumpGLBTextures(t);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "glb_lights", "Output imported GLB lights", "glb_lights",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            dumpGLBLights(t);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "glb_validate", "Validate GLB data: missing textures, UVs, materials", "glb_validate",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            validateGLB(t);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "render_backfaces", "Toggle backface culling (0=cull back faces, 1=show all faces)",
        "render_backfaces <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                printf("[RENDER] render_backfaces = %d (0=cull, 1=show)\n", (int)gRenderBackfaces);
                return;
            }
            gRenderBackfaces = args[0] != "0";
            printf("[RENDER] render_backfaces set to %d (0=cull, 1=show)\n", (int)gRenderBackfaces);
        },
        "2026-06-18", CommandCategory::Debug
    });
}

// ---- GLB Debug dump functions ----

void dumpGLBMaterials(Terminal& t)
{
    if (!gGLBDebug.loaded)
    {
        t.addLog("[GLB] No GLB data loaded. Load a map first.");
        return;
    }
    char buf[256];
    t.addLog("=== GLB MATERIALS ===");
    snprintf(buf, sizeof(buf), "Total: %zu materials", gGLBDebug.materials.size());
    t.addLog(buf);
    for (const auto& m : gGLBDebug.materials)
    {
        snprintf(buf, sizeof(buf), "  [%d] %s", m.index, m.name.c_str());
        t.addLog(buf);
        if (m.hasTexture)
        {
            snprintf(buf, sizeof(buf), "       baseColorTexture = %d", m.baseColorTextureIndex);
            t.addLog(buf);
        }
        if (m.hasColorFactor)
        {
            snprintf(buf, sizeof(buf), "       baseColorFactor = [%.2f, %.2f, %.2f, %.2f]",
                     m.baseColorFactor[0], m.baseColorFactor[1],
                     m.baseColorFactor[2], m.baseColorFactor[3]);
            t.addLog(buf);
        }
        if (m.hasKhrTextureTransform)
        {
            snprintf(buf, sizeof(buf), "       KHR_texture_transform: offset=(%.2f,%.2f) scale=(%.2f,%.2f)",
                     m.texTransformOffset[0], m.texTransformOffset[1],
                     m.texTransformScale[0], m.texTransformScale[1]);
            t.addLog(buf);
        }
        if (!m.hasTexture && !m.hasColorFactor)
        {
            t.addLog("       *** NO TEXTURE AND NO COLOR FACTOR ***");
        }
    }
}

void dumpGLBTextures(Terminal& t)
{
    if (!gGLBDebug.loaded)
    {
        t.addLog("[GLB] No GLB data loaded. Load a map first.");
        return;
    }
    char buf[256];
    t.addLog("=== GLB TEXTURES / IMAGES ===");
    snprintf(buf, sizeof(buf), "Total: %zu images", gGLBDebug.images.size());
    t.addLog(buf);
    for (const auto& img : gGLBDebug.images)
    {
        snprintf(buf, sizeof(buf), "  [%d] name=%s size=%dx%d components=%d embedded=%s",
                 img.index, img.name.c_str(), img.width, img.height, img.components,
                 img.embedded ? "yes" : "no");
        t.addLog(buf);
        if (!img.uri.empty())
        {
            snprintf(buf, sizeof(buf), "       uri=%s", img.uri.c_str());
            t.addLog(buf);
        }
    }
}

void dumpGLBLights(Terminal& t)
{
    if (!gGLBDebug.loaded)
    {
        t.addLog("[GLB] No GLB data loaded. Load a map first.");
        return;
    }
    char buf[256];
    t.addLog("=== GLB LIGHTS ===");
    if (gGLBDebug.lights.empty())
    {
        t.addLog("  No lights imported from GLB.");
        return;
    }
    snprintf(buf, sizeof(buf), "Total: %zu lights", gGLBDebug.lights.size());
    t.addLog(buf);
    for (const auto& l : gGLBDebug.lights)
    {
        snprintf(buf, sizeof(buf), "  %s type=%s intensity=%.2f range=%.2f",
                 l.name.c_str(), l.type.c_str(), l.intensity, l.range);
        t.addLog(buf);
        snprintf(buf, sizeof(buf), "       pos=(%.2f, %.2f, %.2f) dir=(%.2f, %.2f, %.2f)",
                 l.position[0], l.position[1], l.position[2],
                 l.direction[0], l.direction[1], l.direction[2]);
        t.addLog(buf);
        snprintf(buf, sizeof(buf), "       color=(%.2f, %.2f, %.2f)",
                 l.color[0], l.color[1], l.color[2]);
        t.addLog(buf);
        if (l.type == "spot")
        {
            snprintf(buf, sizeof(buf), "       innerCone=%.2f outerCone=%.2f",
                     l.innerConeAngle, l.outerConeAngle);
            t.addLog(buf);
        }
    }
}

void validateGLB(Terminal& t)
{
    if (!gGLBDebug.loaded)
    {
        t.addLog("[GLB] No GLB data loaded. Load a map first.");
        return;
    }
    char buf[256];
    int issues = 0;
    t.addLog("=== GLB VALIDATION ===");
    snprintf(buf, sizeof(buf), "Meshes: %d  Materials: %zu  Images: %zu  Lights: %zu",
             gGLBDebug.meshCount, gGLBDebug.materials.size(),
             gGLBDebug.images.size(), gGLBDebug.lights.size());
    t.addLog(buf);

    // Check materials
    for (const auto& m : gGLBDebug.materials)
    {
        if (!m.hasTexture && !m.hasColorFactor)
        {
            snprintf(buf, sizeof(buf), "  [ISSUE] Material %d (%s): no texture and no color factor",
                     m.index, m.name.c_str());
            t.addLog(buf);
            issues++;
        }
    }

    // Check images
    for (const auto& img : gGLBDebug.images)
    {
        if (img.width <= 0 || img.height <= 0)
        {
            snprintf(buf, sizeof(buf), "  [ISSUE] Image %d (%s): invalid dimensions %dx%d",
                     img.index, img.name.c_str(), img.width, img.height);
            t.addLog(buf);
            issues++;
        }
    }

    if (issues == 0)
        t.addLog("  No issues found.");
    else
    {
        snprintf(buf, sizeof(buf), "  Total issues: %d", issues);
        t.addLog(buf);
    }
}
