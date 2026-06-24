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

    if (mInputLine.empty()) {
        return;
    }

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

void Terminal::handleChar(unsigned int codepoint) {
    if (!mOpen) return;
    if (codepoint >= 32 && codepoint <= 126) {
        mInputLine += (char)codepoint;
        mSelectedResult = -1;
    }
}

void Terminal::handleKey(int key, int mods) {
    if (!mOpen) return;

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
                if (startReplayExport(path, 1280, 720)) {
                    addLog("[REPLAY EXPORT] started: " + path);
                } else {
                    addLog("[ERROR] Failed to start export");
                }
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

    updateSearch();

    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_V) {
        const char* clip = glfwGetClipboardString(mWindow);
        if (clip) mInputLine += clip;
        return;
    }
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_C) {
        if (!mInputLine.empty())
            glfwSetClipboardString(mWindow, mInputLine.c_str());
        return;
    }
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_X) {
        if (!mInputLine.empty()) {
            glfwSetClipboardString(mWindow, mInputLine.c_str());
            mInputLine.clear();
        }
        return;
    }

    if (key == GLFW_KEY_ENTER) {
        if (!mSearchResults.empty()) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            mInputLine = mSearchResults[idx].cmd->name;
        }
        executeCurrent();
    } else if (key == GLFW_KEY_BACKSPACE) {
        if (!mInputLine.empty()) {
            mInputLine.pop_back();
            mSelectedResult = -1;
        }
    } else if ((mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 1, std::max(0, (int)mScrollback.size() - 1));
    } else if ((mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 1);
    } else if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
    } else if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
    } else if (key == GLFW_KEY_PAGE_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
    } else if (key == GLFW_KEY_UP) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < 0)
                mSelectedResult = 0;
            else if (mSelectedResult > 0)
                mSelectedResult--;
        } else if (!mHistory.empty()) {
            if (mHistoryIndex == -1)
                mHistoryIndex = (int)mHistory.size() - 1;
            else if (mHistoryIndex > 0)
                mHistoryIndex--;
            mInputLine = mHistory[mHistoryIndex];
        }
    } else if (key == GLFW_KEY_DOWN) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < (int)mSearchResults.size() - 1)
                mSelectedResult++;
            else
                mSelectedResult = 0;
        } else if (mHistoryIndex >= 0 && mHistoryIndex < (int)mHistory.size() - 1) {
            mHistoryIndex++;
            mInputLine = mHistory[mHistoryIndex];
        } else {
            mHistoryIndex = -1;
            mInputLine.clear();
        }
    } else if (key == GLFW_KEY_TAB) {
        if (!mSearchResults.empty() && !mInputLine.empty()) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            if (idx < (int)mSearchResults.size()) {
                mInputLine = mSearchResults[idx].cmd->name;
                mLastSearchInput = mInputLine;
            }
        }
    }
}

void Terminal::handleScroll(double yOffset)
{
    if (!mOpen)
        return;
    int lines = (int)std::round(std::fabs(yOffset) * 3.0);
    if (lines < 1) lines = 1;
    if (yOffset > 0.0)
        mScrollOffset = std::min(mScrollOffset + lines, std::max(0, (int)mScrollback.size() - 1));
    else if (yOffset < 0.0)
        mScrollOffset = std::max(0, mScrollOffset - lines);
}
