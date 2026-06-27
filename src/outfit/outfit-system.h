#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <filesystem>
#include <glad/glad.h>
#include "outfit-schema.h"

class Player;

class OutfitSystem {
public:
    static OutfitSystem& instance();

    // Load an outfit from assets/avatars/<name>/outfit.json
    bool load(const std::string& outfitName);
    bool applyToPlayer(Player& player);

    // Hot reload - call once per frame
    void pollHotReload(Player* player = nullptr);

    // Queries
    bool hasOutfit() const { return mLoaded; }
    const std::string& currentName() const { return mCurrentName; }
    const OutfitData& data() const { return mData; }
    std::vector<std::string> listOutfits() const;
    static std::string outfitPath(const std::string& name);

    // Apply a single PNG texture to all body parts (replaces OutfitAtlas)
    static bool applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture = false);

private:
    OutfitSystem() = default;

    bool parseOutfitJson(const std::string& path);
    bool buildAtlas(Player& player);
    bool applyAtlasToPlayer(Player& player);

    std::string resolvePath(const std::string& relativePath) const;

    std::string mCurrentName;
    std::string mBasePath;
    OutfitData mData;
    bool mLoaded = false;

    // Hot reload state
    std::filesystem::file_time_type mLastWriteTime;
    std::chrono::steady_clock::time_point mLastCheckTime;
    float mPollInterval = 0.25f;
};
