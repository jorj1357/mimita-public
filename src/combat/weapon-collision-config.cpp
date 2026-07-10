#include "weapon-collision-config.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "entities/player.h"

using json = nlohmann::json;

static const char* WEAPON_COLLISION_CONFIG_PATH = "config/weaponcollisions.json";

WeaponCollisionJsonConfig& WeaponCollisionJsonConfig::instance() {
    static WeaponCollisionJsonConfig cfg;
    return cfg;
}

const WeaponCollisionEntry* WeaponCollisionJsonConfig::get(const std::string& weaponId) const {
    auto it = mConfigs.find(weaponId);
    return it != mConfigs.end() ? &it->second : nullptr;
}

static bool parseVec3(const json& j, glm::vec3& out) {
    if (!j.is_array() || j.size() < 3) return false;
    out = glm::vec3((float)j[0], (float)j[1], (float)j[2]);
    return true;
}

static bool parseCapsuleConfig(const json& root, WeaponCollisionCapsuleConfig& out) {
    out.name = root.value("name", "capsule");
    out.enabled = root.value("enabled", true);
    if (root.contains("start")) parseVec3(root["start"], out.start);
    if (root.contains("end")) parseVec3(root["end"], out.end);
    out.radius = root.value("radius", 0.08f);
    if (root.contains("scale")) parseVec3(root["scale"], out.scale);
    if (root.contains("rotation_degrees")) parseVec3(root["rotation_degrees"], out.rotationDegrees);
    return true;
}

static bool parseSphereConfig(const json& j, WeaponCollisionSphereConfig& out) {
    out.name = j.value("name", "sphere");
    out.enabled = j.value("enabled", true);
    // Support both "offset" (new) and "position" (legacy)
    if (j.contains("offset")) parseVec3(j["offset"], out.offset);
    else if (j.contains("position")) parseVec3(j["position"], out.offset);
    out.radius = j.value("radius", 0.08f);
    if (j.contains("scale")) parseVec3(j["scale"], out.scale);
    if (j.contains("rotation_degrees")) parseVec3(j["rotation_degrees"], out.rotationDegrees);
    return true;
}

static bool parseGeneratedSpheres(const json& root, WeaponCollisionGeneratedSpheresConfig& out) {
    if (!root.is_object()) return false;
    out.enabled = root.value("enabled", false);
    out.count = root.value("count", 8);
    if (root.contains("start")) parseVec3(root["start"], out.start);
    if (root.contains("end")) parseVec3(root["end"], out.end);
    out.radius = root.value("radius", 0.12f);
    return true;
}

