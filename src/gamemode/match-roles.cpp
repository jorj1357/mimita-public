// 09 10 2026
/* purpose
* Loads, hot-reloads, and indexes reusable match-role definitions.
* Roles carry profile IDs only; gameplay configs stay in their own registries.
* Does NOT assign roles or contain gameplay logic.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "gamemode/match-roles.h"

#include <algorithm>
#include <cctype>
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

bool isJsonFile(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return extension == ".json";
}

std::filesystem::path resolveActorPresetDirectory(const std::string& directory)
{
    std::vector<std::filesystem::path> candidates;
    const std::filesystem::path requested(directory);

    auto addCandidate = [&candidates](const std::filesystem::path& candidate) {
        if (candidate.empty()) return;
        for (const auto& existing : candidates) {
            if (existing == candidate) return;
        }
        candidates.push_back(candidate);
    };

    // Keep an explicitly supplied absolute path authoritative.
    addCandidate(requested);

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec)
        addCandidate(cwd / requested);

    // The game is sometimes launched from a build/staging directory. Walk up
    // from the actual executable so config/actor-presets remains discoverable.
    std::filesystem::path executableDirectory(getExecutableDirectory());
    for (int level = 0; level < 6 && !executableDirectory.empty(); ++level) {
        addCandidate(executableDirectory / requested);
        const auto parent = executableDirectory.parent_path();
        if (parent == executableDirectory) break;
        executableDirectory = parent;
    }

    for (const auto& candidate : candidates) {
        std::error_code candidateEc;
        if (!std::filesystem::is_directory(candidate, candidateEc)) continue;
        auto canonical = std::filesystem::weakly_canonical(candidate, candidateEc);
        if (!candidateEc) return canonical;
        return candidate;
    }
    return {};
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
    out.actorPresetId = j.value("actor_preset", out.actorPresetId);
    out.teamId = j.value("team_id", out.teamId);
    out.spawnGroup = j.value("spawn_group", out.spawnGroup);
    out.avatarName = j.value("avatar", out.avatarName);
    if (j.contains("avatar_forced") && j["avatar_forced"].is_boolean())
        out.avatarForced = j["avatar_forced"].get<bool>();
    if (j.contains("objective_permissions") && j["objective_permissions"].is_array()) {
        out.objectivePermissions.clear();
        for (const auto& item : j["objective_permissions"])
            if (item.is_string()) out.objectivePermissions.push_back(item.get<std::string>());
    }
}

template <typename T>
bool readOptional(const json& object, const char* snake, const char* camel, T& out)
{
    const char* key = object.contains(snake) ? snake : camel;
    if (!object.contains(key)) return false;
    try {
        out = object.at(key).get<T>();
        return true;
    } catch (...) {
        return false;
    }
}

void readPresentation(const json& j, ActorPresetPresentation& out)
{
    if (!j.is_object()) return;
    out.hasDamageNumbers = readOptional(j, "damage_numbers", "damageNumbers", out.damageNumbers);
    out.hasHitEffects = readOptional(j, "hit_effects", "hitEffects", out.hitEffects);
    out.hasWorldImpactEffects = readOptional(j, "world_impact_effects", "worldImpactEffects", out.worldImpactEffects);
    out.hasBloodEffects = readOptional(j, "blood_effects", "bloodEffects", out.bloodEffects);
    out.hasMuzzleFlash = readOptional(j, "muzzle_flash", "muzzleFlash", out.muzzleFlash);
    out.hasHitMarkers = readOptional(j, "hit_markers", "hitMarkers", out.hitMarkers);
    out.hasHitSounds = readOptional(j, "hit_sounds", "hitSounds", out.hitSounds);
}

void readWeaponOverride(const json& j, ActorPresetWeaponOverride& out)
{
    if (!j.is_object()) return;
    out.hasDamage = readOptional(j, "damage", "damage", out.damage);
    out.hasDamageScale = readOptional(j, "damage_scale", "damageScale", out.damageScale);
    out.hasHeadshotMultiplier = readOptional(j, "headshot_multiplier", "headshotMultiplier", out.headshotMultiplier);
    out.hasSpread = readOptional(j, "spread", "spread", out.spread);
    out.hasRecoil = readOptional(j, "recoil", "recoil", out.recoil);
    out.hasFireDelay = readOptional(j, "fire_delay", "fireDelay", out.fireDelay);
    out.hasReloadTime = readOptional(j, "reload_time", "reloadTime", out.reloadTime);
    out.hasMagazineSize = readOptional(j, "magazine_size", "magazineSize", out.magazineSize);
    out.hasReserveAmmo = readOptional(j, "reserve_ammo", "reserveAmmo", out.reserveAmmo);

    if (j.contains("hitscan") && j["hitscan"].is_object()) {
        const auto& h = j["hitscan"];
        out.hasHitscan = readOptional(h, "enabled", "enabled", out.hitscan);
        out.hasBeamThickness = readOptional(h, "beam_thickness", "beamThickness", out.beamThickness);
        out.hasWorldThickness = readOptional(h, "world_thickness", "worldThickness", out.worldThickness);
        out.hasSquareSpread = readOptional(h, "square_spread", "squareSpread", out.squareSpread);
        out.hasRange = readOptional(h, "range", "range", out.range);
    }

    if (j.contains("damage_policy") && j["damage_policy"].is_object()) {
        const auto& policy = j["damage_policy"];
        const char* key = policy.contains("allowed_body_parts")
            ? "allowed_body_parts" : "allowedBodyParts";
        if (policy.contains(key) && policy[key].is_array()) {
            for (const auto& part : policy[key])
                if (part.is_string()) out.allowedBodyParts.push_back(part.get<std::string>());
        }
    }

    // A weapon may also carry a presentation override that refines the
    // preset-level policy for that weapon only. Missing keys inherit.
    if (j.contains("presentation") && j["presentation"].is_object())
        readPresentation(j["presentation"], out.presentation);
}

bool readActorPreset(const json& j, const std::string& fallbackId,
                     MatchRoleDefinition& out, std::string& error)
{
    json flat = j;
    if (flat.contains("health") && flat["health"].is_object()) flat.erase("health");
    if (flat.contains("avatar") && flat["avatar"].is_object()) flat.erase("avatar");
    if (flat.contains("team") && flat["team"].is_object()) flat.erase("team");
    readRole(flat, fallbackId, out);
    out.actorPreset = true;

    if (j.contains("npc_behavior")) {
        std::string policyError;
        if (!parseNpcMovementPolicy(j["npc_behavior"], out.movementPolicy, policyError)) {
            error = "npc_behavior: " + policyError;
            return false;
        }
    }
    if (j.contains("navigation")) {
        std::string navError;
        if (!parseNpcNavigationSettings(j["navigation"], out.navigationSettings, navError)) {
            error = "navigation: " + navError;
            return false;
        }
    }
    // Generic movement-executor selector. Top-level key overrides any value in
    // npc_behavior so it is reusable by any mode, not Counter-Strike-specific.
    if (j.contains("movement_executor")) {
        if (!j["movement_executor"].is_string()) {
            error = "movement_executor must be a string";
            return false;
        }
        const std::string exec = j["movement_executor"].get<std::string>();
        if (exec != "sandbox_shared" && exec != "surface_navigation" && exec != "direct") {
            error = "movement_executor has unknown value \"" + exec + "\"";
            return false;
        }
        out.movementPolicy.movementExecutor = exec;
    }
    if (j.contains("displayName")) out.displayName = j.value("displayName", out.displayName);
    if (j.contains("movementPreset")) out.movementPreset = j.value("movementPreset", out.movementPreset);
    if (j.contains("weaponSet")) out.weaponSet = j.value("weaponSet", out.weaponSet);

    if (j.contains("camera") && j["camera"].is_object()) {
        const auto& camera = j["camera"];
        out.cameraFov = camera.value("fov", out.cameraFov);
        out.forceFov = camera.value("force_fov", camera.value("forceFov", out.forceFov));
        const std::string perspective = camera.value("perspective", "");
        out.forceFirstPerson = camera.value("force_perspective",
            camera.value("forcePerspective", out.forceFirstPerson))
            && perspective == "first_person";
    }

    if (j.contains("presentation") && j["presentation"].is_object())
        readPresentation(j["presentation"], out.presentation);

    if (j.contains("weapon_overrides") && j["weapon_overrides"].is_object()) {
        for (auto it = j["weapon_overrides"].begin(); it != j["weapon_overrides"].end(); ++it) {
            ActorPresetWeaponOverride weapon;
            readWeaponOverride(it.value(), weapon);
            out.weaponOverrides[it.key()] = std::move(weapon);
        }
    }
    if (j.contains("health") && j["health"].is_object()) {
        const auto& health = j["health"];
        out.health = std::max(0, health.value("spawn", health.value("max", out.health)));
    }
    if (j.contains("avatar") && j["avatar"].is_object()) {
        const auto& avatar = j["avatar"];
        out.avatarForced = avatar.value("forced", out.avatarForced);
        out.avatarName = avatar.value("name", out.avatarName);
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
    return true;
}

// Log one line per configured navigation block so a reload is visible once.
void logNavigationSettings(const MatchRoleDefinition& def, uint64_t revision)
{
    if (!def.navigationSettings.configured) return;
    const NpcNavigationSettings& n = def.navigationSettings;
    Debug::warn(Debug::Category::Duel,
        "[NPC NAV CFG] preset=%s revision=%llu mode=%s radius=%.1f slopes=%s jumps=%d wall_jump=%d blocked=%s\n",
        def.id.c_str(), (unsigned long long)revision, n.mode.c_str(), n.searchRadius,
        n.maxWalkableSlopeDot > 0.0f ? "preset" : "shared",
        (int)n.allowNavigationJumps, (int)n.allowWallJump, n.blockedBehavior.c_str());
}

// Log one line per configured policy so a reload is visible without spamming.
void logMovementPolicy(const MatchRoleDefinition& def, uint64_t revision)
{
    if (!def.movementPolicy.configured) return;
    const NpcMovementPolicy& p = def.movementPolicy;
    Debug::warn(Debug::Category::Duel,
        "[NPC POLICY] preset=%s revision=%llu travel=%s combat=%s circle=%d strafe=%d noise=%.2f retreat=%.2f jump=%s dash=%s\n",
        def.id.c_str(), (unsigned long long)revision,
        p.travelStyle.c_str(), p.combatStyle.c_str(),
        (int)p.allowCircle, (int)p.allowStrafe, p.movementNoise,
        p.retreatHealthFraction, p.jumpStyle.c_str(), p.dashStyle.c_str());
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
            if (!entry.is_regular_file() || !isJsonFile(entry.path())) continue;
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
    const std::filesystem::path resolvedDirectory = resolveActorPresetDirectory(directory);
    if (resolvedDirectory.empty()) {
        std::error_code cwdEc;
        const auto cwd = std::filesystem::current_path(cwdEc);
        Debug::warn(Debug::Category::Duel,
            "[ACTOR PRESET] Missing directory %s (cwd=%s); no presets loaded.\n",
            directory.c_str(), cwdEc ? "unknown" : cwd.string().c_str());
        return false;
    }

    Debug::warn(Debug::Category::Duel,
        "[ACTOR PRESET] Scanning directory: %s\n", resolvedDirectory.string().c_str());

    // Keep the last valid definition per preset id so a malformed file (bad
    // JSON, unknown policy enum, wrong type) keeps the previous policy instead
    // of erasing it or silently degrading to an unrelated behavior.
    std::unordered_map<std::string, MatchRoleDefinition> previousById;
    for (const auto& role : mRoles)
        if (role.actorPreset) previousById[role.id] = role;

    std::vector<MatchRoleDefinition> loadedPresets;
    std::unordered_map<std::string, std::filesystem::file_time_type> loadedWrites;
    auto keepPrevious = [&](const std::string& path, const std::string& id) {
        auto it = previousById.find(id);
        if (it != previousById.end()) {
            loadedPresets.push_back(it->second);
            // Record the write so pollReload does not retry the bad file until
            // it is edited again; the previous valid policy stays live.
            loadedWrites[path] = getLastWrite(path);
        }
    };

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(resolvedDirectory, ec)) {
        if (ec) {
            Debug::error(Debug::Category::Duel,
                "[ACTOR PRESET] Directory scan failed for %s: %s\n",
                resolvedDirectory.string().c_str(), ec.message().c_str());
            break;
        }
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || entryEc || !isJsonFile(entry.path())) continue;

        const std::string path = entry.path().string();
        const std::string stem = entry.path().stem().string();
        Debug::log(Debug::Category::Duel,
            "[ACTOR PRESET] Found JSON: %s\n", path.c_str());
        std::ifstream file(entry.path());
        if (!file.is_open()) {
            Debug::error(Debug::Category::Duel,
                "[ACTOR PRESET] Cannot open %s. Keeping previous data.\n", path.c_str());
            keepPrevious(path, stem);
            continue;
        }
        try {
            const json root = parseJsonConfig(file);
            MatchRoleDefinition def;
            std::string loadError;
            if (!readActorPreset(root, stem, def, loadError)) {
                Debug::error(Debug::Category::Duel,
                    "[ACTOR PRESET] Invalid %s: %s. Keeping previous policy.\n",
                    path.c_str(), loadError.c_str());
                keepPrevious(path, def.id.empty() ? stem : def.id);
                continue;
            }
            if (def.id.empty()) {
                Debug::error(Debug::Category::Duel,
                    "[ACTOR PRESET] Ignoring %s because it has no id\n", path.c_str());
                continue;
            }
            Debug::log(Debug::Category::Duel,
                "[ACTOR PRESET] Loaded id=%s from %s\n", def.id.c_str(), path.c_str());
            loadedPresets.push_back(std::move(def));
            loadedWrites[path] = getLastWrite(path);
        } catch (const std::exception& e) {
            Debug::error(Debug::Category::Duel,
                "[ACTOR PRESET] Error loading %s: %s. Keeping previous data.\n",
                path.c_str(), e.what());
            keepPrevious(path, stem);
        }
    }

    // Replace only after the directory was found and scanned. A malformed
    // file must not erase the last valid preset set.
    mRoles.erase(std::remove_if(mRoles.begin(), mRoles.end(),
        [](const MatchRoleDefinition& role) { return role.actorPreset; }), mRoles.end());
    for (auto& preset : loadedPresets)
        mRoles.push_back(std::move(preset));
    mPresetDirectory = resolvedDirectory.string();
    mPresetWrites = std::move(loadedWrites);
    mIndexById.clear();
    for (int i = 0; i < (int)mRoles.size(); ++i) mIndexById[mRoles[i].id] = i + 1;
    ++mActorPresetRevision;
    for (const auto* preset : actorPresets()) {
        logMovementPolicy(*preset, mActorPresetRevision);
        logNavigationSettings(*preset, mActorPresetRevision);
    }
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
