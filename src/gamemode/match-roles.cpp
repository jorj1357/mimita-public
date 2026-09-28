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

#include "config/movement-config.h"
#include "debug/debug-log.h"
#include "utils/json-comments.h"
#include "utils/path_utils.h"

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
    out.displayName = j.value("display_name", out.displayName);
    out.team = j.value("team", out.team);
    out.health = std::max(0, j.value("health", out.health));
    out.movementPreset = j.value("movement_preset", out.movementPreset);
    out.weaponSet = j.value("weapon_set", out.weaponSet);
    out.startingWeapon = j.value("starting_weapon", out.startingWeapon);
    out.behaviorProfile = j.value("behavior_profile", out.behaviorProfile);
    out.avatarName = j.value("avatar", out.avatarName);
}

void readActorPreset(const json& j, const std::string& fallbackId,
                     MatchRoleDefinition& out)
{
    json flat = j;
    if (flat.contains("health") && flat["health"].is_object()) flat.erase("health");
    if (flat.contains("avatar") && flat["avatar"].is_object()) flat.erase("avatar");
    readRole(flat, fallbackId, out);
    out.actorPreset = true;
    if (j.contains("displayName")) out.displayName = j.value("displayName", out.displayName);
    if (j.contains("movementPreset")) out.movementPreset = j.value("movementPreset", out.movementPreset);
    if (j.contains("weaponSet")) out.weaponSet = j.value("weaponSet", out.weaponSet);

    if (j.contains("camera") && j["camera"].is_object()) {
        const auto& camera = j["camera"];
        out.cameraFov = camera.value("fov", out.cameraFov);
        out.forceFov = camera.value("forceFov", out.forceFov);
        const std::string perspective = camera.value("perspective", "");
        out.forceFirstPerson = camera.value("forcePerspective", out.forceFirstPerson)
            && perspective == "first_person";
    }
    if (j.contains("health") && j["health"].is_object()) {
        const auto& health = j["health"];
        out.health = std::max(0, health.value("spawn", health.value("max", out.health)));
    }
    if (j.contains("avatar") && j["avatar"].is_object()) {
        const auto& avatar = j["avatar"];
        out.avatarForced = avatar.value("forced", out.avatarForced);
        if (avatar.contains("allowed") && avatar["allowed"].is_array()) {
            for (const auto& item : avatar["allowed"])
                if (item.is_string()) out.allowedAvatars.push_back(item.get<std::string>());
            if (out.avatarName.empty() && out.avatarForced && !out.allowedAvatars.empty())
                out.avatarName = out.allowedAvatars.front();
        }
    }
    if (j.contains("team") && j["team"].is_object() &&
        j["team"].contains("allowed") && j["team"]["allowed"].is_array()) {
        for (const auto& item : j["team"]["allowed"])
            if (item.is_number_integer()) out.allowedTeams.push_back(item.get<int>());
    }
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
        root = parseJsonConfig(file);

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
    bool rolesChanged = false;
    if (writeTime != std::filesystem::file_time_type{} && writeTime != mLastWrite) {
        Debug::warn(Debug::Category::Duel,
            "[ROLES] Detected change: %s\n", fileNameOf(mPath).c_str());
        rolesChanged = load(mPath);
    }
    bool presetsChanged = false;
    if (!mPresetDirectory.empty()) {
        std::error_code presetEc;
        if (!std::filesystem::is_directory(mPresetDirectory, presetEc))
            return rolesChanged;
        for (const auto& entry : std::filesystem::directory_iterator(mPresetDirectory, presetEc)) {
            if (presetEc) break;
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const std::string path = entry.path().string();
            const auto write = getLastWrite(path);
            auto it = mPresetWrites.find(path);
            if (it == mPresetWrites.end() || it->second != write) { presetsChanged = true; break; }
        }
    }
    if (rolesChanged || presetsChanged) loadActorPresets(mPresetDirectory);
    return rolesChanged || presetsChanged;
}

const MatchRoleDefinition* MatchRoleRegistry::get(const std::string& id) const
{
    auto it = mIndexById.find(id);
    if (it == mIndexById.end()) return nullptr;
    const int index = it->second;
    if (index < 1 || index > (int)mRoles.size()) return nullptr;
    return &mRoles[index - 1];
}

