// 2026-10-25
/* purpose
* Load the hot-reloadable map-editor presentation and interaction settings.
* Keep font sizes, outliner geometry, handle sizes, focus distance, camera
* speed, label density, and autosave timing out of hardcoded editor code.
* Does NOT own authored map data or editor history.
* Does NOT change gameplay settings outside the editor.
*/
#pragma once

#include <cstdint>
#include <string>

struct MapEditorConfigData
{
    float titleScale = 0.42f;
    float bodyScale = 0.30f;
    float rowScale = 0.28f;
    float helpScale = 0.24f;
    float outlinerWidth = 380.0f;
    float outlinerTop = 112.0f;
    float outlinerBottomPadding = 32.0f;
    float rowHeight = 26.0f;
    float propertiesHeight = 156.0f;
    float scrollbarWidth = 14.0f;
    float scrollbarInset = 6.0f;
    float handleLength = 12.0f;
    float handlePickPixels = 60.0f;
    float handleRadius = 0.80f;
    float handleThickness = 0.48f;
    float blockWireAlpha = 0.5f;
    float focusMinDistance = 1.5f;
    float focusPadding = 1.35f;
    float cameraSpeedMultiplier = 1.0f;
    int autosaveDelayMs = 400;
    // "all" | "selected" | "hovered" | "off"
    std::string labelMode = "selected";
};

class MapEditorConfig
{
public:
    static MapEditorConfig& instance();
    bool load(const std::string& path = "config/mapeditor.json");
    bool pollReload();
    const MapEditorConfigData& data() const { return mData; }

private:
    MapEditorConfig() = default;
    MapEditorConfigData mData;
    std::string mPath;
    int64_t mLastModified = 0;
};
