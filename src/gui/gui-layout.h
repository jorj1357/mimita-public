#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

struct UIRect;

// ── UiAction ─────────────────────────────────────────────────────────
struct UiAction {
    std::string type;       // e.g. "avatar_editor.select_tab"
    std::string paramsJson; // JSON string of params
};

// ── UiEffect ─────────────────────────────────────────────────────────
struct UiEffect {
    std::string type;          // "sway", "hover_grow", "rainbow", etc.
    bool enabled = true;
    float amplitudeX = 4.0f;
    float amplitudeY = 2.0f;
    float speed = 0.8f;
    float phase = 0.0f;
    float scale = 1.05f;
    float radius = 120.0f;
    float strength = 40.0f;
    float returnSpeed = 8.0f;
    float maxOffset = 100.0f;
};

// ── UiTextRun ────────────────────────────────────────────────────────
struct UiTextRun {
    int start = 0;
    int length = 0;
    std::vector<float> color;    // RGBA
    float scale = 1.0f;
    bool bold = false;
    bool italic = false;
};

// A single editable element in a GUI layout
struct GuiElement {
    // Identity
    std::string id;
    std::string type = "button";   // "button", "text", "image", "panel",
                                   // "checkbox", "slider", "dropdown",
                                   // "tab_button", "modal", "scroll_panel",
                                   // "number_input", "text_input", "divider",
                                   // "spacer", "preview_3d", "container"

    // Layout
    float x = 0.0f;
    float y = 0.0f;
    float w = 100.0f;
    float h = 30.0f;
    float textOffsetX = 8.0f;
    float textOffsetY = 4.0f;
    float padding = 8.0f;
    float margin = 0.0f;
    float rotation = 0.0f;
    std::string anchorX = "left";
    std::string anchorY = "top";
    int layer = 0;
    std::string layoutDirection;   // "vertical","horizontal","grid"
    float layoutSpacing = 0.0f;
    std::string layoutAlign;       // "start","center","end"

    // Appearance
    bool visible = true;
    bool enabled = true;
    float opacity = 1.0f;
    float hoverScale = 1.0f;
    std::string shape;             // "rectangle","rounded_rectangle","circle","ellipse"
    float cornerRadius = 0.0f;
    float borderThickness = 0.0f;

    // Text
    std::string text;
    std::string font;
    float fontSize = 0.0f;
    float animationLifetimeTicks = 0.0f;
    float animationRisePixels = 0.0f;
    std::string textAlign = "left"; // "left", "center", "right"
    std::string verticalAlign = "top"; // "top", "middle", "bottom"
    float paddingX = 8.0f;
    float paddingY = 4.0f;
    std::vector<UiTextRun> textRuns;

    // Text input options
    int maxLength = 200;
    bool selectAllOnFocus = false;
    bool submitOnEnter = false;
    std::string characterFilter; // "server_address", "numeric", etc.

    // Colors: [r, g, b, a] each in 0-1 range (empty vector = not set for overrides)
    std::vector<float> textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<float> backgroundColor = {0.2f, 0.2f, 0.3f, 1.0f};
    std::vector<float> hoverColor;
    std::vector<float> pressedColor;
    std::vector<float> outlineColor;
    std::vector<float> disabledColor;
    std::vector<float> selectedColor;

    // Image / media
    std::string backgroundImage;
    std::string backgroundVideo;
    float imageOffsetX = 0.0f;
    float imageOffsetY = 0.0f;
    float imageScaleX = 1.0f;
    float imageScaleY = 1.0f;
    float imageRotation = 0.0f;
    std::vector<float> imageColor; // RGBA multiplier (default white)
    std::string imageFitMode;       // "fit","fill","stretch","center","tile"

    // Action
    UiAction action;

    // Data binding
    std::string binding;             // data binding ID
    std::string bindingFallback;     // fallback text
    std::string bindingItems;        // binding key for dropdown items (comma-separated)
    std::string visibleWhenBinding;
    std::string visibleWhenOp;       // "equals","not_equals","exists"
    std::string visibleWhenValue;

    // Sound
    std::string hoverSound;
    std::string clickSound;

    // Effects
    std::vector<UiEffect> effects;

    // Children (for containers)
    std::vector<GuiElement> children;

    // State-based styles
    // State overrides use the same field names as the base element
    // Stored as JSON object under "states" key
    std::string statesJson;

    // Helpers for converting color vectors to glm::vec4
    glm::vec4 getTextColorVec() const;
    glm::vec4 getBackgroundColorVec() const;
    bool hasHoverColor() const { return hoverColor.size() == 4; }
    glm::vec4 getHoverColorVec() const;
    bool hasPressedColor() const { return pressedColor.size() == 4; }
    glm::vec4 getPressedColorVec() const;
    bool hasOutlineColor() const { return outlineColor.size() == 4; }
    glm::vec4 getOutlineColorVec() const;

    // Reset all color overrides (hover, pressed, outline) to unset
    void clearColorOverrides();
};

// Manages a single layout file (one per menu/screen)
class GuiLayout {
public:
    bool load(const std::string& filePath);
    bool save(const std::string& filePath) const;

    // Get element rect, returns fallback if not found
    UIRect getRect(const std::string& id, const UIRect& fallback) const;

    // Get element rect in absolute design coordinates (1920x1080 space).
    // Returns fallback if element not found.
    UIRect getRectDesign(const std::string& id, const UIRect& fallback) const;

    // Get element rect with center offset: stored offset is added to (cx, cy)
    // This makes layouts resolution-independent
    UIRect getRectCentered(const std::string& id, const UIRect& fallback,
                           float cx, float cy) const;

    // Get raw element data (nullptr if not found)
    const GuiElement* get(const std::string& id) const;

    // Set or update element position/size
    void set(const std::string& id, float x, float y, float w, float h);
    void set(const std::string& id, float x, float y, float w, float h, float textOffsetX, float textOffsetY);
    void setElement(const GuiElement& element);

    // Check if file changed since last load (for hot reload)
    bool checkFileChanged() const;

    // Get all element IDs (cached, rebuilt on layout changes)
    const std::vector<std::string>& elementIds() const;

    const std::string& filePath() const { return mFilePath; }

    // Dirty tracking
    bool isDirty() const { return mDirty; }
    void clearDirty() { mDirty = false; }

    // Clear all elements (for reset)
    void clear();

private:
    void invalidateIdsCache() { mIdsCacheDirty = true; }

    std::string mFilePath;
    std::unordered_map<std::string, GuiElement> mElements;
    mutable int64_t mLastModified = 0;
    mutable bool mDirty = false;
    mutable bool mIdsCacheDirty = true;
    mutable std::vector<std::string> mCachedIds;
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

    // Save a single layout by file path
    bool saveLayout(const std::string& filePath);

    // Reload a single layout from disk
    bool reloadLayout(const std::string& filePath);

    // Reset a single layout (remove saved elements, reload from defaults)
    void resetLayout(const std::string& filePath);

    // Reset all layouts
    void resetAll();

    // Check if any layout has unsaved changes
    bool hasUnsaved() const;

    // Get list of file paths with unsaved changes
    std::vector<std::string> unsavedLayouts() const;

private:
    GuiLayoutManager() = default;
    std::unordered_map<std::string, GuiLayout> mLayouts;
    bool mEditorEnabled = false;
    int64_t mLastPollTime = 0;
};

// Helper: builds a UIRect from values
UIRect makeRect(float x, float y, float w, float h);
