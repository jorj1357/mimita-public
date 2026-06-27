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
// GuiElement color helper implementations
// ----------------------------------------------------------------
glm::vec4 GuiElement::getTextColorVec() const
{
    if (textColor.size() >= 4)
        return glm::vec4(textColor[0], textColor[1], textColor[2], textColor[3]);
    return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

glm::vec4 GuiElement::getBackgroundColorVec() const
{
    if (backgroundColor.size() >= 4)
        return glm::vec4(backgroundColor[0], backgroundColor[1], backgroundColor[2], backgroundColor[3]);
    return glm::vec4(0.2f, 0.2f, 0.3f, 1.0f);
}

glm::vec4 GuiElement::getHoverColorVec() const
{
    if (hoverColor.size() >= 4)
        return glm::vec4(hoverColor[0], hoverColor[1], hoverColor[2], hoverColor[3]);
    return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec4 GuiElement::getPressedColorVec() const
{
    if (pressedColor.size() >= 4)
        return glm::vec4(pressedColor[0], pressedColor[1], pressedColor[2], pressedColor[3]);
    return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec4 GuiElement::getOutlineColorVec() const
{
    if (outlineColor.size() >= 4)
        return glm::vec4(outlineColor[0], outlineColor[1], outlineColor[2], outlineColor[3]);
    return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void GuiElement::clearColorOverrides()
{
    hoverColor.clear();
    pressedColor.clear();
    outlineColor.clear();
}

// ----------------------------------------------------------------
// File modification time helpers (cross-platform)
// ----------------------------------------------------------------
static int64_t getFileModifiedTime(const std::string& path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        ft.time_since_epoch()).count();
}

// ----------------------------------------------------------------
// GuiLayout
// ----------------------------------------------------------------
bool GuiLayout::load(const std::string& filePath)
{
    mFilePath = filePath;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        printf("[GUI LAYOUT] No layout file: %s (using defaults)\n", filePath.c_str());
        mLastModified = getFileModifiedTime(filePath);
        return false;
    }

    try {
        json j;
        file >> j;
        std::unordered_map<std::string, GuiElement> loadedElements;

        // Helper lambda to parse a single element from JSON
        auto parseElement = [](const json& elem) -> GuiElement {
            GuiElement e;
            e.id = elem.value("id", "");
            e.type = elem.value("type", "button");

            e.x = elem.value("x", 0.0f);
            e.y = elem.value("y", 0.0f);
            e.w = elem.value("width", 0.0f);
            e.h = elem.value("height", 0.0f);
            e.textOffsetX = elem.value("textOffsetX", 8.0f);
            e.textOffsetY = elem.value("textOffsetY", 4.0f);
            e.padding = elem.value("padding", 8.0f);
            e.margin = elem.value("margin", 0.0f);
            e.rotation = elem.value("rotation", 0.0f);
            e.anchorX = elem.value("anchorX", "left");
            e.anchorY = elem.value("anchorY", "top");
            e.layer = elem.value("layer", 0);

            e.visible = elem.value("visible", true);
            e.enabled = elem.value("enabled", true);
            e.opacity = elem.value("opacity", 1.0f);
            e.hoverScale = elem.value("hoverScale", 1.0f);

            e.text = elem.value("text", "");
            e.font = elem.value("font", "");
            e.fontSize = elem.value("fontSize", 0.0f);
            e.textAlign = elem.value("textAlign", "left");

            if (elem.contains("textColor") && elem["textColor"].is_array())
                e.textColor = elem["textColor"].get<std::vector<float>>();
            if (elem.contains("backgroundColor") && elem["backgroundColor"].is_array())
                e.backgroundColor = elem["backgroundColor"].get<std::vector<float>>();
            if (elem.contains("hoverColor") && elem["hoverColor"].is_array())
                e.hoverColor = elem["hoverColor"].get<std::vector<float>>();
            if (elem.contains("pressedColor") && elem["pressedColor"].is_array())
                e.pressedColor = elem["pressedColor"].get<std::vector<float>>();
            if (elem.contains("outlineColor") && elem["outlineColor"].is_array())
                e.outlineColor = elem["outlineColor"].get<std::vector<float>>();

            e.backgroundImage = elem.value("backgroundImage", "");
            e.backgroundVideo = elem.value("backgroundVideo", "");

            e.hoverSound = elem.value("hoverSound", "");
            e.clickSound = elem.value("clickSound", "");
            return e;
        };

        // Support both v2 array format and v3 object format (keyed by ID)
        if (j.contains("elements")) {
            if (j["elements"].is_array()) {
                for (const auto& elem : j["elements"]) {
                    GuiElement e = parseElement(elem);
                    if (!e.id.empty())
                        loadedElements[e.id] = e;
                }
            } else if (j["elements"].is_object()) {
                for (auto it = j["elements"].begin(); it != j["elements"].end(); ++it) {
                    const std::string& id = it.key();
                    json elem = it.value();
                    // If the element already has an "id" field, use it; otherwise use the key
                    GuiElement e = parseElement(elem);
                    if (e.id.empty()) e.id = id;
                    loadedElements[e.id] = e;
                }
            }
        }

        mElements = std::move(loadedElements);
        mLastModified = getFileModifiedTime(filePath);
        mDirty = false;
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
    j["version"] = 3;

    json elementsJson = json::object();
    // Sort elements by ID for stable output
    std::vector<std::string> ids;
    for (const auto& pair : mElements)
        ids.push_back(pair.first);
    std::sort(ids.begin(), ids.end());

    for (const std::string& id : ids) {
        const GuiElement& e = mElements.at(id);
        json obj;
        if (e.type != "button") obj["type"] = e.type;

        // Layout
        obj["x"] = e.x;
        obj["y"] = e.y;
        obj["width"] = e.w;
        obj["height"] = e.h;
        if (e.textOffsetX != 8.0f) obj["textOffsetX"] = e.textOffsetX;
        if (e.textOffsetY != 4.0f) obj["textOffsetY"] = e.textOffsetY;
        if (e.padding != 8.0f) obj["padding"] = e.padding;
        if (e.margin != 0.0f) obj["margin"] = e.margin;
        if (e.rotation != 0.0f) obj["rotation"] = e.rotation;
        if (e.anchorX != "left") obj["anchorX"] = e.anchorX;
        if (e.anchorY != "top") obj["anchorY"] = e.anchorY;
        if (e.layer != 0) obj["layer"] = e.layer;

        // Appearance
        if (!e.visible) obj["visible"] = false;
        if (!e.enabled) obj["enabled"] = false;
        if (e.opacity != 1.0f) obj["opacity"] = e.opacity;
        if (e.hoverScale != 1.0f) obj["hoverScale"] = e.hoverScale;

        // Text
        if (!e.text.empty()) obj["text"] = e.text;
        if (!e.font.empty()) obj["font"] = e.font;
        if (e.fontSize != 0.0f) obj["fontSize"] = e.fontSize;
        if (e.textAlign != "left") obj["textAlign"] = e.textAlign;

        // Colors
        obj["textColor"] = e.textColor;
        obj["backgroundColor"] = e.backgroundColor;
        if (!e.hoverColor.empty()) obj["hoverColor"] = e.hoverColor;
        if (!e.pressedColor.empty()) obj["pressedColor"] = e.pressedColor;
        if (!e.outlineColor.empty()) obj["outlineColor"] = e.outlineColor;

        // Media
        if (!e.backgroundImage.empty()) obj["backgroundImage"] = e.backgroundImage;
        if (!e.backgroundVideo.empty()) obj["backgroundVideo"] = e.backgroundVideo;

        // Sound
        if (!e.hoverSound.empty()) obj["hoverSound"] = e.hoverSound;
        if (!e.clickSound.empty()) obj["clickSound"] = e.clickSound;

        elementsJson[id] = obj;
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
    file.close();
    mLastModified = getFileModifiedTime(filePath);
    mDirty = false;
    printf("[GUI LAYOUT] Saved %d elements to %s\n", (int)mElements.size(), filePath.c_str());
    return true;
}

UIRect GuiLayout::getRect(const std::string& id, const UIRect& fallback) const
{
    auto it = mElements.find(id);
    if (it == mElements.end())
        return fallback;
    return makeRect(it->second.x, it->second.y, it->second.w, it->second.h);
}

UIRect GuiLayout::getRectDesign(const std::string& id, const UIRect& fallback) const
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
    mDirty = true;
}

void GuiLayout::set(const std::string& id, float x, float y, float w, float h, float tx, float ty)
{
    mElements[id].id = id;
    mElements[id].x = x;
    mElements[id].y = y;
    mElements[id].w = w;
    mElements[id].h = h;
    mElements[id].textOffsetX = tx;
    mElements[id].textOffsetY = ty;
    mDirty = true;
}

void GuiLayout::setElement(const GuiElement& element)
{
    if (element.id.empty()) return;
    mElements[element.id] = element;
    mDirty = true;
}

void GuiLayout::clear()
{
    mElements.clear();
    mDirty = true;
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
    int saved = 0;
    for (auto& pair : mLayouts) {
        if (pair.second.isDirty() || !pair.second.elementIds().empty()) {
            pair.second.save(pair.first);
            saved++;
        }
    }
    printf("[GUI LAYOUT] Saved %d layout(s)\n", saved);
}

bool GuiLayoutManager::saveLayout(const std::string& filePath)
{
    auto it = mLayouts.find(filePath);
    if (it == mLayouts.end()) {
        printf("[GUI LAYOUT] No layout loaded for %s\n", filePath.c_str());
        return false;
    }
    return it->second.save(filePath);
}

bool GuiLayoutManager::reloadLayout(const std::string& filePath)
{
    auto it = mLayouts.find(filePath);
    if (it == mLayouts.end()) {
        printf("[GUI LAYOUT] No layout loaded for %s\n", filePath.c_str());
        return false;
    }
    printf("[GUI LAYOUT] Reloading %s\n", filePath.c_str());
    return it->second.load(filePath);
}

void GuiLayoutManager::resetLayout(const std::string& filePath)
{
    auto it = mLayouts.find(filePath);
    if (it == mLayouts.end()) return;
    it->second.clear();

    // Delete the saved file so next launch uses defaults
    std::error_code ec;
    std::filesystem::remove(filePath, ec);
    printf("[GUI LAYOUT] Reset %s (file deleted, will use defaults)\n", filePath.c_str());
}

void GuiLayoutManager::resetAll()
{
    for (auto& pair : mLayouts) {
        pair.second.clear();
        std::error_code ec;
        std::filesystem::remove(pair.first, ec);
    }
    printf("[GUI LAYOUT] Reset all layouts\n");
}

bool GuiLayoutManager::hasUnsaved() const
{
    for (const auto& pair : mLayouts) {
        if (pair.second.isDirty()) return true;
    }
    return false;
}

std::vector<std::string> GuiLayoutManager::unsavedLayouts() const
{
    std::vector<std::string> result;
    for (const auto& pair : mLayouts) {
        if (pair.second.isDirty())
            result.push_back(pair.first);
    }
    return result;
}
