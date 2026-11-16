// C:\important\quiet\n\mimita-priv-v7\src\world\texture-store.h
/**
 * purpos
 * stores the png files that are in /assets/textures
 *  so we can use them as texture
 * and its cool and awesome mhm 
 */



#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>

struct TextureStore {
    std::unordered_map<std::string, std::uint32_t> map;

    std::uint32_t get(const std::string& name);
    std::uint32_t getPath(const std::string& path, bool reload = false);
};

extern TextureStore gTextures;
