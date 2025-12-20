// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-loader.cpp
// dec 18 2025
/**
 * purpose
 * take json file
 * turnn it into stuff we walk on
 */

#include "world/world-loader.h"
#include "world/world.h"

#include <fstream>
#include <cstdio>

// use whatever JSON lib you already use elsewhere
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool loadWorldFromJSON(World& world, const char* path)
{
    world.clear();

    std::ifstream f(path);
    if (!f.is_open()) {
        printf("[WORLD] Failed to open %s\n", path);
        return false;
    }

    json j;
    f >> j;

    for (auto& jb : j["blocks"]) {
        Block b{};

        b.pos = glm::vec3(
            jb["position"][0],
            jb["position"][1],
            jb["position"][2]
        );

        b.size = glm::vec3(
            jb["size"][0],
            jb["size"][1],
            jb["size"][2]
        );

        b.rotEuler = glm::vec3(
            jb["rotation"][0],
            jb["rotation"][1],
            jb["rotation"][2]
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
