// C:\important\quiet\n\mimita-public\mimita-public\src\map\texture.cpp
// dec 19 2025 need update i think 


// todo use this texture or use texture_manager.cpp

// jan 25 2026 
// dont include stb image implementation here? i dont know -jorj 

#include <glad/glad.h>
/*
dont use this
#include <GL/glew.h>   // or <GL/gl.h> if using raw OpenGL
its bad and breaks everythung
*/
#include <cstdio>

#include "stb_image.h"
#include "utils/path_utils.h"
#include <string>

static GLuint makeFallbackTexture()
{
    static GLuint fallback = 0;
    if (fallback)
        return fallback;

    printf("[TEXTURE WARNING] Creating bright checkerboard fallback texture\n");
    unsigned int pixels[16] = {
        0xffff00ff, 0xff00ffff, 0xffff00ff, 0xff00ffff,
        0xff00ffff, 0xffff00ff, 0xff00ffff, 0xffff00ff,
        0xffff00ff, 0xff00ffff, 0xffff00ff, 0xff00ffff,
        0xff00ffff, 0xffff00ff, 0xff00ffff, 0xffff00ff
    };

    glGenTextures(1, &fallback);
    glBindTexture(GL_TEXTURE_2D, fallback);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return fallback;
}

GLuint loadTexture(const char* path) {
    printf("[TEXTURE] Loading texture %s\n", path);

    std::string resolvedPath = path;
    int w, h, n;
    unsigned char* data = stbi_load(resolvedPath.c_str(), &w, &h, &n, 4);

    if (!data) {
        printf("[TEXTURE WARNING] Missing texture %s (resolved from %s)\n", resolvedPath.c_str(), path);
        return makeFallbackTexture();
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    printf("[TEXTURE] Uploading to GPU tex=%u size=%dx%d channels=%d\n", tex, w, h, n);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    printf("[TEXTURE] Upload complete %s tex=%u\n", path, tex);

    return tex;
}
