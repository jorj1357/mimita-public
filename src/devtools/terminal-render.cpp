#include "devtools/terminal.h"
#include "gui/ui-system.h"
#include "gui/ui-text-input.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

void Terminal::drawAutocompleteMenu(float inputLineY, float lineHeight) {
    if (mSearchResults.empty() || !mTextState || mTextState->value.empty())
        return;

    float fbW = uiScreenW();
    float itemH = lineHeight;
    int maxVisible = 12;
    int numShow = std::min((int)mSearchResults.size(), maxVisible);
    float menuH = itemH * numShow + 4;
    float menuX = 16.0f;
    float menuY = inputLineY - 8.0f - menuH;
    float menuW = fbW - 32.0f;

    uiDrawRect({menuX, menuY, menuW, menuH}, {0.06f, 0.06f, 0.06f, 0.95f}, "autocomplete-bg");
    uiDrawRect({menuX, menuY, menuW, 1}, {0.85f, 0.05f, 0.05f, 0.7f}, "autocomplete-accent");

    float descColorMulti = 0.4f;

    for (int i = 0; i < numShow; i++) {
        const SearchResult& sr = mSearchResults[i];
        float y = menuY + 2.0f + itemH * i;

        if (i == mSelectedResult) {
            uiDrawRect({menuX + 2, y, menuW - 4, itemH}, {0.25f, 0.25f, 0.35f, 0.8f}, "autocomplete-sel");
        }

        const std::string& name = sr.cmd->name;
        glm::vec4 nameColor = {0.95f, 0.95f, 0.95f, 1.0f};
        glm::vec4 hlColor = {1.0f, 0.9f, 0.3f, 1.0f};

        if (sr.matchPositions.empty()) {
            uiDrawText(name.c_str(), menuX + 8, y, 0.35f, nameColor);
        } else {
            uiDrawText(name.c_str(), menuX + 8, y, 0.35f, nameColor);
            for (int pos : sr.matchPositions) {
                if (pos < (int)name.size()) {
                    std::string prefix = name.substr(0, pos);
                    float cx = menuX + 8 + uiMeasureText(prefix.c_str(), 0.35f);
                    char ch[2] = { name[pos], '\0' };
                    uiDrawText(ch, cx, y, 0.35f, hlColor);
                }
            }
        }

        if (!sr.cmd->description.empty()) {
            float nameW = uiMeasureText(name.c_str(), 0.35f) + 12;
            std::string desc = sr.cmd->description;
            float maxDescW = menuW - nameW - 24;
            if (maxDescW > 40) {
                float descW = uiMeasureText(desc.c_str(), 0.30f);
                if (descW > maxDescW) {
                    while (!desc.empty() && uiMeasureText((desc + "...").c_str(), 0.30f) > maxDescW)
                        desc.pop_back();
                    desc += "...";
                }
                uiDrawText(desc.c_str(), menuX + 8 + nameW, y, 0.30f,
                          {descColorMulti, descColorMulti, descColorMulti + 0.25f, 0.8f});
            }
        }
    }

    if ((int)mSearchResults.size() > maxVisible) {
        char more[64];
        snprintf(more, sizeof(more), "... %zu more", mSearchResults.size() - (size_t)maxVisible);
        uiDrawText(more, menuX + 8, menuY + menuH, 0.30f, {0.5f, 0.5f, 0.5f, 0.7f});
    }
}

