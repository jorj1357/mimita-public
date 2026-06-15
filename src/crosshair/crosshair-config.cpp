#include "crosshair-config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

static void clampSettings(CrosshairSettings& d)
{
    d.red = std::clamp(d.red, 0, 255);
    d.green = std::clamp(d.green, 0, 255);
    d.blue = std::clamp(d.blue, 0, 255);
    d.alpha = std::clamp(d.alpha, 0, 255);
    d.size = std::clamp(d.size, 0.0f, 64.0f);
    d.thickness = std::clamp(d.thickness, 1.0f, 16.0f);
    d.gap = std::clamp(d.gap, 0.0f, 64.0f);
    d.outlineThickness = std::clamp(d.outlineThickness, 0.0f, 8.0f);
}

CrosshairConfig& CrosshairConfig::instance()
{
    static CrosshairConfig config;
    return config;
}

bool CrosshairConfig::load(const std::string& path)
{
    mPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        save();
        return false;
    }

    try {
        json j;
        file >> j;
        CrosshairSettings d;
        d.enabled = j.value("enabled", d.enabled);
        d.red = j.value("red", d.red);
        d.green = j.value("green", d.green);
        d.blue = j.value("blue", d.blue);
        d.alpha = j.value("alpha", d.alpha);
        d.size = j.value("size", d.size);
        d.thickness = j.value("thickness", d.thickness);
        d.gap = j.value("gap", d.gap);
        d.dot = j.value("dot", d.dot);
        d.outline = j.value("outline", d.outline);
        d.outlineThickness = j.value("outlineThickness", d.outlineThickness);
        d.dynamic = j.value("dynamic", d.dynamic);
        d.showTop = j.value("showTop", d.showTop);
        d.showBottom = j.value("showBottom", d.showBottom);
        d.showLeft = j.value("showLeft", d.showLeft);
        d.showRight = j.value("showRight", d.showRight);
        clampSettings(d);
        mData = d;
        mLastModified = modifiedTime(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool CrosshairConfig::save()
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(mPath).parent_path(), ec);
    const auto& d = mData;
    json j = {
        {"enabled", d.enabled},
        {"red", d.red}, {"green", d.green}, {"blue", d.blue}, {"alpha", d.alpha},
        {"size", d.size}, {"thickness", d.thickness}, {"gap", d.gap},
        {"dot", d.dot}, {"outline", d.outline},
        {"outlineThickness", d.outlineThickness}, {"dynamic", d.dynamic},
        {"showTop", d.showTop}, {"showBottom", d.showBottom},
        {"showLeft", d.showLeft}, {"showRight", d.showRight}
    };
    std::ofstream file(mPath);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    file.close();
    mLastModified = modifiedTime(mPath);
    return true;
}

bool CrosshairConfig::reload()
{
    return load(mPath);
}

bool CrosshairConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    return current != 0 && current != mLastModified && reload();
}

void CrosshairConfig::reset()
{
    mData = CrosshairSettings{};
    save();
}
