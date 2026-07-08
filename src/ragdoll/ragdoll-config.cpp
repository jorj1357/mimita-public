#include "ragdoll/ragdoll-config.h"

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

} // namespace

RagdollConfig& RagdollConfig::instance()
{
    static RagdollConfig config;
    return config;
}

bool RagdollConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        RagdollConfigData next;

        next.enabled = root.value("enabled", true);
        next.lifetimeSeconds = root.value("lifetime_seconds", 30.0f);
        next.deathImpulseMultiplier = root.value("death_impulse_multiplier", 1.0f);
        next.spawnVelocityMultiplier = root.value("spawn_velocity_multiplier", 1.0f);
        next.inheritPlayerVelocity = root.value("inherit_player_velocity", true);
        next.inheritPlayerAngularVelocity = root.value("inherit_player_angular_velocity", true);
        next.gravityScale = root.value("gravity_scale", 1.0f);
        next.linearDamping = root.value("linear_damping", 0.1f);
        next.angularDamping = root.value("angular_damping", 0.2f);
        next.selfCollision = root.value("self_collision", true);
        next.worldCollision = root.value("world_collision", true);
        next.playerCollision = root.value("player_collision", true);
        next.npcCollision = root.value("npc_collision", true);
        next.jointStiffness = root.value("joint_stiffness", 1000.0f);
        next.jointDamping = root.value("joint_damping", 50.0f);
        next.jointBreakForce = root.value("joint_break_force", 1000000.0f);

        if (root.contains("parts")) {
            const auto& parts = root["parts"];
            for (auto it = parts.begin(); it != parts.end(); ++it) {
                RagdollPartConfig pc;
                const auto& part = it.value();
                if (part.contains("offset") && part["offset"].is_array() && part["offset"].size() >= 3) {
                    pc.offset = glm::vec3(
                        part["offset"][0].get<float>(),
                        part["offset"][1].get<float>(),
                        part["offset"][2].get<float>());
                }
                pc.mass = part.value("mass", 1.0f);
                pc.radius = part.value("radius", 0.2f);
                next.parts[it.key()] = pc;
            }
        }

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL CONFIG] Loaded successfully: %s (%zu parts)\n",
            fileName.c_str(), mData.parts.size());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Ragdoll,
            "[RAGDOLL CONFIG] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Ragdoll,
            "[RAGDOLL CONFIG] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool RagdollConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL CONFIG] Detected change: %s\n", fileNameOf(mPath).c_str());
    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL CONFIG] Reloading...\n");
    return load(mPath);
}
