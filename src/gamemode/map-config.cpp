// 10 02 2026
// Per-map objective config: bomb sites + mode objective timers.
#include "gamemode/map-config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <cctype>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"
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

bool mapEntityContainsPoint(const MapEntity& entity, const glm::vec3& point)
{
    const glm::vec3 d = point - entity.position;
    const bool hasBox = entity.size.x > 1.001f || entity.size.y > 1.001f ||
                        entity.size.z > 1.001f;
    // Explicit shape wins; otherwise infer (sized volume -> box, else sphere).
    const bool useSphere = entity.shape == "sphere" ||
                           (entity.shape.empty() && !hasBox);
    const bool useBox = entity.shape == "box" ||
                        (entity.shape.empty() && hasBox);
    if (useBox) {
        const glm::vec3 half(std::max(entity.size.x, 0.0f) * 0.5f,
                             std::max(entity.size.y, 0.0f) * 0.5f,
                             std::max(entity.size.z, 0.0f) * 0.5f);
        if (std::abs(d.x) <= half.x && std::abs(d.y) <= half.y &&
            std::abs(d.z) <= half.z)
            return true;
    }
    if (useSphere)
        return glm::dot(d, d) <= entity.radius * entity.radius;
    return false;
}

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
    const std::string nextPath = resolveUnderConfig(pathForMap(mapId));

    std::ifstream file(nextPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] no config file for map=%s (path=%s)\n",
            mapId.c_str(), nextPath.c_str());
        if (mCurrent.mapId != mapId) {
            mCurrent = MapObjectiveConfig{};
            mCurrent.mapId = mapId;
            mPath = nextPath;
            mLastWrite = {};
        }
        return false;
    }

    try {
        const json root = parseJsonConfig(file);
        MapObjectiveConfig next;
        next.mapId = mapId;
        next.loaded = true;

        if (root.contains("bomb") && root["bomb"].is_object()) {
            const auto& bomb = root["bomb"];
            if (bomb.contains("plant_seconds") && bomb["plant_seconds"].is_number())
                next.plantSeconds = std::max(0.0f, bomb["plant_seconds"].get<float>());
            if (bomb.contains("defuse_seconds") && bomb["defuse_seconds"].is_number())
                next.defuseSeconds = std::max(0.0f, bomb["defuse_seconds"].get<float>());
            if (bomb.contains("explosion_seconds") && bomb["explosion_seconds"].is_number())
                next.explosionSeconds = std::max(0.0f, bomb["explosion_seconds"].get<float>());
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
                next.bombSites.push_back(std::move(site));
            }
        }

        if (root.contains("entities") && root["entities"].is_array()) {
            for (const auto& item : root["entities"]) {
                if (!item.is_object()) continue;
                MapEntity entity;
                entity.id = item.value("id", std::string{});
                entity.type = item.value("type", std::string{});
                if (entity.id.empty() || entity.type.empty()) continue;
                if (item.contains("position") && item["position"].is_array() && item["position"].size() >= 3)
                    entity.position = glm::vec3(item["position"][0].get<float>(), item["position"][1].get<float>(), item["position"][2].get<float>());
                if (item.contains("size") && item["size"].is_array() && item["size"].size() >= 3)
                    entity.size = glm::vec3(item["size"][0].get<float>(), item["size"][1].get<float>(), item["size"][2].get<float>());
                entity.radius = std::max(0.1f, item.value("radius", entity.radius));
                entity.shape = item.value("shape", std::string{});
                entity.enabled = item.value("enabled", entity.enabled);
                entity.visible = item.value("visible", entity.visible);
                entity.oneShot = item.value("oneShot", item.value("one_shot", entity.oneShot));
                entity.tag = item.value("tag", std::string{});
                entity.monsterPool = item.value("monsterPool", item.value("monster_pool", entity.monsterPool));
                entity.monsterRole = item.value("monsterRole", item.value("monster_role", std::string{}));
                entity.pickupId = item.value("pickupId", item.value("pickup_id", std::string{}));
                entity.bossId = item.value("bossId", item.value("boss_id", std::string{}));
                entity.damageType = item.value("damageType", item.value("damage_type", entity.damageType));
                entity.spawnCount = std::max(1, item.value("spawnCount", item.value("spawn_count", entity.spawnCount)));
                entity.maxAlive = std::max(1, item.value("maxAlive", item.value("max_alive", entity.maxAlive)));
                entity.damage = std::max(0, item.value("damage", entity.damage));
                entity.damageIntervalTicks = std::max(1, item.value("damageIntervalTicks", item.value("damage_interval_ticks", entity.damageIntervalTicks)));
                entity.spawnCooldownTicks = std::max(1, item.value("spawnCooldownTicks", item.value("spawn_cooldown_ticks", entity.spawnCooldownTicks)));
                entity.checkpointRequirement = std::max(0, item.value("checkpointRequirement", item.value("checkpoint_requirement", entity.checkpointRequirement)));
                entity.checkpointIndex = std::max(0, item.value("checkpointIndex", item.value("checkpoint_index", entity.checkpointIndex)));
                next.entities.push_back(std::move(entity));
            }
        }

        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] loaded map=%s sites=%zu entities=%zu plant=%.1f defuse=%.1f explode=%.1f\n",
            mapId.c_str(), next.bombSites.size(), next.entities.size(),
            next.plantSeconds, next.defuseSeconds, next.explosionSeconds);
        mCurrent = std::move(next);
        mPath = nextPath;
        mLastWrite = getLastWrite(mPath);
        mEntityVisibility = std::any_of(
            mCurrent.entities.begin(), mCurrent.entities.end(),
            [](const MapEntity& entity) { return entity.visible; });
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-entity.loaded", mapId, "valid map config applied", 0,
            nlohmann::json{{"map", mapId}, {"entity_count", mCurrent.entities.size()}, {"config_path", mPath}, {"result", "success"}},
            __FILE__, __LINE__, __FUNCTION__);
        return true;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel,
            "[MAP CONFIG] parse error in %s: %s; keeping last valid state\n",
            nextPath.c_str(), e.what());
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "map-entity.reload-failed", mapId, "invalid JSON; last valid state retained", 0,
            nlohmann::json{{"map", mapId}, {"config_path", nextPath}, {"result", "retained"}, {"reason", e.what()}},
            __FILE__, __LINE__, __FUNCTION__);
        if (mCurrent.mapId == mapId)
            mLastWrite = getLastWrite(nextPath);
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

