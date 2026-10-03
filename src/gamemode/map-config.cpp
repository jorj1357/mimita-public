// 10 02 2026
// Per-map objective config: bomb sites + mode objective timers.
#include "gamemode/map-config.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

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

// Resolve config/maps/<file>.json across candidate roots (cwd + exe parents),
// the same discovery pattern used for actor presets.
std::string resolveUnderConfig(const std::string& relative)
{
    std::vector<std::filesystem::path> candidates;
    const std::filesystem::path requested(relative);
    candidates.push_back(requested);
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) candidates.push_back(cwd / requested);
    std::filesystem::path exeDir(getExecutableDirectory());
    for (int level = 0; level < 6 && !exeDir.empty(); ++level) {
        candidates.push_back(exeDir / requested);
        const auto parent = exeDir.parent_path();
        if (parent == exeDir) break;
        exeDir = parent;
    }
    for (const auto& candidate : candidates) {
        std::error_code cec;
        if (std::filesystem::exists(candidate, cec) && !cec)
            return candidate.string();
    }
    return requested.string();
}

} // namespace

MapConfigRegistry& MapConfigRegistry::instance()
{
    static MapConfigRegistry registry;
    return registry;
}

std::string MapConfigRegistry::pathForMap(const std::string& mapId)
{
    return "config/maps/" + mapId + ".json";
}

std::string MapConfigRegistry::resolveConfigPath() const
{
    return resolveUnderConfig(pathForMap(mCurrent.mapId));
}

bool MapConfigRegistry::load(const std::string& mapId)
{
    mCurrent = MapObjectiveConfig{};
    mCurrent.mapId = mapId;
    mPath = resolveUnderConfig(pathForMap(mapId));
    mLastWrite = getLastWrite(mPath);

    std::ifstream file(mPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] no objective file for map=%s (path=%s); planting disabled\n",
            mapId.c_str(), mPath.c_str());
        return false;
    }

    try {
        const json root = parseJsonConfig(file);
        mCurrent.loaded = true;

        if (root.contains("bomb") && root["bomb"].is_object()) {
            const auto& bomb = root["bomb"];
            if (bomb.contains("plant_seconds") && bomb["plant_seconds"].is_number())
                mCurrent.plantSeconds = std::max(0.0f, bomb["plant_seconds"].get<float>());
            if (bomb.contains("defuse_seconds") && bomb["defuse_seconds"].is_number())
                mCurrent.defuseSeconds = std::max(0.0f, bomb["defuse_seconds"].get<float>());
            if (bomb.contains("explosion_seconds") && bomb["explosion_seconds"].is_number())
                mCurrent.explosionSeconds = std::max(0.0f, bomb["explosion_seconds"].get<float>());
        }

        // Accept both the plan's nested {"objectives":{"bomb_sites":[...]}} and a
        // flat top-level "bomb_sites":[...] shape.
        const json* sites = nullptr;
        if (root.contains("objectives") && root["objectives"].is_object() &&
            root["objectives"].contains("bomb_sites") &&
            root["objectives"]["bomb_sites"].is_array())
            sites = &root["objectives"]["bomb_sites"];
        else if (root.contains("bomb_sites") && root["bomb_sites"].is_array())
            sites = &root["bomb_sites"];

        if (sites) {
            for (const auto& item : *sites) {
                if (!item.is_object()) continue;
                BombSite site;
                site.id = item.value("id", std::string{});
                if (site.id.empty()) continue;
                site.radius = std::max(0.5f, item.value("radius", site.radius));
                site.visibleDebug = item.value("visible_debug", false);
                if (item.contains("position") && item["position"].is_array() &&
                    item["position"].size() >= 3) {
                    site.position = glm::vec3(
                        item["position"][0].get<float>(),
                        item["position"][1].get<float>(),
                        item["position"][2].get<float>());
                    site.hasPosition = true;
                }
                mCurrent.bombSites.push_back(std::move(site));
            }
        }

        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] loaded map=%s sites=%zu plant=%.1f defuse=%.1f explode=%.1f\n",
            mapId.c_str(), mCurrent.bombSites.size(),
            mCurrent.plantSeconds, mCurrent.defuseSeconds, mCurrent.explosionSeconds);
        return true;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel,
            "[MAP CONFIG] parse error in %s: %s; keeping defaults\n",
            mPath.c_str(), e.what());
        return false;
    }
}

