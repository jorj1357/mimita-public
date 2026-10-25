// 10 02 2026
// Per-map objective config: bomb sites + mode objective timers.
#include "gamemode/map-config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <cctype>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "gamemode/gamemode.h"
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
    if (hasBox) {
        const glm::vec3 half(std::max(entity.size.x, 0.0f) * 0.5f,
                             std::max(entity.size.y, 0.0f) * 0.5f,
                             std::max(entity.size.z, 0.0f) * 0.5f);
        if (std::abs(d.x) <= half.x && std::abs(d.y) <= half.y &&
            std::abs(d.z) <= half.z)
            return true;
    }
    return glm::dot(d, d) <= entity.radius * entity.radius;
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
                entity.yaw = item.value("yaw", entity.yaw);
                entity.radius = std::max(0.1f, item.value("radius", entity.radius));
                entity.enabled = item.value("enabled", entity.enabled);
                entity.visible = item.value("visible", entity.visible);
                entity.oneShot = item.value("oneShot", item.value("one_shot", entity.oneShot));
                entity.tag = item.value("tag", std::string{});
                entity.monsterPool = item.value("monsterPool", item.value("monster_pool", entity.monsterPool));
                entity.pickupId = item.value("pickupId", item.value("pickup_id", std::string{}));
                entity.bossId = item.value("bossId", item.value("boss_id", std::string{}));
                entity.damageType = item.value("damageType", item.value("damage_type", entity.damageType));
                entity.spawnCount = std::max(1, item.value("spawnCount", item.value("spawn_count", entity.spawnCount)));
                entity.maxAlive = std::max(1, item.value("maxAlive", item.value("max_alive", entity.maxAlive)));
                entity.damage = std::max(0, item.value("damage", entity.damage));
                entity.damageIntervalTicks = std::max(1, item.value("damageIntervalTicks", item.value("damage_interval_ticks", entity.damageIntervalTicks)));
                entity.spawnCooldownTicks = std::max(1, item.value("spawnCooldownTicks", item.value("spawn_cooldown_ticks", entity.spawnCooldownTicks)));
                entity.checkpointRequirement = std::max(0, item.value("checkpointRequirement", item.value("checkpoint_requirement", entity.checkpointRequirement)));
                entity.order = std::max(0, item.value("order", entity.order));
                entity.required = item.value("required", entity.required);
                entity.activationRadius = std::max(0.0f, item.value("activationRadius", item.value("activation_radius", entity.activationRadius)));
                entity.healthMultiplier = std::max(0.0f, item.value("healthMultiplier", item.value("health_multiplier", entity.healthMultiplier)));
                // Zombie Tower encounter volumes are one-shot unless explicitly
                // authored as repeatable.
                if (entity.type == "monster_zone" &&
                    !item.contains("oneShot") && !item.contains("one_shot"))
                    entity.oneShot = true;
                next.entities.push_back(std::move(entity));
            }
        }

        if (root.contains("blocks") && root["blocks"].is_array()) {
            for (const auto& item : root["blocks"]) {
                if (!item.is_object()) continue;
                AuthoredBlock block;
                block.id = item.value("id", std::string{});
                if (block.id.empty()) continue;
                block.shape = item.value("shape", std::string{"box"});
                if (item.contains("position") && item["position"].is_array() &&
                    item["position"].size() >= 3) {
                    block.position = glm::vec3(
                        item["position"][0].get<float>(),
                        item["position"][1].get<float>(),
                        item["position"][2].get<float>());
                }
                if (item.contains("size") && item["size"].is_array() &&
                    item["size"].size() >= 3) {
                    block.size = glm::vec3(
                        item["size"][0].get<float>(),
                        item["size"][1].get<float>(),
                        item["size"][2].get<float>());
                }
                const json* rotation = nullptr;
                if (item.contains("rotationEuler") && item["rotationEuler"].is_array())
                    rotation = &item["rotationEuler"];
                else if (item.contains("rotation_euler") && item["rotation_euler"].is_array())
                    rotation = &item["rotation_euler"];
                if (rotation && rotation->size() >= 3) {
                    block.rotationEuler = glm::vec3(
                        (*rotation)[0].get<float>(),
                        (*rotation)[1].get<float>(),
                        (*rotation)[2].get<float>());
                }
                block.material = item.value("material", std::string{});
                block.enabled = item.value("enabled", true);
                if (item.contains("tags") && item["tags"].is_array()) {
                    for (const auto& tag : item["tags"])
                        if (tag.is_string()) block.tags.push_back(tag.get<std::string>());
                }
                next.blocks.push_back(std::move(block));
            }
        }

        if (root.contains("glb_objects") && root["glb_objects"].is_array()) {
            for (const auto& item : root["glb_objects"]) {
                if (!item.is_object()) continue;
                MapGLBObjectOverride object;
                object.id = item.value("id", std::string{});
                if (object.id.empty()) continue;
                object.name = item.value("name", std::string{});
                if (item.contains("position") && item["position"].is_array() &&
                    item["position"].size() >= 3) {
                    object.position = glm::vec3(
                        item["position"][0].get<float>(),
                        item["position"][1].get<float>(),
                        item["position"][2].get<float>());
                }
                next.glbObjects.push_back(std::move(object));
            }
        }

        if (root.contains("payload") && root["payload"].is_object()) {
            const auto& payload = root["payload"];
            next.payload.present = true;
            auto readVec = [](const json& j, const glm::vec3& fallback) {
                if (!j.is_array() || j.size() < 3) return fallback;
                return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
            };
            if (payload.contains("cart_origin")) {
                const auto& origin = payload["cart_origin"];
                if (origin.is_array() && origin.size() >= 3) {
                    next.payload.cartOrigin = glm::vec3(
                        origin[0].get<float>(), origin[1].get<float>(), origin[2].get<float>());
                    next.payload.hasCartOrigin = true;
                } else if (origin.is_object()) {
                    if (origin.contains("position") && origin["position"].is_array() &&
                        origin["position"].size() >= 3) {
                        next.payload.cartOrigin = glm::vec3(
                            origin["position"][0].get<float>(),
                            origin["position"][1].get<float>(),
                            origin["position"][2].get<float>());
                        next.payload.hasCartOrigin = true;
                    }
                    next.payload.cartYaw = origin.value("yaw", next.payload.cartYaw);
                }
            }
            if (payload.contains("facing_mode") && payload["facing_mode"].is_string()) {
                const std::string mode = payload["facing_mode"].get<std::string>();
                next.payload.cartFixedFacing = mode == "fixed";
            }
            if (payload.contains("interaction_box") && payload["interaction_box"].is_object()) {
                const auto& box = payload["interaction_box"];
                if (box.contains("size"))
                    next.payload.interactionSize = readVec(box["size"], next.payload.interactionSize);
                if (box.contains("offset"))
                    next.payload.interactionOffset = readVec(box["offset"], next.payload.interactionOffset);
                if (box.contains("rotationEuler"))
                    next.payload.interactionRotationEuler = readVec(box["rotationEuler"], next.payload.interactionRotationEuler);
                else if (box.contains("rotation_euler"))
                    next.payload.interactionRotationEuler = readVec(box["rotation_euler"], next.payload.interactionRotationEuler);
            }
            if (payload.contains("cart_size"))
                next.payload.cartSize = readVec(payload["cart_size"], next.payload.cartSize);
            else if (payload.contains("cartSize"))
                next.payload.cartSize = readVec(payload["cartSize"], next.payload.cartSize);
            if (payload.contains("route") && payload["route"].is_array()) {
                for (const auto& item : payload["route"]) {
                    if (!item.is_object()) continue;
                    PayloadRoutePoint point;
                    point.id = item.value("id", std::string{});
                    if (point.id.empty()) continue;
                    if (item.contains("position") && item["position"].is_array() &&
                        item["position"].size() >= 3) {
                        point.position = glm::vec3(
                            item["position"][0].get<float>(),
                            item["position"][1].get<float>(),
                            item["position"][2].get<float>());
                    }
                    point.radius = std::max(0.1f, item.value("radius", point.radius));
                    point.order = std::max(0, item.value("order", point.order));
                    point.waitTicks = std::max(0, item.value("waitTicks", item.value("wait_ticks", point.waitTicks)));
                    point.enabled = item.value("enabled", true);
                    point.checkpoint = item.value("checkpoint", true);
                    point.final = item.value("final", false);
                    point.timeBonusSeconds = std::max(0.0f,
                        item.value("time_bonus_seconds",
                            item.value("timeBonusSeconds", point.timeBonusSeconds)));
                    next.payload.route.push_back(std::move(point));
                }
                std::sort(next.payload.route.begin(), next.payload.route.end(),
                    [](const PayloadRoutePoint& a, const PayloadRoutePoint& b) {
                        return a.order < b.order;
                    });
            }
        }

        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] loaded map=%s sites=%zu entities=%zu blocks=%zu route=%zu plant=%.1f defuse=%.1f explode=%.1f\n",
            mapId.c_str(), next.bombSites.size(), next.entities.size(),
            next.blocks.size(), next.payload.route.size(),
            next.plantSeconds, next.defuseSeconds, next.explosionSeconds);
        mCurrent = std::move(next);
        mPath = nextPath;
        mLastWrite = getLastWrite(mPath);
        ++mRevision;
        mSavedRevision = mRevision;
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