const MapEntity* MapConfigRegistry::findEntity(const std::string& id) const
{
    for (const auto& entity : mCurrent.entities)
        if (entity.id == id) return &entity;
    return nullptr;
}

MapEntity* MapConfigRegistry::findEntityMutable(const std::string& id)
{
    for (auto& entity : mCurrent.entities)
        if (entity.id == id) return &entity;
    return nullptr;
}

std::string MapConfigRegistry::createEntity(const std::string& type, const glm::vec3& position,
                                            const std::string& requestedId)
{
    std::string id = requestedId;
    if (id.empty()) {
        std::string stem = type;
        for (char& c : stem) if (c == ' ') c = '_';
        for (int i = 1;; ++i) {
            id = stem + "_" + std::to_string(i);
            if (!findEntity(id)) break;
        }
    }
    if (findEntity(id)) return {};
    MapEntity entity;
    entity.id = id;
    entity.type = type;
    entity.position = position;
    mCurrent.entities.push_back(std::move(entity));
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-entity.created", id, "terminal authoring", 0,
        nlohmann::json{{"map", mCurrent.mapId}, {"entity_id", id}, {"entity_type", type},
                       {"position", {position.x, position.y, position.z}}, {"source", "terminal"}},
        __FILE__, __LINE__, __FUNCTION__);
    return id;
}

