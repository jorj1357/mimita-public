// 2026-10-25
#include "mapeditor/map-editor-config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

float readScale(const json& root, const char* key, float fallback)
{
    if (!root.contains(key) || !root[key].is_number()) return fallback;
    return std::clamp(root[key].get<float>(), 0.05f, 4.0f);
}

int readInt(const json& root, const char* key, int fallback, int minimum, int maximum)
{
    if (!root.contains(key) || !root[key].is_number_integer()) return fallback;
    return std::clamp(root[key].get<int>(), minimum, maximum);
}

float readFloat(const json& root, const char* key, float fallback,
                float minimum, float maximum)
{
    if (!root.contains(key) || !root[key].is_number()) return fallback;
    return std::clamp(root[key].get<float>(), minimum, maximum);
}

} // namespace

MapEditorConfig& MapEditorConfig::instance()
{
    static MapEditorConfig config;
    return config;
}

bool MapEditorConfig::load(const std::string& path)
{
    MapEditorConfigData next;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Render,
            "[MAP EDITOR CONFIG] missing %s; using defaults\n", path.c_str());
        mData = next;
        mPath = path;
        mLastModified = 0;
        return false;
    }

    try {
        const json root = parseJsonConfig(file);
        next.titleScale = readScale(root, "title_scale", next.titleScale);
        next.bodyScale = readScale(root, "body_scale", next.bodyScale);
        next.rowScale = readScale(root, "row_scale", next.rowScale);
        next.helpScale = readScale(root, "help_scale", next.helpScale);
        next.outlinerWidth = readFloat(root, "outliner_width", next.outlinerWidth, 240.0f, 800.0f);
        next.outlinerTop = readFloat(root, "outliner_top", next.outlinerTop, 0.0f, 1000.0f);
        next.outlinerBottomPadding =
            readFloat(root, "outliner_bottom_padding", next.outlinerBottomPadding, 0.0f, 400.0f);
        next.rowHeight = readFloat(root, "row_height", next.rowHeight, 14.0f, 80.0f);
        next.handleLength = readFloat(root, "handle_length", next.handleLength, 0.2f, 20.0f);
        next.handlePickPixels =
            readFloat(root, "handle_pick_pixels", next.handlePickPixels, 4.0f, 80.0f);
        next.handleRadius = readFloat(root, "handle_radius", next.handleRadius, 0.01f, 2.0f);
        next.focusMinDistance =
            readFloat(root, "focus_min_distance", next.focusMinDistance, 0.1f, 100.0f);
        next.focusPadding = readFloat(root, "focus_padding", next.focusPadding, 0.5f, 5.0f);
        next.cameraSpeedMultiplier = readFloat(root, "camera_speed_multiplier",
            next.cameraSpeedMultiplier, 0.01f, 1000.0f);
        next.autosaveDelayMs =
            readInt(root, "autosave_delay_ms", next.autosaveDelayMs, 0, 10000);
        if (root.contains("label_mode") && root["label_mode"].is_string())
            next.labelMode = root["label_mode"].get<std::string>();
        if (next.labelMode != "all" && next.labelMode != "selected" &&
            next.labelMode != "hovered" && next.labelMode != "off")
            next.labelMode = "selected";
        mData = next;
        mPath = path;
        mLastModified = modifiedTime(path);
        Debug::warn(Debug::Category::Render,
            "[MAP EDITOR CONFIG] loaded %s\n", path.c_str());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Render,
            "[MAP EDITOR CONFIG] invalid %s: %s; keeping previous\n", path.c_str(), e.what());
        mPath = path;
        mLastModified = modifiedTime(path);
        return false;
    }
}

bool MapEditorConfig::pollReload()
{
    if (mPath.empty()) return false;
    const int64_t write = modifiedTime(mPath);
    if (write == 0 || write == mLastModified) return false;
    return load(mPath);
}
