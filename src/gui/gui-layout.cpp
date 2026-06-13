#include "gui-layout.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>

#include <nlohmann/json.hpp>

#include "ui-system.h"

using json = nlohmann::json;

UIRect makeRect(float x, float y, float w, float h)
{
    UIRect r;
    r.x = x; r.y = y; r.w = w; r.h = h;
    return r;
}

// ----------------------------------------------------------------
// File modification time helpers (cross-platform)
// ----------------------------------------------------------------
static int64_t getFileModifiedTime(const std::string& path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(
        ft.time_since_epoch()).count();
}

// ----------------------------------------------------------------
// GuiLayout
// ----------------------------------------------------------------
bool GuiLayout::load(const std::string& filePath)
{
    mFilePath = filePath;
    mElements.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        printf("[GUI LAYOUT] No layout file: %s (using defaults)\n", filePath.c_str());
        mLastModified = getFileModifiedTime(filePath);
        return false;
    }

    try {
        json j;
        file >> j;

        if (j.contains("elements") && j["elements"].is_array()) {
            for (const auto& elem : j["elements"]) {
                GuiElement e;
                e.id = elem.value("id", "");
                e.x = elem.value("x", 0.0f);
                e.y = elem.value("y", 0.0f);
                e.w = elem.value("width", 0.0f);
                e.h = elem.value("height", 0.0f);
                if (!e.id.empty())
                    mElements[e.id] = e;
            }
        }

        mLastModified = getFileModifiedTime(filePath);
        printf("[GUI LAYOUT] Loaded %zu elements from %s\n",
               mElements.size(), filePath.c_str());
        return true;

    } catch (const std::exception& e) {
        printf("[GUI LAYOUT] Error loading %s: %s\n", filePath.c_str(), e.what());
        return false;
    }
}

bool GuiLayout::save(const std::string& filePath) const
{
    json j;
    j["format"] = "mimita-gui-layout";
    j["version"] = 1;

    json elementsJson = json::array();
    // Sort elements by ID for stable output
    std::vector<std::string> ids;
    for (const auto& pair : mElements)
        ids.push_back(pair.first);
    std::sort(ids.begin(), ids.end());

    for (const std::string& id : ids) {
        const GuiElement& e = mElements.at(id);
        elementsJson.push_back({
            {"id", e.id},
            {"x", e.x},
            {"y", e.y},
            {"width", e.w},
            {"height", e.h}
        });
    }
    j["elements"] = elementsJson;

    std::error_code ec;
    const std::filesystem::path outputPath(filePath);
    if (outputPath.has_parent_path())
        std::filesystem::create_directories(outputPath.parent_path(), ec);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        printf("[GUI LAYOUT] Failed to write %s\n", filePath.c_str());
        return false;
    }
    file << j.dump(2);
    printf("[GUI LAYOUT] Saved %zu elements to %s\n", mElements.size(), filePath.c_str());
    return true;
}

UIRect GuiLayout::getRect(const std::string& id, const UIRect& fallback) const
{
    auto it = mElements.find(id);
    if (it == mElements.end())
        return fallback;
    return makeRect(it->second.x, it->second.y, it->second.w, it->second.h);
}

UIRect GuiLayout::getRectCentered(const std::string& id, const UIRect& fallback,
                                   float cx, float cy) const
{
    auto it = mElements.find(id);
    if (it == mElements.end())
        return fallback;
    // Stored x,y are offsets from center (cx, cy)
    return makeRect(cx + it->second.x, cy + it->second.y,
                    it->second.w, it->second.h);
}

const GuiElement* GuiLayout::get(const std::string& id) const
{
    auto it = mElements.find(id);
    return it == mElements.end() ? nullptr : &it->second;
}

void GuiLayout::set(const std::string& id, float x, float y, float w, float h)
{
    mElements[id].id = id;
    mElements[id].x = x;
    mElements[id].y = y;
    mElements[id].w = w;
    mElements[id].h = h;
}

bool GuiLayout::checkFileChanged() const
{
    if (mFilePath.empty()) return false;
    int64_t current = getFileModifiedTime(mFilePath);
    return current != mLastModified && current != 0;
}

std::vector<std::string> GuiLayout::elementIds() const
{
    std::vector<std::string> ids;
    for (const auto& pair : mElements)
        ids.push_back(pair.first);
    return ids;
}

// ----------------------------------------------------------------
// GuiLayoutManager
// ----------------------------------------------------------------
GuiLayoutManager& GuiLayoutManager::instance()
{
    static GuiLayoutManager manager;
    return manager;
}

GuiLayout& GuiLayoutManager::getLayout(const std::string& filePath)
{
    auto it = mLayouts.find(filePath);
    if (it == mLayouts.end()) {
        GuiLayout layout;
        layout.load(filePath);
        mLayouts[filePath] = std::move(layout);
        it = mLayouts.find(filePath);
    }
    return it->second;
}

void GuiLayoutManager::pollReload()
{
    // Throttle polling to ~2x/sec to avoid excessive stat calls
    std::time_t now = std::time(nullptr);
    if (now - mLastPollTime < 1) return;
    mLastPollTime = now;

    for (auto& pair : mLayouts) {
        if (pair.second.checkFileChanged()) {
            printf("[GUI LAYOUT] Hot reload: %s\n", pair.first.c_str());
            pair.second.load(pair.first);
        }
    }
}

void GuiLayoutManager::saveAll()
{
    for (auto& pair : mLayouts) {
        pair.second.save(pair.first);
    }
}
