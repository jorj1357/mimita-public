#include "render/render-world.h"
#include "game/gl-resource-bridge-module.h"
#include "render/render-command.h"
#include "render/world-shader.h"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"
#include "debug/gl-debug.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace r = mimita::game::render;

namespace {

std::uint32_t gSkyVao = 0;
std::uint32_t gSkyVbo = 0;
size_t gSkyBuiltVertCount = 0;
size_t gSkyBuiltBatchCount = 0;
uint64_t gSkyBuiltRevision = 0;

} // anonymous namespace

void setUniforms(r::WorldShader& shader, const glm::vec3& cameraPos);

void uploadSkyIfNeeded(const World& world)
{
    const Mesh& mesh = world.skyMesh;
    if (mesh.verts.empty()) return;

    if (mesh.verts.size() == gSkyBuiltVertCount &&
        mesh.batches.size() == gSkyBuiltBatchCount &&
        world.renderRevision == gSkyBuiltRevision)
        return;

    if (!gSkyVao) {
        gSkyVao = mimita::game::genVertexArray();
        gSkyVbo = mimita::game::genBuffer();
    }

    r::bindVertexArray(gSkyVao);
    r::bindBuffer(r::BufferTarget::Array, gSkyVbo);
    r::bufferData(r::BufferTarget::Array, gSkyVbo, mesh.verts.data(),
                  static_cast<std::uint32_t>(mesh.verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Static);

    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));

    r::bindBuffer(r::BufferTarget::Array, 0);
    r::bindVertexArray(0);

    gSkyBuiltVertCount = mesh.verts.size();
    gSkyBuiltBatchCount = mesh.batches.size();
    gSkyBuiltRevision = world.renderRevision;
}

void renderSky(const World& world, const Camera& cam)
{
    if (!gRenderer || !r::worldShader().ensureLoaded() || world.skyMesh.verts.empty())
        return;

    auto& mesh = world.skyMesh;
    uploadSkyIfNeeded(world);

    auto& shader = r::worldShader();
    r::useProgram(shader.program());

    r::disable(r::Capability::DepthTest);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), cam.pos);
    r::uniformMatrix4fv(shader.uniform("model"), 1, &model[0][0]);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->sceneWidth, (float)gRenderer->sceneHeight);
    r::uniformMatrix4fv(shader.uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(shader.uniform("projection"), 1, &proj[0][0]);

    setUniforms(shader);

    r::bindVertexArray(gSkyVao);
    for (const auto& batch : mesh.batches)
    {
        std::uint32_t tex = batch.texture ? batch.texture : gTextures.get("default");
        r::bindTexture(r::TextureTarget::Texture2D, tex);
        r::drawArrays(r::Primitive::Triangles, static_cast<std::int32_t>(batch.first),
                      static_cast<std::int32_t>(batch.count));
    }
    r::bindVertexArray(0);
    r::useProgram(0);

    r::enable(r::Capability::DepthTest);
}
