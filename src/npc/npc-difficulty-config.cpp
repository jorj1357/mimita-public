// 08 08 2026, 22 19
/* purpose
* Loads, hot-reloads, and saves config/npc-difficulty.json.
* Preserves unknown/comment keys from the file when saving so the file stays human-readable.
* Uses Debug::log / Debug::warn with the NpcCombat category for all reporting.
* Does NOT contain any gameplay or aiming logic itself.
* Does NOT fail hard on bad JSON - keeps the last valid settings and logs an error.
*/

#include "npc/npc-difficulty-config.h"

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

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
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

} // namespace

NpcDifficultyConfig& NpcDifficultyConfig::instance()
{
    static NpcDifficultyConfig config;
    return config;
}

bool NpcDifficultyConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        NpcDifficultySettings next;
        next.maxAngularErrorDegrees = std::max(0.0f, optFloat(root, "maxAngularErrorDegrees", next.maxAngularErrorDegrees));
        next.difficultyErrorScale = std::clamp(optFloat(root, "difficultyErrorScale", next.difficultyErrorScale), 0.0f, 1.0f);
        next.damageMultiplier = std::max(0.0f, optFloat(root, "damageMultiplier", next.damageMultiplier));
        next.fireDelayMin = std::max(0.0f, optFloat(root, "fireDelayMin", next.fireDelayMin));
        next.fireDelayMax = std::max(0.0f, optFloat(root, "fireDelayMax", next.fireDelayMax));
        next.aggressionBonus = optFloat(root, "aggressionBonus", next.aggressionBonus);
        next.npcHitRadius = std::max(0.0f, optFloat(root, "npcHitRadius", next.npcHitRadius));
        next.forceHit = optBool(root, "forceHit", next.forceHit);
        next.npcDebugVisuals = optBool(root, "npcDebugVisuals", next.npcDebugVisuals);
        next.turnSpeed = std::max(0.0f, optFloat(root, "turnSpeed", next.turnSpeed));
        next.aimAtTargetMin = std::max(0.0f, optFloat(root, "aimAtTargetMin", next.aimAtTargetMin));
        next.aimAtTargetMax = std::max(0.0f, optFloat(root, "aimAtTargetMax", next.aimAtTargetMax));
        next.faceMovementMin = std::max(0.0f, optFloat(root, "faceMovementMin", next.faceMovementMin));
        next.faceMovementMax = std::max(0.0f, optFloat(root, "faceMovementMax", next.faceMovementMax));

        mRoot = root;
        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Loaded %s: maxErr=%.1fdeg diffScale=%.2f dmg=%.2fx fireDelay=[%.2f,%.2f] aggressionBonus=%.2f hitRadius=%.2f forceHit=%d\n",
            fileName.c_str(),
            mData.maxAngularErrorDegrees, mData.difficultyErrorScale,
            mData.damageMultiplier, mData.fireDelayMin, mData.fireDelayMax,
            mData.aggressionBonus, mData.npcHitRadius, (int)mData.forceHit);
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool NpcDifficultyConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::NpcCombat,
        "[NPC DIFFICULTY] Detected change: %s\n", fileNameOf(mPath).c_str());
    return load(mPath);
}

bool NpcDifficultyConfig::save(const std::string& path)
{
    json j = mRoot.is_object() ? mRoot : json::object();
    j["maxAngularErrorDegrees"] = mData.maxAngularErrorDegrees;
    j["difficultyErrorScale"] = mData.difficultyErrorScale;
    j["damageMultiplier"] = mData.damageMultiplier;
    j["fireDelayMin"] = mData.fireDelayMin;
    j["fireDelayMax"] = mData.fireDelayMax;
    j["aggressionBonus"] = mData.aggressionBonus;
    j["npcHitRadius"] = mData.npcHitRadius;
    j["forceHit"] = mData.forceHit;
    j["npcDebugVisuals"] = mData.npcDebugVisuals;
    j["turnSpeed"] = mData.turnSpeed;
    j["aimAtTargetMin"] = mData.aimAtTargetMin;
    j["aimAtTargetMax"] = mData.aimAtTargetMax;
    j["faceMovementMin"] = mData.faceMovementMin;
    j["faceMovementMax"] = mData.faceMovementMax;

    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << j.dump(2) << std::endl;

    mRoot = j;
    mLastWrite = getLastWrite(mPath);
    Debug::log(Debug::Category::NpcCombat,
        "[NPC DIFFICULTY] Saved %s\n", fileNameOf(path).c_str());
    return true;
}
