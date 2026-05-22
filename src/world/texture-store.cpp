// C:\important\quiet\n\mimita-priv-v7\src\world\texture-store.cpp
/**
 * purpose
 * the file that actaullt does stuff with the pngs folder with textures
 * in it
 * feb 9 2026 refactor
 */

 // texture_store.cpp

#include "texture-store.h"
#include "map/texture.h"

#include <GLFW/glfw3.h>

// And make sure this exists in ONE cpp: feb 9 2026 
TextureStore gTextures;

GLuint TextureStore::get(const std::string& name) {
    auto it = map.find(name);
    if (it != map.end())
        return it->second;

    std::string path = "assets/textures/" + name + ".png";
    // not loadTexturePNG 
    GLuint tex = loadTexture(path.c_str()); // your loader
    map[name] = tex;
    return tex;
}