static bool parseOneWeapon(const std::string& weaponId, const json& root, WeaponCollisionEntry& out) {
    out.enabled = root.value("enabled", true);
    out.collidesWithWorld = root.value("collides_with_world", true);
    out.collisionSkin = root.value("collision_skin", 0.05f);
    out.source = root.value("source", "json");

    if (out.collisionSkin < 0.0f) {
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON COLLISIONS JSON] ERROR %s.collision_skin must be >= 0", weaponId.c_str());
        out.collisionSkin = 0.05f;
    }

    // Backward compat: singular capsule
    if (root.contains("capsule") && root["capsule"].is_object()) {
        parseCapsuleConfig(root["capsule"], out.capsule);
        if (out.capsule.radius <= 0.0f) {
            Debug::warn(Debug::Category::Weapons,
                "[WEAPON COLLISIONS JSON] ERROR %s.capsule.radius must be > 0", weaponId.c_str());
            out.capsule.radius = 0.08f;
        }
    }

    // New: plural capsules array
    if (root.contains("capsules") && root["capsules"].is_array()) {
        out.capsules.clear();
        for (const auto& item : root["capsules"]) {
            if (!item.is_object()) continue;
            WeaponCollisionCapsuleConfig cc;
            parseCapsuleConfig(item, cc);
            if (cc.radius <= 0.0f) {
                Debug::warn(Debug::Category::Weapons,
                    "[WEAPON COLLISIONS JSON] ERROR %s.capsules[].radius must be > 0", weaponId.c_str());
                cc.radius = 0.08f;
            }
            out.capsules.push_back(std::move(cc));
        }
    }

    if (root.contains("spheres") && root["spheres"].is_array()) {
        out.spheres.clear();
        for (const auto& item : root["spheres"]) {
            if (!item.is_object()) continue;
            WeaponCollisionSphereConfig sc;
            parseSphereConfig(item, sc);
            if (sc.radius <= 0.0f) {
                Debug::warn(Debug::Category::Weapons,
                    "[WEAPON COLLISIONS JSON] ERROR %s.spheres[].radius must be > 0", weaponId.c_str());
                sc.radius = 0.08f;
            }
            out.spheres.push_back(std::move(sc));
        }
    }

    if (root.contains("generated_spheres") && root["generated_spheres"].is_object()) {
        parseGeneratedSpheres(root["generated_spheres"], out.generatedSpheres);
        if (out.generatedSpheres.count < 1) {
            Debug::warn(Debug::Category::Weapons,
                "[WEAPON COLLISIONS JSON] ERROR %s.generated_spheres.count must be >= 1", weaponId.c_str());
            out.generatedSpheres.count = 1;
        }
        if (out.generatedSpheres.radius <= 0.0f) {
            Debug::warn(Debug::Category::Weapons,
                "[WEAPON COLLISIONS JSON] ERROR %s.generated_spheres.radius must be > 0", weaponId.c_str());
            out.generatedSpheres.radius = 0.12f;
        }
    }

    return true;
}

void WeaponCollisionJsonConfig::load() {
    mConfigs.clear();
    std::ifstream file(WEAPON_COLLISION_CONFIG_PATH);
    if (!file.is_open()) {
        Debug::log(Debug::Category::Weapons,
            "[WEAPON COLLISIONS JSON] no config at %s; all weapons use defaults",
            WEAPON_COLLISION_CONFIG_PATH);
        mLoaded = true;
        return;
    }

    try {
        json root;
        file >> root;

        int weaponCount = 0;
        for (auto it = root.begin(); it != root.end(); ++it) {
            const std::string& weaponId = it.key();
            if (!it.value().is_object()) continue;
            WeaponCollisionEntry entry;
            parseOneWeapon(weaponId, it.value(), entry);
            mConfigs[weaponId] = std::move(entry);
            ++weaponCount;
        }

        mLoaded = true;
        mLastLoadOk = true;
        Debug::log(Debug::Category::Weapons,
            "[WEAPON COLLISIONS JSON] reload ok weapons=%d", weaponCount);

    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON COLLISIONS JSON] parse failed: %s", e.what());
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON COLLISIONS JSON] keeping previous valid config");
        mLastLoadOk = false;
    }
}

void WeaponCollisionJsonConfig::pollHotReload() {
    const auto now = std::chrono::steady_clock::now();
    if (mLastCheckTime.time_since_epoch().count() != 0 &&
        now - mLastCheckTime < std::chrono::milliseconds(250))
        return;
    mLastCheckTime = now;

    std::error_code ec;
    if (!std::filesystem::exists(WEAPON_COLLISION_CONFIG_PATH, ec) || ec)
        return;
    const auto writeTime = std::filesystem::last_write_time(WEAPON_COLLISION_CONFIG_PATH, ec);
    if (ec || (mLoaded && writeTime == mLastWriteTime))
        return;

    mLastWriteTime = writeTime;
    load();
    Debug::log(Debug::Category::Weapons,
        "[WEAPON COLLISIONS JSON] hot reload triggered");
}

void WeaponCollisionJsonConfig::reloadNow() {
    load();
}

