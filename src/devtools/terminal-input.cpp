#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "devtools/terminal.h"
#include "devtools/command-search.h"
#include "gui/ui-text-input.h"
#include "replay/replay-export.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

void Terminal::rebuildCache() {
    if (!mCacheDirty) return;
    mCachedCommands.clear();
    mCachedCommands.reserve(mCommands.size());
    for (const auto& pair : mCommands) {
        CachedCommand cc;
        cc.cmd = &pair.second;
        cc.lowerName = pair.first;
        std::transform(cc.lowerName.begin(), cc.lowerName.end(), cc.lowerName.begin(), ::tolower);
        cc.lowerDesc = pair.second.description;
        std::transform(cc.lowerDesc.begin(), cc.lowerDesc.end(), cc.lowerDesc.begin(), ::tolower);
        mCachedCommands.push_back(cc);
    }
    mCacheDirty = false;
}

void Terminal::updateSearch() {
    if (!mTextState) return;
    if (mTextState->value == mLastSearchInput && !mCacheDirty) return;
    mLastSearchInput = mTextState->value;
    rebuildCache();

    mSearchResults.clear();
    mGhostSuffix.clear();
    mSelectedResult = -1;

    if (mTextState->value.empty()) return;

    if (mTextState->value.find(' ') != std::string::npos) {
        mSelectedResult = -1;
        return;
    }

    std::string inputLower = mTextState->value;
    std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);

    MatchResult mr;
    for (const auto& cc : mCachedCommands) {
        int score = fuzzyMatch(inputLower, cc.lowerName, mr);
        if (score > 0) {
            SearchResult sr;
            sr.cmd = cc.cmd;
            sr.score = score;
            sr.matchPositions = std::move(mr.positions);
            mSearchResults.push_back(std::move(sr));
        }
        if (score == 0 && cc.lowerDesc.find(inputLower) != std::string::npos) {
            SearchResult sr;
            sr.cmd = cc.cmd;
            sr.score = 1;
            mSearchResults.push_back(std::move(sr));
        }
    }

    std::sort(mSearchResults.begin(), mSearchResults.end(),
        [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.cmd->name < b.cmd->name;
        });

    if (mSearchResults.size() > 20)
        mSearchResults.resize(20);

    if (mSelectedResult >= (int)mSearchResults.size())
        mSelectedResult = mSearchResults.empty() ? -1 : 0;

    mGhostSuffix = computeGhostSuffix(inputLower);
}

std::string Terminal::computeGhostSuffix(const std::string& inputLower) const {
    if (mSearchResults.empty() || inputLower.empty())
        return {};

    const std::string& bestName = mSearchResults[0].cmd->name;
    if (inputLower.size() >= bestName.size())
        return {};

    for (size_t i = 0; i < inputLower.size(); i++) {
        if (inputLower[i] != std::tolower(bestName[i]))
            return {};
    }

    return bestName.substr(inputLower.size());
}

// ── Character input ─────────────────────────────────────────
// Delegates to the reusable UITextInput system.

void Terminal::handleChar(unsigned int codepoint) {
    if (!mOpen || !mTextState) return;
    mTextState->focused = true;
    if (uiTextInputHandleChar(*mTextState, codepoint, {.maxLength = 256}))
    {
        mSelectedResult = -1;
        mTabCycleIndex = -1;
    }
}

// ── Key input ───────────────────────────────────────────────
// Terminal-specific logic first, then falls back to UITextInput for generic editing.

