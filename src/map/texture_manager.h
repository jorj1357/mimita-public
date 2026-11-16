// C:\important\go away v5\s\mimita-v5\src\map\texture_manager.h

#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct TextureManager {
    std::vector<std::string> paths;
    std::vector<std::uint32_t> ids;

    void init();                 // load all textures once
    std::uint32_t get(size_t index);    // return OpenGL texture handle
    size_t count() const { return ids.size(); }
};
