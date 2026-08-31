#include "render/render-world.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "world/world.h"
#include "perf/perf.h"

extern GLuint gVao;

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

    auto& shadowPerf = Perf::state().renderPerf;
    for (const auto& batch : world.mesh.batches) {
        if (!shadowBatchVisible(batch, lightMVP))
            continue;
        shadowPerf.shadowBatches++;
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}