bool MapConfigRegistry::deleteEntity(const std::string& id)
{
    const auto it = std::remove_if(mCurrent.entities.begin(), mCurrent.entities.end(),
        [&](const MapEntity& entity) { return entity.id == id; });
    if (it == mCurrent.entities.end()) return false;
    mCurrent.entities.erase(it, mCurrent.entities.end());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-entity.deleted", id, "terminal authoring", 0,
        nlohmann::json{{"map", mCurrent.mapId}, {"entity_id", id}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

void MapConfigRegistry::setEntityVisibility(bool visible)
{
    mEntityVisibility = visible;
    for (auto& entity : mCurrent.entities) entity.visible = visible;
}

void MapConfigRegistry::setEntityVisibilityFor(const std::string& id, bool visible)
{
    if (auto* entity = findEntityMutable(id)) entity->visible = visible;
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
    // Start from the existing authored file so any top-level keys this owner
    // does not understand (objectives, future sections, hand-authored notes)
    // survive a save. Only the three sections below are owned and rewritten.
    json root = json::object();
    {
        std::ifstream existing(mPath);
        if (existing.is_open()) {
            try {
                json parsed = parseJsonConfig(existing);
                if (parsed.is_object()) root = std::move(parsed);
            } catch (const std::exception&) {
                // A malformed on-disk file is replaced by the in-memory state.
            }
        }
    }
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
    root["entities"] = json::array();
    for (const auto& entity : mCurrent.entities) {
        root["entities"].push_back({
            {"id", entity.id}, {"type", entity.type},
            {"position", {entity.position.x, entity.position.y, entity.position.z}},
            {"size", {entity.size.x, entity.size.y, entity.size.z}},
            {"radius", entity.radius}, {"shape", entity.shape},
            {"enabled", entity.enabled}, {"visible", entity.visible},
            {"oneShot", entity.oneShot}, {"tag", entity.tag}, {"monsterPool", entity.monsterPool},
            {"monsterRole", entity.monsterRole},
            {"pickupId", entity.pickupId}, {"bossId", entity.bossId}, {"damageType", entity.damageType},
            {"spawnCount", entity.spawnCount}, {"maxAlive", entity.maxAlive}, {"damage", entity.damage},
            {"damageIntervalTicks", entity.damageIntervalTicks},
            {"spawnCooldownTicks", entity.spawnCooldownTicks},
            {"checkpointRequirement", entity.checkpointRequirement},
            {"checkpointIndex", entity.checkpointIndex}
        });
    }

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
    Debug::warn(Debug::Category::Duel, "[MAP CONFIG] saved %s entities=%zu\n", mPath.c_str(), mCurrent.entities.size());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-entity.saved", mCurrent.mapId, "terminal authoring", 0,
        nlohmann::json{{"map", mCurrent.mapId}, {"entity_count", mCurrent.entities.size()}, {"config_path", mPath}, {"result", "success"}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

bool mapConfigSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Load the real Counter-Strike map config if present.
    MapConfigRegistry& reg = MapConfigRegistry::instance();
    const bool loaded = reg.load("dust2cyberiav4");
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

bool mapEntityConfigSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Trigger geometry: sphere radius and box size.
    MapEntity sphere;
    sphere.type = "checkpoint";
    sphere.position = glm::vec3(10.0f, 0.0f, 0.0f);
    sphere.radius = 2.0f;
    if (!mapEntityContainsPoint(sphere, glm::vec3(10.0f, 0.0f, 0.0f)))
        fail("sphere center should be inside");
    if (!mapEntityContainsPoint(sphere, glm::vec3(11.5f, 0.0f, 0.0f)))
        fail("point within sphere radius should be inside");
    if (mapEntityContainsPoint(sphere, glm::vec3(13.0f, 0.0f, 0.0f)))
        fail("point outside sphere radius should be outside");

    MapEntity box;
    box.type = "damage_volume";
    box.position = glm::vec3(0.0f);
    box.radius = 0.1f;
    box.size = glm::vec3(4.0f, 2.0f, 2.0f);
    if (!mapEntityContainsPoint(box, glm::vec3(1.9f, 0.9f, 0.9f)))
        fail("point inside box should be inside");
    if (mapEntityContainsPoint(box, glm::vec3(2.5f, 0.0f, 0.0f)))
        fail("point outside box should be outside");

    // Save round-trip must preserve unknown top-level keys.
    MapConfigRegistry& reg = MapConfigRegistry::instance();
    const std::string mapId = "zz_entity_selftest";
    const std::string path = MapConfigRegistry::pathForMap(mapId);
    {
        std::ofstream out(path);
        out << R"({"custom_section":{"note":"keep me"},"entities":[{"id":"cp1","type":"checkpoint","position":[1,2,3],"radius":5}]})";
    }
    if (!reg.load(mapId))
        fail("selftest map failed to load");
    if (!reg.save())
        fail("selftest map failed to save");
    {
        std::ifstream in(path);
        if (!in.is_open()) {
            fail("saved selftest map missing");
        } else {
            try {
                const json root = parseJsonConfig(in);
                if (!root.contains("custom_section"))
                    fail("save dropped unknown top-level key");
                if (!root.contains("entities") || root["entities"].size() != 1)
                    fail("entities not preserved across save");
            } catch (const std::exception& e) {
                fail(std::string("saved json unparsable: ") + e.what());
            }
        }
    }

    // Live reload: an external JSON edit must be picked up by pollReload().
    {
        std::ofstream out(path);
        out << R"({"custom_section":{"note":"keep me"},"entities":[{"id":"cp1","type":"checkpoint","position":[9,9,9],"radius":5}]})";
    }
    {
        // Force a distinct modification time so the mtime poll cannot miss it.
        auto t = std::filesystem::last_write_time(path);
        std::filesystem::last_write_time(path, t + std::chrono::seconds(5));
    }
    if (!reg.pollReload()) {
        fail("pollReload did not detect an external JSON edit");
    } else {
        const MapEntity* cp = reg.findEntity("cp1");
        if (!cp) {
            fail("reloaded entity cp1 missing");
        } else if (std::abs(cp->position.x - 9.0f) > 0.01f ||
                   std::abs(cp->position.y - 9.0f) > 0.01f ||
                   std::abs(cp->position.z - 9.0f) > 0.01f) {
            fail("reloaded checkpoint position not updated from JSON");
        }
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    reg.load("zombietower4");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