bool MapConfigRegistry::pollReload()
{
    if (mPath.empty() || mCurrent.mapId.empty()) return false;
    const auto write = getLastWrite(mPath);
    if (write == std::filesystem::file_time_type{} || write == mLastWrite)
        return false;
    Debug::warn(Debug::Category::Duel,
        "[MAP CONFIG] detected change: %s\n", mPath.c_str());
    return load(mCurrent.mapId);
}

const BombSite* MapConfigRegistry::findSite(const std::string& id) const
{
    for (const auto& site : mCurrent.bombSites)
        if (site.id == id) return &site;
    return nullptr;
}

int MapConfigRegistry::siteIndexAt(const glm::vec3& position) const
{
    for (size_t i = 0; i < mCurrent.bombSites.size(); ++i) {
        const BombSite& site = mCurrent.bombSites[i];
        if (!site.hasPosition) continue;
        const glm::vec3 d = position - site.position;
        const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
        if (distSq <= site.radius * site.radius) return (int)i;
    }
    return -1;
}

bool MapConfigRegistry::setSitePosition(const std::string& id, const glm::vec3& position)
{
    for (auto& site : mCurrent.bombSites) {
        if (site.id != id) continue;
        site.position = position;
        site.hasPosition = true;
        return true;
    }
    return false;
}

bool MapConfigRegistry::setSiteVisibility(const std::string& id, bool visible)
{
    for (auto& site : mCurrent.bombSites) {
        if (site.id != id) continue;
        site.visibleDebug = visible;
        return true;
    }
    return false;
}

bool MapConfigRegistry::save()
{
    if (mPath.empty()) return false;
    json root;
    root["bomb_sites"] = json::array();
    for (const auto& site : mCurrent.bombSites) {
        json s;
        s["id"] = site.id;
        s["position"] = {site.position.x, site.position.y, site.position.z};
        s["radius"] = site.radius;
        s["visible_debug"] = site.visibleDebug;
        root["bomb_sites"].push_back(std::move(s));
    }
    root["bomb"] = {
        {"plant_seconds", mCurrent.plantSeconds},
        {"defuse_seconds", mCurrent.defuseSeconds},
        {"explosion_seconds", mCurrent.explosionSeconds},
    };

    // Preserve the directory if it already exists; create it otherwise.
    std::error_code ec;
    const std::filesystem::path p(mPath);
    if (!p.parent_path().empty())
        std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream out(mPath);
    if (!out.is_open()) {
        Debug::error(Debug::Category::Duel, "[MAP CONFIG] cannot write %s\n", mPath.c_str());
        return false;
    }
    out << root.dump(2) << "\n";
    mLastWrite = getLastWrite(mPath);
    Debug::warn(Debug::Category::Duel, "[MAP CONFIG] saved %s\n", mPath.c_str());
    return true;
}

bool mapConfigSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Load the real Counter-Strike map config if present.
    MapConfigRegistry& reg = MapConfigRegistry::instance();
    const bool loaded = reg.load("dust2cyberiav3");
    report += "loaded=" + std::string(loaded ? "yes" : "no") +
              " sites=" + std::to_string(reg.current().bombSites.size()) + "\n";

    if (loaded) {
        const auto& cfg = reg.current();
        if (cfg.plantSeconds <= 0.0f) fail("plant_seconds should be > 0");
        if (cfg.defuseSeconds <= 0.0f) fail("defuse_seconds should be > 0");
        if (cfg.explosionSeconds <= 0.0f) fail("explosion_seconds should be > 0");
        // sites may legitimately be empty until authored against the map.
    }

    // siteIndexAt should honor radius and hasPosition.
    MapConfigRegistry::instance();
    const auto& sites = reg.current().bombSites;
    int checked = 0;
    for (const auto& site : sites) {
        if (!site.hasPosition) continue;
        if (reg.siteIndexAt(site.position) < 0) fail("site position should be inside itself");
        const glm::vec3 outsidePoint = site.position + glm::vec3(0, 0, site.radius + 5.0f);
        if (reg.siteIndexAt(outsidePoint) >= 0 && sites.size() > 1) {
            // Only a failure if the far point is outside every site.
            bool insideAnother = false;
            for (const auto& other : sites) {
                if (!other.hasPosition) continue;
                const glm::vec3 d = outsidePoint - other.position;
                if (d.x * d.x + d.y * d.y + d.z * d.z <= other.radius * other.radius) {
                    insideAnother = true; break;
                }
            }
            if (!insideAnother) fail("point outside all sites should not resolve");
        }
        ++checked;
    }
    report += "sites_checked=" + std::to_string(checked) + "\n";

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
