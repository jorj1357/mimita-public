// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-loader.cpp
// dec 18 2025
/**
 * purpose
 * take json file
 * turnn it into stuff we walk on
 */

#include "world/world-loader.h"

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

    for (auto& b : j["blocks"]) {
        world.blocks.push_back({
            glm::vec3(
                b["position"][0],
                b["position"][1],
                b["position"][2]
            ),
            glm::vec3(
                b["size"][0],
                b["size"][1],
                b["size"][2]
            ),
            glm::vec3(
                b["rotation"][0],
                b["rotation"][1],
                b["rotation"][2]
            )
        });
    }

    for (auto& s : j["spheres"]) {
        world.spheres.push_back({
            glm::vec3(
                s["position"][0],
                s["position"][1],
                s["position"][2]
            ),
            (float)s["radius"]
        });
    }

    printf(
        "[WORLD] Loaded %zu blocks, %zu spheres from %s\n",
        world.blocks.size(),
        world.spheres.size(),
        path
    );

    world.rebuildChunks();
    return true;
}