void Terminal::handleKey(int key, int mods) {
    if (!mOpen || !mTextState) return;

    // Export picker: route all keys to picker navigation
    if (mExportPickerActive) {
        int count = (int)mExportPickerReplays.size();
        if (key == GLFW_KEY_ESCAPE) {
            closeExportPicker();
            addLog("[REPLAY PICKER] cancelled");
        } else if (key == GLFW_KEY_ENTER) {
            if (count > 0 && mExportPickerIndex >= 0 && mExportPickerIndex < count) {
                std::string path = mExportPickerReplays[mExportPickerIndex].path;
                addLog("[REPLAY PICKER] selected: " + path);
                closeExportPicker();
                if (!std::filesystem::exists(path)) {
                    addLog("[ERROR] File not found: " + path);
                    return;
                }
                if (startReplayExport(path, gExportConfig.exportWidth, gExportConfig.exportHeight))
                    addLog("[REPLAY EXPORT] started: " + path);
                else
                    addLog("[ERROR] Failed to start export");
            }
        } else if (key == GLFW_KEY_UP) {
            if (mExportPickerIndex > 0) {
                mExportPickerIndex--;
                if (mExportPickerIndex < mExportPickerScroll)
                    mExportPickerScroll = mExportPickerIndex;
            }
        } else if (key == GLFW_KEY_DOWN) {
            if (mExportPickerIndex < count - 1) {
                mExportPickerIndex++;
                if (mExportPickerIndex >= mExportPickerScroll + 20)
                    mExportPickerScroll = mExportPickerIndex - 19;
            }
        } else if (key == GLFW_KEY_PAGE_UP) {
            mExportPickerIndex = std::max(0, mExportPickerIndex - 10);
            mExportPickerScroll = std::max(0, mExportPickerIndex);
        } else if (key == GLFW_KEY_PAGE_DOWN) {
            mExportPickerIndex = std::min(count - 1, mExportPickerIndex + 10);
            if (mExportPickerIndex >= mExportPickerScroll + 20)
                mExportPickerScroll = mExportPickerIndex - 19;
        } else if (key == GLFW_KEY_HOME) {
            mExportPickerIndex = 0;
            mExportPickerScroll = 0;
        } else if (key == GLFW_KEY_END) {
            mExportPickerIndex = count - 1;
            mExportPickerScroll = std::max(0, count - 20);
        }
        return;
    }

    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // ── Ctrl+L: clear scrollback ──────────────────────────────
    if (ctrl && key == GLFW_KEY_L) {
        mScrollback.clear();
        mScrollOffset = 0;
        return;
    }

    // ── Shift+Up/Down: scroll ─────────────────────────────────
    if (shift && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 1, std::max(0, (int)mScrollback.size() - 1));
        return;
    }
    if (shift && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 1);
        return;
    }

    // Alt+Up/Down: scroll by 10
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
        return;
    }

    // Page Up/Down
    if (key == GLFW_KEY_PAGE_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
        return;
    }
    if (key == GLFW_KEY_PAGE_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
        return;
    }

    updateSearch();

    // ── Enter: execute command ────────────────────────────────
    if (key == GLFW_KEY_ENTER) {
        if (!mSearchResults.empty() && mTextState->value.find(' ') == std::string::npos) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            mTextState->value = mSearchResults[idx].cmd->name;
            mTextState->cursorPos = (int)mTextState->value.size();
        }
        executeCurrent();
        mTabCycleIndex = -1;
        return;
    }

    // ── Escape: clear selection, then line, then close ────────
    if (key == GLFW_KEY_ESCAPE) {
        if (uiTextInputHasSelection(*mTextState)) {
            uiTextInputClearSelection(*mTextState);
        } else if (!mTextState->value.empty()) {
            mTextState->value.clear();
            mTextState->cursorPos = 0;
            mHistoryIndex = -1;
            mTabCycleIndex = -1;
        } else {
            toggle();
        }
        return;
    }

    // ── Tab: autocomplete ─────────────────────────────────────
    if (key == GLFW_KEY_TAB) {
        if (!mSearchResults.empty() && !mTextState->value.empty()) {
            if (mTabCycleIndex < 0) mTabCycleIndex = 0;
            else mTabCycleIndex = (mTabCycleIndex + 1) % (int)mSearchResults.size();
            mSelectedResult = mTabCycleIndex;
            mTextState->value = mSearchResults[mTabCycleIndex].cmd->name;
            mTextState->cursorPos = (int)mTextState->value.size();
            mLastSearchInput = mTextState->value;
        }
        return;
    }

    // ── History navigation ────────────────────────────────────
    if (key == GLFW_KEY_UP && !mSearchResults.empty()) {
        if (mSelectedResult < 0) mSelectedResult = 0;
        else if (mSelectedResult < (int)mSearchResults.size() - 1) mSelectedResult++;
        return;
    }
    if (key == GLFW_KEY_DOWN && !mSearchResults.empty()) {
        if (mSelectedResult < 0) mSelectedResult = 0;
        else mSelectedResult = std::max(0, mSelectedResult - 1);
        if ((int)mSearchResults.size() > 1 && mSelectedResult > (int)mSearchResults.size() - 1)
            mSelectedResult = 0;
        return;
    }

    if (key == GLFW_KEY_UP && mSearchResults.empty()) {
        if (!mHistory.empty()) {
            if (mHistoryIndex == -1) {
                mHistorySavedLine = mTextState->value;
                mHistoryIndex = (int)mHistory.size() - 1;
            } else if (mHistoryIndex > 0) {
                mHistoryIndex--;
            }
            mTextState->value = mHistory[mHistoryIndex];
            mTextState->cursorPos = (int)mTextState->value.size();
            uiTextInputClearSelection(*mTextState);
        }
        return;
    }

    if (key == GLFW_KEY_DOWN && mSearchResults.empty()) {
        if (mHistoryIndex >= 0) {
            mHistoryIndex++;
            if (mHistoryIndex >= (int)mHistory.size()) {
                mHistoryIndex = -1;
                mTextState->value = mHistorySavedLine;
                mHistorySavedLine.clear();
            } else {
                mTextState->value = mHistory[mHistoryIndex];
            }
            mTextState->cursorPos = (int)mTextState->value.size();
            uiTextInputClearSelection(*mTextState);
        }
        return;
    }

    // ── Generic text editing: delegate to UITextInput ─────────
    mTextState->focused = true;
    UITextInputOptions opts;
    opts.maxLength = 256;
    opts.characterFilter = nullptr;
    uiTextInputHandleKey(mWindow, *mTextState, key, GLFW_PRESS, mods, opts);

    // After editing, reset terminal-specific state
    if (key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_DELETE ||
        key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT ||
        key == GLFW_KEY_HOME || key == GLFW_KEY_END)
    {
        mSelectedResult = -1;
        mTabCycleIndex = -1;
    }
}

void Terminal::handleScroll(double yOffset) {
    if (!mOpen) return;
    int lines = (int)std::round(std::fabs(yOffset) * 3.0);
    if (lines < 1) lines = 1;
    if (yOffset > 0.0)
        mScrollOffset = std::min(mScrollOffset + lines, std::max(0, (int)mScrollback.size() - 1));
    else if (yOffset < 0.0)
        mScrollOffset = std::max(0, mScrollOffset - lines);
}