void Terminal::render() {
    if (!mOpen || !mWindow) return;

    if (mExportPickerActive) {
        renderExportPicker();
        return;
    }

    updateSearch();

    uiBeginFrame(mWindow, "terminal");

    float fbW = uiScreenW();
    float fbH = uiScreenH();

    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.92f}, "terminal-bg");
    uiDrawRect({0, 0, fbW, 3}, {0.85f, 0.05f, 0.05f, 0.9f}, "terminal-accent");

    float lineHeight = 22.0f;
    float inputLineY = fbH - 40.0f;
    float startY = inputLineY - 12.0f - lineHeight;

    int visibleLines = (int)(startY / lineHeight);
    int endExclusive = std::max(0, (int)mScrollback.size() - mScrollOffset);
    int scrollStart = std::max(0, endExclusive - visibleLines);

    float y = startY - lineHeight * (endExclusive - scrollStart - 1);
    for (int i = scrollStart; i < endExclusive; i++) {
        const std::string& line = mScrollback[i];
        glm::vec4 color = {0.7f, 0.8f, 0.9f, 1.0f};
        if (line.find("[OK]") == 0)
            color = {0.2f, 1.0f, 0.3f, 1.0f};
        else if (line.find("[ERROR]") == 0)
            color = {1.0f, 0.2f, 0.2f, 1.0f};
        else if (line.find("] ") == 0)
            color = {1.0f, 0.85f, 0.3f, 1.0f};
        else if (line.find("[TERMINAL]") == 0)
            color = {0.6f, 0.6f, 1.0f, 1.0f};

        uiDrawText(line.c_str(), 16.0f, y, 0.38f, color);
        y += lineHeight;
    }
    if (mScrollOffset > 0) {
        char scrollText[64];
        snprintf(scrollText, sizeof(scrollText), "[SCROLLBACK: %d lines above newest]", mScrollOffset);
        uiDrawText(scrollText, fbW - 330.0f, 20.0f, 0.30f, {1.0f, 0.8f, 0.25f, 1.0f});
    }

    drawAutocompleteMenu(inputLineY, lineHeight);

    uiDrawRect({0, inputLineY - 6.0f, fbW, 36.0f}, {0.08f, 0.08f, 0.08f, 0.95f}, "terminal-input-bg");
    uiDrawRect({0, inputLineY - 6.0f, fbW, 1}, {0.85f, 0.05f, 0.05f, 0.7f}, "terminal-input-accent");

    float promptX = 16.0f;
    float textScale = 0.42f;

    // Draw prompt (shows the pending interactive prompt when active)
    const std::string promptText = hasPendingInput() ? pendingPrompt() : "] ";
    uiDrawText(promptText.c_str(), promptX, inputLineY, textScale,
               hasPendingInput() ? glm::vec4{0.3f, 0.9f, 0.6f, 1.0f}
                                 : glm::vec4{0.85f, 0.85f, 0.85f, 1.0f});
    float promptW = uiMeasureText(promptText.c_str(), textScale);

    // Draw selection highlight background (use UITextInput)
    if (mTextState && uiTextInputHasSelection(*mTextState)) {
        int selA = std::min(mTextState->selectionStart, mTextState->cursorPos);
        int selB = std::max(mTextState->selectionStart, mTextState->cursorPos);
        std::string beforeSel = mTextState->value.substr(0, selA);
        std::string selText = mTextState->value.substr(selA, selB - selA);
        float selX = promptX + promptW + uiMeasureText(beforeSel.c_str(), textScale);
        float selW = uiMeasureText(selText.c_str(), textScale);
        uiDrawRect({selX, inputLineY - 1.0f, selW, 22.0f}, {0.25f, 0.35f, 0.55f, 0.7f}, "terminal-sel");
    }

    // Draw input text with selection-colored portions
    if (mTextState) {
        const std::string& val = mTextState->value;
        int selA = std::min(mTextState->selectionStart, mTextState->cursorPos);
        int selB = std::max(mTextState->selectionStart, mTextState->cursorPos);
        if (uiTextInputHasSelection(*mTextState)) {
            if (selA > 0) {
                std::string before = val.substr(0, selA);
                uiDrawText(before.c_str(), promptX + promptW, inputLineY, textScale, {0.95f, 0.95f, 0.95f, 1.0f});
            }
            if (selB > selA) {
                std::string selPart = val.substr(selA, selB - selA);
                float selPartX = promptX + promptW + uiMeasureText(val.substr(0, selA).c_str(), textScale);
                uiDrawText(selPart.c_str(), selPartX, inputLineY, textScale, {1.0f, 1.0f, 1.0f, 1.0f});
            }
            if (selB < (int)val.size()) {
                std::string after = val.substr(selB);
                float afterX = promptX + promptW + uiMeasureText(val.substr(0, selB).c_str(), textScale);
                uiDrawText(after.c_str(), afterX, inputLineY, textScale, {0.95f, 0.95f, 0.95f, 1.0f});
            }
        } else {
            uiDrawText(val.c_str(), promptX + promptW, inputLineY, textScale, {0.95f, 0.95f, 0.95f, 1.0f});
        }

        // Ghost suffix
        if (!mGhostSuffix.empty()) {
            float inputEndX = promptX + promptW + uiMeasureText(val.c_str(), textScale);
            uiDrawText(mGhostSuffix.c_str(), inputEndX, inputLineY, textScale, {0.4f, 0.4f, 0.45f, 0.7f});
        }

        // Cursor
        mCursorBlink += 0.05f;
        bool cursorVisible = fmodf(mCursorBlink, 1.0f) < 0.6f;
        if (cursorVisible) {
            std::string beforeCursor = val.substr(0, mTextState->cursorPos);
            float cursorX = promptX + promptW + uiMeasureText(beforeCursor.c_str(), textScale);
            uiDrawText("_", cursorX, inputLineY, textScale, {0.95f, 0.95f, 0.95f, 1.0f});
        }
    }

    uiEndFrame();
}

