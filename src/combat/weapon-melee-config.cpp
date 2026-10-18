// 2026-10-17
/* purpose
* Parses and owns the data-driven Melee move-set component (config/weapons.json).
* Validates and clamps phase ticks to 1..999 and clamps hitbox active windows to
* the active phase so changing a phase length cannot desynchronize combat timing.
* Publishes a monotonically increasing revision and a content hash per move set.
* Does NOT run combat, sweeps, damage, movement, animation, or networking.
* Does NOT add a C++ function per move; JSON selects and configures this owner.
*/

#include "weapon-melee-config.h"

#include "../debug/debug-log.h"
#include "../debug/structured-log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "../utils/path_utils.h"

namespace WeaponData {

namespace {

using json = nlohmann::json;

bool loadClipTimeline(const std::string& clipId, int& duration,
                      int& windupEnd, int& activeEnd)
{
    const std::filesystem::path root(resolveAssetPath("config/animations/library"));
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec)
        return false;

    bool found = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) return false;
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        std::ifstream file(entry.path());
        if (!file.is_open()) continue;
        json document;
        try { file >> document; } catch (...) { continue; }
        if (!document.contains("clips") || !document["clips"].is_object() ||
            !document["clips"].contains(clipId)) continue;
        if (found) return false; // ambiguous IDs are invalid, never pick by directory order
        found = true;
        const json& clip = document["clips"][clipId];
        if (!clip.is_object() || !clip.contains("durationTicks") ||
            !clip["durationTicks"].is_number_integer() ||
            !clip.contains("markers") || !clip["markers"].is_array()) return false;
        duration = clip["durationTicks"].get<int>();
        int starts[4] = {-1, -1, -1, -1};
        const char* names[4] = {"phase.windup", "phase.active", "phase.recovery", "action.end"};
        for (const json& marker : clip["markers"]) {
            if (!marker.is_object() || !marker.contains("name") ||
                !marker["name"].is_string() || !marker.contains("tick") ||
                !marker["tick"].is_number_integer()) continue;
            const std::string name = marker["name"].get<std::string>();
            for (int i = 0; i < 4; ++i) {
                if (name == names[i]) {
                    if (starts[i] >= 0) return false;
                    starts[i] = marker["tick"].get<int>();
                }
            }
        }
        if (starts[0] != 0 || starts[1] <= starts[0] ||
            starts[2] <= starts[1] || starts[3] <= starts[2] ||
            starts[3] != duration) return false;
        windupEnd = starts[1];
        activeEnd = starts[2];
    }
    return found;
}

