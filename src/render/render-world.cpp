// C:\important\quiet\n\mimita-priv-v7\src\render\render-world.cpp
// feb 10 2026
/**
 * prupose 
 * small wrapper so that
 * renderer can cal?
 * idk
 * keepinng file size low rewrite
 */

#include "render-world.h"
#include "world/world.h"
#include "camera.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"

#include <glad/glad.h>
#include <vector>
#include <string>
#include <cstdio>
#include <utility>

extern Renderer* gRenderer;

struct SimpleVert {
    float x,y,z;
    float u,v;
};

static GLuint vao = 0;
static GLuint vbo = 0;
struct WorldBatch {
    std::string textureName;
    GLsizei first = 0;
    GLsizei count = 0;
};
static std::vector<WorldBatch> batches;
struct BuildBatch {
    std::string textureName;
    std::vector<SimpleVert> verts;
};

static void pushQuad(
    std::vector<SimpleVert>& verts,
    glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d
){
    verts.push_back({a.x,a.y,a.z, 0,0});
    verts.push_back({b.x,b.y,b.z, 1,0});
    verts.push_back({c.x,c.y,c.z, 1,1});

    verts.push_back({a.x,a.y,a.z, 0,0});
    verts.push_back({c.x,c.y,c.z, 1,1});
    verts.push_back({d.x,d.y,d.z, 0,1});
}

static std::vector<SimpleVert>& batchVertsFor(std::vector<BuildBatch>& buildBatches, const std::string& textureName)
{
    std::string key = textureName.empty() ? "default" : textureName;
    for (BuildBatch& batch : buildBatches)
    {
        if (batch.textureName == key)
            return batch.verts;
    }
    buildBatches.push_back({key, {}});
    return buildBatches.back().verts;
}

static void buildBlockBatched(std::vector<BuildBatch>& buildBatches, const Block& b)
{
    glm::vec3 h = b.size * 0.5f;
    glm::vec3 p = b.pos;

    glm::vec3 v000 = p + glm::vec3(-h.x,-h.y,-h.z);
    glm::vec3 v001 = p + glm::vec3(-h.x,-h.y, h.z);
    glm::vec3 v010 = p + glm::vec3(-h.x, h.y,-h.z);
    glm::vec3 v011 = p + glm::vec3(-h.x, h.y, h.z);
    glm::vec3 v100 = p + glm::vec3( h.x,-h.y,-h.z);
    glm::vec3 v101 = p + glm::vec3( h.x,-h.y, h.z);
    glm::vec3 v110 = p + glm::vec3( h.x, h.y,-h.z);
    glm::vec3 v111 = p + glm::vec3( h.x, h.y, h.z);

    auto faceTex = [&](int face) -> std::string {
        if (face >= 0 && face < 6 && !b.faceTexName[face].empty())
            return b.faceTexName[face];
        return b.texName.empty() ? "default" : b.texName;
    };

    pushQuad(batchVertsFor(buildBatches, faceTex(0)), v000,v100,v110,v010); // bottom
    pushQuad(batchVertsFor(buildBatches, faceTex(1)), v001,v011,v111,v101); // top
    pushQuad(batchVertsFor(buildBatches, faceTex(2)), v000,v010,v011,v001); // left
    pushQuad(batchVertsFor(buildBatches, faceTex(3)), v100,v101,v111,v110); // right
    pushQuad(batchVertsFor(buildBatches, faceTex(4)), v010,v110,v111,v011); // front
    pushQuad(batchVertsFor(buildBatches, faceTex(5)), v000,v001,v101,v100); // back
}

void renderWorld(const World& world, const Camera& cam)
{
    if (!gRenderer || !gRenderer->shaderProgram)
        return;

    static bool initialized = false;
    static size_t builtBlockCount = 0;
    static std::vector<SimpleVert> verts;

    if (!initialized || builtBlockCount != world.blocks.size())
    {
        verts.clear();
        batches.clear();
        std::vector<BuildBatch> buildBatches;
        printf("[RENDER] building world mesh (%zu blocks)\n", world.blocks.size());

        for (auto& b : world.blocks)
            buildBlockBatched(buildBatches, b);

        for (const BuildBatch& buildBatch : buildBatches)
        {
            WorldBatch batch;
            batch.textureName = buildBatch.textureName;
            batch.first = (GLsizei)verts.size();
            batch.count = (GLsizei)buildBatch.verts.size();
            verts.insert(verts.end(), buildBatch.verts.begin(), buildBatch.verts.end());
            batches.push_back(batch);
        }

        printf("[RENDER] verts=%zu textureBatches=%zu\n", verts.size(), batches.size());

        if (!vao)
            glGenVertexArrays(1, &vao);
        if (!vbo)
            glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(
            GL_ARRAY_BUFFER,
            verts.size()*sizeof(SimpleVert),
            verts.data(),
            GL_STATIC_DRAW
        );

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(SimpleVert),(void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(SimpleVert),(void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);

        initialized = true;
        builtBlockCount = world.blocks.size();
    }

    glUseProgram(gRenderer->shaderProgram);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram,"uUseColor"), 0);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram,"uTex"), 0);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width,(float)gRenderer->height);
    glm::mat4 model(1.0f);

    GLint uModel = glGetUniformLocation(gRenderer->shaderProgram,"model");
    GLint uView  = glGetUniformLocation(gRenderer->shaderProgram,"view");
    GLint uProj  = glGetUniformLocation(gRenderer->shaderProgram,"projection");

    glUniformMatrix4fv(uModel,1,GL_FALSE,&model[0][0]);
    glUniformMatrix4fv(uView ,1,GL_FALSE,&view[0][0]);
    glUniformMatrix4fv(uProj ,1,GL_FALSE,&proj[0][0]);

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);

    for (const WorldBatch& batch : batches)
    {
        GLuint tex = gTextures.get(batch.textureName);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
}