void Terminal::renderExportPicker() {
    uiBeginFrame(mWindow, "export-picker");

    float fbW = uiScreenW();
    float fbH = uiScreenH();

    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.92f}, "picker-bg");
    uiDrawRect({0, 0, fbW, 3}, {0.2f, 0.6f, 0.3f, 0.9f}, "picker-accent");

    float lineHeight = 22.0f;
    float titleY = 16.0f;

    uiDrawText("[ REPLAY EXPORT ]", 16.0f, titleY, 0.50f, {0.2f, 0.9f, 0.3f, 1.0f});

    int count = (int)mExportPickerReplays.size();
    int visibleLines = (int)((fbH - 80.0f) / lineHeight) - 2;
    int endIdx = std::min(count, mExportPickerScroll + visibleLines);

    float y = titleY + 36.0f;
    for (int i = mExportPickerScroll; i < endIdx; i++) {
        const ReplayPickerEntry& e = mExportPickerReplays[i];
        bool selected = (i == mExportPickerIndex);

        if (selected) {
            float textW = uiMeasureText(e.filename.c_str(), 0.38f) + 400.0f;
            uiDrawRect({14.0f, y - 2.0f, textW, lineHeight}, {0.2f, 0.3f, 0.4f, 0.6f}, "picker-sel");
        }

        glm::vec4 textColor = selected
            ? glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}
            : glm::vec4{0.7f, 0.8f, 0.9f, 0.8f};
        uiDrawText(e.filename.c_str(), 16.0f, y, 0.38f, textColor);

        char info[128];
        snprintf(info, sizeof(info), "%s  |  %.1f KB  |  %.1f sec",
                 e.dateStr.c_str(),
                 (double)e.fileSize / 1024.0,
                 e.durationSec);
        float infoX = 16.0f + uiMeasureText(e.filename.c_str(), 0.38f) + 24.0f;
        uiDrawText(info, infoX, y, 0.30f, {0.5f, 0.6f, 0.7f, 0.7f});

        y += lineHeight;
    }

    char nav[256];
    snprintf(nav, sizeof(nav),
             "\x18\x19 Navigate  |  PgUp/PgDn Page  |  Home/End Edges  |  Enter Export  |  Esc Cancel");
    uiDrawText(nav, 16.0f, fbH - 24.0f, 0.30f, {0.5f, 0.5f, 0.5f, 0.7f});

    uiEndFrame();
}
