// 09 10 2026
/* purpose
* Loads, hot-reloads, and indexes reusable match-role definitions.
* Roles carry profile IDs only; gameplay configs stay in their own registries.
* Does NOT assign roles or contain gameplay logic.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "gamemode/match-roles.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "hot-reload/hot-movement-presets.h"
#include "physics/movement/movement-conversion.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

void readRole(const json& j, const std::string& fallbackId, MatchRoleDefinition& out)
{
    out.id = j.value("id", fallbackId);
    out.team = j.value("team", out.team);
    out.health = std::max(0, j.value("health", out.health));
    out.movementPreset = j.value("movement_preset", out.movementPreset);
    out.weaponSet = j.value("weapon_set", out.weaponSet);
    out.startingWeapon = j.value("starting_weapon", out.startingWeapon);
    out.behaviorProfile = j.value("behavior_profile", out.behaviorProfile);
}

} // namespace

MatchRoleRegistry& MatchRoleRegistry::instance()
{
    static MatchRoleRegistry registry;
    return registry;
}

bool MatchRoleRegistry::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Duel,
            "[ROLES] Missing %s; no roles loaded.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        std::vector<MatchRoleDefinition> roles;
        if (root.contains("roles")) {
            const auto& r = root["roles"];
            if (r.is_array()) {
                for (const auto& item : r) {
                    if (!item.is_object()) continue;
                    MatchRoleDefinition def;
                    readRole(item, "", def);
                    if (!def.id.empty()) roles.push_back(std::move(def));
                }
            } else if (r.is_object()) {
                for (auto it = r.begin(); it != r.end(); ++it) {
                    if (!it.value().is_object()) continue;
                    MatchRoleDefinition def;
                    readRole(it.value(), it.key(), def);
                    if (!def.id.empty()) roles.push_back(std::move(def));
                }
            }
        }

        mRoles = std::move(roles);
        mIndexById.clear();
        for (int i = 0; i < (int)mRoles.size(); ++i)
            mIndexById[mRoles[i].id] = i + 1;  // 1-based; 0 reserved for none

        mLastWrite = writeTime;
        if (!mWatchLogged) {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] Watching: %s\n", fileNameOf(mPath).c_str());
            mWatchLogged = true;
        }
        Debug::warn(Debug::Category::Duel,
            "[ROLES] Loaded %zu role(s) from %s\n", mRoles.size(), fileNameOf(mPath).c_str());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Duel,
            "[ROLES] Parse error in %s: %s. Keeping previous data.\n", mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Duel,
            "[ROLES] Error loading %s: %s. Keeping previous data.\n", mPath.c_str(), e.what());
    }
    return false;
}

bool MatchRoleRegistry::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Duel,
        "[ROLES] Detected change: %s\n", fileNameOf(mPath).c_str());
    return load(mPath);
}

const MatchRoleDefinition* MatchRoleRegistry::get(const std::string& id) const
{
    auto it = mIndexById.find(id);
    if (it == mIndexById.end()) return nullptr;
    const int index = it->second;
    if (index < 1 || index > (int)mRoles.size()) return nullptr;
    return &mRoles[index - 1];
}

int MatchRoleRegistry::indexOf(const std::string& id) const
{
    auto it = mIndexById.find(id);
    return it == mIndexById.end() ? 0 : it->second;
}

const std::string& MatchRoleRegistry::idForIndex(int index) const
{
    static const std::string empty;
    if (index < 1 || index > (int)mRoles.size()) return empty;
    return mRoles[index - 1].id;
}

RoleMovementCache& RoleMovementCache::instance()
{
    static RoleMovementCache cache;
    return cache;
}

const MovementConfig* RoleMovementCache::get(const std::string& preset)
{
    if (preset.empty())
        return nullptr;

    auto it = mEntries.find(preset);
    if (it != mEntries.end())
        return it->second.valid ? &it->second.config : nullptr;

    Entry entry;
    // One runtime authority: resolve the preset from the hot C++ registry
    // (hot-movement-presets.h). Movement JSON is comparison-only and never read.
    if (MimitaHotMovement::movementPresetNameExists(preset.c_str())) {
        entry.valid = true;
        entry.config = makeMovementConfigForPreset(static_cast<std::uint32_t>(
            MimitaHotMovement::movementPresetIdFromName(preset.c_str())));
        Debug::log(Debug::Category::Duel,
            "[ROLE MOVEMENT] resolved preset '%s' from hot C++ registry\n",
            preset.c_str());
    } else {
        Debug::warn(Debug::Category::Duel,
            "[ROLE MOVEMENT] unknown preset '%s' (hot C++ registry)\n",
            preset.c_str());
    }
    auto inserted = mEntries.emplace(preset, std::move(entry)).first;
    return inserted->second.valid ? &inserted->second.config : nullptr;
}

bool RoleMovementCache::pollReload()
{
    // Presets are hot C++ values now; there is no file to watch. Cached configs
    // stay valid for the process lifetime and refresh on the next get() after a
    // hot module activation replaces the code. Nothing to reload from disk.
    return false;
}
