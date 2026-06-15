#include "shadow-config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int64_t getFileModifiedTime(const std::string& path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ft.time_since_epoch()).count();
}

ShadowConfig& ShadowConfig::instance()
{
    static ShadowConfig cfg;
    return cfg;
}

bool ShadowConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[SHADOWS] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = getFileModifiedTime(path);
        return false;
    }

    try {
        json j;
        file >> j;

        ShadowConfigData d;

        d.enabled = j.value("enabled", d.enabled);
        d.shadowMapSize = j.value("shadowMapSize", d.shadowMapSize);
        d.shadowDistance = j.value("shadowDistance", d.shadowDistance);
        d.shadowBias = j.value("shadowBias", d.shadowBias);
        d.shadowDarkness = j.value("shadowDarkness", d.shadowDarkness);
        d.shadowSoftness = j.value("shadowSoftness", d.shadowSoftness);
        d.stabilize = j.value("stabilize", d.stabilize);
        d.debugDrawShadowFrustum = j.value("debugDrawShadowFrustum", d.debugDrawShadowFrustum);
        d.debugShadowMap = j.value("debugShadowMap", d.debugShadowMap);

        d.playersCastShadows = j.value("playersCastShadows", d.playersCastShadows);
        d.npcsCastShadows = j.value("npcsCastShadows", d.npcsCastShadows);
        d.weaponsCastShadows = j.value("weaponsCastShadows", d.weaponsCastShadows);
        d.effectsCastShadows = j.value("effectsCastShadows", d.effectsCastShadows);
        d.particlesCastShadows = j.value("particlesCastShadows", d.particlesCastShadows);
        d.playersReceiveShadows = j.value("playersReceiveShadows", d.playersReceiveShadows);
        d.npcsReceiveShadows = j.value("npcsReceiveShadows", d.npcsReceiveShadows);
        d.worldReceivesShadows = j.value("worldReceivesShadows", d.worldReceivesShadows);
        d.effectsReceiveShadows = j.value("effectsReceiveShadows", d.effectsReceiveShadows);
        d.effectShadowCutoffAlpha = j.value("effectShadowCutoffAlpha", d.effectShadowCutoffAlpha);

        if (j.contains("shadowTint") && j["shadowTint"].is_array() && j["shadowTint"].size() == 3) {
            d.shadowTint.r = j["shadowTint"][0].get<float>();
            d.shadowTint.g = j["shadowTint"][1].get<float>();
            d.shadowTint.b = j["shadowTint"][2].get<float>();
        }

        mData = d;
        mLastModified = getFileModifiedTime(path);
        printf("[SHADOWS] Loaded config from %s\n", path.c_str());
        return true;

    } catch (const std::exception& e) {
        printf("[SHADOWS] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

void ShadowConfig::reset()
{
    mData = ShadowConfigData{};
    printf("[SHADOWS] Reset to defaults\n");
}

bool ShadowConfig::checkFileChanged() const
{
    if (mPath.empty()) return false;
    int64_t current = getFileModifiedTime(mPath);
    return current != mLastModified && current != 0;
}

bool ShadowConfig::pollReload()
{
    if (!checkFileChanged()) return false;
    printf("[SHADOWS] File changed\n");
    printf("[SHADOWS] Reloaded %s\n", mPath.c_str());
    return load(mPath);
}
