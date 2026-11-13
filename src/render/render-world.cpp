#include "render-world.h"
#include "game/resident-state-module.h"

#include <cstdio>
#include <cstddef>
#include <limits>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "config/camera-config.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "debug/gl-debug.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "modules/api/world-rendering-api.h"
#include "terminal/terminal-state.h"
#include "renderer/renderer.h"
#include "map/map_common.h"
#include "map/map_loader.h"
#include "world/world.h"
#include "world/texture-store.h"
#include "world/streamed-world-layer.h"
#include "render/lighting-config.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "devtools/terminal.h"
#include "gui/font-stuff/font-loader.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

extern GLuint gVao;
extern void uploadMeshIfNeeded(const World& world);
extern void renderWorldMeshBatches(const World& world, const Camera& cam);

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

    const GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLint blendSrcRgbWas = GL_ONE, blendDstRgbWas = GL_ZERO;
    GLint blendSrcAlphaWas = GL_ONE, blendDstAlphaWas = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgbWas);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgbWas);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlphaWas);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlphaWas);
    if (CamConfig::instance().data().proximityFadeEnabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    renderWorldMeshBatches(world, cam);

    StreamedWorldLayer::instance().render(cam);

    glBlendFuncSeparate((GLenum)blendSrcRgbWas, (GLenum)blendDstRgbWas,
                        (GLenum)blendSrcAlphaWas, (GLenum)blendDstAlphaWas);
    if (blendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    if (DebugVis::normals())
    {
        const Mesh& mesh = world.mesh;
        const std::vector<Vertex>& verts = mesh.verts;
        for (size_t i = 0; i + 2 < verts.size(); i += 3)
        {
            const glm::vec3& a = verts[i].pos;
            const glm::vec3& b = verts[i + 1].pos;
            const glm::vec3& c = verts[i + 2].pos;

            glm::vec3 centroid = (a + b + c) / 3.0f;
            glm::vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
            glm::vec3 viewDir = glm::normalize(centroid - cam.pos);

            bool towardCamera = glm::dot(faceNormal, viewDir) < 0.0f;
            glm::vec4 color = towardCamera
                ? glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)
                : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

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
        "worldrender",
        "Audit authored box face normals, winding, materials, and culling expectation",
        "worldrender audit_faces <blockId>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2 || args[0] != "audit_faces") {
                Terminal::instance().addLog(
                    "[WORLD RENDER] usage: worldrender audit_faces <blockId>");
                return;
            }
            if (!mimita::game::worldPtr() || !mimita::game::cameraPtr()) {
                Terminal::instance().addLog(
                    "[WORLD RENDER] no world or camera loaded");
                return;
            }
            const std::string objectId = "block:" + args[1];
            const MapGLBObject* object = nullptr;
            for (const MapGLBObject& candidate : mimita::game::worldPtr()->mesh.objects) {
                if (candidate.id == objectId) {
                    object = &candidate;
                    break;
                }
            }
            if (!object) {
                for (const MapGLBObject& candidate : mimita::game::worldPtr()->authoredObjects) {
                    if (candidate.id == objectId) {
                        object = &candidate;
                        break;
                    }
                }
            }
            if (!object) {
                Terminal::instance().addLog(
                    "[WORLD RENDER] no authored block object: " + objectId);
                return;
            }

            static const glm::vec3 normals[6] = {
                {0, 0, 1}, {0, 0, -1}, {1, 0, 0},
                {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
            };
            mimita_world_rendering::FaceAuditV1 moduleFaces[6]{};
            const float cameraPosition[3] = {
                mimita::game::cameraPtr()->pos.x, mimita::game::cameraPtr()->pos.y, mimita::game::cameraPtr()->pos.z};
            const bool moduleHandled = ModuleHost::instance().worldRenderingAnalyzeFaces(
                &object->transform[0][0], cameraPosition, moduleFaces, 6);
            const glm::mat3 normalMatrix =
                glm::transpose(glm::inverse(glm::mat3(object->transform)));

            nlohmann::json faces = nlohmann::json::array();
            size_t outsideCount = 0;
            for (int face = 0; face < 6; ++face) {
                glm::vec3 center(0.0f);
                glm::vec3 normal(0.0f);
                bool cameraOutside = false;
                if (moduleHandled) {
                    center = glm::vec3(moduleFaces[face].center[0],
                                       moduleFaces[face].center[1],
                                       moduleFaces[face].center[2]);
                    normal = glm::vec3(moduleFaces[face].normal[0],
                                       moduleFaces[face].normal[1],
                                       moduleFaces[face].normal[2]);
                    cameraOutside = moduleFaces[face].cameraOutside != 0;
                } else {
                    center = glm::vec3(object->transform *
                        glm::vec4(normals[face] * 0.5f, 1.0f));
                    normal = glm::normalize(normalMatrix * normals[face]);
                    cameraOutside =
                        glm::dot(normal, mimita::game::cameraPtr()->pos - center) > 0.0f;
                }
                if (cameraOutside)
                    ++outsideCount;

                std::string material;
                size_t first = 0;
                size_t count = 0;
                const size_t expectedFirst =
                    object->vertexFirst + static_cast<size_t>(face) * 6;
                for (const Mesh::Batch& batch : mimita::game::worldPtr()->mesh.batches) {
                    if (batch.first < object->vertexFirst ||
                        batch.first >= object->vertexFirst + object->vertexCount)
                        continue;
                    if (expectedFirst >= batch.first &&
                        expectedFirst < batch.first + batch.count) {
                        material = batch.materialName;
                        first = batch.first;
                        count = batch.count;
                        break;
                    }
                }
                faces.push_back({
                    {"index", face},
                    {"normal", {normal.x, normal.y, normal.z}},
                    {"center", {center.x, center.y, center.z}},
                    {"winding", "ccw"},
                    {"material", material},
                    {"batch_first", first},
                    {"batch_count", count},
                    {"camera_outside", cameraOutside},
                });
            }

            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "world-render.face-audit", objectId,
                "controlled six-face culling/winding/material audit", 0,
                nlohmann::json{{"map", MapConfigRegistry::instance().mapId()},
                               {"object", objectId},
                               {"backface_culling_enabled", !gRenderBackfaces},
                               {"backface_toggle", gRenderBackfaces},
                               {"handled_by_world_module", moduleHandled},
                               {"world_module_generation",
                                ModuleHost::instance().activeGeneration("world-rendering")},
                               {"shader_program", gRenderer ? gRenderer->shaderProgram : 0},
                               {"vertex_first", object->vertexFirst},
                               {"vertex_count", object->vertexCount},
                               {"camera_position", {mimita::game::cameraPtr()->pos.x, mimita::game::cameraPtr()->pos.y, mimita::game::cameraPtr()->pos.z}},
                               {"faces_outside_camera", outsideCount},
                               {"faces", faces}},
                __FILE__, __LINE__, __FUNCTION__);

            Terminal::instance().addLog(
                "[WORLD RENDER] face audit " + objectId + ": faces=6 outside=" +
                std::to_string(outsideCount) + " culling=" +
                (gRenderBackfaces ? "off" : "on") +
                " (human visual review remains separate)");
        },
        "2026-11-07", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "world_debug_color", "Force world to solid magenta (diagnose shader/lighting issues)",
        "world_debug_color [0|1]",
        [](const std::vector<std::string>& args) {
            bool val = args.empty() ? !worldSolidRedDebug() : (args[0] != "0");
            setWorldSolidRedDebug(val);
            printf("[WORLD DEBUG] color override = %d\n", (int)val);
            Terminal::instance().addLog(std::string("[WORLD DEBUG] color override = ") + (val ? "1 (magenta)" : "0 (normal)"));
        }
    });

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
