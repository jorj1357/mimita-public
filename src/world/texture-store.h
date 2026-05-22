// C:\important\quiet\n\mimita-priv-v7\src\world\texture-store.h
/**
 * purpos
 * stores the png files that are in /assets/textures
 *  so we can use them as texture
 * and its cool and awesome mhm 
 */



#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <string>

struct TextureStore {
    std::unordered_map<std::string, GLuint> map;

    GLuint get(const std::string& name);
};

extern TextureStore gTextures;
