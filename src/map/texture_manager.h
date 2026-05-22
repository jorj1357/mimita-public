// C:\important\go away v5\s\mimita-v5\src\map\texture_manager.h

#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>

struct TextureManager {
    std::vector<std::string> paths;
    std::vector<GLuint> ids;

    void init();                 // load all textures once
    GLuint get(size_t index);    // return OpenGL texture handle
    size_t count() const { return ids.size(); }
};