std::string normalizedToken(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return c == '_' || c == '-' || c == ' ';
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

MeleeButton parseButton(const std::string& raw, bool& ok)
{
    ok = true;
    const std::string t = normalizedToken(raw);
    if (t == "primary" || t == "left" || t == "a" || t == "lmb" ||
        t == "attack1" || t == "normal")
        return MeleeButton::Primary;
    if (t == "secondary" || t == "right" || t == "b" || t == "rmb" ||
        t == "attack2" || t == "special")
        return MeleeButton::Secondary;
    if (t == "grab" || t == "e")
        return MeleeButton::Grab;
    if (t == "shield" || t == "q" || t == "block")
        return MeleeButton::Shield;
    ok = false;
    return MeleeButton::Primary;
}

MeleeActorState parseActorState(const std::string& raw, bool& ok)
{
    ok = true;
    const std::string t = normalizedToken(raw);
    if (t == "any" || t.empty())
        return MeleeActorState::Any;
    if (t == "grounded" || t == "ground" || t == "on_ground" || t == "onground")
        return MeleeActorState::Grounded;
    if (t == "airborne" || t == "air" || t == "in_air" || t == "inair")
        return MeleeActorState::Airborne;
    ok = false;
    return MeleeActorState::Any;
}

MeleeModifier parseModifier(const std::string& raw)
{
    const std::string t = normalizedToken(raw);
    if (t == "jump" || t == "space")
        return MeleeModifier::Jump;
    if (t == "crouch" || t == "ctrl" || t == "down" || t == "control")
        return MeleeModifier::Crouch;
    return MeleeModifier::None;
}

bool jsonVec3(const json& root, const char* key, glm::vec3& out)
{
    if (!root.contains(key) || !root[key].is_array() || root[key].size() < 3)
        return false;
    out = glm::vec3(root[key][0].get<float>(),
                    root[key][1].get<float>(),
                    root[key][2].get<float>());
    return true;
}

void parsePhase(const json& j, MeleePhase& out)
{
    if (!j.is_object())
        return;
    if (j.contains("ticks") && j["ticks"].is_number())
        out.ticks = j["ticks"].get<int>();
    out.ticks = std::clamp(out.ticks, 1, 999);
    if (j.contains("clip_id") && j["clip_id"].is_string())
        out.clipId = j["clip_id"].get<std::string>();
    else if (j.contains("clip") && j["clip"].is_string())
        out.clipId = j["clip"].get<std::string>();
    else if (j.contains("pose") && j["pose"].is_string())
        out.clipId = j["pose"].get<std::string>();
}

void parseHitbox(const json& j, MeleeHitbox& out)
{
    if (!j.is_object())
        return;
    if (j.contains("id") && j["id"].is_string())
        out.id = j["id"].get<std::string>();
    if (j.contains("attached_part") && j["attached_part"].is_string())
        out.attachedPart = j["attached_part"].get<std::string>();
    else if (j.contains("part") && j["part"].is_string())
        out.attachedPart = j["part"].get<std::string>();
    if (j.contains("shape") && j["shape"].is_string())
        out.shape = normalizedToken(j["shape"].get<std::string>());
    jsonVec3(j, "size", out.size);
    jsonVec3(j, "local_offset", out.localOffset);
    if (j.contains("per_tick") && j["per_tick"].is_array()) {
        for (const json& entry : j["per_tick"]) {
            if (!entry.is_object() || !entry.contains("tick") || !entry["tick"].is_number_integer())
                continue;
            MeleeHitboxTick override;
            override.tick = entry["tick"].get<int>();
            override.attachedPart = out.attachedPart;
            override.shape = out.shape;
            override.size = out.size;
            override.localOffset = out.localOffset;
            if (entry.contains("attached_part") && entry["attached_part"].is_string())
                override.attachedPart = entry["attached_part"].get<std::string>();
            if (entry.contains("shape") && entry["shape"].is_string())
                override.shape = normalizedToken(entry["shape"].get<std::string>());
            jsonVec3(entry, "size", override.size);
            jsonVec3(entry, "local_offset", override.localOffset);
            out.perTick.push_back(std::move(override));
        }
    }
    if (j.contains("active_start_tick") && j["active_start_tick"].is_number())
        out.activeStartTick = j["active_start_tick"].get<int>();
    if (j.contains("active_end_tick") && j["active_end_tick"].is_number())
        out.activeEndTick = j["active_end_tick"].get<int>();
}

void parseImpulse(const json& j, MeleeImpulse& out)
{
    if (!j.is_object())
        return;
    if (j.contains("phase") && j["phase"].is_string())
        out.phase = normalizedToken(j["phase"].get<std::string>());
    if (j.contains("tick") && j["tick"].is_number())
        out.tick = j["tick"].get<int>();
    if (j.contains("forward") && j["forward"].is_number())
        out.forward = j["forward"].get<float>();
    if (j.contains("right") && j["right"].is_number())
        out.right = j["right"].get<float>();
    if (j.contains("up") && j["up"].is_number())
        out.up = j["up"].get<float>();
    glm::vec3 v(0.0f);
    if (jsonVec3(j, "impulse", v)) {
        out.forward = v.x;
        out.right = v.y;
        out.up = v.z;
    }
}

void parseAudioEvent(const json& j, MeleeAudioEvent& out)
{
    if (!j.is_object())
        return;
    if (j.contains("phase") && j["phase"].is_string())
        out.phase = normalizedToken(j["phase"].get<std::string>());
    if (j.contains("tick") && j["tick"].is_number())
        out.tick = j["tick"].get<int>();
    if (j.contains("sound_id") && j["sound_id"].is_string())
        out.soundId = j["sound_id"].get<std::string>();
    else if (j.contains("sound") && j["sound"].is_string())
        out.soundId = j["sound"].get<std::string>();
    if (j.contains("pitch") && j["pitch"].is_number())
        out.pitch = j["pitch"].get<float>();
    if (j.contains("volume") && j["volume"].is_number())
        out.volume = j["volume"].get<float>();
    jsonVec3(j, "offset", out.offset);
}

bool parseAction(const std::string& actionId, const json& j, MeleeAction& out)
{
    if (!j.is_object())
        return false;
    out.id = actionId;
    if (j.contains("id") && j["id"].is_string() && !j["id"].get<std::string>().empty())
        out.id = j["id"].get<std::string>();
    if (out.id.empty())
        return false;

    if (j.contains("input") && j["input"].is_object()) {
        const json& in = j["input"];
        bool ok = true;
        if (in.contains("button") && in["button"].is_string())
            out.button = parseButton(in["button"].get<std::string>(), ok);
        else if (in.contains("actor_state") && in["actor_state"].is_string())
            ok = ok; // no-op; button defaults below
        if (in.contains("actor_state") && in["actor_state"].is_string()) {
            bool stateOk = true;
            out.actorState = parseActorState(in["actor_state"].get<std::string>(), stateOk);
            ok = ok && stateOk;
        }
        if (in.contains("modifier") && in["modifier"].is_string())
            out.modifier = parseModifier(in["modifier"].get<std::string>());
        if (!ok) {
            Debug::warn(Debug::Category::Weapons,
                "[MELEE] action '%s' has an unrecognized input condition; skipped\n",
                out.id.c_str());
            return false;
        }
    }
    if (j.contains("modifier") && j["modifier"].is_string())
        out.modifier = parseModifier(j["modifier"].get<std::string>());
    if (j.contains("button") && j["button"].is_string()) {
        bool ok = true;
        out.button = parseButton(j["button"].get<std::string>(), ok);
    }
    if (j.contains("actor_state") && j["actor_state"].is_string()) {
        bool ok = true;
        out.actorState = parseActorState(j["actor_state"].get<std::string>(), ok);
    }

    if (j.contains("priority") && j["priority"].is_number())
        out.priority = j["priority"].get<int>();
    if (j.contains("double_click_window_ticks") && j["double_click_window_ticks"].is_number())
        out.doubleClickWindowTicks = std::max(0, j["double_click_window_ticks"].get<int>());
    if (j.contains("double_click_action") && j["double_click_action"].is_string())
        out.doubleClickAction = j["double_click_action"].get<std::string>();

    if (j.contains("animation_clip_id") && !j["animation_clip_id"].is_string()) {
        Debug::warn(Debug::Category::Weapons,
            "[MELEE] action '%s' has a non-string animation_clip_id; skipped\n",
            out.id.c_str());
        return false;
    }
    if (j.contains("animation_clip_id") && j["animation_clip_id"].is_string()) {
        out.animationClipId = j["animation_clip_id"].get<std::string>();
        int duration = 0, activeStart = 0, recoveryStart = 0;
        if (out.animationClipId.empty() ||
            !loadClipTimeline(out.animationClipId, duration, activeStart, recoveryStart)) {
            Debug::warn(Debug::Category::Weapons,
                "[MELEE] action '%s' has missing, ambiguous, or invalid clip timeline '%s'; skipped\n",
                out.id.c_str(), out.animationClipId.c_str());
            return false;
        }
        out.windup.ticks = activeStart;
        out.active.ticks = recoveryStart - activeStart;
        out.recovery.ticks = duration - recoveryStart;
    } else if (j.contains("phases") && j["phases"].is_object()) {
        // Explicit compatibility for existing actions not migrated to one clip yet.
        const json& p = j["phases"];
        if (p.contains("windup")) parsePhase(p["windup"], out.windup);
        if (p.contains("active")) parsePhase(p["active"], out.active);
        if (p.contains("recovery")) parsePhase(p["recovery"], out.recovery);
    }

    if (j.contains("hitboxes") && j["hitboxes"].is_array()) {
        for (const json& h : j["hitboxes"]) {
            MeleeHitbox hb;
            parseHitbox(h, hb);
            if (!hb.attachedPart.empty())
                out.hitboxes.push_back(hb);
        }
    } else if (j.contains("hitbox") && j["hitbox"].is_object()) {
        MeleeHitbox hb;
        parseHitbox(j["hitbox"], hb);
        if (!hb.attachedPart.empty())
            out.hitboxes.push_back(hb);
    }
    // Clamp each hitbox window to the action's active phase.
    for (MeleeHitbox& hb : out.hitboxes) {
        hb.activeStartTick = std::clamp(hb.activeStartTick, 0, std::max(0, out.active.ticks - 1));
        hb.activeEndTick = std::clamp(hb.activeEndTick, hb.activeStartTick,
                                      std::max(0, out.active.ticks - 1));
    }

    if (j.contains("damage") && j["damage"].is_object()) {
        const json& d = j["damage"];
        if (d.contains("base_percent") && d["base_percent"].is_number())
            out.basePercent = d["base_percent"].get<float>();
        if (d.contains("shield_damage") && d["shield_damage"].is_number()) {
            out.shieldDamage = d["shield_damage"].get<float>();
            out.hasShieldDamage = true;
        }
    }
    if (j.contains("knockback") && j["knockback"].is_object()) {
        const json& k = j["knockback"];
        if (k.contains("base_impulse") && k["base_impulse"].is_number())
            out.baseKnockback = k["base_impulse"].get<float>();
        if (k.contains("vertical_fraction") && k["vertical_fraction"].is_number())
            out.knockbackVerticalFraction = k["vertical_fraction"].get<float>();
        if (k.contains("force_scale") && k["force_scale"].is_number())
            out.knockbackForceScale = k["force_scale"].get<float>();
    }
    if (j.contains("grab") && j["grab"].is_object()) {
        const json& g = j["grab"];
        if (g.contains("enabled") && g["enabled"].is_boolean())
            out.isGrab = g["enabled"].get<bool>();
        if (g.contains("radius") && g["radius"].is_number())
            out.grabRadius = g["radius"].get<float>();
        if (g.contains("pull_ticks") && g["pull_ticks"].is_number())
            out.grabPullTicks = std::max(1, g["pull_ticks"].get<int>());
    }

    if (j.contains("impulses") && j["impulses"].is_array()) {
        for (const json& i : j["impulses"]) {
            MeleeImpulse imp;
            parseImpulse(i, imp);
            out.impulses.push_back(imp);
        }
    }

    if (j.contains("audio") && j["audio"].is_object()) {
        const json& a = j["audio"];
        if (a.contains("on_start") && a["on_start"].is_object()) {
            const json& s = a["on_start"];
            if (s.contains("sound_id") && s["sound_id"].is_string())
                out.onStartSound = s["sound_id"].get<std::string>();
            else if (s.contains("sound") && s["sound"].is_string())
                out.onStartSound = s["sound"].get<std::string>();
            if (s.contains("pitch") && s["pitch"].is_number())
                out.onStartPitch = s["pitch"].get<float>();
            if (s.contains("volume") && s["volume"].is_number())
                out.onStartVolume = s["volume"].get<float>();
            jsonVec3(s, "offset", out.onStartOffset);
        }
        if (a.contains("events") && a["events"].is_array()) {
            for (const json& e : a["events"]) {
                MeleeAudioEvent ev;
                parseAudioEvent(e, ev);
                out.audioEvents.push_back(ev);
            }
        }
    }

    // A grab action is always a grab even if only grab.radius was authored.
    if (out.grabRadius > 0.0f && out.id.find("grab") != std::string::npos)
        out.isGrab = true;

    return true;
}

} // namespace

