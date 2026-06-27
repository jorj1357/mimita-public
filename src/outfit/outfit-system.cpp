#include "outfit-system.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "entities/player.h"
#include "world/texture-store.h"

using json = nlohmann::json;

extern TextureStore gTextures;

// ── Singleton ───────────────────────────────────────────────────────
OutfitSystem& OutfitSystem::instance()
{
    static OutfitSystem sys;
    return sys;
}

std::string OutfitSystem::outfitPath(const std::string& name)
{
    return "assets/avatars/" + name;
}

std::string OutfitSystem::resolvePath(const std::string& relativePath) const
{
    if (relativePath.empty()) return {};
    if (relativePath[0] == '/' || relativePath[0] == '\\' ||
        (relativePath.size() > 1 && relativePath[1] == ':'))
        return relativePath;
    return mBasePath + "/" + relativePath;
}

// ── JSON parsing ────────────────────────────────────────────────────
bool OutfitSystem::parseOutfitJson(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[OUTFIT] No outfit.json at %s\n", path.c_str());
        return false;
    }

    try {
        json j;
        file >> j;
        mData.name = j.value("name", mCurrentName);

        if (j.contains("textures") && j["textures"].is_object()) {
            for (auto& [key, val] : j["textures"].items())
                mData.textures[key] = val.get<std::string>();
        }

        if (j.contains("faces") && j["faces"].is_object()) {
            for (auto& [partKey, partVal] : j["faces"].items()) {
                if (!partVal.is_object()) continue;
                for (auto& [faceKey, faceVal] : partVal.items()) {
                    PartFaceAssignment assign;
                    if (faceVal.is_string())
                        assign.textureAlias = faceVal.get<std::string>();
                    else if (faceVal.is_object())
                        assign.textureAlias = faceVal.value("texture", "");
                    mData.faces[partKey][faceKey] = assign;
                }
            }
        }

        if (j.contains("faceOverrides") && j["faceOverrides"].is_object()) {
            for (auto& [key, ov] : j["faceOverrides"].items()) {
                size_t us = key.find('_');
                if (us == std::string::npos) continue;
                std::string part = key.substr(0, us);
                std::string face = key.substr(us + 1);
                auto it = mData.faces.find(part);
                if (it == mData.faces.end()) continue;
                auto fit = it->second.find(face);
                if (fit == it->second.end()) continue;
                auto& o = fit->second.overrides;
                if (ov.contains("stretchMode")) o.stretchMode = ov["stretchMode"];
                if (ov.contains("rotation")) o.rotation = ov["rotation"];
                if (ov.contains("offsetX")) o.offsetX = ov["offsetX"];
                if (ov.contains("offsetY")) o.offsetY = ov["offsetY"];
                if (ov.contains("scaleX")) o.scaleX = ov["scaleX"];
                if (ov.contains("scaleY")) o.scaleY = ov["scaleY"];
                if (ov.contains("hue")) o.hue = ov["hue"];
                if (ov.contains("saturation")) o.saturation = ov["saturation"];
                if (ov.contains("brightness")) o.brightness = ov["brightness"];
                if (ov.contains("contrast")) o.contrast = ov["contrast"];
                if (ov.contains("opacity")) o.opacity = ov["opacity"];
                if (ov.contains("tint") && ov["tint"].is_array() && ov["tint"].size() >= 3)
                    o.tint = {ov["tint"][0], ov["tint"][1], ov["tint"][2]};
            }
        }

        if (j.contains("colors") && j["colors"].is_object()) {
            for (auto& [key, val] : j["colors"].items()) {
                if (val.is_array() && val.size() >= 3)
                    mData.colors[key] = {val[0], val[1], val[2]};
            }
        }

        loadCosmetics(j);

        printf("[OUTFIT] Parsed outfit: %s (%zu textures, %zu cosmetics)\n",
               mData.name.c_str(), mData.textures.size(), mData.cosmetics.size());
        return true;
    } catch (const std::exception& e) {
        printf("[OUTFIT] Failed to parse outfit.json: %s\n", e.what());
        return false;
    }
}

// ── Load ────────────────────────────────────────────────────────────
bool OutfitSystem::load(const std::string& outfitName)
{
    mCurrentName = outfitName;
    mBasePath = outfitPath(outfitName);
    mData = OutfitData{};
    mData.name = outfitName;
    mData.basePath = mBasePath;

    std::string jsonPath = mBasePath + "/outfit.json";
    if (!parseOutfitJson(jsonPath)) {
        printf("[OUTFIT] No valid outfit.json for %s\n", outfitName.c_str());
        mLoaded = false;
        return false;
    }

    mLoaded = true;
    if (std::filesystem::exists(jsonPath))
        mLastJsonWriteTime = std::filesystem::last_write_time(jsonPath);

    mAssetWriteTimes.clear();
    for (const auto& [alias, path] : mData.textures) {
        std::string fullPath = resolvePath(path);
        if (std::filesystem::exists(fullPath))
            mAssetWriteTimes[fullPath] = std::filesystem::last_write_time(fullPath);
    }
    watchCosmeticFiles();

    mLastCheckTime = std::chrono::steady_clock::now();
    printf("[OUTFIT] Loaded: %s\n", outfitName.c_str());
    return true;
}

bool OutfitSystem::applyToPlayer(Player& player)
{
    if (!mLoaded || mCurrentName.empty()) return false;
    if (!buildAtlas(player)) return false;
    return applyAtlasToPlayer(player);
}

// ── Hot reload ──────────────────────────────────────────────────────
void OutfitSystem::pollHotReload(Player* player)
{
    if (!mLoaded || mCurrentName.empty()) return;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    std::string jsonPath = mBasePath + "/outfit.json";
    if (std::filesystem::exists(jsonPath)) {
        auto writeTime = std::filesystem::last_write_time(jsonPath);
        if (writeTime != mLastJsonWriteTime) {
            mLastJsonWriteTime = writeTime;
            printf("[OUTFIT] Hot reload (outfit.json) for: %s\n", mCurrentName.c_str());
            load(mCurrentName);
            if (player) applyToPlayer(*player);
            return;
        }
    }

    for (auto& [path, cachedTime] : mAssetWriteTimes) {
        if (!std::filesystem::exists(path)) continue;
        auto curTime = std::filesystem::last_write_time(path);
        if (curTime != cachedTime) {
            cachedTime = curTime;
            bool isGlb = (path.size() > 4 && path.substr(path.size()-4) == ".glb");
            printf("[OUTFIT] Hot reload (asset) for: %s (%s)\n", mCurrentName.c_str(), path.c_str());
            if (player) {
                if (isGlb) {
                    load(mCurrentName);
                    buildAtlas(*player);
                    applyAtlasToPlayer(*player);
                } else {
                    buildAtlas(*player);
                    applyAtlasToPlayer(*player);
                }
            }
            return;
        }
    }
}

// ── List outfits ────────────────────────────────────────────────────
std::vector<std::string> OutfitSystem::listOutfits() const
{
    std::vector<std::string> result;
    const std::string dir = "assets/avatars";
    if (!std::filesystem::exists(dir)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory())
            result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}
