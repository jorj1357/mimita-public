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
        Block b;

        // 1. read raw JSON values
        glm::vec3 jsonPos(
            jb["position"][0],
            jb["position"][1],
            jb["position"][2]
        );

        glm::vec3 jsonRot(
            jb["rotation"][0],
            jb["rotation"][1],
            jb["rotation"][2]
        );

        glm::vec3 jsonSize(
            jb["size"][0],
            jb["size"][1],
            jb["size"][2]
        );

        // 2. convert Blender → engine space
        b.pos  = blenderPosToEngine(jsonPos);
        b.size = jsonSize;

        glm::vec3 rotDeg = blenderRotToEngine(jsonRot);

        // optional snap (recommended for blocks)
        rotDeg = glm::round(rotDeg);

        // 3. bake rotation ONCE
        b.rot = eulerToMat(rotDeg);

        world.blocks.push_back(b);
    }

    for (auto& js : j["spheres"]) {
        Sphere s;
        s.pos = blenderPosToEngine(glm::vec3(
            js["position"][0],
            js["position"][1],
            js["position"][2]
        ));
        s.radius = (float)js["radius"];

        world.spheres.push_back(s);
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
