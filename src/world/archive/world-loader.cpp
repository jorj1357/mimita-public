// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-loader.cpp
// dec 18 2025
/**
 * purpose
 * take json file
 * turnn it into stuff we walk on
 */

#include "world/world-loader.h"
#include "world/world.h"

#include <cstdio>
#include <vector>
#include <string>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// todo maek this better idk if this is bad or whatver
// but its just smth quick so we have textures in mimita 
// feb 9 2026
// commented out feb 9 2026 bc not used 
// static uint8_t hashTex(const std::string& s) {
//     // super simple, deterministic
//     uint32_t h = 2166136261u;
//     for (char c : s) {
//         h ^= (uint8_t)c;
//         h *= 16777619u;
//     }
//     return (uint8_t)(h & 0xFF);
// }

using json = nlohmann::json;

bool loadWorldFromJSON(World& world, const char* path)
{
    printf("[WORLD] opening via fopen: %s\n", path);

    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("[WORLD] fopen failed\n");
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string raw;
    raw.resize(size);

    fread(raw.data(), 1, size, f);
    fclose(f);

    // Replace nan/inf
    const char* bad[] = { "nan", "NaN", "inf", "Infinity", "-inf", "-Infinity" };
    for (auto b : bad) {
        size_t p;
        while ((p = raw.find(b)) != std::string::npos)
            raw.replace(p, strlen(b), "0");
    }

    json j;
    try {
        j = json::parse(raw);
    }
    catch (const std::exception& e) {
        printf("[WORLD] JSON parse error: %s\n", e.what());
        return false;
    }

    if (!j.contains("blocks") || !j["blocks"].is_array()) {
        printf("[WORLD] no blocks array\n");
        return false;
    }

    printf("[WORLD] blocks = %zu\n", j["blocks"].size());

    world.clear();

    // declare here so we can count 
    int idx = 0;
    for (auto& jb : j["blocks"]) {
        bool verboseBlockLog = idx < 10 || (idx % 100) == 0;
        if (verboseBlockLog)
            printf("[WORLD] parsing block %d\n", idx);

        if (verboseBlockLog && jb.contains("name"))
            printf("  name = %s\n", jb["name"].get<std::string>().c_str());

        Block b{};
        // b.pos = glm::vec3(
        //     jb["pos"][0],
        //     jb["pos"][1],
        //     jb["pos"][2]
        // );

        // b.size = glm::vec3(
        //     jb["size"][0],
        //     jb["size"][1],
        //     jb["size"][2]
        // );

        // names moer accurate? idk 
        // b.pos = glm::vec3(
        //     jb["position"][0].get<float>(),
        //     jb["position"][1].get<float>(),
        //     jb["position"][2].get<float>()
        // );

        // b.size = glm::vec3(
        //     jb["size"][0].get<float>(),
        //     jb["size"][1].get<float>(),
        //     jb["size"][2].get<float>()
        // );

        // // rotation optional
        // if (jb.contains("rotation")) {
        //     b.rotEuler = glm::vec3(
        //         jb["rotation"][0].get<float>(),
        //         jb["rotation"][1].get<float>(),
        //         jb["rotation"][2].get<float>()
        //     );
        // } else {
        //     b.rotEuler = glm::vec3(0.0f);
        // }

        // dont crash if json is messed up? i think? idk 
        auto readVec3 = [&](const json& j, const char* key) -> glm::vec3 {
            if (!j.contains(key) || !j[key].is_array() || j[key].size() != 3) {
                printf("[WORLD] missing/bad vec3 '%s'\n", key);
                return glm::vec3(0.0f);
            }

            return glm::vec3(
                j[key][0].get<float>(),
                j[key][1].get<float>(),
                j[key][2].get<float>()
            );
        };

        // POSITION: support both schemas
        if (jb.contains("pos") && jb["pos"].is_array())
            b.pos = readVec3(jb, "pos");
        else if (jb.contains("position") && jb["position"].is_array())
            b.pos = readVec3(jb, "position");
        else {
            printf("[WORLD] block has no valid position\n");
            b.pos = glm::vec3(0.0f);
        }

        const char* textureKeys[] = {"tex", "texture", "material", "mat"};
        std::string foundTexture;
        for (const char* textureKey : textureKeys)
        {
            if (jb.contains(textureKey) && jb[textureKey].is_string())
            {
                foundTexture = jb[textureKey].get<std::string>();
                break;
            }
        }

        if (foundTexture.empty()) {
            printf("[TEXTURE WARNING] Block missing texture/material field\n");
            b.texName = "missing_texture";
        }

        // SIZE (always "size")
        b.size = readVec3(jb, "size");

        // AABB only for now
        b.rot = glm::mat3(1.0f);
        b.isSlope = false;

        if (!foundTexture.empty())
            b.texName = foundTexture;
        else if (b.texName.empty())
            b.texName = "default";

        for (int face = 0; face < 6; ++face)
            b.faceTexName[face] = b.texName;

        const char* faceArrayKeys[] = {"faceTextures", "face_textures", "textures"};
        for (const char* faceKey : faceArrayKeys)
        {
            if (jb.contains(faceKey) && jb[faceKey].is_array() && jb[faceKey].size() >= 6)
            {
                for (int face = 0; face < 6; ++face)
                {
                    if (jb[faceKey][face].is_string())
                        b.faceTexName[face] = jb[faceKey][face].get<std::string>();
                }
                printf("[TEXTURE] Block uses per-face texture array key=%s\n", faceKey);
                break;
            }
        }

        world.blocks.push_back(b);
        // now idx add ? idk 
        idx++;
    }

    world.finalize();
    world.rebuildChunks();

    printf("[WORLD] world built OK\n");
    return true;
}
