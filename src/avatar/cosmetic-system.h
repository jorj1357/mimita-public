#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "map/map_common.h"

class Player;
struct CosmeticSlot;

// Runtime data for a single loaded cosmetic
struct CosmeticInstance
{
    std::string choice;       // GLB filename (key)
    Mesh mesh;                // loaded GLB render mesh
    bool loaded = false;
};

class CosmeticSystem
{
public:
    static CosmeticSystem& instance();

    // Load/reload all cosmetics from a list of CosmeticSlot definitions
    void loadCosmetics(const std::vector<CosmeticSlot>& slots);

    // Scan the cosmetics directory for available .glb files
    std::vector<std::string> scanAvailableCosmetics() const;

    // Render all currently loaded cosmetics on a player
    void renderCosmetics(const Player& player) const;

    // Get runtime instance by choice name
    const CosmeticInstance* find(const std::string& choice) const;

    // Clear all loaded cosmetics
    void clear();

private:
    CosmeticSystem() = default;

    // Load a single GLB file as a cosmetic
    bool loadCosmeticGLB(const std::string& choice);

    std::unordered_map<std::string, CosmeticInstance> mCosmetics;
};