void MapConfigRegistry::resetRuntimeState()
{
    for (auto& entity : mCurrent.entities) {
        entity.runtimeSpawned = 0;
        entity.runtimeCleared = false;
        entity.runtimeActivated = false;
        entity.runtimePlayerActivated = false;
        entity.lastActivationTick = 0;
    }
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

std::string MapConfigRegistry::uniqueId(const std::string& prefix) const
{
    std::string stem = prefix.empty() ? "object" : prefix;
    for (char& c : stem) if (c == ' ') c = '_';
    for (int i = 1;; ++i) {
        const std::string id = stem + "_" + std::to_string(i);
        bool used = findEntity(id) != nullptr;
        if (!used) {
            for (const auto& block : mCurrent.blocks) {
                if (block.id == id) { used = true; break; }
            }
        }
        if (!used) {
            for (const auto& point : mCurrent.payload.route) {
                if (point.id == id) { used = true; break; }
            }
        }
        if (!used) return id;
    }
}

std::string MapConfigRegistry::createEntity(const std::string& type, const glm::vec3& position,
                                            const std::string& requestedId)
{
    std::string id = requestedId;
    if (id.empty()) id = uniqueId(type);
    if (findEntity(id)) return {};
    MapEntity entity;
    entity.id = id;
    entity.type = type;
    entity.position = position;
    mCurrent.entities.push_back(std::move(entity));
    markChanged();
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
    markChanged();
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
    markChanged();
}

void MapConfigRegistry::setEntityVisibilityFor(const std::string& id, bool visible)
{
    if (auto* entity = findEntityMutable(id)) {
        entity->visible = visible;
        markChanged();
    }
}

const AuthoredBlock* MapConfigRegistry::findBlock(const std::string& id) const
{
    for (const auto& block : mCurrent.blocks)
        if (block.id == id) return &block;
    return nullptr;
}

AuthoredBlock* MapConfigRegistry::findBlockMutable(const std::string& id)
{
    for (auto& block : mCurrent.blocks)
        if (block.id == id) return &block;
    return nullptr;
}

std::string MapConfigRegistry::createBlock(const glm::vec3& position, const glm::vec3& size,
                                           const std::string& requestedId)
{
    std::string id = requestedId.empty() ? uniqueId("block") : requestedId;
    if (findBlock(id)) return {};
    AuthoredBlock block;
    block.id = id;
    block.position = position;
    block.size = glm::vec3(std::max(0.05f, size.x), std::max(0.05f, size.y),
                           std::max(0.05f, size.z));
    mCurrent.blocks.push_back(std::move(block));
    markChanged();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-block.created", id, "map editor authored block", 0,
        nlohmann::json{{"map", mCurrent.mapId}, {"block_id", id},
                       {"position", {position.x, position.y, position.z}},
                       {"size", {size.x, size.y, size.z}},
                       {"source", "map_editor"}},
        __FILE__, __LINE__, __FUNCTION__);
    return id;
}

