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

namespace {
GLuint gTriVao = 0;
GLuint gTriVbo = 0;
} // anonymous namespace

void flushDebugTris(const Camera& camera)
{
    if (gTriVerts.empty())
        return;
    if (!gRenderer || !gRenderer->shaderProgram) {
        gTriVerts.clear();
        return;
    }

    if (!gTriVao)
    {
        MIMITA_GL_CLEAR_STAGE("flushDebugTris init");
        MIMITA_GL_CALL(glGenVertexArrays(1, &gTriVao));
        MIMITA_GL_CALL(glGenBuffers(1, &gTriVbo));
    }

    MIMITA_GL_CLEAR_STAGE("flushDebugTris");
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));
    MIMITA_GL_CALL(glDepthMask(GL_FALSE));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    MIMITA_GL_CALL(glUseProgram(gRenderer->shaderProgram));

    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj(
        (float)gRenderer->width,
        (float)gRenderer->height
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "model"),
        1,
        GL_FALSE,
        &model[0][0]
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "view"),
        1,
        GL_FALSE,
        &view[0][0]
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "projection"),
        1,
        GL_FALSE,
        &proj[0][0]
    );

    glUniform1i(
        glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"),
        2
    );

    MIMITA_GL_CALL(glBindVertexArray(gTriVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gTriVbo));

    MIMITA_GL_CALL(glBufferData(
        GL_ARRAY_BUFFER,
        gTriVerts.size() * sizeof(DebugTriVertex),
        gTriVerts.data(),
        GL_DYNAMIC_DRAW
    ));

    // position
    MIMITA_GL_CALL(glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugTriVertex),
        (void*)offsetof(DebugTriVertex, pos)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    // per-vertex color
    MIMITA_GL_CALL(glVertexAttribPointer(
        3,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugTriVertex),
        (void*)offsetof(DebugTriVertex, color)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(3));

    MIMITA_GL_CALL(glDisableVertexAttribArray(1));
    MIMITA_GL_CALL(glDisableVertexAttribArray(2));

    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, (GLsizei)gTriVerts.size()));
    diagRenderCountEffectDraw();

    MIMITA_GL_CALL(glDepthMask(GL_TRUE));

    gTriVerts.clear();
}

namespace DebugVis {

void flushTris(const Camera& camera) {
    ::flushDebugTris(camera);
}

} // namespace DebugVis
