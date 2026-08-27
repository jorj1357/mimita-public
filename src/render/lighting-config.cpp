#include "lighting-config.h"

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
    return std::chrono::duration_cast<std::chrono::seconds>(
        ft.time_since_epoch()).count();
}

LightingConfig& LightingConfig::instance()
{
    static LightingConfig cfg;
    return cfg;
}

bool LightingConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[LIGHTING] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = getFileModifiedTime(path);
        return false;
    }

    try {
        json j;
        file >> j;

        LightingConfigData d;

        if (j.contains("ambient")) {
            auto& a = j["ambient"];
            d.ambientColor.r = a.value("r", d.ambientColor.r);
            d.ambientColor.g = a.value("g", d.ambientColor.g);
            d.ambientColor.b = a.value("b", d.ambientColor.b);
            d.ambientIntensity = a.value("intensity", d.ambientIntensity);
        }

        if (j.contains("light")) {
            auto& l = j["light"];
            if (l.contains("dir")) {
                d.lightDir.x = l["dir"].value("x", d.lightDir.x);
                d.lightDir.y = l["dir"].value("y", d.lightDir.y);
                d.lightDir.z = l["dir"].value("z", d.lightDir.z);
                d.lightDir = glm::normalize(d.lightDir);
            }
            d.diffuseStrength = l.value("diffuseStrength", d.diffuseStrength);
        }

        if (j.contains("edge")) {
            auto& e = j["edge"];
            d.edgeDarkness = e.value("darkness", d.edgeDarkness);
            d.edgeWidth = e.value("width", d.edgeWidth);
        }

        if (j.contains("ao")) {
            auto& a = j["ao"];
            d.aoDarkness = a.value("darkness", d.aoDarkness);
            d.aoContrast = a.value("contrast", d.aoContrast);
            d.aoEnabled = a.value("enabled", d.aoEnabled);
            d.aoStrength = a.value("strength", d.aoStrength);
        }

        if (j.contains("texture")) {
            auto& t = j["texture"];
            d.textureContrast = t.value("contrast", d.textureContrast);
            d.textureBrightness = t.value("brightness", d.textureBrightness);
        }

        if (j.contains("fog")) {
            auto& f = j["fog"];
            d.fogEnabled = f.value("enabled", d.fogEnabled);
            d.fogDensity = f.value("density", d.fogDensity);
            d.fogColor.r = f.value("r", d.fogColor.r);
            d.fogColor.g = f.value("g", d.fogColor.g);
            d.fogColor.b = f.value("b", d.fogColor.b);
        }

        if (j.contains("post")) {
            auto& p = j["post"];
            d.brightness = p.value("brightness", d.brightness);
            d.contrast = p.value("contrast", d.contrast);
            d.saturation = p.value("saturation", d.saturation);
            d.gamma = p.value("gamma", d.gamma);
            d.hueShift = p.value("hueShift", d.hueShift);
        }

        mData = d;
        mLastModified = getFileModifiedTime(path);
        printf("[LIGHTING] Loaded config from %s\n", path.c_str());
        return true;

    } catch (const std::exception& e) {
        printf("[LIGHTING] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

void LightingConfig::reset()
{
    mData = LightingConfigData{};
    printf("[LIGHTING] Reset to defaults\n");
}

bool LightingConfig::checkFileChanged() const
{
    if (mPath.empty()) return false;
    int64_t current = getFileModifiedTime(mPath);
    return current != mLastModified && current != 0;
}

bool LightingConfig::pollReload()
{
    static auto sLastPoll = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - sLastPoll).count() < 1.0)
        return false;
    sLastPoll = now;
    if (!checkFileChanged()) return false;
    printf("[LIGHTING] Detected change in %s, reloading...\n", mPath.c_str());
    return load(mPath);
}
