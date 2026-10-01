// 2026-09-28
/* purpose
* Implement generated-mesh rendering for destructible physical entities.
* Uploads the entity's cached triangle soup into a shared dynamic buffer and
* draws it with the model matrix interpolated between physics transforms.
* Does NOT generate geometry, own collision, or manage textures.
*/

#include "impact/destructible-render.h"

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

extern Renderer* gRenderer;

namespace {

// One GPU buffer set per entity. A single shared VBO re-uploaded on every
// entity switch (the old behavior) re-uploaded every fragment/holey crate every
// frame; keying by entity id + geometry revision uploads once per revision.
struct GpuEntityMesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
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
            if (pair.second.vbo) glDeleteBuffers(1, &pair.second.vbo);
            if (pair.second.vao) glDeleteVertexArrays(1, &pair.second.vao);
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
    if (it->second.vbo) glDeleteBuffers(1, &it->second.vbo);
    if (it->second.vao) glDeleteVertexArrays(1, &it->second.vao);
    gEntityMeshes.erase(it);
}

bool drawGeneratedEntityMesh(const PhysicalEntity& entity, const Camera& camera)
{
    const MimitaImpact::DestructibleGeometry& geometry = entity.destructible;
    if (!geometry.enabled || geometry.renderVertices.empty())
        return false;
    if (!gRenderer || !gRenderer->shaderProgram)
        return false;

    glUseProgram(gRenderer->shaderProgram);

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
    const glm::mat4 proj = camera.getProj((float)gRenderer->width,
                                          (float)gRenderer->height);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                       1, GL_FALSE, &renderTransform[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"),
                       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"),
                       1, GL_FALSE, &proj[0][0]);
    setUniforms(gRenderer->shaderProgram, camera.pos);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextures.getPath(entity.texturePath));

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint cullModeWas = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &cullModeWas);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GpuEntityMesh& gpu = gpuMeshFor(entity.id);
    if (!gpu.vao)
    {
        glGenVertexArrays(1, &gpu.vao);
        glGenBuffers(1, &gpu.vbo);
    }
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    const size_t vertexCount = geometry.renderVertices.size();
    if (gpu.revision != geometry.geometryRevision ||
        gpu.vertexCount != vertexCount)
    {
        recordCrashBreadcrumb("render-upload", "id=%u rev=%llu verts=%zu",
            entity.id, (unsigned long long)geometry.geometryRevision, vertexCount);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(vertexCount * sizeof(Vertex)),
                     geometry.renderVertices.data(), GL_DYNAMIC_DRAW);
        gpu.revision = geometry.geometryRevision;
        gpu.vertexCount = vertexCount;
    }
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);

    if (!cullWasEnabled)
        glDisable(GL_CULL_FACE);
    glCullFace(cullModeWas);
    glBindVertexArray(0);
    return true;
}
