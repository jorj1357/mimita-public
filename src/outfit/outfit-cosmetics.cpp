#include "outfit-system.h"

#include <cstdio>

#include "entities/player.h"
#include "map/map_loader.h"

// ── Load cosmetics from parsed JSON ─────────────────────────────────
void OutfitSystem::loadCosmetics(const nlohmann::json& j)
{
    if (!j.contains("cosmetics") || !j["cosmetics"].is_array()) return;

    for (const auto& c : j["cosmetics"]) {
        CosmeticInstance ci;
        ci.id = c.value("id", "");
        ci.parentBone = c.value("parent", "root");
        if (c.contains("position") && c["position"].is_array() && c["position"].size() >= 3)
            ci.position = {c["position"][0], c["position"][1], c["position"][2]};
        if (c.contains("rotation") && c["rotation"].is_array() && c["rotation"].size() >= 3)
            ci.rotation = {c["rotation"][0], c["rotation"][1], c["rotation"][2]};
        if (c.contains("scale") && c["scale"].is_array() && c["scale"].size() >= 3)
            ci.scale = {c["scale"][0], c["scale"][1], c["scale"][2]};
        if (c.contains("model")) {
            std::string modelPath = resolvePath(c["model"].get<std::string>());
            ci.renderMesh = loadGLB(modelPath.c_str());
            if (ci.renderMesh.verts.empty())
                printf("[OUTFIT] Failed to load cosmetic model: %s\n", modelPath.c_str());
        }
        mData.cosmetics.push_back(std::move(ci));
    }
}

// ── Track cosmetic GLB files for hot reload ─────────────────────────
void OutfitSystem::watchCosmeticFiles()
{
    for (const auto& c : mData.cosmetics) {
        for (const auto& candidate : {
            mBasePath + "/cosmetics/" + c.id + ".glb",
            "assets/objects/things/cosmetics/" + c.id + ".glb"
        }) {
            if (std::filesystem::exists(candidate)) {
                mAssetWriteTimes[candidate] = std::filesystem::last_write_time(candidate);
                break;
            }
        }
    }
}
