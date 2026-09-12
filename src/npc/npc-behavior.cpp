// 09 11 2026
/* purpose
* Loads, hot-reloads, and indexes reusable NPC behavior profiles.
* Profiles carry combat tuning values only; roles reference them by id.
* Does NOT assign profiles or contain combat logic.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/

#include "npc/npc-behavior.h"

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
    out.baseFear = j.value("base_fear", out.baseFear);
    out.baseConfidence = j.value("base_confidence", out.baseConfidence);
    out.damagePanicSensitivity = j.value("damage_panic_sensitivity", out.damagePanicSensitivity);
    out.damageFearSensitivity = j.value("damage_fear_sensitivity", out.damageFearSensitivity);
    out.panicAimPenalty = j.value("panic_aim_penalty", out.panicAimPenalty);
    out.fearRetreatWeight = j.value("fear_retreat_weight", out.fearRetreatWeight);
    out.confidenceAttackWeight = j.value("confidence_attack_weight", out.confidenceAttackWeight);
    out.panicDecayPerSecond = j.value("panic_decay_per_second", out.panicDecayPerSecond);
    out.stressDecayPerSecond = j.value("stress_decay_per_second", out.stressDecayPerSecond);
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
    // Emotional baseline + sensitivity.
    if (def->baseFear >= 0.0f) out.baseFear = def->baseFear;
    if (def->baseConfidence >= 0.0f) out.baseConfidence = def->baseConfidence;
    if (def->damagePanicSensitivity >= 0.0f) out.damagePanicSensitivity = def->damagePanicSensitivity;
    if (def->damageFearSensitivity >= 0.0f) out.damageFearSensitivity = def->damageFearSensitivity;
    if (def->panicAimPenalty >= 0.0f) out.panicAimPenalty = def->panicAimPenalty;
    if (def->fearRetreatWeight >= 0.0f) out.fearRetreatWeight = def->fearRetreatWeight;
    if (def->confidenceAttackWeight >= 0.0f) out.confidenceAttackWeight = def->confidenceAttackWeight;
    if (def->panicDecayPerSecond >= 0.0f) out.panicDecayPerSecond = def->panicDecayPerSecond;
    if (def->stressDecayPerSecond >= 0.0f) out.stressDecayPerSecond = def->stressDecayPerSecond;
    return out;
}
