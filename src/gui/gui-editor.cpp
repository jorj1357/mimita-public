#include "gui-editor.h"
#include "gui-coord.h"

#include "gui-layout.h"
#include "gui-element-render.h"
#include "ui-system.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

GuiEditor& GuiEditor::instance()
{
    static GuiEditor editor;
    return editor;
}

void GuiEditor::setEnabled(bool e)
{
    mEnabled = e;
    uiSetEditMode(e);
    if (!e) {
        mSelectedId.clear();
        mDragging = false;
        mResizing = false;
        mResizeCorner = -1;
        mSnapGuides.clear();
        GuiLayoutManager::instance().saveAll();
    }
}

void GuiEditor::setActiveLayout(const std::string& filePath)
{
    mActiveLayoutFile = filePath;
    if (!filePath.empty())
        GuiLayoutManager::instance().getLayout(filePath);
}

void GuiEditor::update(GLFWwindow* win)
{
    if (!mEnabled) return;
    autoSave();
    handleInput(win);
    handleKeyboard(win);
    renderOverlay(win);
}

void GuiEditor::autoSave()
{
    if (!GuiLayoutManager::instance().hasUnsaved()) return;
    double now = glfwGetTime();
    if (now - mLastEditTime >= AUTO_SAVE_DELAY) {
        GuiLayoutManager::instance().saveAll();
        mLastEditTime = now;
    }
}

void GuiEditor::markEdited()
{
    mLastEditTime = glfwGetTime();
}

void GuiEditor::handleChar(unsigned int codepoint)
{
    if (codepoint < 32 || codepoint > 126) return;

    if (mFilterFocused) {
        if (mHierarchyFilter.size() < 40) {
            mHierarchyFilter.push_back((char)codepoint);
        }
        return;
    }

    if (!mEditingText || mSelectedId.empty() || mActiveLayoutFile.empty()) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mActiveLayoutFile);
    GuiElement* elem = const_cast<GuiElement*>(layout.get(mSelectedId));
    if (!elem) return;

    mTextEditBuffer.push_back((char)codepoint);
    elem->text = mTextEditBuffer;
    layout.setElement(*elem);
    markEdited();
}