bool MapConfigRegistry::deleteBlock(const std::string& id)
{
    const auto it = std::remove_if(mCurrent.blocks.begin(), mCurrent.blocks.end(),
        [&](const AuthoredBlock& block) { return block.id == id; });
    if (it == mCurrent.blocks.end()) return false;
    mCurrent.blocks.erase(it, mCurrent.blocks.end());
    markChanged();
    return true;
}

const MapGLBObjectOverride* MapConfigRegistry::findGLBObjectOverride(
    const std::string& id) const
{
    for (const auto& object : mCurrent.glbObjects)
        if (object.id == id) return &object;
    return nullptr;
}

MapGLBObjectOverride* MapConfigRegistry::findGLBObjectOverrideMutable(
    const std::string& id)
{
    for (auto& object : mCurrent.glbObjects)
        if (object.id == id) return &object;
    return nullptr;
}

void MapConfigRegistry::setGLBObjectOverride(const std::string& id,
                                             const std::string& name,
                                             const glm::vec3& position)
{
    if (id.empty()) return;
    if (MapGLBObjectOverride* object = findGLBObjectOverrideMutable(id)) {
        object->name = name;
        object->position = position;
    } else {
        mCurrent.glbObjects.push_back({id, name, position});
    }
    markChanged();
}

void MapConfigRegistry::removeGLBObjectOverride(const std::string& id)
{
    const auto it = std::remove_if(mCurrent.glbObjects.begin(), mCurrent.glbObjects.end(),
        [&](const MapGLBObjectOverride& object) { return object.id == id; });
    if (it == mCurrent.glbObjects.end()) return;
    mCurrent.glbObjects.erase(it, mCurrent.glbObjects.end());
    markChanged();
}

