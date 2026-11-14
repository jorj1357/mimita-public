#include "render/render-world.h"
#include "render/render-command.h"
#include "render/shader-program.h"

#include <cstdint>
#include <glm/glm.hpp>

#include "world/world.h"
#include "perf/perf.h"

namespace r = mimita::game::render;

extern std::uint32_t gVao;

static bool shadowBatchVisible(const Mesh::Batch& batch, const glm::mat4& lightMVP)
{
    if (!batch.hasBounds)
        return true;
    const glm::vec3& mn = batch.boundsMin;
    const glm::vec3& mx = batch.boundsMax;
    const glm::vec3 corners[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
        {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
        {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}
    };
    bool outsideLeft = true, outsideRight = true;
    bool outsideBottom = true, outsideTop = true;
    bool outsideNear = true, outsideFar = true;
    for (const glm::vec3& corner : corners) {
        const glm::vec4 clip = lightMVP * glm::vec4(corner, 1.0f);
        outsideLeft &= clip.x < -clip.w;
        outsideRight &= clip.x > clip.w;
        outsideBottom &= clip.y < -clip.w;
        outsideTop &= clip.y > clip.w;
        outsideNear &= clip.z < -clip.w;
        outsideFar &= clip.z > clip.w;
    }
    return !(outsideLeft || outsideRight || outsideBottom ||
             outsideTop || outsideNear || outsideFar);
}

void uploadMeshIfNeeded(const World& world);

void renderWorldDepth(const World& world, mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightMVP)
{
    if (world.mesh.verts.empty()) return;
    uploadMeshIfNeeded(world);

    r::useProgram(shadowShader.program());
    const auto loc = shadowShader.uniform("uLightMVP");
    if (loc >= 0)
        r::uniformMatrix4fv(loc, 1, &lightMVP[0][0]);

    r::bindVertexArray(gVao);
    r::enable(r::Capability::DepthTest);
    r::depthFunc(r::DepthFunc::Less);
    r::depthMask(true);
    r::disable(r::Capability::Blend);
    r::enable(r::Capability::CullFace);
    r::cullFace(r::CullFaceMode::Back);

    auto& shadowPerf = Perf::state().renderPerf;
    for (const auto& batch : world.mesh.batches) {
        if (!shadowBatchVisible(batch, lightMVP))
            continue;
        shadowPerf.shadowBatches++;
        r::drawArrays(r::Primitive::Triangles,
                      static_cast<std::int32_t>(batch.first),
                      static_cast<std::int32_t>(batch.count));
    }

    r::bindVertexArray(0);
    r::useProgram(0);
}
