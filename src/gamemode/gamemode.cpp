// 08 10 2026, 14 34
/* purpose
* Loads, hot-reloads, and lists gamemodes from config/gamemodes/*.json.
* Parses every knobs into a Gamemode struct with safe defaults for missing keys.
* Used by the menu (list gamemodes) and the duel engine (read rules).
* Does NOT contain gameplay logic or duel simulation.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "gamemode/gamemode.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "game/duel.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "utils/json-comments.h"
#include "npc/npc-targeting.h"
#include "notifications/notifications.h"

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

std::string optString(const json& root, const char* key, const std::string& fallback)
{
    if (root.contains(key) && root[key].is_string())
        return root[key].get<std::string>();
    return fallback;
}

int optInt(const json& root, const char* key, int fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<int>();
    return fallback;
}

float optFloat(const json& root, const char* key, float fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<float>();
    return fallback;
}

bool optBool(const json& root, const char* key, bool fallback)
{
    if (root.contains(key) && root[key].is_boolean())
        return root[key].get<bool>();
    return fallback;
}

std::vector<std::string> optStringArray(const json& root, const char* key)
{
    std::vector<std::string> out;
    if (!root.contains(key) || !root[key].is_array())
        return out;
    for (const auto& item : root[key]) {
        if (item.is_string())
            out.push_back(item.get<std::string>());
    }
    return out;
}

std::string joinPath(const std::string& base, const std::string& key)
{
    return base.empty() ? key : base + "." + key;
}

std::string joinIndex(const std::string& base, size_t index)
{
    return base + "[" + std::to_string(index) + "]";
}

bool isIntegerJson(const json& value)
{
    return value.is_number_integer() || value.is_number_unsigned();
}

void validateSchema(const json& value, const json& schema,
                    const std::string& path, std::vector<std::string>& errors)
{
    if (schema.is_boolean()) return;
    if (!schema.is_object()) return;

    if (value.is_null() && schema.value("nullable", false))
        return;

    const std::string type = schema.value("type", "");
    if (!type.empty() && type != "any") {
        bool ok = false;
        if (type == "object") ok = value.is_object();
        else if (type == "array") ok = value.is_array();
        else if (type == "string") ok = value.is_string();
        else if (type == "boolean") ok = value.is_boolean();
        else if (type == "number") ok = value.is_number();
        else if (type == "integer") ok = isIntegerJson(value);
        else ok = true;
        if (!ok) {
            errors.push_back(path + ": expected " + type);
            return;
        }
    }

    if (value.is_number()) {
        if (schema.contains("min") && value.get<double>() < schema["min"].get<double>())
            errors.push_back(path + ": " + std::to_string(value.get<double>()) +
                             " is below minimum " + std::to_string(schema["min"].get<double>()));
        if (schema.contains("max") && value.get<double>() > schema["max"].get<double>())
            errors.push_back(path + ": " + std::to_string(value.get<double>()) +
                             " is above maximum " + std::to_string(schema["max"].get<double>()));
    }
    if (value.is_string() && schema.contains("enum") && schema["enum"].is_array()) {
        bool found = false;
        for (const auto& allowed : schema["enum"])
            if (allowed.is_string() && allowed.get<std::string>() == value.get<std::string>())
                found = true;
        if (!found)
            errors.push_back(path + ": \"" + value.get<std::string>() +
                             "\" is not one of the allowed values");
    }
    if (value.is_array()) {
        if (schema.contains("min_items") && value.size() < schema["min_items"].get<size_t>())
            errors.push_back(path + ": expected at least " +
                             std::to_string(schema["min_items"].get<size_t>()) + " items");
        if (schema.contains("max_items") && value.size() > schema["max_items"].get<size_t>())
            errors.push_back(path + ": expected at most " +
                             std::to_string(schema["max_items"].get<size_t>()) + " items");
        if (schema.contains("item")) {
            for (size_t i = 0; i < value.size(); ++i)
                validateSchema(value[i], schema["item"], joinIndex(path, i), errors);
        }
    }
    if (value.is_object()) {
        if (schema.contains("fields") && schema["fields"].is_object()) {
            const json& fields = schema["fields"];
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                if (it.value().value("required", false) && !value.contains(it.key()))
                    errors.push_back(joinPath(path, it.key()) + ": required setting is missing");
            }
            for (auto it = value.begin(); it != value.end(); ++it) {
                if (!fields.contains(it.key())) {
                    errors.push_back(joinPath(path, it.key()) + ": unknown setting");
                    continue;
                }
                validateSchema(it.value(), fields[it.key()],
                               joinPath(path, it.key()), errors);
            }
        } else if (schema.contains("additional")) {
            for (auto it = value.begin(); it != value.end(); ++it)
                validateSchema(it.value(), schema["additional"],
                               joinPath(path, it.key()), errors);
        }
    }
}

std::string modeDisplayName(const json& root)
{
    if (root.contains("id") && root["id"].is_string())
        return root["id"].get<std::string>();
    return "gamemode";
}

} // namespace

GamemodeRegistry& GamemodeRegistry::instance()
{
    static GamemodeRegistry registry;
    return registry;
}

bool GamemodeRegistry::loadMasterlist()
{
    mMasterlistLoaded = false;
    mMasterlist = json{};
    if (directory_.empty()) return false;

    const std::filesystem::path path =
        std::filesystem::path(directory_) / "masterlist" / "gamemode-masterlist.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE] Missing masterlist %s; using permissive legacy loading.\n",
            path.string().c_str());
        return false;
    }
    try {
        json root = parseJsonConfig(file);
        if (!root.is_object() || !root.contains("settings") ||
            !root["settings"].is_object()) {
            Debug::warn(Debug::Category::Duel,
                "[GAMEMODE] Masterlist %s has no settings object; using permissive legacy loading.\n",
                path.string().c_str());
            return false;
        }
        mMasterlist = std::move(root);
        mMasterlistLoaded = true;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE] Loaded masterlist %s (%zu settings)\n",
            path.string().c_str(), mMasterlist["settings"].size());
        return true;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel,
            "[GAMEMODE] Masterlist parse error in %s: %s; using permissive legacy loading.\n",
            path.string().c_str(), e.what());
        return false;
    }
}

bool GamemodeRegistry::validateFileJson(const std::string& path,
                                        const json& root,
                                        std::vector<std::string>& errors) const
{
    errors.clear();
    if (!mMasterlistLoaded || !mMasterlist.contains("settings"))
        return false;

    if (!root.is_object()) {
        errors.push_back("root: expected an object");
        return true;
    }

    const json& settings = mMasterlist["settings"];
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        if (it.value().value("required", false) && !root.contains(it.key()))
            errors.push_back(it.key() + ": required setting is missing");
    }
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!settings.contains(it.key())) {
            errors.push_back(it.key() + ": unknown setting");
            continue;
        }
        validateSchema(it.value(), settings[it.key()], it.key(), errors);
    }

    // The file identity must match its filename. A renamed/copied mode file
    // that still declares another id is a configuration error, not a new mode.
    if (root.contains("id") && root["id"].is_string()) {
        const std::string id = root["id"].get<std::string>();
        const std::string stem = std::filesystem::path(path).stem().string();
        if (id != stem)
            errors.push_back("id: \"" + id + "\" does not match filename stem \"" + stem + "\"");
    }
    return !errors.empty();
}

void GamemodeRegistry::applyAvatarGroup(GamemodeAvatarPolicy& policy) const
{
    if (policy.group.empty() || !mMasterlistLoaded) return;
    if (!mMasterlist.contains("avatar_groups") ||
        !mMasterlist["avatar_groups"].is_object())
        return;
    const auto& groups = mMasterlist["avatar_groups"];
    if (!groups.contains(policy.group) || !groups[policy.group].is_object())
        return;
    const auto& group = groups[policy.group];
    if (group.contains("allowed") && group["allowed"].is_array()) {
        for (const auto& item : group["allowed"])
            if (item.is_string()) policy.allowed.push_back(item.get<std::string>());
    }
    if (group.contains("denied") && group["denied"].is_array()) {
        for (const auto& item : group["denied"])
            if (item.is_string()) policy.denied.push_back(item.get<std::string>());
    }
}

void GamemodeRegistry::reportInvalidFile(const std::string& path,
                                         const std::vector<std::string>& errors)
{
    if (errors.empty()) return;
    const std::string fileName = fileNameOf(path);
    for (size_t i = 0; i < errors.size() && i < 24; ++i) {
        const std::string message = fileName + " -> " + errors[i];
        mValidationErrors.push_back(message);
        Debug::error(Debug::Category::Duel, "[GAMEMODE CONFIG] %s\n", message.c_str());
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Errors,
            "config.gamemode.invalid", fileName, errors[i], 0,
            json{{"file", path}, {"setting_path", errors[i]},
                 {"error", "invalid_gamemode_setting"}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    if (errors.size() > 24) {
        Debug::error(Debug::Category::Duel,
            "[GAMEMODE CONFIG] %s has %zu more errors (truncated)\n",
            fileName.c_str(), errors.size() - 24);
    }

    // Player-facing tip: one per file per second, naming the exact setting.
    static std::unordered_map<std::string, std::chrono::steady_clock::time_point> sLastTip;
    const auto now = std::chrono::steady_clock::now();
    auto it = sLastTip.find(path);
    if (it != sLastTip.end() &&
        now - it->second < std::chrono::seconds(1))
        return;
    sLastTip[path] = now;

    std::string message = fileName + " -> " + errors.front();
    if (errors.size() > 1)
        message += " (+" + std::to_string(errors.size() - 1) + " more)";
    NotificationSystem::instance().pushImportant(
        "CONFIG ERROR", message, 0);
}

void GamemodeRegistry::loadDirectory(const std::string& dir)
{
    directory_ = dir;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Missing directory %s; no gamemodes loaded.\n", dir.c_str());
        return;
    }

    loadMasterlist();
    modes_.clear();
    mValidationErrors.clear();
    std::set<std::string> seenIds;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".json") continue;

        LoadedMode slot;
        slot.path = entry.path().string();
        slot.writeTime = getLastWrite(slot.path);
        if (!loadFile(slot.path, slot))
            continue;
        if (!seenIds.insert(slot.mode.id).second) {
            reportInvalidFile(slot.path,
                {"id: duplicate mode id \"" + slot.mode.id + "\""});
            continue;
        }
        modes_.push_back(std::move(slot));
    }

    Debug::warn(Debug::Category::Duel, "[GAMEMODE] Loaded %zu gamemode(s) from %s\n", modes_.size(), dir.c_str());
}

void GamemodeRegistry::pollReload()
{
    if (directory_.empty()) return;

    bool changed = false;
    for (auto& slot : modes_) {
        const auto writeTime = getLastWrite(slot.path);
        if (writeTime == std::filesystem::file_time_type{} || writeTime == slot.writeTime)
            continue;
        changed = true;
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Detected change: %s\n", fileNameOf(slot.path).c_str());
        slot.writeTime = writeTime;
        loadFile(slot.path, slot);
    }

    if (changed)
        Debug::warn(Debug::Category::Duel, "[GAMEMODE] Reload complete\n");
}

bool GamemodeRegistry::loadFile(const std::string& path, LoadedMode& slot)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        reportInvalidFile(path, {"file: cannot open"});
        return false;
    }

    try {
        json root;
        root = parseJsonConfig(file);
        if (!root.is_object() || !root.contains("id") || !root["id"].is_string()) {
            reportInvalidFile(path, {"id: required string setting is missing"});
            return false;
        }
        {
            std::vector<std::string> errors;
            if (validateFileJson(path, root, errors)) {
                reportInvalidFile(path, errors);
                return false;
            }
        }

        Gamemode next;
        next.id = root["id"].get<std::string>();
        next.name = optString(root, "name", next.name);
        next.description = optString(root, "description", next.description);
        next.teamNames = optStringArray(root, "team_names");
        if (next.teamNames.size() < 2)
            next.teamNames = {"RED", "BLUE"};
        next.goalValue = std::max(1, optInt(root, "goal_value", next.goalValue));
        next.weaponSetId = std::max(1, optInt(root, "weapon_set_id", next.weaponSetId));
        next.timeLimitSeconds = std::max(0, optInt(root, "time_limit_seconds", next.timeLimitSeconds));
        next.timeLimitOutcome = optString(root, "time_limit_outcome", next.timeLimitOutcome);
        next.respawnSeconds = std::max(0.0f, optFloat(root, "respawn_seconds", next.respawnSeconds));
        next.killHeals = optBool(root, "kill_heals", next.killHeals);
        next.friendlyFire = optBool(root, "friendly_fire", next.friendlyFire);
        next.countdownSeconds = std::max(0.0f, optFloat(root, "countdown_seconds", next.countdownSeconds));
        next.goSeconds = std::max(0.0f, optFloat(root, "go_seconds", next.goSeconds));
        next.rematchSeconds = std::max(0.0f, optFloat(root, "rematch_seconds", next.rematchSeconds));
        next.spawnTracerSeconds = std::max(0.0f, optFloat(root, "spawn_tracer_seconds", next.spawnTracerSeconds));
        next.allowRematch = optBool(root, "allow_rematch", next.allowRematch);
        next.spawnOffsetRadius = std::max(0.0f, optFloat(root, "spawn_offset_radius", next.spawnOffsetRadius));
        next.spawnStrategy = optString(root, "spawn_strategy", next.spawnStrategy);
        next.intermissionSeconds = std::max(0, optInt(root, "intermission_seconds", next.intermissionSeconds));
        next.resultsSeconds = std::max(0, optInt(root, "results_seconds", next.resultsSeconds));
        next.maps = optStringArray(root, "maps");

        // ── Ordered roster (optional). Defines team order, display names,
        // capacity, per-team role, and spawn group. When present it also
        // supplies teamNames so existing HUD/team code keeps working. ──
        if (root.contains("teams") && root["teams"].is_array()) {
            for (const auto& t : root["teams"]) {
                if (!t.is_object()) continue;
                GamemodeTeam team;
                team.id = optString(t, "id", "");
                team.displayName = optString(t, "display_name", "");
                team.capacity = std::max(0, optInt(t, "capacity", 0));
                team.role = optString(t, "role", "");
                team.spawnGroup = optString(t, "spawn_group", "");
                team.spawnTagContains = optString(t, "spawn_tag_contains", "");
                team.spawnTagExcludes = optString(t, "spawn_tag_excludes", "");
                team.attackersPerSite = std::max(0, optInt(t, "attackers_per_site", 0));
                team.defendersPerSite = std::max(0, optInt(t, "defenders_per_site", 0));
                if (t.contains("one_rotator") && t["one_rotator"].is_boolean())
                    team.oneRotator = t["one_rotator"].get<bool>() ? 1 : 0;
                team.npcRole = optString(t, "npc_role", "");
                team.players = optBool(t, "players", team.players);
                team.npcs = optBool(t, "npcs", team.npcs);
                team.preferHumans = optBool(t, "prefer_humans", team.preferHumans);
                if (team.id.empty() && team.displayName.empty()) continue;
                if (team.displayName.empty()) team.displayName = team.id;
                next.teams.push_back(std::move(team));
            }
            if (!next.teams.empty()) {
                next.teamNames.clear();
                for (const auto& team : next.teams)
                    next.teamNames.push_back(team.displayName);
            }
        }

        // ── Spawn groups (optional): named team spawn sets. ─────────
        if (root.contains("spawn_groups") && root["spawn_groups"].is_object()) {
            for (auto it = root["spawn_groups"].begin(); it != root["spawn_groups"].end(); ++it) {
                if (!it.value().is_object()) continue;
                GamemodeSpawnGroup group;
                group.id = it.key();
                group.team = optString(it.value(), "team", "");
                if (it.value().contains("points") && it.value()["points"].is_array()) {
                    for (const auto& p : it.value()["points"]) {
                        if (!p.is_array() || p.size() < 3) continue;
                        GamemodeSpawnGroup::Point point;
                        if (p[0].is_number()) point.x = p[0].get<float>();
                        if (p[1].is_number()) point.y = p[1].get<float>();
                        if (p[2].is_number()) point.z = p[2].get<float>();
                        group.points.push_back(point);
                    }
                }
                next.spawnGroups.push_back(std::move(group));
            }
        }

        // ── Objective definitions (optional). ───────────────────────
        if (root.contains("objectives") && root["objectives"].is_array()) {
            for (const auto& o : root["objectives"]) {
                if (!o.is_object()) continue;
                GamemodeObjectiveDefinition def;
                def.id = optString(o, "id", "");
                def.kind = optString(o, "kind", "");
                def.carrierTeam = optString(o, "carrier_team", "");
                def.siteGroup = optString(o, "site_group", "");
                def.plantSeconds = std::max(0.0f, optFloat(o, "plant_seconds", 0.0f));
                def.defuseSeconds = std::max(0.0f, optFloat(o, "defuse_seconds", 0.0f));
                def.explosionSeconds = std::max(0.0f, optFloat(o, "explosion_seconds", 0.0f));
                // Optional world-space pulse visual (client render reads this).
                if (o.contains("visual") && o["visual"].is_object()) {
                    const auto& v = o["visual"];
                    def.visual.enabled = optBool(v, "pulse", def.visual.enabled);
                    def.visual.radius = std::max(0.0f, optFloat(v, "radius", def.visual.radius));
                    def.visual.pulseAmplitude =
                        std::max(0.0f, optFloat(v, "pulse_amplitude", def.visual.pulseAmplitude));
                    def.visual.periodSeconds =
                        optFloat(v, "period_seconds", def.visual.periodSeconds) > 0.0f
                            ? optFloat(v, "period_seconds", def.visual.periodSeconds)
                            : def.visual.periodSeconds;
                    if (v.contains("color") && v["color"].is_array() && v["color"].size() >= 3) {
                        const auto& c = v["color"];
                        if (c[0].is_number()) def.visual.color.x = c[0].get<float>();
                        if (c[1].is_number()) def.visual.color.y = c[1].get<float>();
                        if (c[2].is_number()) def.visual.color.z = c[2].get<float>();
                        if (c.size() >= 4 && c[3].is_number()) def.visual.alpha = c[3].get<float>();
                    }
                }
                if (def.id.empty()) continue;
                next.objectives.push_back(std::move(def));
            }
        }

        // ── Round rules + victory (optional). ───────────────────────
        if (root.contains("rounds") && root["rounds"].is_object()) {
            const auto& r = root["rounds"];
            next.rounds.maxRounds = std::max(0, optInt(r, "max_rounds", 0));
            next.rounds.roundsToWin = std::max(0, optInt(r, "rounds_to_win", 0));
            next.rounds.endlessRounds = optBool(r, "endless_rounds", next.rounds.endlessRounds);
            next.rounds.roundSeconds = std::max(0.0f, optFloat(r, "round_seconds", 0.0f));
            next.rounds.freezeSeconds = std::max(0.0f, optFloat(r, "freeze_seconds", 0.0f));
            next.rounds.countdownSeconds = std::max(0.0f, optFloat(r, "countdown_seconds", 0.0f));
            next.rounds.intermissionSeconds = std::max(0.0f, optFloat(r, "intermission_seconds", 0.0f));
            next.rounds.resultsSeconds = std::max(0.0f, optFloat(r, "results_seconds", 0.0f));
        }
        if (root.contains("victory") && root["victory"].is_object())
            next.victoryCondition = optString(root["victory"], "type", next.victoryCondition);
        // Legacy alias. The nested victory.type is canonical when it is set;
        // an empty value falls back to victory_condition so the canonical
        // migration cannot silently drop a legacy-only mode.
        if (next.victoryCondition.empty())
            next.victoryCondition = optString(
                root, "victory_condition", next.victoryCondition);

        // ── Mode-wide avatar policy (optional). ─────────────────────
        if (root.contains("avatar") && root["avatar"].is_object()) {
            const auto& a = root["avatar"];
            next.avatar.configured = optBool(a, "enabled", true);
            next.avatar.forced = optBool(a, "forced", next.avatar.forced);
            next.avatar.name = optString(a, "name", next.avatar.name);
            next.avatar.group = optString(a, "group", next.avatar.group);
            if (a.contains("allowed") && a["allowed"].is_array()) {
                for (const auto& item : a["allowed"])
                    if (item.is_string()) next.avatar.allowed.push_back(item.get<std::string>());
            }
            if (a.contains("denied") && a["denied"].is_array()) {
                for (const auto& item : a["denied"])
                    if (item.is_string()) next.avatar.denied.push_back(item.get<std::string>());
            }
            applyAvatarGroup(next.avatar);
        }

        // ── Payload rules (optional). Map data owns the cart origin, route,
        // interaction box, and checkpoints; this block owns mode-wide values. ──
        if (root.contains("payload") && root["payload"].is_object()) {
            const auto& p = root["payload"];
            next.payload.configured = optBool(p, "enabled", true);
            next.payload.attackerTeam = optString(p, "attacker_team", next.payload.attackerTeam);
            next.payload.defenderTeam = optString(p, "defender_team", next.payload.defenderTeam);
            next.payload.overtimeSeconds = std::max(0.0f,
                optFloat(p, "overtime_seconds", next.payload.overtimeSeconds));
            next.payload.pushMetersPerSecondPerAttacker = std::max(0.0f,
                optFloat(p, "push_meters_per_second_per_attacker",
                         next.payload.pushMetersPerSecondPerAttacker));
            next.payload.maxPushAttackers = std::max(1,
                optInt(p, "max_push_attackers", next.payload.maxPushAttackers));
            next.payload.unattendedRollbackDelaySeconds = std::max(0.0f,
                optFloat(p, "unattended_rollback_delay_seconds",
                         next.payload.unattendedRollbackDelaySeconds));
            next.payload.rollbackMetersPerSecond = std::max(0.0f,
                optFloat(p, "rollback_meters_per_second",
                         next.payload.rollbackMetersPerSecond));
            next.payload.healHpPer10Ticks = std::max(0,
                optInt(p, "heal_hp_per_10_ticks", next.payload.healHpPer10Ticks));
            next.payload.healDefenders = optBool(p, "heal_defenders",
                                                next.payload.healDefenders);
            next.payload.spawnProtectionTicks = std::max(0,
                optInt(p, "spawn_protection_ticks", next.payload.spawnProtectionTicks));
            next.payload.teamChangeCooldownSeconds = std::max(0.0f,
                optFloat(p, "team_change_cooldown_seconds",
                         next.payload.teamChangeCooldownSeconds));
            next.payload.classPool = optStringArray(p, "class_pool");
        }

        // ── Presentation policy (optional). Missing keys keep ordinary
        // behavior; the actor preset refines these where both exist. ──
        if (root.contains("presentation") && root["presentation"].is_object()) {
            const auto& p = root["presentation"];
            auto readFlag = [&p](const char* key, bool& has, bool& value) {
                if (p.contains(key) && p[key].is_boolean()) {
                    has = true;
                    value = p[key].get<bool>();
                }
            };
            readFlag("damage_numbers", next.presentation.hasDamageNumbers, next.presentation.damageNumbers);
            readFlag("hit_effects", next.presentation.hasHitEffects, next.presentation.hitEffects);
            readFlag("world_impact_effects", next.presentation.hasWorldImpactEffects, next.presentation.worldImpactEffects);
            readFlag("world_impact_decals", next.presentation.hasWorldImpactDecals, next.presentation.worldImpactDecals);
            readFlag("world_hit_effects", next.presentation.hasWorldHitEffects, next.presentation.worldHitEffects);
            readFlag("hit_markers", next.presentation.hasHitMarkers, next.presentation.hitMarkers);
            readFlag("hit_sounds", next.presentation.hasHitSounds, next.presentation.hitSounds);
            readFlag("hit_flinch", next.presentation.hasHitFlinch, next.presentation.hitFlinch);
            readFlag("blood", next.presentation.hasBlood, next.presentation.blood);
            readFlag("killfeed", next.presentation.hasKillfeed, next.presentation.killfeed);
            readFlag("ragdolls", next.presentation.hasRagdolls, next.presentation.ragdolls);
            readFlag("enemy_healthbars", next.presentation.hasEnemyHealthbars, next.presentation.enemyHealthbars);
            readFlag("player_outlines", next.presentation.hasPlayerOutlines, next.presentation.playerOutlines);
            readFlag("friendly_outlines", next.presentation.hasFriendlyOutlines, next.presentation.friendlyOutlines);
            readFlag("player_wireframes", next.presentation.hasPlayerWireframes, next.presentation.playerWireframes);
            readFlag("friendly_wireframes", next.presentation.hasFriendlyWireframes, next.presentation.friendlyWireframes);
            if (p.contains("enemy_outline_alpha") && p["enemy_outline_alpha"].is_number()) {
                next.presentation.enemyOutlineAlpha = std::clamp(p["enemy_outline_alpha"].get<float>(), 0.0f, 1.0f);
                next.presentation.hasEnemyOutlineAlpha = true;
            }
            auto readScale = [&p](const char* key, float& value) {
                if (p.contains(key) && p[key].is_number())
                    value = std::clamp(p[key].get<float>(), 0.0f, 4.0f);
            };
            readScale("blood_scale", next.presentation.bloodScale);
            readScale("blood_lifetime_scale", next.presentation.bloodLifetimeScale);
            readScale("world_impact_decal_scale", next.presentation.worldImpactDecalScale);
            readScale("world_impact_decal_lifetime_scale", next.presentation.worldImpactDecalLifetimeScale);
            readScale("world_impact_decal_alpha_scale", next.presentation.worldImpactDecalAlphaScale);
            readScale("world_hit_effect_scale", next.presentation.worldHitEffectScale);
            readScale("world_hit_effect_lifetime_scale", next.presentation.worldHitEffectLifetimeScale);
            if (p.contains("friendly_outline_color") && p["friendly_outline_color"].is_array() && p["friendly_outline_color"].size() >= 3) {
                next.presentation.friendlyOutlineColor = {
                    p["friendly_outline_color"][0].get<float>(),
                    p["friendly_outline_color"][1].get<float>(),
                    p["friendly_outline_color"][2].get<float>()};
                next.presentation.hasTeamOutlineColors = true;
            }
            if (p.contains("enemy_outline_color") && p["enemy_outline_color"].is_array() && p["enemy_outline_color"].size() >= 3) {
                next.presentation.enemyOutlineColor = {
                    p["enemy_outline_color"][0].get<float>(),
                    p["enemy_outline_color"][1].get<float>(),
                    p["enemy_outline_color"][2].get<float>()};
                next.presentation.hasTeamOutlineColors = true;
            }
            if (p.contains("friendly_wireframe_color") && p["friendly_wireframe_color"].is_array() && p["friendly_wireframe_color"].size() >= 3) {
                next.presentation.friendlyWireframeColor = {
                    p["friendly_wireframe_color"][0].get<float>(),
                    p["friendly_wireframe_color"][1].get<float>(),
                    p["friendly_wireframe_color"][2].get<float>()};
                next.presentation.hasTeamWireframeColors = true;
            }
            if (p.contains("enemy_wireframe_color") && p["enemy_wireframe_color"].is_array() && p["enemy_wireframe_color"].size() >= 3) {
                next.presentation.enemyWireframeColor = {
                    p["enemy_wireframe_color"][0].get<float>(),
                    p["enemy_wireframe_color"][1].get<float>(),
                    p["enemy_wireframe_color"][2].get<float>()};
                next.presentation.hasTeamWireframeColors = true;
            }

            auto readTrail = [&p](const char* key, bool& has,
                                  GamemodeTrailSettings& out) {
                if (!p.contains(key) || !p[key].is_object()) return;
                const auto& t = p[key];
                bool any = false;
                auto readBool = [&](const char* k, bool& dst) {
                    if (t.contains(k) && t[k].is_boolean()) {
                        dst = t[k].get<bool>();
                        any = true;
                    }
                };
                auto readFloat = [&](const char* k, float& dst) {
                    if (t.contains(k) && t[k].is_number()) {
                        dst = t[k].get<float>();
                        any = true;
                    }
                };
                readBool("enabled", out.enabled);
                readFloat("minSpeed", out.minSpeed);
                readFloat("sampleIntervalSeconds", out.sampleIntervalSeconds);
                readFloat("lifetimeSeconds", out.lifetimeSeconds);
                readFloat("alpha", out.alpha);
                readFloat("width", out.width);
                if (t.contains("maxSamples") && t["maxSamples"].is_number_integer()) {
                    out.maxSamples = t["maxSamples"].get<int>();
                    any = true;
                }
                if (t.contains("color") && t["color"].is_array() &&
                    t["color"].size() >= 3) {
                    out.color = {t["color"][0].get<float>(),
                                 t["color"][1].get<float>(),
                                 t["color"][2].get<float>()};
                    any = true;
                }
                has = any;
            };
            readTrail("enemy_trail", next.presentation.hasEnemyTrail,
                      next.presentation.enemyTrail);
            readTrail("teammate_trail", next.presentation.hasTeammateTrail,
                      next.presentation.teammateTrail);
        }

        // ── Bomb Tag specific fields ─────────────────────────────────
        next.bombTimerTicks = std::max(60, optInt(root, "bomb_timer_ticks", next.bombTimerTicks));
        next.inactiveTicks = std::max(0, optInt(root, "inactive_ticks", next.inactiveTicks));
        next.blinkTicks = std::max(1, optInt(root, "blink_ticks", next.blinkTicks));
        next.maxPassSanityDistance = std::max(0.5f, optFloat(root, "max_pass_sanity_distance", next.maxPassSanityDistance));

        // ── Feature declarations ─────────────────────────────────────
        if (root.contains("features") && root["features"].is_object()) {
            const auto& f = root["features"];
            next.features.worldTimer = optBool(f, "world_timer", next.features.worldTimer);
            next.features.bombHolderText = optBool(f, "bomb_holder_text", next.features.bombHolderText);
            next.features.bombBlink = optBool(f, "bomb_blink", next.features.bombBlink);
            next.features.infiniteRounds = optBool(f, "infinite_rounds", next.features.infiniteRounds);
            next.features.noWeaponsExceptBomb = optBool(f, "no_weapons_except_bomb", next.features.noWeaponsExceptBomb);
            next.features.bossHealthbar = optBool(f, "boss_healthbar", next.features.bossHealthbar);
            next.features.worldText = optBool(f, "world_text", next.features.worldText);
            next.features.timerAboveEntity = optBool(f, "timer_above_entity", next.features.timerAboveEntity);
            next.features.percentAboveHead = optBool(f, "percent_above_head", next.features.percentAboveHead);
            next.features.fog = optBool(f, "fog", next.features.fog);
            next.features.rain = optBool(f, "rain", next.features.rain);
            next.features.torchLighting = optBool(f, "torch_lighting", next.features.torchLighting);
            next.features.freezeDuringCountdown =
                optBool(f, "freeze_during_countdown", next.features.freezeDuringCountdown);
            next.features.freezeDuringGo =
                optBool(f, "freeze_during_go", next.features.freezeDuringGo);
        }

        // ── Visual/settings overrides ───────────────────────────────
        next.cameraFov = std::max(0.0f, optFloat(root, "camera_fov", next.cameraFov));
        next.forceFirstPerson = optBool(root, "force_first_person", next.forceFirstPerson);
        next.hideHealthbars = optBool(root, "hide_healthbars", next.hideHealthbars);
        if (root.contains("ragdoll_enabled") && root["ragdoll_enabled"].is_boolean()) {
            next.ragdollExplicit = true;
            next.ragdollEnabled = root["ragdoll_enabled"].get<bool>();
        }
        if (root.contains("blood_enabled") && root["blood_enabled"].is_boolean()) {
            next.bloodExplicit = true;
            next.bloodEnabled = root["blood_enabled"].get<bool>();
        }
        // Nested presentation keys are the canonical mode policy. The legacy
        // root aliases remain supported, with an explicit nested value taking
        // precedence when both are present.
        if (next.presentation.hasRagdolls) {
            next.ragdollExplicit = true;
            next.ragdollEnabled = next.presentation.ragdolls;
        }
        if (next.presentation.hasBlood) {
            next.bloodExplicit = true;
            next.bloodEnabled = next.presentation.blood;
        }

        // ── Match role counts (optional) ────────────────────────────
        if (root.contains("roles") && root["roles"].is_object()) {
            for (auto it = root["roles"].begin(); it != root["roles"].end(); ++it) {
                if (!it.value().is_number()) continue;
                const int count = it.value().get<int>();
                if (count > 0)
                    next.roleCounts.push_back({it.key(), count});
            }
        }

        // ── Elimination / win rules (optional) ──────────────────────
        next.winCondition = optString(root, "win_condition", next.winCondition);
        next.actorPresetId = optString(root, "actor_preset", next.actorPresetId);
        next.movementPreset = optString(root, "movement_preset", next.movementPreset);
        next.npcBehaviorProfile = optString(
            root, "npc_behavior_profile", next.npcBehaviorProfile);

        // ── Mode-level NPC targeting (optional) ─────────────────────
        if (root.contains("npc_targeting") && root["npc_targeting"].is_object()) {
            const auto& t = root["npc_targeting"];
            next.npcTargeting.configured = optBool(t, "enabled", true);
            next.npcTargeting.mode = optString(t, "mode", next.npcTargeting.mode);
            next.npcTargeting.includePlayers =
                optBool(t, "include_players", next.npcTargeting.includePlayers);
            next.npcTargeting.includeNpcs =
                optBool(t, "include_npcs", next.npcTargeting.includeNpcs);
            bool modeOk = false;
            npcTargetingModeFromString(next.npcTargeting.mode, modeOk);
            if (!modeOk) {
                Debug::warn(Debug::Category::Duel,
                    "[GAMEMODE] %s npc_targeting.mode \"%s\" unknown; using closest\n",
                    next.id.c_str(), next.npcTargeting.mode.c_str());
                next.npcTargeting.mode = "closest";
            }
        }

        // ── Mode-level NPC travel weighting (optional) ──────────────
        if (root.contains("npc_travel") && root["npc_travel"].is_object()) {
            const auto& tw = root["npc_travel"];
            next.npcTravel.configured = optBool(tw, "enabled", true);
            next.npcTravel.objectiveBias = std::clamp(
                optFloat(tw, "objective_bias", next.npcTravel.objectiveBias), 0.0f, 5.0f);
            next.npcTravel.huntBias = std::clamp(
                optFloat(tw, "hunt_bias", next.npcTravel.huntBias), 0.0f, 5.0f);
            next.npcTravel.exploreBias = std::clamp(
                optFloat(tw, "explore_bias", next.npcTravel.exploreBias), 0.0f, 5.0f);
        }

        // ── Mode-level RMB aim-FOV override (optional) ──────────────
        if (root.contains("camera") && root["camera"].is_object() &&
            root["camera"].contains("aim_fov") && root["camera"]["aim_fov"].is_object()) {
            const auto& f = root["camera"]["aim_fov"];
            next.aimFov.enabled = optBool(f, "enabled", next.aimFov.enabled);
            next.aimFov.input = optString(f, "input", next.aimFov.input);
            next.aimFov.multiplier = std::clamp(
                optFloat(f, "multiplier", next.aimFov.multiplier), 0.05f, 1.0f);
            next.aimFov.duration = std::clamp(
                optFloat(f, "duration", next.aimFov.duration), 0.01f, 10.0f);
            next.aimFov.easing = optString(f, "easing", next.aimFov.easing);
        }
        next.waveStartCount = std::max(1, optInt(root, "wave_start_count", next.waveStartCount));
        next.waveIncrement = std::max(0, optInt(root, "wave_increment", next.waveIncrement));
        next.waveNpcsPerWave = std::max(0, optInt(root, "npcs_per_wave", next.waveNpcsPerWave));
        next.lives = std::max(1, optInt(root, "lives", next.lives));
        next.waveBannerSeconds = std::max(0.0f, optFloat(root, "wave_banner_seconds", next.waveBannerSeconds));
        next.waveStaggerEnabled = optBool(root, "wave_stagger_enabled", next.waveStaggerEnabled);
        next.waveNpcsPerTick = std::max(1, optInt(root, "wave_npcs_per_tick", next.waveNpcsPerTick));
        next.runAttempts = std::max(1, optInt(root, "run_attempts", next.runAttempts));
        next.allowMidrunTeamChange =
            optBool(root, "allow_midrun_team_change", next.allowMidrunTeamChange);
        next.runMode = optBool(root, "run_mode", next.runMode);
        if (root.contains("music") && root["music"].is_object()) {
            const auto& music = root["music"];
            next.musicOnGoTrack = optString(music, "on_go_track", next.musicOnGoTrack);
            next.musicRandomPool = optStringArray(music, "random_pool");
        }
        if (root.contains("npc_spawn") && root["npc_spawn"].is_object()) {
            const auto& s = root["npc_spawn"];
            next.npcSpawnPolicy = optString(s, "policy", next.npcSpawnPolicy);
            next.npcSpawnMax = std::max(0, optInt(s, "max_count", next.npcSpawnMax));
            next.npcSpawnIntervalTicks = std::max(
                1, optInt(s, "interval_ticks", next.npcSpawnIntervalTicks));
            next.npcSpawnPerInterval = std::max(
                1, optInt(s, "per_interval", next.npcSpawnPerInterval));
        }

        slot.mode = next;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE] Loaded %s: %s | goal=%d | time=%d | respawn=%.1fs | heal=%d | maps=%zu | fov=%.0f ragdoll=%d(%d) blood=%d(%d) win=%s | teams=%zu rounds_to_win=%d objectives=%zu\n",
            fileNameOf(path).c_str(), next.name.c_str(), next.goalValue,
            next.timeLimitSeconds, next.respawnSeconds, (int)next.killHeals, next.maps.size(),
            next.cameraFov, (int)next.ragdollEnabled, (int)next.ragdollExplicit,
            (int)next.bloodEnabled, (int)next.bloodExplicit,
            next.winCondition.empty() ? "default" : next.winCondition.c_str(),
            next.teams.size(), next.rounds.roundsToWin, next.objectives.size());
        return true;
    } catch (const json::parse_error& e) {
        reportInvalidFile(path, {std::string("json: parse error: ") + e.what()});
    } catch (const std::exception& e) {
        reportInvalidFile(path, {std::string("json: load error: ") + e.what()});
    }
    return false;
}

GamemodeRegistry::LoadedMode* GamemodeRegistry::findSlot(const std::string& id)
{
    for (auto& slot : modes_) {
        if (slot.mode.id == id)
            return &slot;
    }
    return nullptr;
}

const GamemodeRegistry::LoadedMode* GamemodeRegistry::findSlot(const std::string& id) const
{
    for (const auto& slot : modes_) {
        if (slot.mode.id == id)
            return &slot;
    }
    return nullptr;
}

const Gamemode& GamemodeRegistry::get(const std::string& id) const
{
    static const Gamemode fallback;
    const LoadedMode* slot = findSlot(id);
    if (slot) return slot->mode;
    Debug::warn(Debug::Category::Duel, "[GAMEMODE] Unknown gamemode \"%s\"; returning defaults.\n", id.c_str());
    return fallback;
}

bool GamemodeRegistry::has(const std::string& id) const
{
    return findSlot(id) != nullptr;
}

std::vector<std::string> GamemodeRegistry::ids() const
{
    std::vector<std::string> out;
    out.reserve(modes_.size());
    for (const auto& slot : modes_)
        out.push_back(slot.mode.id);
    return out;
}

void applyGamemodeToDuelConfig(DuelConfig& cfg, const Gamemode& gm)
{
    cfg.gamemodeId = gm.id;
    cfg.teamNames = gm.teamNames;
    cfg.killsToWin = gm.goalValue;
    cfg.duelLengthSeconds = gm.timeLimitSeconds;
    cfg.respawnDelaySeconds = gm.respawnSeconds;
    cfg.killHeals = gm.killHeals;
    cfg.countdownSeconds = gm.countdownSeconds;
    cfg.rematchSeconds = gm.rematchSeconds;
    cfg.spawnTracerSeconds = gm.spawnTracerSeconds;
    cfg.allowRematch = gm.allowRematch;
}
