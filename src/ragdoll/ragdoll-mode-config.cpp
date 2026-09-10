#include "ragdoll/ragdoll-mode-config.h"

#include <filesystem>
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

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

glm::vec3 readJsonVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

} // namespace

RagdollModeConfig& RagdollModeConfig::instance()
{
    static RagdollModeConfig config;
    return config;
}

bool RagdollModeConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL MODE CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL MODE CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        RagdollModeConfigData next;

        next.enabled = root.value("enabled", true);
        next.toggleKey = root.value("toggle_key", "G");
        next.gravityScale = root.value("gravity_scale", 1.0f);
        next.linearDamping = root.value("linear_damping", 0.15f);
        next.angularDamping = root.value("angular_damping", 0.3f);
        next.jointStiffness = root.value("joint_stiffness", 2000.0f);
        next.jointDamping = root.value("joint_damping", 80.0f);
        next.worldCollision = root.value("world_collision", true);

        next.solverIterations = root.value("solver_iterations", next.solverIterations);
        next.maxFallSpeed = root.value("max_fall_speed", next.maxFallSpeed);
        next.restitution = root.value("restitution", next.restitution);
        next.friction = root.value("friction", next.friction);
        next.selfCollision = root.value("self_collision", next.selfCollision);
        next.bodyLinearDamping = root.value("body_linear_damping", next.bodyLinearDamping);
        next.bodyAngularDamping = root.value("body_angular_damping", next.bodyAngularDamping);

        // Mass
        if (root.contains("mass")) {
            const auto& m = root["mass"];
            next.massGlobalMultiplier = m.value("globalMultiplier",
                m.value("global_multiplier", next.massGlobalMultiplier));
            for (auto it = m.begin(); it != m.end(); ++it) {
                if (it.key() == "globalMultiplier" || it.key() == "global_multiplier") continue;
                if (!it.value().is_number()) continue;
                next.massKg[it.key()] = it.value().get<float>();
            }
        }

        // Capsules
        if (root.contains("capsules")) {
            for (auto it = root["capsules"].begin(); it != root["capsules"].end(); ++it) {
                RagdollModeCapsuleConfig cc;
                const auto& c = it.value();
                cc.radius = c.value("radius", 0.15f);
                cc.halfHeight = c.value("half_height", 0.15f);
                cc.offset = readJsonVec3(c, "offset", cc.offset);
                next.capsules[it.key()] = cc;
            }
        }

        // Attachments
        if (root.contains("attachments")) {
            for (auto it = root["attachments"].begin(); it != root["attachments"].end(); ++it) {
                RagdollModeAttachmentConfig ac;
                const auto& a = it.value();
                ac.parent = a.value("parent", "torso");
                ac.offset = readJsonVec3(a, "offset", ac.offset);
                ac.coneLimitDeg = a.value("cone_limit_deg", 90.0f);
                next.attachments[it.key()] = ac;
            }
        }

        // Grab
        if (root.contains("grab")) {
            const auto& g = root["grab"];
            next.leftKey = g.value("left_key", "A");
            next.rightKey = g.value("right_key", "D");
            next.extendLeftMouse = g.value("extend_left_mouse", 0);
            next.extendRightMouse = g.value("extend_right_mouse", 1);
            next.extendForce = g.value("extend_force", 50.0f);
            next.grabReach = g.value("grab_reach", 2.5f);
            next.grabRadius = g.value("grab_radius", 0.3f);
            next.grabCompliance = g.value("compliance", next.grabCompliance);
            next.grabGraceDistance = g.value("grace_distance", next.grabGraceDistance);
        }

        // Weapon
        if (root.contains("weapon")) {
            const auto& w = root["weapon"];
            next.oneHandedWeaponEnabled = w.value("one_handed_enabled", true);
            next.weaponHand = w.value("hand", "right");
        }

        // Camera
        if (root.contains("camera")) {
            const auto& cam = root["camera"];
            next.cameraMode = cam.value("mode", "locked_to_head");
            next.cameraSmoothFactor = cam.value("smooth_factor", 0.0f);
        }

        next.torsoLookSpring = root.value("torso_look_spring", 8.0f);
        next.torsoMaxAngularStep = root.value("torso_max_angular_step", 15.0f);

        if (root.contains("head")) {
            const auto& h = root["head"];
            next.headRotationStrength = h.value("rotation_strength", next.headRotationStrength);
            next.headRotationSpeed = h.value("rotation_speed", next.headRotationSpeed);
        }

        if (root.contains("exit")) {
            const auto& e = root["exit"];
            next.exitPreserveVelocity = e.value("preserve_velocity", next.exitPreserveVelocity);
            next.exitHopVelocity = e.value("hop_velocity", next.exitHopVelocity);
        }

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL MODE CONFIG] Loaded: %s (capsules=%zu attachments=%zu)\n",
            fileName.c_str(), mData.capsules.size(), mData.attachments.size());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Ragdoll,
            "[RAGDOLL MODE CONFIG] Parse error in %s: %s\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Ragdoll,
            "[RAGDOLL MODE CONFIG] Error loading %s: %s\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool RagdollModeConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE CONFIG] Detected change: %s — reloading\n",
        fileNameOf(mPath).c_str());
    return load(mPath);
}
