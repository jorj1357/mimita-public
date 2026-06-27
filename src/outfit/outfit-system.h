#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <filesystem>
#include <glad/glad.h>
#include "outfit-schema.h"
#include <nlohmann/json.hpp>

class Player;

class OutfitSystem {
public:
    static OutfitSystem& instance();

    bool load(const std::string& outfitName);
    bool applyToPlayer(Player& player);
    void pollHotReload(Player* player = nullptr);

    bool hasOutfit() const { return mLoaded; }
    const std::string& currentName() const { return mCurrentName; }
    const OutfitData& data() const { return mData; }
    std::vector<std::string> listOutfits() const;
    static std::string outfitPath(const std::string& name);
    static bool applySingleTexture(Player& player, const std::string& texturePath, bool reloadTexture = false);

private:
    OutfitSystem() = default;
    bool parseOutfitJson(const std::string& path);
    bool buildAtlas(Player& player);
    bool applyAtlasToPlayer(Player& player);
    void loadCosmetics(const nlohmann::json& j);
    void watchCosmeticFiles();
    std::string resolvePath(const std::string& relativePath) const;

    std::string mCurrentName;
    std::string mBasePath;
    OutfitData mData;
    bool mLoaded = false;
    std::filesystem::file_time_type mLastJsonWriteTime;
    std::unordered_map<std::string, std::filesystem::file_time_type> mAssetWriteTimes;
    std::chrono::steady_clock::time_point mLastCheckTime;
    float mPollInterval = 0.25f;
};
