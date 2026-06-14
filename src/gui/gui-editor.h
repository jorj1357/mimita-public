#pragma once

#include <string>

struct GLFWwindow;

// In-game GUI element editor
// Renders after the normal UI to show selection handles
// Handles click-to-select, drag-to-move, keyboard nudge
class GuiEditor {
public:
    static GuiEditor& instance();

    // Main update: call after the menu/UI has been drawn
    // This renders selection overlay and handles input
    void update(GLFWwindow* win);

    // Set the reference center point for the current layout
    void setCenter(float cx, float cy) { mCenterX = cx; mCenterY = cy; }

    // Set the active layout file path for the current menu
    void setActiveLayout(const std::string& filePath);

    // Get the active layout file path
    const std::string& activeLayout() const { return mActiveLayoutFile; }

    // Check if editor is active
    bool isEnabled() const { return mEnabled; }
    void setEnabled(bool e);
    void toggle() { setEnabled(!mEnabled); }

    // Get current selection info
    const std::string& selectedElement() const { return mSelectedId; }
    bool hasSelection() const { return !mSelectedId.empty(); }

    // Get screen center (for external use)
    float centerX() const { return mCenterX; }
    float centerY() const { return mCenterY; }

private:
    GuiEditor() = default;

    bool mEnabled = false;
    float mCenterX = 0.0f;
    float mCenterY = 0.0f;

    std::string mSelectedId;
    std::string mActiveLayoutFile;
    bool mDragging = false;
    float mDragOffsetX = 0.0f;
    float mDragOffsetY = 0.0f;
    bool mHasOverlap = false;

    void renderOverlay(GLFWwindow* win);
    void handleInput(GLFWwindow* win);
    void handleKeyboard(GLFWwindow* win);
    void checkOverlaps();
};