void MeleeMoveSetRegistry::set(MeleeMoveSet moveSet)
{
    static uint32_t sRevisionCounter = 0;
    moveSet.revision = ++sRevisionCounter;
    const std::string defId = moveSet.definitionId;
    const size_t actionCount = moveSet.actions.size();
    const uint32_t hash = moveSet.hash;
    const uint32_t revision = moveSet.revision;
    mSets[defId] = std::move(moveSet);
    // Durable owner-level diagnostic: proves the data-driven move set loaded
    // and published a revision/hash without needing the console.
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Weapons,
        StructuredLevel::Important,
        "weapon.melee.moveset_loaded",
        defId,
        "data-driven Melee move set parsed and published",
        0,
        {{"definition_id", defId},
         {"action_count", actionCount},
         {"hash", hash},
         {"revision", revision}},
        __FILE__, __LINE__, __func__);
}

const MeleeMoveSet* MeleeMoveSetRegistry::get(const std::string& definitionId) const
{
    auto it = mSets.find(definitionId);
    return it != mSets.end() ? &it->second : nullptr;
}

void MeleeMoveSetRegistry::clear()
{
    mSets.clear();
}

MeleeMoveSetRegistry& MeleeMoveSetRegistry::instance()
{
    static MeleeMoveSetRegistry registry;
    return registry;
}

