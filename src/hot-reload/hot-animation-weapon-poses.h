// 09 23 2026
/* purpose
* The afad20a per-weapon arm pose table, restored as a hot evaluator.
*
* At afad20a, config/animations.json held `weapons.<id>.poses.<state>` with
* `leftArm`/`rightArm` translation + rotation and a `useWeaponPose` gate. The
* animator picked a state from the weapon runtime facts (equipping > reloading >
* shooting/slash/lunge > just_shot > idle) and, for arms only, REPLACED the
* rotation and ADDED the translation.
*
* This header owns the table parse (mtime-refreshed) and the state selection.
* `weaponPoseSource` selects the implementation: "json" reads the table above;
* "cpp" leaves the compiled carry-stance/tool-phase path in charge. Both are
* hot: the selector and the JSON are read live.
* Hot-only header. Does NOT link into the EXE.
*/
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation-clips.h"

namespace HotWeaponPose {

struct ArmPose {
    float trans[3] = {0.0f, 0.0f, 0.0f};
    float rot[3] = {0.0f, 0.0f, 0.0f};
};
struct Pose {
    ArmPose left;
    ArmPose right;
    bool useWeaponPose = true;
};
struct WeaponEntry {
    std::unordered_map<std::string, Pose> poses;
    std::string activePose = "idle";
};

struct Cache {
    bool loaded = false;
    std::filesystem::file_time_type write{};
    bool sourceJson = true;
    std::unordered_map<std::uint64_t, WeaponEntry> weapons;
};

inline void readArm(const nlohmann::json& node, ArmPose& out)
{
    if (!node.is_object())
        return;
    const auto tr = node.value("translation", nlohmann::json::array());
    const auto ro = node.value("rotation", nlohmann::json::array());
    for (int k = 0; k < 3 && k < (int)tr.size(); ++k)
        if (tr[k].is_number())
            out.trans[k] = tr[k].get<float>();
    for (int k = 0; k < 3 && k < (int)ro.size(); ++k)
        if (ro[k].is_number())
            out.rot[k] = ro[k].get<float>();
}

inline void reload(Cache& c)
{
    c.weapons.clear();
    c.sourceJson = true;
    std::ifstream file("config/animations.json");
    if (!file) {
        c.loaded = false;
        return;
    }
    try {
        const nlohmann::json root =
            nlohmann::json::parse(file, nullptr, true, true);
        c.sourceJson =
            root.value("weaponPoseSource", std::string("json")) == "json";
        const auto weapons = root.value("weapons", nlohmann::json::object());
        for (auto wit = weapons.begin(); wit != weapons.end(); ++wit) {
            if (!wit.value().is_object())
                continue;
            WeaponEntry entry;
            entry.activePose = wit.value().value("active_pose", "idle");
            const auto poses = wit.value().value("poses", nlohmann::json::object());
            for (auto pit = poses.begin(); pit != poses.end(); ++pit) {
                if (!pit.value().is_object())
                    continue;
                Pose p;
                p.useWeaponPose =
                    pit.value().value("useWeaponPose",
                                      pit.value().value("use_weapon_pose", false));
                readArm(pit.value().value("leftArm", nlohmann::json::object()),
                        p.left);
                readArm(pit.value().value("rightArm", nlohmann::json::object()),
                        p.right);
                entry.poses[pit.key()] = p;
            }
            c.weapons[gameHash(wit.key().c_str())] = std::move(entry);
        }
        c.loaded = true;
    } catch (...) {
        c.loaded = false;
    }
}

inline Cache& cache()
{
    static Cache c;
    std::error_code ec;
    const auto write = std::filesystem::last_write_time("config/animations.json", ec);
    if (!ec && (!c.loaded || write != c.write)) {
        reload(c);
        c.write = write;
    }
    return c;
}

// "json" reads the weapons table; "cpp" leaves the compiled path in charge.
inline bool sourceIsJson() { return cache().sourceJson; }

// afad20a state precedence mapped from the hot action id.
inline const char* primaryState(std::uint64_t actionId)
{
    switch (actionId) {
        case HOT_ACTION_EQUIP: return "equipping";
        case HOT_ACTION_RELOAD: return "reloading";
        case HOT_ACTION_SLASH: return "slash";
        case HOT_ACTION_LUNGE: return "lunge";
        case HOT_ACTION_SHOOT: return "shooting";
        case HOT_ACTION_JUST_SHOT: return "cooldown";
        default: return "idle";
    }
}
inline const char* fallbackState(const char* state)
{
    if (std::string(state) == "equipping") return "equip";
    if (std::string(state) == "reloading") return "reload";
    if (std::string(state) == "shooting") return "fire";
    if (std::string(state) == "cooldown") return "just_shot";
    if (std::string(state) == "idle") return "equipped";
    return nullptr;
}

// Resolve the afad20a arm pose for a weapon + action. Returns false when the
// weapon is unknown or no usable pose exists (caller keeps its current arms).
inline bool poseFor(std::uint64_t weaponKey, std::uint64_t actionId, Pose& out)
{
    Cache& c = cache();
    if (!c.loaded || weaponKey == 0)
        return false;
    auto it = c.weapons.find(weaponKey);
    if (it == c.weapons.end())
        return false;
    const WeaponEntry& entry = it->second;
    const char* candidates[4] = {primaryState(actionId),
                                 fallbackState(primaryState(actionId)),
                                 entry.activePose.c_str(), "idle"};
    for (const char* name : candidates) {
        if (!name)
            continue;
        auto pit = entry.poses.find(name);
        if (pit != entry.poses.end() && pit->second.useWeaponPose) {
            out = pit->second;
            return true;
        }
    }
    return false;
}

// afad20a arm application: REPLACE rotation, ADD translation, arms only.
inline void apply(HotAnim::Pose& target, const Pose& p)
{
    const std::uint32_t arms = HotAnim::MaskArms;
    for (int k = 0; k < 3; ++k) {
        target.part[HotAnim::PartLeftArm].rot[k] = p.left.rot[k];
        target.part[HotAnim::PartLeftArm].trans[k] += p.left.trans[k];
        target.part[HotAnim::PartRightArm].rot[k] = p.right.rot[k];
        target.part[HotAnim::PartRightArm].trans[k] += p.right.trans[k];
    }
    target.mask |= arms;
}

// Deterministic self-test: state mapping, arm application semantics, and (when
// the table is present) a known-weapon resolution.
inline bool runWeaponPoseSelfTest(char* message, std::uint32_t messageSize)
{
    auto fail = [&](const char* m) {
        if (message && messageSize)
            std::snprintf(message, messageSize, "%s", m);
        return false;
    };
    if (std::string(primaryState(HOT_ACTION_EQUIP)) != "equipping")
        return fail("weapon pose: equip state mapping");
    if (std::string(primaryState(HOT_ACTION_RELOAD)) != "reloading")
        return fail("weapon pose: reload state mapping");
    if (std::string(primaryState(HOT_ACTION_SLASH)) != "slash")
        return fail("weapon pose: slash state mapping");
    if (std::string(primaryState(HOT_ACTION_SHOOT)) != "shooting")
        return fail("weapon pose: shoot state mapping");
    if (std::string(primaryState(HOT_ACTION_JUST_SHOT)) != "cooldown")
        return fail("weapon pose: just_shot state mapping");
    if (std::string(fallbackState("shooting")) != "fire")
        return fail("weapon pose: shoot fallback mapping");

    HotAnim::Pose pose{};
    pose.part[HotAnim::PartLeftArm].rot[0] = 5.0f;
    pose.part[HotAnim::PartLeftArm].trans[0] = 1.0f;
    Pose wp{};
    wp.left.rot[0] = -20.0f;
    wp.left.trans[0] = 0.2f;
    apply(pose, wp);
    if (pose.part[HotAnim::PartLeftArm].rot[0] != -20.0f)
        return fail("weapon pose: rotation not replaced");
    if (std::fabs(pose.part[HotAnim::PartLeftArm].trans[0] - 1.2f) > 1e-4f)
        return fail("weapon pose: translation not added");
    if ((pose.mask & HotAnim::MaskArms) != HotAnim::MaskArms)
        return fail("weapon pose: arms mask missing");

    Cache& c = cache();
    if (c.loaded) {
        Pose rp{};
        if (!poseFor(gameHash("revolver"), HOT_ACTION_IDLE, rp))
            return fail("weapon pose: revolver idle unresolved");
    }
    return true;
}

} // namespace HotWeaponPose
