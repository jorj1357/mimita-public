// C:\important\quiet\n\mimita-public\mimita-public\src\map\texture.cpp
// dec 19 2025 need update i think 

// todo use this texture or use texture_manager.cpp

#define STB_IMAGE_IMPLEMENTATION

#include <glad/glad.h>
/*
dont use this
#include <GL/glew.h>   // or <GL/gl.h> if using raw OpenGL
its bad and breaks everythung
*/
#include <cstdio>

#include "deps/stb_image.h"
#include "utils/path_utils.h"
#include <string>

GLuint loadTexture(const char* path) {
    std::string resolvedPath = resolveAssetPath(path);
    int w, h, n;
    unsigned char* data = stbi_load(resolvedPath.c_str(), &w, &h, &n, 4);
    if (!data) { printf("Failed to load texture %s (resolved from %s)\n", resolvedPath.c_str(), path); return 0; }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return tex;
}
