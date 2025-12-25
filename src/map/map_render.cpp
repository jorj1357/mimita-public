// C:\important\go away v5\s\mimita-v5\src\map\map_render.cpp

#include "map_common.h"
#include <glad/glad.h>
#include "texture_manager.h"

extern TextureManager TEX;

GLuint createMapVAO(const Mesh& mesh) {
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return vao;
}

void drawMap(GLuint vao, size_t vertexCount) {
    glBindVertexArray(vao);

    // Pick one texture to actually render with (for now)
    GLuint tex = TEX.get(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}
