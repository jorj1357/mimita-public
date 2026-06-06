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
#include "debug/debug-log.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <functional>

// And make sure this exists in ONE cpp: feb 9 2026 
TextureStore gTextures;

GLuint TextureStore::get(const std::string& name) {
    std::string key = name.empty() ? "default" : name;
    if (key.rfind("assets/textures/", 0) == 0)
        key = key.substr(16);
    if (key.size() > 4 && key.substr(key.size() - 4) == ".png")
        key = key.substr(0, key.size() - 4);
    std::replace(key.begin(), key.end(), '\\', '/');

    auto it = map.find(key);
    if (it != map.end())
    {
        Debug::logThrottled(Debug::Category::Render, "texture-bind", DebugConfig::PRINT_INTERVAL, "[TEXTURE] Bound texture name=%s tex=%u\n", key.c_str(), it->second);
        return it->second;
    }

    std::string path = "assets/textures/" + key + ".png";
    GLuint tex = loadTexture(path.c_str()); // your loader
    map[key] = tex;
    Debug::logOnce(Debug::Category::Render, key.c_str(), "[TEXTURE] Registered texture name=%s tex=%u\n", key.c_str(), tex);
    return tex;
}

GLuint TextureStore::getPath(const std::string& path, bool reload)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::string key = "path:" + normalized;
    auto it = map.find(key);
    if (it != map.end() && !reload)
        return it->second;
    if (it != map.end() && reload && it->second)
        glDeleteTextures(1, &it->second);
    GLuint texture = loadTexture(normalized.c_str());
    map[key] = texture;
    return texture;
}
