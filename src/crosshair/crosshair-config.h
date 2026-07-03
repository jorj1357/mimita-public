#pragma once

#include <cstdint>
#include <string>

struct CrosshairSettings {
    bool enabled = true;
    int red = 0;
    int green = 255;
    int blue = 255;
    int alpha = 255;
    float size = 8.0f;
    float thickness = 2.0f;
    float gap = 5.0f;
    bool dot = false;
    bool outline = true;
    float outlineThickness = 1.0f;
    bool dynamic = false;
    bool showTop = true;
    bool showBottom = true;
    bool showLeft = true;
    bool showRight = true;
};

class CrosshairConfig {
public:
    static CrosshairConfig& instance();

    bool load(const std::string& path = "config/crosshair.json");
    bool save();
    bool reload();
    bool pollReload();
    void reset();

    const CrosshairSettings& data() const { return mData; }
    CrosshairSettings& edit();

private:
    CrosshairConfig() = default;

    CrosshairSettings mData;
    std::string mPath = "config/crosshair.json";
    int64_t mLastModified = 0;
};
