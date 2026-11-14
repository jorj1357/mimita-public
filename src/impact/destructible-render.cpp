// 2026-09-28
/* purpose
* Implement generated-mesh rendering for destructible physical entities.
* Uploads the entity's cached triangle soup into a shared dynamic buffer and
* draws it with the model matrix interpolated between physics transforms.
* Does NOT generate geometry, own collision, or manage textures.
*/

#include "impact/destructible-render.h"
#include "game/gl-resource-bridge-module.h"

#include <cstddef>
#include <unordered_map>

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "debug/crash-handler.h"
#include "impact/destructible-geometry.h"
#include "map/map_common.h"
#include "physics/physical-entity.h"
#include "render/render-world.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "render/render-command.h"
#include "render/world-shader.h"

namespace r = mimita::game::render;

extern Renderer* gRenderer;

namespace {

// One GPU buffer set per entity. A single shared VBO re-uploaded on every
// entity switch (the old behavior) re-uploaded every fragment/holey crate every
// frame; keying by entity id + geometry revision uploads once per revision.
struct GpuEntityMesh
{
    std::uint32_t vao = 0;
    std::uint32_t vbo = 0;
    uint64_t revision = 0;
    size_t vertexCount = 0;
};

std::unordered_map<uint32_t, GpuEntityMesh> gEntityMeshes;

constexpr size_t kMaxGpuEntityMeshes = 512;

GpuEntityMesh& gpuMeshFor(uint32_t entityId)
{
    if (gEntityMeshes.size() > kMaxGpuEntityMeshes)
    {
        for (auto& pair : gEntityMeshes)
        {
            if (pair.second.vbo) mimita::game::deleteBuffer(pair.second.vbo);
            if (pair.second.vao) mimita::game::deleteVertexArray(pair.second.vao);
        }
        gEntityMeshes.clear();
    }
    return gEntityMeshes[entityId];
}

} // anonymous namespace

void releaseGeneratedEntityMesh(unsigned int entityId)
{
    auto it = gEntityMeshes.find(entityId);
    if (it == gEntityMeshes.end())
        return;
    if (it->second.vbo) mimita::game::deleteBuffer(it->second.vbo);
    if (it->second.vao) mimita::game::deleteVertexArray(it->second.vao);
    gEntityMeshes.erase(it);
}

bool drawGeneratedEntityMesh(const PhysicalEntity& entity, const Camera& camera)
{
    const MimitaImpact::DestructibleGeometry& geometry = entity.destructible;
    if (!geometry.enabled || geometry.renderVertices.empty())
        return false;
    if (!gRenderer || !r::worldShader().ensureLoaded())
        return false;

    r::useProgram(r::worldShader().program());

    // Interpolate the model matrix between the previous and current transform,
    // matching the physical-entity box path.
    static glm::mat4 renderTransform(1.0f);
    const float alpha = PhysicalEntitySystem::instance().renderAlpha();
    const glm::vec3 previousPosition(entity.previousTransform[3]);
    const glm::vec3 currentPosition(entity.transform[3]);
    const glm::quat previousOrientation =
        glm::normalize(glm::quat_cast(glm::mat3(entity.previousTransform)));
    renderTransform = glm::translate(
        glm::mat4(1.0f),
        glm::mix(previousPosition, currentPosition, alpha)) *
        glm::mat4_cast(glm::normalize(glm::slerp(previousOrientation, entity.orientation, alpha)));

    const glm::mat4 view = camera.getView();
    const glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth,
                                          (float)gRenderer->sceneHeight);
    r::uniformMatrix4fv(r::worldShader().uniform("model"), 1, &renderTransform[0][0]);
    r::uniformMatrix4fv(r::worldShader().uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(r::worldShader().uniform("projection"), 1, &proj[0][0]);
    setUniforms(r::worldShader(), camera.pos);

    r::pushState();
    r::activeTexture(0);
    r::bindTexture(r::TextureTarget::Texture2D, entity.renderTextureHandle != 0
        ? entity.renderTextureHandle : gTextures.getPath(entity.texturePath));

    r::enable(r::Capability::DepthTest);
    r::depthMask(true);
    r::enable(r::Capability::CullFace);
    r::cullFace(r::CullFaceMode::Back);

    GpuEntityMesh& gpu = gpuMeshFor(entity.id);
    if (!gpu.vao)
    {
        gpu.vao = mimita::game::genVertexArray();
        gpu.vbo = mimita::game::genBuffer();
    }
    r::bindVertexArray(gpu.vao);
    r::bindBuffer(r::BufferTarget::Array, gpu.vbo);
    const size_t vertexCount = geometry.renderVertices.size();
    if (gpu.revision != geometry.geometryRevision ||
        gpu.vertexCount != vertexCount)
    {
        recordCrashBreadcrumb("render-upload", "id=%u rev=%llu verts=%zu",
            entity.id, (unsigned long long)geometry.geometryRevision, vertexCount);
        r::bufferData(r::BufferTarget::Array, gpu.vbo,
                      geometry.renderVertices.data(),
                      static_cast<std::uint32_t>(vertexCount * sizeof(Vertex)),
                      r::BufferUsage::Dynamic);
        gpu.revision = geometry.geometryRevision;
        gpu.vertexCount = vertexCount;
    }
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::drawArrays(r::Primitive::Triangles, 0,
                  static_cast<std::int32_t>(vertexCount));

    r::bindVertexArray(0);
    r::popState();
    return true;
}