bool parseMeleeMoveSet(const std::string& definitionId,
                       const nlohmann::json& root,
                       MeleeMoveSet& out)
{
    if (!root.is_object())
        return false;
    if (!root.contains("components") || !root["components"].is_object())
        return false;
    const json& components = root["components"];
    if (!components.contains("melee") || !components["melee"].is_object())
        return false;
    const json& melee = components["melee"];
    if (!melee.contains("actions"))
        return false;

    MeleeMoveSet parsed;
    parsed.definitionId = definitionId;

    const json& actions = melee["actions"];
    if (actions.is_object()) {
        for (auto it = actions.begin(); it != actions.end(); ++it) {
            MeleeAction action;
            if (parseAction(it.key(), it.value(), action))
                parsed.actions.push_back(std::move(action));
        }
    } else if (actions.is_array()) {
        for (const json& a : actions) {
            MeleeAction action;
            std::string id = (a.is_object() && a.contains("id") && a["id"].is_string())
                ? a["id"].get<std::string>() : std::string();
            if (parseAction(id, a, action))
                parsed.actions.push_back(std::move(action));
        }
    }

    if (parsed.actions.empty())
        return false;

    parsed.hash = (uint32_t)std::hash<std::string>{}(melee.dump());
    out = std::move(parsed);
    Debug::log(Debug::Category::Weapons,
        "[MELEE] parsed moveset '%s' actions=%zu hash=%u\n",
        definitionId.c_str(), out.actions.size(), out.hash);
    return true;
}

} // namespace WeaponData
