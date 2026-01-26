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


GLuint loadTexture(const char* path) {
    printf("loadTexture: started\n");

    std::string resolvedPath = resolveAssetPath(path);
    int w, h, n;
    unsigned char* data = stbi_load(resolvedPath.c_str(), &w, &h, &n, 4);
        printf("loadTexture: abt to print failed to loadtexture prob \n");

    if (!data) { printf("Failed to load texture %s (resolved from %s)\n", resolvedPath.c_str(), path); return 0; }

    printf("loadTexture: %s\n", path);
    fflush(stdout);

    printf("loadTexture: gluint text righ after \n");

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    printf("loadTexture: params before \n");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    printf("loadTexture: teximage 2d before  \n");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    printf("loadTexture: reached the return tex; thing   \n");

    return tex;
}
