// C:\important\go away v5\s\mimita-v5\src\map\map_render.h

#pragma once
#include "map_common.h"
#include <glad/glad.h>

GLuint createMapVAO(const Mesh& mesh);
void drawMap(GLuint vao, size_t vertexCount);