const PayloadRoutePoint* MapConfigRegistry::findPayloadRoutePoint(const std::string& id) const
{
    for (const auto& point : mCurrent.payload.route)
        if (point.id == id) return &point;
    return nullptr;
}

PayloadRoutePoint* MapConfigRegistry::findPayloadRoutePointMutable(const std::string& id)
{
    for (auto& point : mCurrent.payload.route)
        if (point.id == id) return &point;
    return nullptr;
}

std::string MapConfigRegistry::createPayloadRoutePoint(const glm::vec3& position,
                                                       const std::string& requestedId)
{
    std::string id = requestedId.empty() ? uniqueId("route") : requestedId;
    if (findPayloadRoutePoint(id)) return {};
    PayloadRoutePoint point;
    point.id = id;
    point.position = position;
    point.order = (int)mCurrent.payload.route.size();
    mCurrent.payload.present = true;
    mCurrent.payload.route.push_back(std::move(point));
    renumberPayloadRoute();
    markChanged();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-payload-route-point.created", id, "map editor authored route point", 0,
        nlohmann::json{{"map", mCurrent.mapId}, {"route_id", id},
                       {"position", {position.x, position.y, position.z}},
                       {"source", "map_editor"}},
        __FILE__, __LINE__, __FUNCTION__);
    return id;
}

bool MapConfigRegistry::deletePayloadRoutePoint(const std::string& id)
{
    const auto it = std::remove_if(mCurrent.payload.route.begin(), mCurrent.payload.route.end(),
        [&](const PayloadRoutePoint& point) { return point.id == id; });
    if (it == mCurrent.payload.route.end()) return false;
    mCurrent.payload.route.erase(it, mCurrent.payload.route.end());
    renumberPayloadRoute();
    markChanged();
    return true;
}

void MapConfigRegistry::renumberPayloadRoute()
{
    std::sort(mCurrent.payload.route.begin(), mCurrent.payload.route.end(),
        [](const PayloadRoutePoint& a, const PayloadRoutePoint& b) {
            if (a.order != b.order) return a.order < b.order;
            return a.id < b.id;
        });
    for (size_t i = 0; i < mCurrent.payload.route.size(); ++i)
        mCurrent.payload.route[i].order = (int)i;
}

void MapConfigRegistry::markChanged()
{
    ++mRevision;
}

MapObjectiveConfig MapConfigRegistry::snapshotAuthored() const
{
    MapObjectiveConfig snapshot = mCurrent;
    for (auto& entity : snapshot.entities) {
        entity.runtimeSpawned = 0;
        entity.runtimeCleared = false;
        entity.runtimeActivated = false;
        entity.runtimePlayerActivated = false;
        entity.lastActivationTick = 0;
    }
    return snapshot;
}

