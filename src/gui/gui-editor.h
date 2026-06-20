#pragma once

#include <string>
#include <vector>

struct GLFWwindow;
struct GuiElement;

class GuiEditor {
public:
    static GuiEditor& instance();

    void update(GLFWwindow* win);

    void setActiveLayout(const std::string& filePath);
    const std::string& activeLayout() const { return mActiveLayoutFile; }

    bool isEnabled() const { return mEnabled; }
    void setEnabled(bool e);
    void toggle() { setEnabled(!mEnabled); }

    const std::string& selectedElement() const { return mSelectedId; }
    bool hasSelection() const { return !mSelectedId.empty(); }

    // Handle character input for text editing
    void handleChar(unsigned int codepoint);

private:
    GuiEditor() = default;

    bool mEnabled = false;
    std::string mSelectedId;
    std::string mActiveLayoutFile;
    bool mDragging = false;
    bool mResizing = false;
    int mResizeCorner = -1;
    float mDragOffsetX = 0.0f;
    float mDragOffsetY = 0.0f;
    float mResizeStartX = 0.0f, mResizeStartY = 0.0f;
    float mResizeStartW = 0.0f, mResizeStartH = 0.0f;
    bool mHasOverlap = false;

    double mLastEditTime = 0.0;
    static constexpr double AUTO_SAVE_DELAY = 2.0;

    struct SnapGuide { float pos; bool vertical; };
    std::vector<SnapGuide> mSnapGuides;

    // Inline text editing
    bool mEditingText = false;
    std::string mTextEditBuffer;
    double mLastClickTime = 0.0;

    // Hierarchy filter
    std::string mHierarchyFilter;
    bool mFilterFocused = false;

    // Color picker
    bool mColorPickerOpen = false;
    int mColorPickerTarget = -1; // which color slider to set (5-12 for T-R through B-A, or -1)

    void handleInput(GLFWwindow* win);
    void handleKeyboard(GLFWwindow* win);
    void checkOverlaps();
    void autoSave();
    void markEdited();

    void renderOverlay(GLFWwindow* win);
    void renderSelectionHandles(const GuiElement& elem);
    void renderPropertyPanel(GLFWwindow* win, const GuiElement& elem);
    void renderHierarchyView();
    void renderSnapGuides();
    void renderColorPicker();

    void computeSnapGuides(const GuiElement& elem);
    void snapPosition(float& x, float& y, float w, float h) const;
    static float roundValue(float val);
    static float roundCoord(float val);
};
