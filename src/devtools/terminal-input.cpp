#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "devtools/terminal.h"
#include "devtools/command-search.h"
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
    if (mInputLine == mLastSearchInput && !mCacheDirty) return;
    mLastSearchInput = mInputLine;
    rebuildCache();

    mSearchResults.clear();
    mGhostSuffix.clear();
    mSelectedResult = -1;

    if (mInputLine.empty()) return;

    if (mInputLine.find(' ') != std::string::npos) {
        mSelectedResult = -1;
        return;
    }

    std::string inputLower = mInputLine;
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

// ── Selection helpers ───────────────────────────────────────

void Terminal::clearSelection() { mSelectionStart = -1; }

bool hasSelectionRange(int selStart, int cursor, int len) {
    if (selStart < 0) return false;
    int a = std::min(selStart, cursor);
    int b = std::max(selStart, cursor);
    return a >= 0 && b <= len && a != b;
}

std::string Terminal::selectedText() const {
    if (mSelectionStart < 0) return {};
    int a = std::min(mSelectionStart, mCursorPos);
    int b = std::max(mSelectionStart, mCursorPos);
    if (a >= b) return {};
    return mInputLine.substr(a, b - a);
}

void Terminal::deleteSelection() {
    if (mSelectionStart < 0) return;
    int a = std::min(mSelectionStart, mCursorPos);
    int b = std::max(mSelectionStart, mCursorPos);
    if (a >= b) { clearSelection(); return; }
    mInputLine.erase(a, b - a);
    mCursorPos = a;
    clearSelection();
}

void Terminal::replaceSelection(const std::string& replacement) {
    if (mSelectionStart >= 0)
        deleteSelection();
    mInputLine.insert(mCursorPos, replacement);
    mCursorPos += (int)replacement.size();
    clearSelection();
}

int Terminal::cursorWordLeft(int pos) const {
    if (pos <= 0) return 0;
    int i = pos - 1;
    while (i > 0 && mInputLine[i - 1] == ' ') i--;
    while (i > 0 && mInputLine[i - 1] != ' ') i--;
    return i;
}

int Terminal::cursorWordRight(int pos) const {
    int len = (int)mInputLine.size();
    if (pos >= len) return len;
    int i = pos;
    while (i < len && mInputLine[i] == ' ') i++;
    while (i < len && mInputLine[i] != ' ') i++;
    return i;
}

// ── Character input ─────────────────────────────────────────

void Terminal::handleChar(unsigned int codepoint) {
    if (!mOpen) return;
    if (codepoint >= 32 && codepoint <= 126) {
        std::string ch(1, (char)codepoint);
        replaceSelection(ch);
        mSelectedResult = -1;
        mTabCycleIndex = -1;
    }
}

// ── Key input ───────────────────────────────────────────────