void MapConfigRegistry::restoreAuthored(const MapObjectiveConfig& snapshot)
{
    const std::string mapId = mCurrent.mapId;
    const bool loaded = mCurrent.loaded;
    mCurrent = snapshot;
    mCurrent.mapId = mapId;
    mCurrent.loaded = loaded;
    resetRuntimeState();
    mEntityVisibility = std::any_of(
        mCurrent.entities.begin(), mCurrent.entities.end(),
        [](const MapEntity& entity) { return entity.visible; });
    markChanged();
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

bool MapConfigRegistry::writeBackup()
{
    if (mPath.empty() || mCurrent.mapId.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(mPath, ec) || ec) return false;

    const std::filesystem::path source(mPath);
    const std::filesystem::path dir = source.parent_path() / "backups" / mCurrent.mapId;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char stamp[32] = {};
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &utc);

    std::filesystem::path target = dir / (mCurrent.mapId + "_" + stamp + "_save.json");
    for (int suffix = 2; std::filesystem::exists(target, ec) && suffix < 100; ++suffix)
        target = dir / (mCurrent.mapId + "_" + stamp + "_save_" + std::to_string(suffix) + ".json");

    std::filesystem::copy_file(source, target,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        Debug::warn(Debug::Category::Duel,
            "[MAP CONFIG] backup failed %s: %s\n", target.string().c_str(), ec.message().c_str());
        return false;
    }
    pruneBackups();
    Debug::warn(Debug::Category::Duel,
        "[MAP CONFIG] backup %s\n", target.string().c_str());
    return true;
}

void MapConfigRegistry::pruneBackups()
{
    constexpr size_t kMaxBackups = 50;
    const std::filesystem::path dir =
        std::filesystem::path(mPath).parent_path() / "backups" / mCurrent.mapId;
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) return;
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
            files.push_back(entry.path());
    }
    if (files.size() <= kMaxBackups) return;
    std::sort(files.begin(), files.end());
    for (size_t i = 0; i + kMaxBackups < files.size(); ++i) {
        std::error_code removeError;
        std::filesystem::remove(files[i], removeError);
    }
}

