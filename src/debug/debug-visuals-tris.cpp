#include "debug/debug-visuals.h"
#include "game/gl-resource-bridge-module.h"

#include <cstdio>
#include <cmath>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "renderer/renderer.h"
#include "debug/gl-debug.h"
#include "debug/debug-diag.h"
#include "render/render-command.h"
#include "render/world-shader.h"

namespace r = mimita::game::render;

extern Renderer* gRenderer;

struct DebugTriVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

extern std::vector<DebugTriVertex> gTriVerts;
extern std::vector<DebugTriVertex> gOverlayTriVerts;
std::vector<DebugTriVertex> gSolidTriVerts;

namespace {
std::uint32_t gTriVao = 0;
std::uint32_t gTriVbo = 0;
std::uint32_t gOverlayVao = 0;
std::uint32_t gOverlayVbo = 0;
std::uint32_t gSolidVao = 0;
std::uint32_t gSolidVbo = 0;
} // anonymous namespace

// Shared helper to upload and draw a tri buffer (used by both regular and overlay flushes).
static void flushTriBuffer(const Camera& camera,
    std::vector<DebugTriVertex>& verts,
    std::uint32_t& vao, std::uint32_t& vbo,
    bool depthTest,
    bool depthWrite = false)
{
    if (verts.empty()) return;
    if (!gRenderer || !r::worldShader().ensureLoaded()) { verts.clear(); return; }

    if (!vao)
    {
        vao = mimita::game::genVertexArray();
        vbo = mimita::game::genBuffer();
    }

    MIMITA_GL_CLEAR_STAGE("flushTriBuffer");
    if (depthTest)
        r::enable(r::Capability::DepthTest);
    else
        r::disable(r::Capability::DepthTest);
    r::depthMask(depthWrite);
    r::enable(r::Capability::Blend);
    r::blendFunc(r::BlendFactor::SrcAlpha, r::BlendFactor::OneMinusSrcAlpha);

    r::useProgram(r::worldShader().program());

    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth, (float)gRenderer->sceneHeight);

    r::uniformMatrix4fv(r::worldShader().uniform("model"), 1, &model[0][0]);
    r::uniformMatrix4fv(r::worldShader().uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(r::worldShader().uniform("projection"), 1, &proj[0][0]);
    r::uniform1i(r::worldShader().uniform("uUseColor"), 2);

    r::bindVertexArray(vao);
    r::bindBuffer(r::BufferTarget::Array, vbo);
    r::bufferData(r::BufferTarget::Array, vbo, verts.data(),
                  static_cast<std::uint32_t>(verts.size() * sizeof(DebugTriVertex)),
                  r::BufferUsage::Dynamic);

    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(DebugTriVertex),
                           static_cast<std::uint32_t>(offsetof(DebugTriVertex, pos)));
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(3, 4, r::VertexType::Float, false, sizeof(DebugTriVertex),
                           static_cast<std::uint32_t>(offsetof(DebugTriVertex, color)));
    r::enableVertexAttribArray(3);
    r::disableVertexAttribArray(1);
    r::disableVertexAttribArray(2);

    r::drawArrays(r::Primitive::Triangles, 0,
                  static_cast<std::int32_t>(verts.size()));
    diagRenderCountEffectDraw();

    r::depthMask(true);
    verts.clear();
}

void flushDebugTris(const Camera& camera)
{
    flushTriBuffer(camera, gTriVerts, gTriVao, gTriVbo, true);
}

void flushOverlayTris(const Camera& camera)
{
    flushTriBuffer(camera, gOverlayTriVerts, gOverlayVao, gOverlayVbo, false);
}

void flushSolidTris(const Camera& camera)
{
    flushTriBuffer(camera, gSolidTriVerts, gSolidVao, gSolidVbo, true, true);
}

namespace DebugVis {

void flushTris(const Camera& camera) {
    ::flushSolidTris(camera);
    ::flushDebugTris(camera);
    ::flushOverlayTris(camera);
}

} // namespace DebugVis
