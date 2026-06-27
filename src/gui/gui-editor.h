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
    float mCurrentDragX = 0.0f, mCurrentDragY = 0.0f;
    bool mDragActive = false;
    bool mHasOverlap = false;

    double mLastEditTime = 0.0;
    static constexpr double AUTO_SAVE_DELAY = 2.0;

    struct SnapGuide { float pos; bool vertical; };
    std::vector<SnapGuide> mSnapGuides;

    bool mEditingText = false;
    std::string mTextEditBuffer;
    double mLastClickTime = 0.0;

    std::string mHierarchyFilter;
    bool mFilterFocused = false;

    std::vector<std::string> mMultiSelectedIds;

    bool mColorPickerOpen = false;
    int mColorPickerTarget = -1;

    // Layout constants (1920x1080 design space)
    static constexpr float PP_X = 1440.0f, PP_Y = 80.0f, PP_W = 460.0f;
    static constexpr float PP_LABEL_X = 1450.0f;
    static constexpr float PP_TRACK_X = 1540.0f, PP_TRACK_W = 250.0f;
    static constexpr float PP_VAL_X = 1800.0f;
    static constexpr float PP_ROW_H = 22.0f;

    void handleInput(GLFWwindow* win);
    void handleKeyboard(GLFWwindow* win);
    void checkOverlaps();
    void autoSave();
    void markEdited();

    void renderOverlay(GLFWwindow* win);
    void renderSelectionHandles(const GuiElement& elem);
    void renderInfoPanel(const GuiElement& elem);
    void renderDragOverlay(const GuiElement& elem);
    void renderDebugOverlay();
    void renderPropertyPanel(GLFWwindow* win, const GuiElement& elem);
    void renderHierarchyView();
    void renderSnapGuides();
    void renderColorPicker();

    void computeSnapGuides(const GuiElement& elem);
    void snapPosition(float& x, float& y, float w, float h) const;
    static float roundValue(float val);
    static float roundCoord(float val);
};
