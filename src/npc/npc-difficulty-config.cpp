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

int optInt(const json& root, const char* key, int fallback)
{
    if (root.contains(key) && root[key].is_number_integer())
        return root[key].get<int>();
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
        next.spawnFireDelayMinTicks = std::max(0, optInt(
            root, "spawnFireDelayMinTicks", next.spawnFireDelayMinTicks));
        next.spawnFireDelayMaxTicks = std::max(next.spawnFireDelayMinTicks, optInt(
            root, "spawnFireDelayMaxTicks", next.spawnFireDelayMaxTicks));
        next.spawnActionDelayTicks = std::max(0, optInt(
            root, "spawnActionDelayTicks", next.spawnActionDelayTicks));
        next.freezeDuringWaveBanner = optBool(
            root, "freezeDuringWaveBanner", next.freezeDuringWaveBanner);
        next.aggressionBonus = optFloat(root, "aggressionBonus", next.aggressionBonus);
        next.npcHitRadius = std::max(0.0f, optFloat(root, "npcHitRadius", next.npcHitRadius));
        next.forceHit = optBool(root, "forceHit", next.forceHit);
        next.npcDebugVisuals = optBool(root, "npcDebugVisuals", next.npcDebugVisuals);
        next.targetMode = lowercaseCopy(optString(root, "targetMode", next.targetMode));
        if (next.targetMode != "closest" && next.targetMode != "player") {
            Debug::warn(Debug::Category::NpcCombat,
                "[NPC DIFFICULTY] Invalid targetMode '%s'; using 'closest'.\n",
                next.targetMode.c_str());
            next.targetMode = "closest";
        }
        next.damageOtherNpcs = optBool(root, "damageOtherNpcs", next.damageOtherNpcs);
        next.perceptionFovDegrees = std::clamp(optFloat(root, "perceptionFovDegrees", next.perceptionFovDegrees), 5.0f, 360.0f);
        next.perceptionSightRange = std::max(1.0f, optFloat(root, "perceptionSightRange", next.perceptionSightRange));
        next.perceptionHearingRange = std::max(0.0f, optFloat(root, "perceptionHearingRange", next.perceptionHearingRange));
        next.perceptionReactionTicks = std::max(0, optInt(root, "perceptionReactionTicks", next.perceptionReactionTicks));
        next.perceptionMemoryTicks = std::max(1, optInt(root, "perceptionMemoryTicks", next.perceptionMemoryTicks));
        next.perceptionPredictionSeconds = std::max(0.0f, optFloat(root, "perceptionPredictionSeconds", next.perceptionPredictionSeconds));
        next.perceptionPredictionErrorMeters = std::max(0.0f, optFloat(root, "perceptionPredictionErrorMeters", next.perceptionPredictionErrorMeters));
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
        next.wallAvoidanceEnabled = optBool(root, "wallAvoidanceEnabled", next.wallAvoidanceEnabled);
        next.wallCastDistance = std::clamp(optFloat(root, "wallCastDistance", next.wallCastDistance), 0.25f, 8.0f);
        next.wallSearchDistance = std::clamp(optFloat(root, "wallSearchDistance", next.wallSearchDistance), 0.5f, 8.0f);
        next.wallBacktrackEnabled = optBool(root, "wallBacktrackEnabled", next.wallBacktrackEnabled);
        next.wallBacktrackDistance = std::clamp(optFloat(root, "wallBacktrackDistance", next.wallBacktrackDistance), 0.5f, 12.0f);
        next.wallBacktrackDuration = std::clamp(optFloat(root, "wallBacktrackDuration", next.wallBacktrackDuration), 0.25f, 6.0f);
        next.wallGroundSupportRequired = optBool(root, "wallGroundSupportRequired", next.wallGroundSupportRequired);
        next.wallGroundProbeDepth = std::clamp(optFloat(root, "wallGroundProbeDepth", next.wallGroundProbeDepth), 0.5f, 20.0f);

        // Search / exploration memory.
        next.searchHeadingCommitSeconds = std::clamp(
            optFloat(root, "searchHeadingCommitSeconds", next.searchHeadingCommitSeconds), 0.5f, 30.0f);
        next.searchMemorySeconds = std::clamp(
            optFloat(root, "searchMemorySeconds", next.searchMemorySeconds), 2.0f, 60.0f);
        next.searchMemoryPoints = std::clamp(
            optInt(root, "searchMemoryPoints", next.searchMemoryPoints), 4, 32);
        next.searchSnapshotSeconds = std::clamp(
            optFloat(root, "searchSnapshotSeconds", next.searchSnapshotSeconds), 0.1f, 2.0f);
        next.searchLookahead = std::clamp(
            optFloat(root, "searchLookahead", next.searchLookahead), 1.0f, 12.0f);
        next.searchAvoidRadius = std::clamp(
            optFloat(root, "searchAvoidRadius", next.searchAvoidRadius), 0.5f, 12.0f);
        next.searchNoProgressSeconds = std::clamp(
            optFloat(root, "searchNoProgressSeconds", next.searchNoProgressSeconds), 0.5f, 10.0f);

        // Live-tunable movement / navigation knobs.
        next.wallAvoidMinProbe = std::clamp(
            optFloat(root, "wallAvoidMinProbe", next.wallAvoidMinProbe), 1.0f, 12.0f);
        next.turnSideBias = std::clamp(
            optFloat(root, "turnSideBias", next.turnSideBias), 0.0f, 4.0f);
        next.jumpCooldownSeconds = std::clamp(
            optFloat(root, "jumpCooldownSeconds", next.jumpCooldownSeconds), 0.0f, 3.0f);
        next.areaEscapeRadiusMeters = std::clamp(
            optFloat(root, "areaEscapeRadiusMeters", next.areaEscapeRadiusMeters), 1.0f, 30.0f);
        next.areaEscapeSeconds = std::clamp(
            optFloat(root, "areaEscapeSeconds", next.areaEscapeSeconds), 1.0f, 30.0f);
        next.areaEscapeHoldSeconds = std::clamp(
            optFloat(root, "areaEscapeHoldSeconds", next.areaEscapeHoldSeconds), 0.5f, 10.0f);
        next.lowObstacleJumpEnabled = optBool(
            root, "lowObstacleJumpEnabled", next.lowObstacleJumpEnabled);
        next.lowObstacleProbe = std::clamp(
            optFloat(root, "lowObstacleProbe", next.lowObstacleProbe), 0.3f, 4.0f);
        next.lowObstacleLowOffset = std::clamp(
            optFloat(root, "lowObstacleLowOffset", next.lowObstacleLowOffset), -2.5f, 1.0f);
        next.lowObstacleHighOffset = std::clamp(
            optFloat(root, "lowObstacleHighOffset", next.lowObstacleHighOffset), -1.0f, 2.5f);
        next.exploreDistanceMeters = std::clamp(
            optFloat(root, "exploreDistanceMeters", next.exploreDistanceMeters), 15.0f, 300.0f);
        next.exploreHoldSeconds = std::clamp(
            optFloat(root, "exploreHoldSeconds", next.exploreHoldSeconds), 2.0f, 60.0f);
        next.exploreMinProgressMeters = std::clamp(
            optFloat(root, "exploreMinProgressMeters", next.exploreMinProgressMeters), 0.5f, 40.0f);
        next.useNavGraph = optBool(root, "useNavGraph", next.useNavGraph);
        next.navGraphChunkSize = std::clamp(
            optFloat(root, "navGraphChunkSize", next.navGraphChunkSize), 8.0f, 128.0f);
        next.navGraphCellSize = std::clamp(
            optFloat(root, "navGraphCellSize", next.navGraphCellSize), 0.5f, 6.0f);
        next.navGraphMaxRoutes = std::clamp(
            optInt(root, "navGraphMaxRoutes", next.navGraphMaxRoutes), 1, 8);
        next.navGraphMaxDropHeight = std::clamp(
            optFloat(root, "navGraphMaxDropHeight", next.navGraphMaxDropHeight), 1.0f, 60.0f);

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
        ++mRevision;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::NpcCombat,
            "[NPC DIFFICULTY] Loaded %s: maxErr=%.1fdeg diffScale=%.2f dmg=%.2fx fireDelay=[%.2f,%.2f] spawnFireDelayTicks=[%d,%d] aggressionBonus=%.2f hitRadius=%.2f forceHit=%d panic=%d targetMode=%s damageOtherNpcs=%d loadout=%zu mirror=%d\n",
            fileName.c_str(),
            mData.maxAngularErrorDegrees, mData.difficultyErrorScale,
            mData.damageMultiplier, mData.fireDelayMin, mData.fireDelayMax,
            mData.spawnFireDelayMinTicks, mData.spawnFireDelayMaxTicks,
            mData.aggressionBonus, mData.npcHitRadius, (int)mData.forceHit,
            (int)mData.hitReactionEnabled, mData.targetMode.c_str(),
            (int)mData.damageOtherNpcs, mData.weaponLoadout.size(),
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
    j["spawnFireDelayMinTicks"] = mData.spawnFireDelayMinTicks;
    j["spawnFireDelayMaxTicks"] = mData.spawnFireDelayMaxTicks;
    j["spawnActionDelayTicks"] = mData.spawnActionDelayTicks;
    j["freezeDuringWaveBanner"] = mData.freezeDuringWaveBanner;
    j["aggressionBonus"] = mData.aggressionBonus;
    j["npcHitRadius"] = mData.npcHitRadius;
    j["forceHit"] = mData.forceHit;
    j["npcDebugVisuals"] = mData.npcDebugVisuals;
    j["targetMode"] = mData.targetMode;
    j["damageOtherNpcs"] = mData.damageOtherNpcs;
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
    j["wallAvoidanceEnabled"] = mData.wallAvoidanceEnabled;
    j["wallCastDistance"] = mData.wallCastDistance;
    j["wallSearchDistance"] = mData.wallSearchDistance;
    j["wallBacktrackEnabled"] = mData.wallBacktrackEnabled;
    j["wallBacktrackDistance"] = mData.wallBacktrackDistance;
    j["wallBacktrackDuration"] = mData.wallBacktrackDuration;
    j["wallGroundSupportRequired"] = mData.wallGroundSupportRequired;
    j["wallGroundProbeDepth"] = mData.wallGroundProbeDepth;
    j["searchHeadingCommitSeconds"] = mData.searchHeadingCommitSeconds;
    j["searchMemorySeconds"] = mData.searchMemorySeconds;
    j["searchMemoryPoints"] = mData.searchMemoryPoints;
    j["searchSnapshotSeconds"] = mData.searchSnapshotSeconds;
    j["searchLookahead"] = mData.searchLookahead;
    j["searchAvoidRadius"] = mData.searchAvoidRadius;
    j["searchNoProgressSeconds"] = mData.searchNoProgressSeconds;
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
