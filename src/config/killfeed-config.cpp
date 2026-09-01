// 09 01 2026, 00 00
/* purpose
* Implements validated loading and hot reload for config/killfeed.json.
* Resolves mode, shared colors, verbs, and weapon presentation overrides.
* Preserves the last valid configuration when a reload fails.
* DOES NOT render killfeed entries or append chat messages.
* DOES NOT own combat, damage, scoring, or network state.
* DOES NOT grant rewards or decide kill authority.
*/
#include "killfeed-config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {
int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

void readColor(const json& root, const char* key, glm::vec4& out)
{
    const auto it = root.find(key);
    if (it == root.end() || !it->is_array() || it->size() < 4) return;
    out.r = std::clamp((*it)[0].get<float>(), 0.0f, 1.0f);
    out.g = std::clamp((*it)[1].get<float>(), 0.0f, 1.0f);
    out.b = std::clamp((*it)[2].get<float>(), 0.0f, 1.0f);
    out.a = std::clamp((*it)[3].get<float>(), 0.0f, 1.0f);
}
}

KillfeedConfig& KillfeedConfig::instance()
{
    static KillfeedConfig config;
    return config;
}

bool KillfeedConfig::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Gui, "[KILLFEED CONFIG] open failed path=%s\n", path.c_str());
        return false;
    }

    try {
        json root;
        file >> root;
        KillfeedConfigData loaded;

        const std::string requestedMode = root.value("mode", std::string("hud"));
        if (requestedMode == "chat" || requestedMode == "hud")
            loaded.mode = requestedMode;
        else
            Debug::warn(Debug::Category::Gui,
                        "[KILLFEED CONFIG] invalid mode=%s; using hud\n",
                        requestedMode.c_str());

        loaded.defaultKillVerb = root.value("defaultKillVerb",
                                             root.value("defaultKillVerbs", std::string("killed")));
        loaded.showTick = root.value("showTick", true);
        loaded.tickPrefix = root.value("tickPrefix", std::string("TICK"));
        readColor(root.value("colors", json::object()), "killer", loaded.killerColor);
        readColor(root.value("colors", json::object()), "victim", loaded.victimColor);
        readColor(root.value("colors", json::object()), "verb", loaded.verbColor);
        readColor(root.value("colors", json::object()), "weapon", loaded.weaponColor);
        readColor(root.value("colors", json::object()), "distance", loaded.distanceColor);

        if (root.contains("verbs") && root["verbs"].is_object()) {
            for (const auto& [key, value] : root["verbs"].items()) {
                if (!value.is_object()) continue;
                KillfeedVerbConfig verb;
                verb.text = value.value("text", key);
                readColor(value, "color", verb.color);
                loaded.verbs[key] = std::move(verb);
            }
        }

        if (root.contains("weapons") && root["weapons"].is_object()) {
            for (const auto& [key, value] : root["weapons"].items()) {
                if (!value.is_object()) continue;
                KillfeedWeaponConfig weapon;
                weapon.displayName = value.value("displayName", key);
                weapon.killVerb = value.value("killVerb", std::string());
                readColor(value, "color", weapon.color);
                loaded.weapons[key] = std::move(weapon);
            }
        }

        mData = std::move(loaded);
        mPath = path;
        mLastModified = modifiedTime(path);
        Debug::log(Debug::Category::Gui,
                   "[KILLFEED CONFIG] loaded mode=%s verbs=%zu weapons=%zu\n",
                   mData.mode.c_str(), mData.verbs.size(), mData.weapons.size());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Gui,
                    "[KILLFEED CONFIG] parse failed path=%s reason=%s\n",
                    path.c_str(), e.what());
        return false;
    }
}

bool KillfeedConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath.empty() ? "config/killfeed.json" : mPath);
    if (current == 0 || current == mLastModified) return false;
    return load(mPath.empty() ? "config/killfeed.json" : mPath);
}