void Terminal::handleKey(int key, int mods) {
    if (!mOpen) return;

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
                if (startReplayExport(path, 1280, 720))
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

    // ── Ctrl+ shortcuts ───────────────────────────────────
    if (mods & GLFW_MOD_CONTROL) {
        switch (key) {
        case GLFW_KEY_V: {
            const char* clip = glfwGetClipboardString(mWindow);
            if (clip) {
                std::string text = clip;
                // Replace newlines with spaces for command safety
                for (auto& ch : text) if (ch == '\n' || ch == '\r') ch = ' ';
                replaceSelection(text);
                mTabCycleIndex = -1;
            }
            return;
        }
        case GLFW_KEY_C: {
            if (hasSelection()) {
                glfwSetClipboardString(mWindow, selectedText().c_str());
            }
            // If no selection, do nothing (don't interrupt the game)
            return;
        }
        case GLFW_KEY_X: {
            if (hasSelection()) {
                glfwSetClipboardString(mWindow, selectedText().c_str());
                deleteSelection();
                mTabCycleIndex = -1;
            }
            return;
        }
        case GLFW_KEY_A: {
            if (!mInputLine.empty()) {
                mSelectionStart = 0;
                mCursorPos = (int)mInputLine.size();
            }
            return;
        }
        case GLFW_KEY_L: {
            mScrollback.clear();
            mScrollOffset = 0;
            return;
        }
        case GLFW_KEY_LEFT: {
            clearSelection();
            mCursorPos = cursorWordLeft(mCursorPos);
            mTabCycleIndex = -1;
            return;
        }
        case GLFW_KEY_RIGHT: {
            clearSelection();
            mCursorPos = cursorWordRight(mCursorPos);
            mTabCycleIndex = -1;
            return;
        }
        case GLFW_KEY_BACKSPACE: {
            if (hasSelection()) {
                deleteSelection();
            } else if (mCursorPos > 0) {
                int wordStart = cursorWordLeft(mCursorPos);
                mInputLine.erase(wordStart, mCursorPos - wordStart);
                mCursorPos = wordStart;
            }
            mSelectedResult = -1;
            mTabCycleIndex = -1;
            return;
        }
        case GLFW_KEY_DELETE: {
            if (hasSelection()) {
                deleteSelection();
            } else if (mCursorPos < (int)mInputLine.size()) {
                int wordEnd = cursorWordRight(mCursorPos);
                mInputLine.erase(mCursorPos, wordEnd - mCursorPos);
            }
            mSelectedResult = -1;
            mTabCycleIndex = -1;
            return;
        }
        }
    }

    // ── Navigation (no modifiers, or Shift) ───────────────
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // Shift+Up/Down: scroll
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

    // ── Enter ─────────────────────────────────────────────
    if (key == GLFW_KEY_ENTER) {
        if (!mSearchResults.empty() && mInputLine.find(' ') == std::string::npos) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            mInputLine = mSearchResults[idx].cmd->name;
            mCursorPos = (int)mInputLine.size();
        }
        executeCurrent();
        mTabCycleIndex = -1;
        return;
    }

    // ── Escape ────────────────────────────────────────────
    if (key == GLFW_KEY_ESCAPE) {
        if (hasSelection()) {
            clearSelection();
        } else {
            mInputLine.clear();
            mCursorPos = 0;
            mHistoryIndex = -1;
            mTabCycleIndex = -1;
        }
        return;
    }

    // ── Tab ───────────────────────────────────────────────
    if (key == GLFW_KEY_TAB) {
        if (!mSearchResults.empty() && !mInputLine.empty()) {
            if (mTabCycleIndex < 0) mTabCycleIndex = 0;
            else mTabCycleIndex = (mTabCycleIndex + 1) % (int)mSearchResults.size();
            mSelectedResult = mTabCycleIndex;
            mInputLine = mSearchResults[mTabCycleIndex].cmd->name;
            mCursorPos = (int)mInputLine.size();
            mLastSearchInput = mInputLine;
        }
        return;
    }

    // ── Cursor movement ───────────────────────────────────
    bool extendSelection = shift;

    auto moveCursor = [&](int newPos) {
        if (extendSelection && !hasSelection()) {
            mSelectionStart = mCursorPos;
        }
        mCursorPos = std::clamp(newPos, 0, (int)mInputLine.size());
        if (!extendSelection && !hasSelection()) {
            // no-op: cursor moved without selection
        }
        if (!extendSelection) {
            clearSelection();
        }
        mTabCycleIndex = -1;
    };

    if (key == GLFW_KEY_LEFT) {
        moveCursor(mCursorPos - 1);
        return;
    }
    if (key == GLFW_KEY_RIGHT) {
        moveCursor(mCursorPos + 1);
        return;
    }
    if (key == GLFW_KEY_HOME) {
        moveCursor(0);
        return;
    }
    if (key == GLFW_KEY_END) {
        moveCursor((int)mInputLine.size());
        return;
    }

    // ── Backspace ─────────────────────────────────────────
    if (key == GLFW_KEY_BACKSPACE) {
        if (hasSelection()) {
            deleteSelection();
        } else if (mCursorPos > 0) {
            mInputLine.erase(mCursorPos - 1, 1);
            mCursorPos--;
        }
        mSelectedResult = -1;
        mTabCycleIndex = -1;
        return;
    }

    // ── Delete ────────────────────────────────────────────
    if (key == GLFW_KEY_DELETE) {
        if (hasSelection()) {
            deleteSelection();
        } else if (mCursorPos < (int)mInputLine.size()) {
            mInputLine.erase(mCursorPos, 1);
        }
        mSelectedResult = -1;
        mTabCycleIndex = -1;
        return;
    }

    // ── History navigation (only when search results empty) ──
    if (key == GLFW_KEY_UP) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < 0) mSelectedResult = 0;
            else if (mSelectedResult < (int)mSearchResults.size() - 1) mSelectedResult++;
        } else if (!mHistory.empty()) {
            if (mHistoryIndex == -1) {
                mHistorySavedLine = mInputLine;
                mHistoryIndex = (int)mHistory.size() - 1;
            } else if (mHistoryIndex > 0) {
                mHistoryIndex--;
            }
            mInputLine = mHistory[mHistoryIndex];
            mCursorPos = (int)mInputLine.size();
            clearSelection();
        }
        return;
    }

    if (key == GLFW_KEY_DOWN) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < 0) mSelectedResult = 0;
            else mSelectedResult = std::max(0, mSelectedResult - 1);
            if ((int)mSearchResults.size() > 1 && mSelectedResult > (int)mSearchResults.size() - 1)
                mSelectedResult = 0;
        } else if (mHistoryIndex >= 0) {
            mHistoryIndex++;
            if (mHistoryIndex >= (int)mHistory.size()) {
                mHistoryIndex = -1;
                mInputLine = mHistorySavedLine;
                mHistorySavedLine.clear();
            } else {
                mInputLine = mHistory[mHistoryIndex];
            }
            mCursorPos = (int)mInputLine.size();
            clearSelection();
        }
        return;
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
