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

#include <glad/glad.h>
#include <vector>
#include <cstdio>

extern Renderer* gRenderer;

struct SimpleVert {
    float x,y,z;
    float u,v;
};

static GLuint vao = 0;
static GLuint vbo = 0;

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

static void buildBlock(
    std::vector<SimpleVert>& verts,
    const Block& b
){
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

    pushQuad(verts, v000,v100,v110,v010); // bottom
    pushQuad(verts, v001,v011,v111,v101); // top
    pushQuad(verts, v000,v010,v011,v001); // left
    pushQuad(verts, v100,v101,v111,v110); // right
    pushQuad(verts, v010,v110,v111,v011); // front
    pushQuad(verts, v000,v001,v101,v100); // back
}

void renderWorld(const World& world, const Camera& cam)
{
    if (!gRenderer || !gRenderer->shaderProgram)
        return;

    static bool initialized = false;
    static std::vector<SimpleVert> verts;

    if (!initialized)
    {
        printf("[RENDER] building world mesh (%zu blocks)\n", world.blocks.size());

        for (auto& b : world.blocks)
            buildBlock(verts, b);

        printf("[RENDER] verts = %zu\n", verts.size());

        glGenVertexArrays(1, &vao);
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
    }

    glUseProgram(gRenderer->shaderProgram);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram,"uUseColor"), 0);

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

    glDrawArrays(GL_TRIANGLES,0,(GLsizei)verts.size());
}
