#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

struct UIRect;

// A single editable element in a GUI layout
struct GuiElement {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

// Manages a single layout file (one per menu/screen)
class GuiLayout {
public:
    bool load(const std::string& filePath);
    bool save(const std::string& filePath) const;

    // Get element rect, returns fallback if not found
    UIRect getRect(const std::string& id, const UIRect& fallback) const;

    // Get element rect with center offset: stored offset is added to (cx, cy)
    // This makes layouts resolution-independent
    UIRect getRectCentered(const std::string& id, const UIRect& fallback,
                           float cx, float cy) const;

    // Get raw element data (nullptr if not found)
    const GuiElement* get(const std::string& id) const;

    // Set or update element position/size
    void set(const std::string& id, float x, float y, float w, float h);

    // Check if file changed since last load (for hot reload)
    bool checkFileChanged() const;

    // Get all element IDs
    std::vector<std::string> elementIds() const;

    const std::string& filePath() const { return mFilePath; }

private:
    std::string mFilePath;
    std::unordered_map<std::string, GuiElement> mElements;
    int64_t mLastModified = 0;
};

// Manages ALL layouts with global hot-reload
class GuiLayoutManager {
public:
    static GuiLayoutManager& instance();

    // Get or create a layout for a given config file
    GuiLayout& getLayout(const std::string& filePath);

    // Call once per frame to reload changed files
    void pollReload();

    // Access editor state
    bool isEditorEnabled() const { return mEditorEnabled; }
    void setEditorEnabled(bool enabled) { mEditorEnabled = enabled; }
    void toggleEditor() { mEditorEnabled = !mEditorEnabled; }

    // Editor: currently selected element info
    std::string selectedLayoutFile;
    std::string selectedElementId;

    // Editor: save all layouts
    void saveAll();

private:
    GuiLayoutManager() = default;
    std::unordered_map<std::string, GuiLayout> mLayouts;
    bool mEditorEnabled = false;
    int64_t mLastPollTime = 0;
};

// Helper: builds a UIRect from values
UIRect makeRect(float x, float y, float w, float h);
