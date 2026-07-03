#include "healthbar-config.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
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

static float read01(const json& j, const char* key, float def)
{
    return std::clamp(j.value(key, def), 0.0f, 1.0f);
}

static glm::vec4 readColor(const json& j, const char* rk, const char* gk, const char* bk, const char* ak,
                           float rdef, float gdef, float bdef, float adef)
{
    return glm::vec4{
        read01(j, rk, rdef),
        read01(j, gk, gdef),
        read01(j, bk, bdef),
        read01(j, ak, adef)
    };
}

HealthbarConfig& HealthbarConfig::instance()
{
    static HealthbarConfig config;
    return config;
}

bool HealthbarConfig::load(const std::string& path)
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
        HealthbarConfigData d;
        d.aimModeEnabled = j.value("aim_mode_enabled", d.aimModeEnabled);
        d.aimConeDegrees = std::max(0.0f, j.value("aim_cone_degrees", d.aimConeDegrees));
        d.triangleSize = std::max(1.0f, j.value("triangle_size", d.triangleSize));
        d.triangleOffset = j.value("triangle_offset", d.triangleOffset);
        d.triangleAlpha = read01(j, "triangle_alpha", d.triangleAlpha);
        d.triangleFadeTicks = std::max(1, j.value("triangle_fade_ticks", d.triangleFadeTicks));
        d.blinkHpThreshold = std::max(0, j.value("blink_hp_threshold", d.blinkHpThreshold));
        d.blinkTicks = std::max(1, j.value("blink_ticks", d.blinkTicks));
        d.showNameInAimMode = j.value("show_name_in_aim_mode", d.showNameInAimMode);
        d.showHpTextInAimMode = j.value("show_hp_text_in_aim_mode", d.showHpTextInAimMode);
        d.showBarInAimMode = j.value("show_bar_in_aim_mode", d.showBarInAimMode);
        d.greenColor = readColor(j, "green_r", "green_g", "green_b", "green_a", 0.2f, 1.0f, 0.3f, 1.0f);
        d.yellowColor = readColor(j, "yellow_r", "yellow_g", "yellow_b", "yellow_a", 1.0f, 1.0f, 0.2f, 1.0f);
        d.orangeColor = readColor(j, "orange_r", "orange_g", "orange_b", "orange_a", 1.0f, 0.5f, 0.1f, 1.0f);
        d.redColor = readColor(j, "red_r", "red_g", "red_b", "red_a", 1.0f, 0.1f, 0.1f, 1.0f);
        d.blackColor = readColor(j, "black_r", "black_g", "black_b", "black_a", 0.0f, 0.0f, 0.0f, 1.0f);
        mData = d;
        mLastModified = modifiedTime(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool HealthbarConfig::save()
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(mPath).parent_path(), ec);
    const auto& d = mData;
    json j = {
        {"aim_mode_enabled", d.aimModeEnabled},
        {"aim_cone_degrees", d.aimConeDegrees},
        {"triangle_size", d.triangleSize},
        {"triangle_offset", d.triangleOffset},
        {"triangle_alpha", d.triangleAlpha},
        {"triangle_fade_ticks", d.triangleFadeTicks},
        {"blink_hp_threshold", d.blinkHpThreshold},
        {"blink_ticks", d.blinkTicks},
        {"show_name_in_aim_mode", d.showNameInAimMode},
        {"show_hp_text_in_aim_mode", d.showHpTextInAimMode},
        {"show_bar_in_aim_mode", d.showBarInAimMode},
        {"green_r", d.greenColor.r}, {"green_g", d.greenColor.g}, {"green_b", d.greenColor.b}, {"green_a", d.greenColor.a},
        {"yellow_r", d.yellowColor.r}, {"yellow_g", d.yellowColor.g}, {"yellow_b", d.yellowColor.b}, {"yellow_a", d.yellowColor.a},
        {"orange_r", d.orangeColor.r}, {"orange_g", d.orangeColor.g}, {"orange_b", d.orangeColor.b}, {"orange_a", d.orangeColor.a},
        {"red_r", d.redColor.r}, {"red_g", d.redColor.g}, {"red_b", d.redColor.b}, {"red_a", d.redColor.a},
        {"black_r", d.blackColor.r}, {"black_g", d.blackColor.g}, {"black_b", d.blackColor.b}, {"black_a", d.blackColor.a},
    };
    std::ofstream file(mPath);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    file.close();
    mLastModified = modifiedTime(mPath);
    return true;
}

bool HealthbarConfig::reload()
{
    return load(mPath);
}

bool HealthbarConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    return current != 0 && current != mLastModified && reload();
}

HealthbarConfigData& HealthbarConfig::edit()
{
    return mData;
}