bool MapConfigRegistry::save(bool withBackup)
{
    if (mPath.empty()) return false;
    // Start from the existing authored file so top-level keys this owner does
    // not understand survive a save. Only authored map sections are rewritten.
    json root = json::object();
    std::string existingContents;
    {
        std::ifstream existing(mPath, std::ios::binary);
        if (existing.is_open()) {
            std::ostringstream buffer;
            buffer << existing.rdbuf();
            existingContents = buffer.str();
            try {
                std::istringstream input(existingContents);
                json parsed = parseJsonConfig(input);
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
            {"yaw", entity.yaw},
            {"radius", entity.radius}, {"enabled", entity.enabled}, {"visible", entity.visible},
            {"oneShot", entity.oneShot}, {"tag", entity.tag}, {"monsterPool", entity.monsterPool},
            {"pickupId", entity.pickupId}, {"bossId", entity.bossId}, {"damageType", entity.damageType},
            {"spawnCount", entity.spawnCount}, {"maxAlive", entity.maxAlive}, {"damage", entity.damage},
            {"damageIntervalTicks", entity.damageIntervalTicks},
            {"spawnCooldownTicks", entity.spawnCooldownTicks},
            {"checkpointRequirement", entity.checkpointRequirement},
            {"order", entity.order}, {"required", entity.required},
            {"activationRadius", entity.activationRadius},
            {"healthMultiplier", entity.healthMultiplier}
        });
    }
    root["blocks"] = json::array();
    for (const auto& block : mCurrent.blocks) {
        json b = {
            {"id", block.id}, {"shape", block.shape},
            {"position", {block.position.x, block.position.y, block.position.z}},
            {"size", {block.size.x, block.size.y, block.size.z}},
            {"rotationEuler", {block.rotationEuler.x, block.rotationEuler.y, block.rotationEuler.z}},
            {"material", block.material}, {"enabled", block.enabled}
        };
        b["tags"] = block.tags;
        root["blocks"].push_back(std::move(b));
    }
    root["glb_objects"] = json::array();
    for (const auto& object : mCurrent.glbObjects) {
        root["glb_objects"].push_back({
            {"id", object.id}, {"name", object.name},
            {"position", {object.position.x, object.position.y, object.position.z}}
        });
    }
    if (mCurrent.payload.present || mCurrent.payload.hasCartOrigin ||
        !mCurrent.payload.route.empty()) {
        json payload;
        payload["cart_origin"] = {
            {"position", {mCurrent.payload.cartOrigin.x,
                          mCurrent.payload.cartOrigin.y,
                          mCurrent.payload.cartOrigin.z}},
            {"yaw", mCurrent.payload.cartYaw}
        };
        payload["cart_size"] = {mCurrent.payload.cartSize.x,
                                mCurrent.payload.cartSize.y,
                                mCurrent.payload.cartSize.z};
        payload["facing_mode"] = mCurrent.payload.cartFixedFacing ? "fixed" : "route";
        payload["interaction_box"] = {
            {"size", {mCurrent.payload.interactionSize.x,
                      mCurrent.payload.interactionSize.y,
                      mCurrent.payload.interactionSize.z}},
            {"offset", {mCurrent.payload.interactionOffset.x,
                        mCurrent.payload.interactionOffset.y,
                        mCurrent.payload.interactionOffset.z}},
            {"rotationEuler", {mCurrent.payload.interactionRotationEuler.x,
                               mCurrent.payload.interactionRotationEuler.y,
                               mCurrent.payload.interactionRotationEuler.z}}
        };
        payload["route"] = json::array();
        for (const auto& point : mCurrent.payload.route) {
            payload["route"].push_back({
                {"id", point.id},
                {"position", {point.position.x, point.position.y, point.position.z}},
                {"radius", point.radius}, {"order", point.order},
                {"waitTicks", point.waitTicks}, {"enabled", point.enabled},
                {"checkpoint", point.checkpoint}, {"final", point.final},
                {"time_bonus_seconds", point.timeBonusSeconds}
            });
        }
        root["payload"] = std::move(payload);
    }

    const std::string contents = root.dump(2) + "\n";
    if (!existingContents.empty() && existingContents == contents) {
        mSavedRevision = mRevision;
        return true;
    }

    std::error_code ec;
    const std::filesystem::path p(mPath);
    if (!p.parent_path().empty())
        std::filesystem::create_directories(p.parent_path(), ec);

    const std::string tempPath = mPath + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            Debug::error(Debug::Category::Duel, "[MAP CONFIG] cannot write %s\n", tempPath.c_str());
            return false;
        }
        out << contents;
        if (!out.good()) {
            Debug::error(Debug::Category::Duel, "[MAP CONFIG] write failed %s\n", tempPath.c_str());
            return false;
        }
    }
    {
        std::ifstream verify(tempPath);
        try {
            parseJsonConfig(verify);
        } catch (const std::exception& e) {
            Debug::error(Debug::Category::Duel,
                "[MAP CONFIG] generated JSON invalid: %s\n", e.what());
            std::error_code cleanup;
            std::filesystem::remove(tempPath, cleanup);
            return false;
        }
    }

    if (withBackup && std::filesystem::exists(mPath, ec))
        writeBackup();
    std::filesystem::remove(mPath, ec);
    std::filesystem::rename(tempPath, mPath, ec);
    if (ec) {
        Debug::error(Debug::Category::Duel,
            "[MAP CONFIG] cannot replace %s: %s\n", mPath.c_str(), ec.message().c_str());
        return false;
    }

    mLastWrite = getLastWrite(mPath);
    mSavedRevision = mRevision;
    Debug::warn(Debug::Category::Duel,
        "[MAP CONFIG] saved %s entities=%zu blocks=%zu route=%zu\n",
        mPath.c_str(), mCurrent.entities.size(), mCurrent.blocks.size(),
        mCurrent.payload.route.size());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-entity.saved", mCurrent.mapId, "map editor save", 0,
        nlohmann::json{{"map", mCurrent.mapId},
                       {"entity_count", mCurrent.entities.size()},
                       {"block_count", mCurrent.blocks.size()},
                       {"route_count", mCurrent.payload.route.size()},
                       {"config_path", mPath}, {"result", "success"}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

bool validatePayloadAuthoring(const PayloadConfig& payload,
                              std::vector<PayloadAuthoringIssue>& issues)
{
    issues.clear();
    auto add = [&](const char* severity, const char* code, const std::string& message) {
        issues.push_back({severity, code, message});
    };

    if (!payload.present) {
        add("error", "payload-missing", "map has no payload section");
        return false;
    }
    if (!payload.hasCartOrigin) {
        add("error", "cart-origin-missing", "payload.cart_origin.position is required");
    }

    std::vector<const PayloadRoutePoint*> enabled;
    std::unordered_set<std::string> ids;
    for (const PayloadRoutePoint& point : payload.route) {
        if (point.id.empty()) {
            add("error", "route-id-empty", "route point has no id");
        } else if (!ids.insert(point.id).second) {
            add("error", "route-id-duplicate",
                "duplicate route point id '" + point.id + "'");
        }
        if (!point.enabled) continue;
        enabled.push_back(&point);
        if (point.radius < 0.1f) {
            add("warning", "route-radius-tiny",
                "route point '" + point.id + "' radius is below 0.1 m");
        }
    }

    if (enabled.size() < 2) {
        add("error", "route-too-short",
            "payload route needs at least two enabled points");
        return false;
    }

    if (payload.hasCartOrigin) {
        const float startDistance = glm::length(enabled.front()->position - payload.cartOrigin);
        if (startDistance > 0.75f) {
            add("warning", "route-start-mismatch",
                "first enabled route point is " + std::to_string(startDistance) +
                " m from payload.cart_origin; move the cart origin with the route start");
        }
    }

    float totalDistance = 0.0f;
    bool hasFinal = false;
    int finalCount = 0;
    int checkpointCount = 0;
    for (size_t i = 0; i < enabled.size(); ++i) {
        const PayloadRoutePoint& point = *enabled[i];
        if (i > 0) {
            const float segment =
                glm::length(point.position - enabled[i - 1]->position);
            totalDistance += segment;
            if (segment > 250.0f) {
                add("warning", "route-long-segment",
                    "route segment into '" + point.id + "' is " +
                    std::to_string(segment) + " m; check for a misplaced point");
            }
            const float rise = std::abs(point.position.z - enabled[i - 1]->position.z);
            if (rise > 150.0f) {
                add("warning", "route-steep-segment",
                    "route segment into '" + point.id + "' rises/falls " +
                    std::to_string(rise) + " m; check for a misplaced point");
            }
        }
        if (point.final) hasFinal = true;
        if (point.checkpoint) ++checkpointCount;
        if (point.checkpoint && !point.final && point.timeBonusSeconds <= 0.0f &&
            i != 0 && i + 1 != enabled.size()) {
            add("warning", "checkpoint-zero-bonus",
                "checkpoint '" + point.id + "' awards no time bonus");
        }
        if (point.waitTicks > 0) {
            // Informational only; wait ticks are a valid authored pause.
            add("info", "route-wait",
                "route point '" + point.id + "' pauses the cart for " +
                std::to_string(point.waitTicks) + " ticks");
        }
    }

    if (totalDistance <= 0.1f) {
        add("error", "route-distance-zero", "payload route has no usable length");
    }
    if (!hasFinal) {
        add("warning", "route-final-implicit",
            "no route point is marked final; the runtime treats the last enabled point as final");
    }
    if (checkpointCount < 2) {
        add("warning", "route-no-checkpoints",
            "fewer than two checkpoints are authored");
    }

    for (const PayloadAuthoringIssue& issue : issues) {
        if (issue.severity == "error") return false;
    }
    return true;
}

bool payloadAuthoringSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) {
        ok = false;
        report += "FAIL: " + why + "\n";
    };

    auto& modes = GamemodeRegistry::instance();
    if (!modes.has("payload"))
        modes.loadDirectory("config/gamemodes");
    const Gamemode& mode = modes.get("payload");
    if (mode.id != "payload")
        fail("payload gamemode did not load");
    else {
        if (mode.spawnStrategy == "shared")
            fail("payload spawn_strategy must not be shared");
        if (mode.teams.size() < 2) {
            fail("payload must declare two playing teams");
        } else {
            if (mode.teams[0].spawnGroup != "attacker_spawn")
                fail("payload team 0 spawn_group != attacker_spawn");
            if (mode.teams[1].spawnGroup != "defender_spawn")
                fail("payload team 1 spawn_group != defender_spawn");
            if (mode.teams[0].spawnTagContains.empty() ||
                mode.teams[1].spawnTagContains.empty())
                fail("payload teams must declare spawn_tag_contains");
        }
    }

    for (const char* mapId : {"payload1", "payload2"}) {
        MapConfigRegistry& registry = MapConfigRegistry::instance();
        if (!registry.load(mapId)) {
            report += std::string("FAIL: ") + mapId + " map config did not load\n";
            ok = false;
            continue;
        }
        std::vector<PayloadAuthoringIssue> issues;
        const bool valid = validatePayloadAuthoring(registry.current().payload, issues);
        report += std::string(mapId) + ": " + (valid ? "valid" : "invalid") +
                  " route=" + std::to_string(registry.current().payload.route.size()) +
                  " issues=" + std::to_string(issues.size()) + "\n";
        for (const PayloadAuthoringIssue& issue : issues) {
            report += "  " + issue.severity + " " + issue.code + ": " + issue.message + "\n";
        }
        ok = ok && valid;

        auto hasSpawnTag = [&](const std::string& tag) {
            for (const MapEntity& entity : registry.current().entities) {
                if (entity.type == "spawnpoint" && entity.enabled && entity.tag == tag)
                    return true;
            }
            return false;
        };
        if (!hasSpawnTag("attacker_spawn"))
            fail(std::string(mapId) + " has no enabled attacker_spawn entity");
        if (!hasSpawnTag("defender_spawn"))
            fail(std::string(mapId) + " has no enabled defender_spawn entity");
    }
    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
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

    // Blocks and payload routes must round-trip through save/load.
    {
        std::ofstream out(path);
        out << R"({
  "glb_objects":[{"id":"node:2","name":"TestNode","position":[4,5,6]}],
  "blocks":[{"id":"b1","shape":"box","position":[1,2,3],"size":[4,5,6],
             "rotationEuler":[0,0,15],"material":"","enabled":true,"tags":["a"]}],
  "payload":{"cart_origin":{"position":[7,8,9],"yaw":45},
             "cart_size":[2,1.5,1],
             "route":[{"id":"r1","position":[1,1,1],"radius":2,"order":0},
                      {"id":"r2","position":[2,2,2],"radius":3,"order":1,
                       "checkpoint":true,"final":true,"time_bonus_seconds":300}]}
})";
    }
    if (!reg.load(mapId))
        fail("block/route selftest map failed to load");
    if (reg.current().blocks.size() != 1)
        fail("block was not parsed");
    if (reg.current().glbObjects.size() != 1)
        fail("glb object override was not parsed");
    else if (reg.current().glbObjects[0].id != "node:2" ||
             std::abs(reg.current().glbObjects[0].position.z - 6.0f) > 0.001f)
        fail("glb object override fields were not parsed");
    if (reg.current().payload.route.size() != 2)
        fail("payload route was not parsed");
    if (reg.current().payload.route[1].id != "r2")
        fail("payload route order was not applied");
    if (!reg.current().payload.route[1].final)
        fail("payload final route flag was not parsed");
    if (reg.current().payload.route[1].timeBonusSeconds != 300.0f)
        fail("payload checkpoint time bonus was not parsed");
    if (std::abs(reg.current().payload.cartSize.x - 2.0f) > 0.001f)
        fail("payload cart size was not parsed");
    if (!reg.save())
        fail("block/route selftest map failed to save");
    {
        std::ifstream in(path);
        try {
            const json root = parseJsonConfig(in);
            if (!root.contains("blocks") || root["blocks"].size() != 1)
                fail("blocks missing after save");
            if (!root.contains("payload") || root["payload"]["route"].size() != 2)
                fail("payload route missing after save");
            if (!root["payload"]["route"][1].value("final", false))
                fail("payload final flag missing after save");
            if (!root["payload"].contains("cart_size"))
                fail("payload cart size missing after save");
            if (!root.contains("glb_objects") || root["glb_objects"].size() != 1)
                fail("glb object override missing after save");
        } catch (const std::exception& e) {
            fail(std::string("block/route saved json unparsable: ") + e.what());
        }
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove_all(
        std::filesystem::path(path).parent_path() / "backups" / mapId, ec);
    reg.load("zombietower4");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