void WeaponCollisionJsonConfig::applyCollisionConfig(Player& player) {
    const std::string& weaponId = player.equippedWeaponId;
    if (weaponId.empty()) return;

    const WeaponCollisionEntry* entry = get(weaponId);
    if (!entry || !entry->enabled) return;

    // Check if JSON has any valid enabled colliders
    bool hasEnabledSpheres = false;
    for (const auto& sc : entry->spheres)
        if (sc.enabled) { hasEnabledSpheres = true; break; }
    bool hasEnabledCapsules = entry->capsule.enabled;
    for (const auto& cc : entry->capsules)
        if (cc.enabled) { hasEnabledCapsules = true; break; }
    bool hasGenerated = entry->generatedSpheres.enabled;

    bool hasAnyCollider = hasEnabledSpheres || hasEnabledCapsules || hasGenerated;

    WeaponCollisionRuntimeDebug& dbg = player.weaponCollisionDebug;

    if (!hasAnyCollider && entry->source == "json") {
        // JSON mode but no valid colliders: log fallback
        Debug::log(Debug::Category::Weapons,
            "[WEAPON COLLISION] %s using fallback C++ collider because JSON has no enabled colliders",
            weaponId.c_str());
        dbg.fromJsonConfig = false;
        return;
    }

    // Mark as JSON-driven
    dbg.fromJsonConfig = true;
    dbg.valid = true;
    dbg.weaponId = weaponId;

    // Save previous sphere data before overwriting
    glm::vec3 oldPrevCenter[256];
    int oldCount = (int)dbg.spheres.size();
    for (int i = 0; i < oldCount && i < 256; ++i)
        oldPrevCenter[i] = dbg.spheres[i].currentCenter;

    const glm::mat4& weaponXform = player.weaponCollisionWorld;

    // Transform a local-space point to world space
    auto toWorld = [&](const glm::vec3& local) -> glm::vec3 {
        return glm::vec3(weaponXform * glm::vec4(local, 1.0f));
    };

    // Build local-to-world rotation matrix from euler degrees
    auto buildRotMat = [](const glm::vec3& rotDeg) -> glm::mat4 {
        glm::mat4 m(1.0f);
        m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1,0,0));
        m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0,1,0));
        m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0,0,1));
        return m;
    };

    // Compute collision radius from base radius * max scale axis
    auto collisionRadius = [](float r, const glm::vec3& s) -> float {
        return r * std::max({s.x, s.y, s.z});
    };

    dbg.spheres.clear();
    dbg.capsule.enabled = false;
    int sphereIdx = 0;

    // Process explicit spheres from config
    for (const auto& sc : entry->spheres) {
        if (!sc.enabled) continue;
        glm::mat4 rot = buildRotMat(sc.rotationDegrees);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), sc.scale);
        glm::vec3 localPos = glm::vec3(rot * scaleMat * glm::vec4(sc.offset, 1.0f));
        glm::vec3 worldCenter = toWorld(localPos);

        WeaponColliderDebugSphere ds;
        ds.name = sc.name;
        ds.currentCenter = worldCenter;
        ds.radius = collisionRadius(sc.radius, sc.scale);
        ds.collidesWithWorld = entry->collidesWithWorld;
        ds.sourceType = WeaponColliderDebugSphere::SourceType::JsonSphere;

        // Preserve prev center if we had a sphere at the same index
        if (sphereIdx < oldCount)
            ds.previousCenter = oldPrevCenter[sphereIdx];
        else
            ds.previousCenter = worldCenter;

        ds.sweepDelta = ds.currentCenter - ds.previousCenter;
        dbg.spheres.push_back(std::move(ds));
        ++sphereIdx;
    }

    // Process capsule configs (both singular and plural): sample spheres along capsule axis
    auto processCapsule = [&](const WeaponCollisionCapsuleConfig& cc) {
        if (!cc.enabled) return;
        glm::mat4 rot = buildRotMat(cc.rotationDegrees);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), cc.scale);
        glm::vec3 localStart = glm::vec3(rot * scaleMat * glm::vec4(cc.start, 1.0f));
        glm::vec3 localEnd   = glm::vec3(rot * scaleMat * glm::vec4(cc.end, 1.0f));
        glm::vec3 worldStart = toWorld(localStart);
        glm::vec3 worldEnd   = toWorld(localEnd);
        float capRadius = collisionRadius(cc.radius, cc.scale);

        // Store capsule wireframe data (first processed capsule wins)
        if (!dbg.capsule.enabled) {
            dbg.capsule.enabled = true;
            dbg.capsule.radius = capRadius;
            dbg.capsule.previousStart = dbg.capsule.currentStart;
            dbg.capsule.previousEnd   = dbg.capsule.currentEnd;
            dbg.capsule.currentStart  = worldStart;
            dbg.capsule.currentEnd    = worldEnd;
        }

        // Sample 8 spheres along capsule for collision
        constexpr int CAPSULE_SAMPLES = 8;
        for (int si = 0; si < CAPSULE_SAMPLES; ++si) {
            float t = (CAPSULE_SAMPLES > 1) ? (float)si / (float)(CAPSULE_SAMPLES - 1) : 0.5f;
            glm::vec3 curPos = worldStart + (worldEnd - worldStart) * t;

            WeaponColliderDebugSphere ds;
            ds.name = cc.name.empty() ? "capsule" : cc.name + "_" + std::to_string(si);
            ds.currentCenter = curPos;
            ds.radius = capRadius;
            ds.collidesWithWorld = entry->collidesWithWorld;
            ds.sourceType = WeaponColliderDebugSphere::SourceType::CapsuleSample;

            if (sphereIdx < oldCount)
                ds.previousCenter = oldPrevCenter[sphereIdx];
            else
                ds.previousCenter = curPos;

            ds.sweepDelta = ds.currentCenter - ds.previousCenter;
            dbg.spheres.push_back(std::move(ds));
            ++sphereIdx;
        }
    };

    // Process singular capsule (backward compat)
    processCapsule(entry->capsule);
    // Process plural capsules array
    for (const auto& cc : entry->capsules)
        processCapsule(cc);

    // Process generated spheres (sampled along start→end line)
    if (entry->generatedSpheres.enabled) {
        int count = entry->generatedSpheres.count;
        if (count < 1) count = 1;
        glm::vec3 s = entry->generatedSpheres.start;
        glm::vec3 e = entry->generatedSpheres.end;
        for (int si = 0; si < count; ++si) {
            float t = (count > 1) ? (float)si / (float)(count - 1) : 0.5f;
            glm::vec3 localPos = s + (e - s) * t;
            glm::vec3 worldCenter = toWorld(localPos);

            WeaponColliderDebugSphere ds;
            ds.name = "generated_" + std::to_string(si);
            ds.currentCenter = worldCenter;
            ds.radius = entry->generatedSpheres.radius;
            ds.collidesWithWorld = entry->collidesWithWorld;
            ds.sourceType = WeaponColliderDebugSphere::SourceType::GeneratedProbe;

            if (sphereIdx < oldCount)
                ds.previousCenter = oldPrevCenter[sphereIdx];
            else
                ds.previousCenter = worldCenter;

            ds.sweepDelta = ds.currentCenter - ds.previousCenter;
            dbg.spheres.push_back(std::move(ds));
            ++sphereIdx;
        }
    }

    int ncaps = entry->capsule.enabled ? 1 : 0;
    for (const auto& cc : entry->capsules) if (cc.enabled) ++ncaps;

    // Log per-type counts
    int jsonCount = 0, capsuleCount = 0, generatedCount = 0;
    for (const auto& s : dbg.spheres) {
        if (!s.collidesWithWorld) continue;
        switch (s.sourceType) {
            case WeaponColliderDebugSphere::SourceType::JsonSphere: ++jsonCount; break;
            case WeaponColliderDebugSphere::SourceType::CapsuleSample: ++capsuleCount; break;
            case WeaponColliderDebugSphere::SourceType::GeneratedProbe: ++generatedCount; break;
        }
    }
    Debug::log(Debug::Category::Weapons,
        "[WEAPON COLLISION] weapon=%s json=%d capsules=%d generated=%d caps=%d source=%s",
        weaponId.c_str(), jsonCount, capsuleCount, generatedCount, ncaps, entry->source.c_str());
}
