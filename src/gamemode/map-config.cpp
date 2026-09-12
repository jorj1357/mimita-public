// 09 11 2026
/* purpose
* Loads, caches, and hot-reloads per-map team spawns and bomb sites.
* Data-driven: adding a map JSON adds team spawns and plant zones with no code.
* Does NOT read geometry or render; only returns stored gameplay metadata.
* Does NOT fail hard on bad JSON - keeps the last valid config and logs an error.
*/

#include "gamemode/map-config.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string sanitizeId(const std::string& mapId)
{
    std::string out = mapId;
    for (char& c : out)
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    return out;
}

glm::vec3 readVec3(const json& j)
{
    glm::vec3 v(0.0f);
    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        v.x = j["position"][0].get<float>();
        v.y = j["position"][1].get<float>();
        v.z = j["position"][2].get<float>();
    } else {
        v.x = j.value("x", 0.0f);
        v.y = j.value("y", 0.0f);
        v.z = j.value("z", 0.0f);
    }
    return v;
}

void readSpawns(const json& arr, std::vector<MapTeamSpawn>& out)
{
    if (!arr.is_array()) return;
    for (const auto& item : arr) {
        if (!item.is_object()) continue;
        MapTeamSpawn sp;
        sp.position = readVec3(item);
        sp.yaw = item.value("yaw", 0.0f);
        out.push_back(sp);
    }
}

} // namespace

MapConfigRegistry& MapConfigRegistry::instance()
{
    static MapConfigRegistry registry;
    return registry;
}

const MapConfig& MapConfigRegistry::get(const std::string& mapId)
{
    static MapConfig empty;
    if (mapId.empty())
        return empty;

    auto it = mEntries.find(mapId);
    if (it != mEntries.end())
        return it->second.config;

    Entry entry;
    load(mapId, entry);
    auto inserted = mEntries.emplace(mapId, std::move(entry)).first;
    return inserted->second.config;
}

bool MapConfigRegistry::load(const std::string& mapId, Entry& entry)
{
    const std::string path = "config/maps/" + sanitizeId(mapId) + ".json";
    entry.path = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        // Missing per-map config is normal for maps without objective data.
        Debug::log(Debug::Category::Duel,
            "[MAP CONFIG] no per-map config for \"%s\" (%s)\n", mapId.c_str(), path.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        MapConfig next;
        next.id = root.value("id", mapId);
        next.loaded = true;
        if (root.contains("team_spawns") && root["team_spawns"].is_object()) {
            const auto& ts = root["team_spawns"];
            readSpawns(ts.value("T", json::array()), next.teamSpawns[0]);
            readSpawns(ts.value("CT", json::array()), next.teamSpawns[1]);
        }
        if (root.contains("bomb_sites") && root["bomb_sites"].is_array()) {
            for (const auto& item : root["bomb_sites"]) {
                if (!item.is_object()) continue;
                MapBombSite site;
                site.name = item.value("name", "");
                site.center = readVec3(item);
                site.radius = std::max(1.0f, item.value("radius", 6.0f));
                next.bombSites.push_back(std::move(site));
            }
        }
        if (root.contains("bomb") && root["bomb"].is_object()) {
            const auto& b = root["bomb"];
            next.bombTimerSeconds = std::max(1.0f, b.value("timer_seconds", next.bombTimerSeconds));
            next.bombPlantSeconds = std::max(0.0f, b.value("plant_seconds", next.bombPlantSeconds));
            next.bombDefuseSeconds = std::max(0.0f, b.value("defuse_seconds", next.bombDefuseSeconds));
        }

        entry.config = std::move(next);
        entry.write = getLastWrite(path);
        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] loaded map=%s T=%zu CT=%zu sites=%zu\n",
            entry.config.id.c_str(), entry.config.teamSpawns[0].size(),
            entry.config.teamSpawns[1].size(), entry.config.bombSites.size());
        return true;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel,
            "[MAP CONFIG] parse error in %s: %s. Keeping defaults.\n", path.c_str(), e.what());
    }
    return false;
}

bool MapConfigRegistry::pollReload()
{
    bool changed = false;
    for (auto& kv : mEntries) {
        Entry& entry = kv.second;
        if (entry.path.empty())
            continue;
        const auto write = getLastWrite(entry.path);
        if (write == std::filesystem::file_time_type{} || write == entry.write)
            continue;
        Entry next;
        if (load(kv.first, next)) {
            entry.config = std::move(next.config);
            entry.write = write;
            changed = true;
        } else {
            entry.write = write;  // stop retrying until the file changes again
        }
    }
    return changed;
}