const MatchRoleDefinition* MatchRoleRegistry::getActorPreset(const std::string& id) const
{
    const MatchRoleDefinition* def = get(id);
    return def && def->actorPreset ? def : nullptr;
}

std::vector<const MatchRoleDefinition*> MatchRoleRegistry::actorPresets() const
{
    std::vector<const MatchRoleDefinition*> out;
    for (const auto& role : mRoles)
        if (role.actorPreset) out.push_back(&role);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });
    return out;
}

bool MatchRoleRegistry::loadActorPresets(const std::string& directory)
{
    std::filesystem::path resolvedDirectory = directory;
    std::error_code ec;
    if (!std::filesystem::is_directory(resolvedDirectory, ec)) {
        const std::filesystem::path exeDirectory = getExecutableDirectory();
        const std::filesystem::path candidates[] = {
            exeDirectory / directory,
            exeDirectory / ".." / directory,
            exeDirectory / ".." / ".." / directory
        };
        for (const auto& candidate : candidates) {
            if (std::filesystem::is_directory(candidate, ec)) {
                resolvedDirectory = std::filesystem::weakly_canonical(candidate, ec);
                break;
            }
        }
    }
    if (!std::filesystem::is_directory(resolvedDirectory, ec)) {
        Debug::warn(Debug::Category::Duel,
            "[ACTOR PRESET] Missing directory %s (cwd=%s); no presets loaded.\n",
            directory.c_str(), std::filesystem::current_path(ec).string().c_str());
        return false;
    }
    mPresetDirectory = resolvedDirectory.string();

    mRoles.erase(std::remove_if(mRoles.begin(), mRoles.end(),
        [](const MatchRoleDefinition& role) { return role.actorPreset; }), mRoles.end());
    mPresetWrites.clear();
    for (const auto& entry : std::filesystem::directory_iterator(resolvedDirectory, ec)) {
        if (ec || !entry.is_regular_file(ec) || entry.path().extension() != ".json") continue;
        std::ifstream file(entry.path());
        if (!file.is_open()) continue;
        try {
            const json root = parseJsonConfig(file);
            MatchRoleDefinition def;
            readActorPreset(root, entry.path().stem().string(), def);
            if (!def.id.empty()) mRoles.push_back(std::move(def));
            mPresetWrites[entry.path().string()] = getLastWrite(entry.path().string());
        } catch (const std::exception& e) {
            Debug::error(Debug::Category::Duel,
                "[ACTOR PRESET] Error loading %s: %s. Keeping previous data.\n",
                entry.path().string().c_str(), e.what());
        }
    }
    mIndexById.clear();
    for (int i = 0; i < (int)mRoles.size(); ++i) mIndexById[mRoles[i].id] = i + 1;
    Debug::warn(Debug::Category::Duel, "[ACTOR PRESET] Loaded %zu preset(s) from %s\n",
        actorPresets().size(), mPresetDirectory.c_str());
    return true;
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
    std::string path;
    // loadPresetInto warns once on an unknown/unparseable preset.
    entry.valid = MovementJsonConfig::instance().loadPresetInto(
        preset, entry.config, &path);
    if (entry.valid) {
        entry.path = path;
        entry.write = getLastWrite(path);
        Debug::log(Debug::Category::Duel,
            "[ROLE MOVEMENT] resolved preset '%s' from %s\n",
            preset.c_str(), path.c_str());
    }
    auto inserted = mEntries.emplace(preset, std::move(entry)).first;
    return inserted->second.valid ? &inserted->second.config : nullptr;
}

bool RoleMovementCache::pollReload()
{
    bool changed = false;
    for (auto& kv : mEntries) {
        Entry& entry = kv.second;
        if (!entry.valid || entry.path.empty())
            continue;  // miss entries never retry on their own
        const auto write = getLastWrite(entry.path);
        if (write == std::filesystem::file_time_type{} || write == entry.write)
            continue;

        MovementConfig next;
        std::string path;
        if (MovementJsonConfig::instance().loadPresetInto(kv.first, next, &path)) {
            entry.config = next;
            entry.path = path;
            entry.write = write;
            changed = true;
            Debug::warn(Debug::Category::Duel,
                "[ROLE MOVEMENT] reloaded preset '%s' from %s\n",
                kv.first.c_str(), path.c_str());
        } else {
            // Keep the last valid config and stop retrying this file.
            entry.write = write;
        }
    }
    return changed;
}
