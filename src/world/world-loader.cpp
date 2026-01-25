// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-loader.cpp
// dec 18 2025
/**
 * purpose
 * take json file
 * turnn it into stuff we walk on
 */

#include "world/world-loader.h"
#include "world/world.h"
#include "map/texture_manager.h"

#include <fstream>
#include <cstdio>

// use whatever JSON lib you already use elsewhere
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// bool loadWorldFromJSON(World& world, const char* path)
bool loadWorldFromJSON(World& world, TextureManager& tex, const char* path)
{
    world.clear();

    std::ifstream f(path);
    if (!f.is_open()) {
        printf("[WORLD] Failed to open %s\n", path);
        return false;
    }

    json j;
    f >> j;

    if (!j.is_object() || !j.contains("blocks") || !j["blocks"].is_array()) {
        printf("[WORLD] Invalid world JSON format (missing blocks array)\n");
        return false;
    }

    printf("[WORLD] blocks count = %zu\n", j["blocks"].size());

    int idx = 0;
    for (auto& jb : j["blocks"]) {
        printf("[WORLD] parsing block %d\n", idx++);

        if (!jb.contains("position") || !jb.contains("size") || !jb.contains("rotation"))
            continue;

        if (jb["position"].size() < 3 || jb["size"].size() < 3 || jb["rotation"].size() < 3)
            continue;

        Block b{};

        b.pos = glm::vec3(
            jb["position"][0].get<float>(),
            jb["position"][1].get<float>(),
            jb["position"][2].get<float>()
        );

        b.size = glm::vec3(
            jb["size"][0].get<float>(),
            jb["size"][1].get<float>(),
            jb["size"][2].get<float>()
        );

        b.rotEuler = glm::vec3(
            jb["rotation"][0].get<float>(),
            jb["rotation"][1].get<float>(),
            jb["rotation"][2].get<float>()
        );

        b.rot = glm::mat3(1.0f);

        for (int i = 0; i < 6; i++)
            b.tex[i] = 0;

        world.blocks.push_back(b);
    }

    world.finalize();      
    world.rebuildChunks(); // builds spatial structure

    return true;
}
