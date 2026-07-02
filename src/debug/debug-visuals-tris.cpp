#include "debug/debug-visuals.h"

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

extern Renderer* gRenderer;

struct DebugTriVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

extern std::vector<DebugTriVertex> gTriVerts;
extern std::vector<DebugTriVertex> gOverlayTriVerts;

namespace {
GLuint gTriVao = 0;
GLuint gTriVbo = 0;
GLuint gOverlayVao = 0;
GLuint gOverlayVbo = 0;
} // anonymous namespace

// Shared helper to upload and draw a tri buffer (used by both regular and overlay flushes).
static void flushTriBuffer(const Camera& camera,
    std::vector<DebugTriVertex>& verts,
    GLuint& vao, GLuint& vbo,
    bool depthTest)
{
    if (verts.empty()) return;
    if (!gRenderer || !gRenderer->shaderProgram) { verts.clear(); return; }

    if (!vao)
    {
        MIMITA_GL_CALL(glGenVertexArrays(1, &vao));
        MIMITA_GL_CALL(glGenBuffers(1, &vbo));
    }

    MIMITA_GL_CLEAR_STAGE("flushTriBuffer");
    if (depthTest)
        MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));
    else
        MIMITA_GL_CALL(glDisable(GL_DEPTH_TEST));
    MIMITA_GL_CALL(glDepthMask(GL_FALSE));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    MIMITA_GL_CALL(glUseProgram(gRenderer->shaderProgram));

    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);

    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 2);

    MIMITA_GL_CALL(glBindVertexArray(vao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(DebugTriVertex), verts.data(), GL_DYNAMIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugTriVertex), (void*)offsetof(DebugTriVertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(DebugTriVertex), (void*)offsetof(DebugTriVertex, color)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(3));
    MIMITA_GL_CALL(glDisableVertexAttribArray(1));
    MIMITA_GL_CALL(glDisableVertexAttribArray(2));

    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size()));
    diagRenderCountEffectDraw();

    MIMITA_GL_CALL(glDepthMask(GL_TRUE));
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

namespace DebugVis {

void flushTris(const Camera& camera) {
    ::flushDebugTris(camera);
    ::flushOverlayTris(camera);
}

} // namespace DebugVis
