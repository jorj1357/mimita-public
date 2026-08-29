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
#include <cctype>
#include <fstream>

#include <nlohmann/json.hpp>

#include "config/movement-config.h"
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

std::string optString(const json& root, const char* key, const std::string& fallback)
{
    if (root.contains(key) && root[key].is_string())
        return root[key].get<std::string>();
    return fallback;
}

std::string lowercaseCopy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
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
        root = json::parse(file, nullptr, true, true);

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

        // Weapon loadout
        if (root.contains("weaponLoadout") && root["weaponLoadout"].is_array()) {
            next.weaponLoadout.clear();
            for (const auto& item : root["weaponLoadout"]) {
                if (item.is_string()) {
                    std::string wid = item.get<std::string>();
                    if (!wid.empty())
                        next.weaponLoadout.push_back(wid);
                }
            }
        }
        if (next.weaponLoadout.empty())
            next.weaponLoadout = {"revolver", "shotgun", "rocket_launcher", "grenade_launcher"};
        next.startingWeapon = optString(root, "startingWeapon", next.startingWeapon);
        next.switchCooldown = std::max(0.0f, optFloat(root, "switchCooldown", next.switchCooldown));
        next.closeSwitchDist = std::max(0.0f, optFloat(root, "closeSwitchDist", next.closeSwitchDist));
        next.farSwitchDist = std::max(0.0f, optFloat(root, "farSwitchDist", next.farSwitchDist));

        // Panic toggle
        next.hitReactionEnabled = optBool(root, "hitReactionEnabled", next.hitReactionEnabled);
        next.hitReactionDurationScale = std::max(0.0f, optFloat(root, "hitReactionDurationScale", next.hitReactionDurationScale));

        // Movement expressiveness
        next.dashChance = std::max(0.0f, optFloat(root, "dashChance", next.dashChance));
        next.downDashChance = std::max(0.0f, optFloat(root, "downDashChance", next.downDashChance));
        next.freezeChance = std::max(0.0f, optFloat(root, "freezeChance", next.freezeChance));
        next.movementNoiseScale = std::max(0.0f, optFloat(root, "movementNoiseScale", next.movementNoiseScale));
        next.jukeFrequency = std::max(0.0f, optFloat(root, "jukeFrequency", next.jukeFrequency));

        // Force weapon mode
        next.forceWeapon = optString(root, "forceWeapon", next.forceWeapon);

        // Mirror movement
        next.mirrorMovementEnabled = optBool(root, "mirrorMovementEnabled", next.mirrorMovementEnabled);
        next.mirrorNormalDuration = std::max(0.1f, optFloat(root, "mirrorNormalDuration", next.mirrorNormalDuration));
        next.mirrorReplayDuration = std::max(0.1f, optFloat(root, "mirrorReplayDuration", next.mirrorReplayDuration));
        next.mirrorHistorySeconds = std::max(0.1f, optFloat(root, "mirrorHistorySeconds", next.mirrorHistorySeconds));
        next.mirrorKeepAimingAtTarget = optBool(root, "mirrorKeepAimingAtTarget", next.mirrorKeepAimingAtTarget);
        next.mirrorDashEnabled = optBool(root, "mirrorDashEnabled", next.mirrorDashEnabled);
        next.mirrorDownDashEnabled = optBool(root, "mirrorDownDashEnabled", next.mirrorDownDashEnabled);
        next.mirrorFreezeEnabled = optBool(root, "mirrorFreezeEnabled", next.mirrorFreezeEnabled);
        next.mirrorJumpEnabled = optBool(root, "mirrorJumpEnabled", next.mirrorJumpEnabled);

        // NPC movement preset: "follow" uses the player's global movement config;
        // any other value resolves a preset from config/movement/*.json.
        next.movementPreset = "follow";
        mHasNpcMovement = false;
        mNpcPresetPath.clear();
        mNpcPresetWrite = {};
        if (root.contains("movementPreset") && root["movementPreset"].is_string())
        {
            const std::string preset = lowercaseCopy(root["movementPreset"].get<std::string>());
            if (!preset.empty() && preset != "follow")
            {
                MovementConfig npcCfg;
                std::string npcPath;
                if (MovementJsonConfig::instance().loadPresetInto(preset, npcCfg, &npcPath))
                {
                    next.movementPreset = preset;
                    mNpcMovement = npcCfg;
                    mHasNpcMovement = true;
                    mNpcPresetPath = npcPath;
                    mNpcPresetWrite = getLastWrite(npcPath);
                    Debug::warn(Debug::Category::NpcCombat,
                        "[NPC DIFFICULTY] NPC movement preset: %s (%s)\n",
                        preset.c_str(), npcPath.c_str());
                }
                else
                {
                    Debug::warn(Debug::Category::NpcCombat,
                        "[NPC DIFFICULTY] Unknown movementPreset '%s'; falling back to 'follow'.\n",
                        preset.c_str());
                }
            }
        }

        mRoot = root;
        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Loaded %s: maxErr=%.1fdeg diffScale=%.2f dmg=%.2fx fireDelay=[%.2f,%.2f] aggressionBonus=%.2f hitRadius=%.2f forceHit=%d panic=%d loadout=%zu mirror=%d\n",
            fileName.c_str(),
            mData.maxAngularErrorDegrees, mData.difficultyErrorScale,
            mData.damageMultiplier, mData.fireDelayMin, mData.fireDelayMax,
            mData.aggressionBonus, mData.npcHitRadius, (int)mData.forceHit,
            (int)mData.hitReactionEnabled, mData.weaponLoadout.size(),
            (int)mData.mirrorMovementEnabled);
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
    if (mHasNpcMovement && !mNpcPresetPath.empty())
    {
        const auto npcWrite = getLastWrite(mNpcPresetPath);
        if (npcWrite != mNpcPresetWrite)
        {
            Debug::warn(Debug::Category::NpcCombat,
                "[NPC DIFFICULTY] NPC movement preset changed on disk: %s\n",
                fileNameOf(mNpcPresetPath).c_str());
            return load(mPath);
        }
    }

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
    j["movementPreset"] = mData.movementPreset;

    j["weaponLoadout"] = json::array();
    for (const auto& wid : mData.weaponLoadout)
        j["weaponLoadout"].push_back(wid);
    j["startingWeapon"] = mData.startingWeapon;
    j["switchCooldown"] = mData.switchCooldown;
    j["closeSwitchDist"] = mData.closeSwitchDist;
    j["farSwitchDist"] = mData.farSwitchDist;
    j["hitReactionEnabled"] = mData.hitReactionEnabled;
    j["hitReactionDurationScale"] = mData.hitReactionDurationScale;
    j["dashChance"] = mData.dashChance;
    j["downDashChance"] = mData.downDashChance;
    j["freezeChance"] = mData.freezeChance;
    j["movementNoiseScale"] = mData.movementNoiseScale;
    j["jukeFrequency"] = mData.jukeFrequency;
    j["forceWeapon"] = mData.forceWeapon;

    j["mirrorMovementEnabled"] = mData.mirrorMovementEnabled;
    j["mirrorNormalDuration"] = mData.mirrorNormalDuration;
    j["mirrorReplayDuration"] = mData.mirrorReplayDuration;
    j["mirrorHistorySeconds"] = mData.mirrorHistorySeconds;
    j["mirrorKeepAimingAtTarget"] = mData.mirrorKeepAimingAtTarget;
    j["mirrorDashEnabled"] = mData.mirrorDashEnabled;
    j["mirrorDownDashEnabled"] = mData.mirrorDownDashEnabled;
    j["mirrorFreezeEnabled"] = mData.mirrorFreezeEnabled;
    j["mirrorJumpEnabled"] = mData.mirrorJumpEnabled;

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
