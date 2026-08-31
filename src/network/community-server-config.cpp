// 08 31 2026, 18 10
/* purpose
* Loads and hot-reloads the community server mode and weapon-set JSON files.
* Validates list entries and keeps the last valid configuration on errors.
* Formats the exact dropdown and explanation strings consumed by the menu.
* Does NOT own server launch, authoritative scoring, or client networking.
* Does NOT infer weapon definitions from unordered runtime state.
* Does NOT send external webhook requests.
*/

#include "network/community-server-config.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace MimitaNet {

CommunityServerConfig& CommunityServerConfig::instance()
{
    static CommunityServerConfig config;
    return config;
}

std::filesystem::file_time_type CommunityServerConfig::writeTime(const std::string& path) const
{
    std::error_code ec;
    const auto value = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : value;
}

bool CommunityServerConfig::load(const std::string& modesPath, const std::string& weaponSetsPath)
{
    modesPath_ = modesPath;
    weaponSetsPath_ = weaponSetsPath;
    const bool modesOk = loadModes(modesPath_);
    const bool setsOk = loadWeaponSets(weaponSetsPath_);
    modesWriteTime_ = writeTime(modesPath_);
    weaponSetsWriteTime_ = writeTime(weaponSetsPath_);
    return modesOk && setsOk;
}

bool CommunityServerConfig::loadModes(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Networking,
            "[COMMUNITY CONFIG] missing modes file=%s\n", path.c_str());
        return false;
    }
    try {
        json root;
        file >> root;
        if (!root.contains("modes") || !root["modes"].is_array())
            throw std::runtime_error("missing modes array");
        std::vector<CommunityMode> next;
        for (const auto& item : root["modes"]) {
            if (!item.is_object() || !item.contains("id") || !item["id"].is_string()) continue;
            CommunityMode mode;
            mode.id = item.value("id", "");
            mode.name = item.value("name", mode.id);
            mode.description = item.value("description", "");
            mode.teams = item.value("teams", 0);
            mode.scoreLimit = std::max(0, item.value("score_limit", 0));
            if (!mode.id.empty()) next.push_back(std::move(mode));
        }
        if (next.empty()) throw std::runtime_error("no valid modes");
        modes_ = std::move(next);
        Debug::log(Debug::Category::Networking,
            "[COMMUNITY CONFIG] loaded modes=%zu file=%s\n", modes_.size(), path.c_str());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Networking,
            "[COMMUNITY CONFIG] modes parse failed file=%s error=%s\n", path.c_str(), e.what());
        return false;
    }
}

bool CommunityServerConfig::loadWeaponSets(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Networking,
            "[COMMUNITY CONFIG] missing weapon sets file=%s\n", path.c_str());
        return false;
    }
    try {
        json root;
        file >> root;
        if (!root.contains("sets") || !root["sets"].is_array())
            throw std::runtime_error("missing sets array");
        std::vector<CommunityWeaponSet> next;
        for (const auto& item : root["sets"]) {
            if (!item.is_object()) continue;
            CommunityWeaponSet set;
            set.id = item.value("id", 0);
            set.name = item.value("name", "Weapon Set " + std::to_string(set.id));
            set.description = item.value("description", "");
            if (item.contains("weapons") && item["weapons"].is_array())
                for (const auto& weapon : item["weapons"])
                    if (weapon.is_string()) set.weapons.push_back(weapon.get<std::string>());
            if (set.id > 0 && !set.weapons.empty()) next.push_back(std::move(set));
        }
        if (next.empty()) throw std::runtime_error("no valid weapon sets");
        std::sort(next.begin(), next.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        weaponSets_ = std::move(next);
        Debug::log(Debug::Category::Networking,
            "[COMMUNITY CONFIG] loaded weaponSets=%zu file=%s\n", weaponSets_.size(), path.c_str());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Networking,
            "[COMMUNITY CONFIG] weapon sets parse failed file=%s error=%s\n", path.c_str(), e.what());
        return false;
    }
}

bool CommunityServerConfig::pollReload()
{
    bool changed = false;
    const auto modesTime = writeTime(modesPath_);
    if (modesTime != std::filesystem::file_time_type{} && modesTime != modesWriteTime_) {
        changed = loadModes(modesPath_) || changed;
        modesWriteTime_ = modesTime;
    }
    const auto setsTime = writeTime(weaponSetsPath_);
    if (setsTime != std::filesystem::file_time_type{} && setsTime != weaponSetsWriteTime_) {
        changed = loadWeaponSets(weaponSetsPath_) || changed;
        weaponSetsWriteTime_ = setsTime;
    }
    return changed;
}

const CommunityMode* CommunityServerConfig::modeById(const std::string& id) const
{
    for (const auto& mode : modes_) if (mode.id == id) return &mode;
    return modes_.empty() ? nullptr : &modes_.front();
}

const CommunityMode* CommunityServerConfig::modeByName(const std::string& name) const
{
    for (const auto& mode : modes_) if (mode.name == name) return &mode;
    return modeById(name);
}

const CommunityWeaponSet* CommunityServerConfig::weaponSetById(int id) const
{
    for (const auto& set : weaponSets_) if (set.id == id) return &set;
    return weaponSets_.empty() ? nullptr : &weaponSets_.front();
}

bool CommunityServerConfig::weaponAllowed(int setId, const std::string& weaponId) const
{
    const CommunityWeaponSet* set = weaponSetById(setId);
    if (!set) return true;
    for (const std::string& allowed : set->weapons) {
        if (allowed == "*" || allowed == weaponId)
            return true;
    }
    return false;
}

std::string CommunityServerConfig::modeItems() const
{
    std::string out;
    for (const auto& mode : modes_) {
        if (!out.empty()) out += ',';
        out += mode.name;
    }
    return out;
}

std::string CommunityServerConfig::weaponSetItems() const
{
    std::string out;
    for (const auto& set : weaponSets_) {
        if (!out.empty()) out += ',';
        out += "Set " + std::to_string(set.id) + ": " + set.name;
    }
    return out;
}

std::string CommunityServerConfig::weaponSetDescription(int id) const
{
    const auto* set = weaponSetById(id);
    return set ? set->description : "No weapon set selected.";
}

} // namespace MimitaNet
