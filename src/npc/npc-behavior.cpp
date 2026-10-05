// 09 11 2026
/* purpose
* Loads, hot-reloads, and indexes reusable NPC behavior profiles.
* Profiles carry combat and target-pursuit tuning; roles reference them by id.
* Does NOT assign profiles or contain combat logic.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "npc/npc-behavior.h"

#include <algorithm>
#include <cmath>
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

// Clamp a numeric field and emit an explicit validation diagnostic naming the
// profile and field when the requested value was out of range. Missing or
// wrong-typed fields fall back to `def` without a warning (safe default).
float clampWarn(const json& j, const char* key, float lo, float hi, float def,
                const std::string& profileId)
{
    if (!j.contains(key) || !j[key].is_number())
        return def;
    const float requested = j[key].get<float>();
    const float clamped = std::clamp(requested, lo, hi);
    if (std::fabs(clamped - requested) > 1e-6f)
        Debug::warn(Debug::Category::NpcCombat,
            "[BEHAVIOR] %s.%s=%.3f out of range [%.2f,%.2f]; clamped to %.3f\n",
            profileId.c_str(), key, requested, lo, hi, clamped);
    return clamped;
}

bool boolOr(const json& j, const char* key, bool def)
{
    return (j.contains(key) && j[key].is_boolean()) ? j[key].get<bool>() : def;
}

void readProfile(const json& j, const std::string& fallbackId,
                 BehaviorProfileDefinition& out)
{
    out.id = j.value("id", fallbackId);
    out.aimErrorDeg = j.value("aim_error_deg", out.aimErrorDeg);
    out.reactionDelay = j.value("reaction_delay", out.reactionDelay);
    out.fireCadenceMultiplier = j.value("fire_cadence_multiplier", out.fireCadenceMultiplier);
    out.aggression = j.value("aggression", out.aggression);
    out.preferredRange = j.value("preferred_range", out.preferredRange);
    out.targetStickiness = j.value("target_stickiness", out.targetStickiness);
    out.targetSwitchThreshold = j.value("target_switch_threshold", out.targetSwitchThreshold);
    out.lowHealthTargetBias = j.value("low_health_target_bias", out.lowHealthTargetBias);
    out.threatBias = j.value("threat_bias", out.threatBias);
    out.distanceTargetBias = j.value("distance_target_bias", out.distanceTargetBias);
    out.weaponRangeBias = j.value("weapon_range_bias", out.weaponRangeBias);
    out.weaponDamageBias = j.value("weapon_damage_bias", out.weaponDamageBias);
    out.weaponSafetyBias = j.value("weapon_safety_bias", out.weaponSafetyBias);
    out.weaponSwitchThreshold = j.value("weapon_switch_threshold", out.weaponSwitchThreshold);
    out.pursuitMode = j.value("pursuit_mode", out.pursuitMode);
    out.continueThroughCover = j.value("continue_through_cover", out.continueThroughCover);
    out.pursueLastKnownPosition = j.value("pursue_last_known_position", out.pursueLastKnownPosition);
    out.afterReachingLastKnown = j.value("after_reaching_last_known", out.afterReachingLastKnown);
    out.pursuitSearchTicks = j.value("pursuit_search_ticks", out.pursuitSearchTicks);
    out.informationMode = j.value("information_mode", out.informationMode);
    out.radarDelayTicks = j.value("radar_delay_ticks", out.radarDelayTicks);
    out.radarErrorMeters = j.value("radar_error_meters", out.radarErrorMeters);
    out.radarMemoryMode = j.value("radar_memory_mode", out.radarMemoryMode);
    out.radarMemoryTicks = j.value("radar_memory_ticks", out.radarMemoryTicks);
    out.rememberedPathPoints = j.value("remembered_path_points", out.rememberedPathPoints);
    out.continuePredictedPath = j.value("continue_predicted_path", out.continuePredictedPath);

    // Movement replanning + commitment. Timers keep the -1 sentinel when
    // omitted; the nested commitment object carries concrete safe defaults.
    out.repathIntervalSeconds = clampWarn(j, "repath_interval_seconds",
                                          0.1f, 30.0f, out.repathIntervalSeconds, out.id);
    out.goalMoveThresholdMeters = clampWarn(j, "goal_move_threshold_meters",
                                            0.0f, 50.0f, out.goalMoveThresholdMeters, out.id);
    if (j.contains("movement_commitment") && j["movement_commitment"].is_object()) {
        const json& c = j["movement_commitment"];
        out.commitmentEnabled = boolOr(c, "enabled", out.commitmentEnabled);
        out.commitmentDirectionSeconds = clampWarn(
            c, "direction_commit_seconds", 0.5f, 120.0f,
            out.commitmentDirectionSeconds, out.id);
        out.commitmentProgressCheckSeconds = clampWarn(
            c, "progress_check_seconds", 0.1f, 10.0f,
            out.commitmentProgressCheckSeconds, out.id);
        out.commitmentMinimumProgressMeters = clampWarn(
            c, "minimum_progress_meters", 0.0f, 20.0f,
            out.commitmentMinimumProgressMeters, out.id);
        out.commitmentCandidateDistanceMeters = clampWarn(
            c, "candidate_distance_meters", 2.0f, 40.0f,
            out.commitmentCandidateDistanceMeters, out.id);
        out.commitmentAllowReverse = boolOr(c, "allow_reverse", out.commitmentAllowReverse);
        out.commitmentAvoidRecentPath = boolOr(c, "avoid_recent_path", out.commitmentAvoidRecentPath);
        out.commitmentRecentPathAvoidRadius = clampWarn(
            c, "recent_path_avoid_radius", 0.5f, 30.0f,
            out.commitmentRecentPathAvoidRadius, out.id);
        out.commitmentVisibleEnemyAllowsCombatMovement = boolOr(
            c, "visible_enemy_allows_combat_movement",
            out.commitmentVisibleEnemyAllowsCombatMovement);
        out.commitmentForwardBias = clampWarn(
            c, "forward_bias", 0.0f, 50.0f, out.commitmentForwardBias, out.id);
        out.commitmentTargetProgressBias = clampWarn(
            c, "target_progress_bias", 0.0f, 50.0f,
            out.commitmentTargetProgressBias, out.id);
        out.commitmentOpenDistanceBias = clampWarn(
            c, "open_distance_bias", 0.0f, 50.0f, out.commitmentOpenDistanceBias, out.id);
        out.commitmentReversePenalty = clampWarn(
            c, "reverse_penalty", 0.0f, 50.0f, out.commitmentReversePenalty, out.id);
    }
    out.travelTargetDistanceMeters = clampWarn(
        j, "travel_target_distance_meters", 15.0f, 300.0f,
        out.travelTargetDistanceMeters, out.id);
    out.travelTargetReachedMeters = clampWarn(
        j, "travel_target_reached_meters", 2.0f, 30.0f,
        out.travelTargetReachedMeters, out.id);
}

} // anonymous namespace

BehaviorProfileRegistry& BehaviorProfileRegistry::instance()
{
    static BehaviorProfileRegistry registry;
    return registry;
}

bool BehaviorProfileRegistry::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::NpcCombat,
            "[BEHAVIOR] Missing %s; no behavior profiles loaded.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        root = json::parse(file, nullptr, true, true);

        std::vector<BehaviorProfileDefinition> profiles;
        if (root.contains("profiles")) {
            const auto& p = root["profiles"];
            if (p.is_array()) {
                for (const auto& item : p) {
                    if (!item.is_object()) continue;
                    BehaviorProfileDefinition def;
                    readProfile(item, "", def);
                    if (!def.id.empty()) profiles.push_back(std::move(def));
                }
            } else if (p.is_object()) {
                for (auto it = p.begin(); it != p.end(); ++it) {
                    if (!it.value().is_object()) continue;
                    BehaviorProfileDefinition def;
                    readProfile(it.value(), it.key(), def);
                    if (!def.id.empty()) profiles.push_back(std::move(def));
                }
            }
        }

        mProfiles = std::move(profiles);
        mIndexById.clear();
        for (int i = 0; i < (int)mProfiles.size(); ++i)
            mIndexById[mProfiles[i].id] = i;

        mLastWrite = writeTime;
        ++mRevision;
        if (!mWatchLogged) {
            Debug::warn(Debug::Category::NpcCombat,
                "[BEHAVIOR] Watching: %s\n", fileNameOf(mPath).c_str());
            mWatchLogged = true;
        }
        Debug::warn(Debug::Category::NpcCombat,
            "[BEHAVIOR] Loaded %zu profile(s) from %s\n",
            mProfiles.size(), fileNameOf(mPath).c_str());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::NpcCombat,
            "[BEHAVIOR] Parse error in %s: %s. Keeping previous data.\n", mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::NpcCombat,
            "[BEHAVIOR] Error loading %s: %s. Keeping previous data.\n", mPath.c_str(), e.what());
    }
    return false;
}

bool BehaviorProfileRegistry::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::NpcCombat,
        "[BEHAVIOR] Detected change: %s\n", fileNameOf(mPath).c_str());
    return load(mPath);
}

const BehaviorProfileDefinition* BehaviorProfileRegistry::get(const std::string& id) const
{
    auto it = mIndexById.find(id);
    if (it == mIndexById.end()) return nullptr;
    const int index = it->second;
    if (index < 0 || index >= (int)mProfiles.size()) return nullptr;
    return &mProfiles[index];
}

NpcBehaviorTuning resolveNpcBehavior(const std::string& id)
{
    NpcBehaviorTuning out;
    if (id.empty())
        return out;

    const BehaviorProfileDefinition* def = BehaviorProfileRegistry::instance().get(id);
    if (!def)
        return out;

    out.active = true;
    out.aimErrorDeg = def->aimErrorDeg;
    out.reactionDelay = std::max(0.0f, def->reactionDelay);
    out.fireCadenceMultiplier = std::max(0.1f, def->fireCadenceMultiplier);
    out.aggression = def->aggression;
    out.preferredRange = def->preferredRange;
    // Target selection: only override when the profile sets the field (>= 0).
    if (def->targetStickiness >= 0.0f) out.targetStickiness = def->targetStickiness;
    if (def->targetSwitchThreshold >= 0.0f) out.targetSwitchThreshold = def->targetSwitchThreshold;
    if (def->lowHealthTargetBias >= 0.0f) out.lowHealthTargetBias = def->lowHealthTargetBias;
    if (def->threatBias >= 0.0f) out.threatBias = def->threatBias;
    if (def->distanceTargetBias >= 0.0f) out.distanceTargetBias = def->distanceTargetBias;
    // Weapon selection.
    if (def->weaponRangeBias >= 0.0f) out.weaponRangeBias = def->weaponRangeBias;
    if (def->weaponDamageBias >= 0.0f) out.weaponDamageBias = def->weaponDamageBias;
    if (def->weaponSafetyBias >= 0.0f) out.weaponSafetyBias = def->weaponSafetyBias;
    if (def->weaponSwitchThreshold >= 0.0f) out.weaponSwitchThreshold = def->weaponSwitchThreshold;
    out.pursuitMode = def->pursuitMode;
    out.continueThroughCover = def->continueThroughCover;
    out.pursueLastKnownPosition = def->pursueLastKnownPosition;
    out.afterReachingLastKnown = def->afterReachingLastKnown;
    out.pursuitSearchTicks = std::max(0, def->pursuitSearchTicks);
    out.informationMode = def->informationMode;
    out.radarDelayTicks = std::max(0, def->radarDelayTicks);
    out.radarErrorMeters = std::max(0.0f, def->radarErrorMeters);
    out.radarMemoryMode = def->radarMemoryMode;
    out.radarMemoryTicks = std::max(1, def->radarMemoryTicks);
    out.rememberedPathPoints = std::clamp(def->rememberedPathPoints, 1, 120);
    out.continuePredictedPath = def->continuePredictedPath;
    // Movement replanning + commitment. -1 sentinel on the timers keeps the
    // compiled compatibility defaults for profiles that omit them.
    out.repathIntervalSeconds = def->repathIntervalSeconds >= 0.0f
        ? def->repathIntervalSeconds : 0.9f;
    out.goalMoveThresholdMeters = def->goalMoveThresholdMeters >= 0.0f
        ? def->goalMoveThresholdMeters : 2.5f;
    out.commitmentEnabled = def->commitmentEnabled;
    out.commitmentDirectionSeconds = def->commitmentDirectionSeconds;
    out.commitmentProgressCheckSeconds = def->commitmentProgressCheckSeconds;
    out.commitmentMinimumProgressMeters = def->commitmentMinimumProgressMeters;
    out.commitmentCandidateDistanceMeters = def->commitmentCandidateDistanceMeters;
    out.commitmentAllowReverse = def->commitmentAllowReverse;
    out.commitmentAvoidRecentPath = def->commitmentAvoidRecentPath;
    out.commitmentRecentPathAvoidRadius = def->commitmentRecentPathAvoidRadius;
    out.commitmentVisibleEnemyAllowsCombatMovement = def->commitmentVisibleEnemyAllowsCombatMovement;
    out.commitmentForwardBias = def->commitmentForwardBias;
    out.commitmentTargetProgressBias = def->commitmentTargetProgressBias;
    out.commitmentOpenDistanceBias = def->commitmentOpenDistanceBias;
    out.commitmentReversePenalty = def->commitmentReversePenalty;
    out.travelTargetDistanceMeters = def->travelTargetDistanceMeters;
    out.travelTargetReachedMeters = def->travelTargetReachedMeters;
    return out;
}

bool behaviorProfileSelfTest(std::string& report)
{
    bool ok = true;
    auto check = [&](bool cond, const char* what) {
        report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
        ok = ok && cond;
    };

    const std::string id = "selftest";
    {
        // All fields supplied.
        BehaviorProfileDefinition def;
        readProfile(json::parse(R"({
            "id": "selftest",
            "repath_interval_seconds": 2.0,
            "goal_move_threshold_meters": 8.0,
            "movement_commitment": {
                "enabled": true,
                "direction_commit_seconds": 12.0,
                "progress_check_seconds": 1.5,
                "minimum_progress_meters": 0.75,
                "candidate_distance_meters": 18.0,
                "allow_reverse": true,
                "avoid_recent_path": false,
                "recent_path_avoid_radius": 4.0,
                "visible_enemy_allows_combat_movement": false,
                "forward_bias": 3.0,
                "target_progress_bias": 5.0,
                "open_distance_bias": 7.0,
                "reverse_penalty": 9.0
            }
        })"), "", def);
        check(def.id == id, "id parsed");
        check(std::fabs(def.repathIntervalSeconds - 2.0f) < 1e-5f, "repath interval parsed");
        check(std::fabs(def.goalMoveThresholdMeters - 8.0f) < 1e-5f, "goal threshold parsed");
        check(def.commitmentEnabled, "commitment enabled parsed");
        check(std::fabs(def.commitmentDirectionSeconds - 12.0f) < 1e-5f, "direction commit parsed");
        check(std::fabs(def.commitmentProgressCheckSeconds - 1.5f) < 1e-5f, "progress check parsed");
        check(std::fabs(def.commitmentMinimumProgressMeters - 0.75f) < 1e-5f, "min progress parsed");
        check(std::fabs(def.commitmentCandidateDistanceMeters - 18.0f) < 1e-5f, "candidate distance parsed");
        check(def.commitmentAllowReverse, "allow reverse parsed");
        check(!def.commitmentAvoidRecentPath, "avoid recent path parsed");
        check(std::fabs(def.commitmentRecentPathAvoidRadius - 4.0f) < 1e-5f, "recent radius parsed");
        check(!def.commitmentVisibleEnemyAllowsCombatMovement, "visible-enemy rule parsed");
        check(std::fabs(def.commitmentForwardBias - 3.0f) < 1e-5f, "forward bias parsed");
        check(std::fabs(def.commitmentTargetProgressBias - 5.0f) < 1e-5f, "target progress bias parsed");
        check(std::fabs(def.commitmentOpenDistanceBias - 7.0f) < 1e-5f, "open distance bias parsed");
        check(std::fabs(def.commitmentReversePenalty - 9.0f) < 1e-5f, "reverse penalty parsed");
    }
    {
        // Omitted fields keep safe defaults; timers keep the -1 sentinel.
        BehaviorProfileDefinition def;
        readProfile(json::parse(R"({"id": "selftest"})"), "", def);
        check(def.repathIntervalSeconds < 0.0f, "omitted repath keeps sentinel");
        check(def.goalMoveThresholdMeters < 0.0f, "omitted goal threshold keeps sentinel");
        check(def.commitmentEnabled, "omitted commitment enabled defaults true");
        check(std::fabs(def.commitmentDirectionSeconds - 10.0f) < 1e-5f, "omitted direction commit default");
        check(std::fabs(def.commitmentCandidateDistanceMeters - 20.0f) < 1e-5f, "omitted candidate distance default");
    }
    {
        // Out-of-range values clamp instead of being silently accepted.
        BehaviorProfileDefinition def;
        readProfile(json::parse(R"({
            "id": "selftest",
            "repath_interval_seconds": 100.0,
            "goal_move_threshold_meters": -5.0,
            "movement_commitment": {
                "direction_commit_seconds": 0.0,
                "progress_check_seconds": 0.0,
                "candidate_distance_meters": 999.0,
                "forward_bias": 999.0,
                "reverse_penalty": 999.0
            }
        })"), "", def);
        check(std::fabs(def.repathIntervalSeconds - 30.0f) < 1e-5f, "repath clamps high to 30");
        check(std::fabs(def.goalMoveThresholdMeters - 0.0f) < 1e-5f, "goal threshold clamps low to 0");
        check(std::fabs(def.commitmentDirectionSeconds - 0.5f) < 1e-5f, "direction commit clamps low to 0.5");
        check(std::fabs(def.commitmentProgressCheckSeconds - 0.1f) < 1e-5f, "progress check clamps low to 0.1");
        check(std::fabs(def.commitmentCandidateDistanceMeters - 40.0f) < 1e-5f, "candidate distance clamps to 40");
        check(std::fabs(def.commitmentForwardBias - 50.0f) < 1e-5f, "forward bias clamps to 50");
        check(std::fabs(def.commitmentReversePenalty - 50.0f) < 1e-5f, "reverse penalty clamps to 50");
    }
    {
        // resolveNpcBehavior applies the compatibility defaults to a profile
        // that omits the timers (matches the compiled 0.9 s / 2.5 m values).
        NpcBehaviorTuning t;
        BehaviorProfileDefinition def;
        def.repathIntervalSeconds = -1.0f;
        def.goalMoveThresholdMeters = -1.0f;
        t.repathIntervalSeconds = def.repathIntervalSeconds >= 0.0f
            ? def.repathIntervalSeconds : 0.9f;
        check(std::fabs(t.repathIntervalSeconds - 0.9f) < 1e-5f, "resolve keeps 0.9 s compatibility default");
    }

    report += ok ? "  PASS\n" : "  FAIL\n";
    return ok;
}
