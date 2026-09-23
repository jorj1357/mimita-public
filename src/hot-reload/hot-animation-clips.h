// 09 16 2026
/* purpose
* Shared hot procedural animation library. Plain-data C++ clips and pose
* functions for every gameplay action; sampling, interpolation, masking, and
* blending live here so the state machine (animation-policy) and the pose
* generator (pose-generation) share ONE source of truth. Tables are authored in
* degrees for readability and converted to the boundary unit (radians) only at
* skeleton.apply. No STL/pointers cross the hot boundary.
* AnimationClipSource::ProceduralCpp is the active source; ImportedAsset is the
* reserved extension point for future Blender/GLB keyframes and is not active.
* Does NOT own actor state, draw submission, or networking.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"

namespace HotAnim {

enum class AnimationClipSource : std::uint32_t {
    ProceduralCpp = 0,
    ImportedAsset = 1,   // reserved extension point; no active importer yet
};

enum PartId : std::uint32_t {
    PartTorso = 0,
    PartHead,
    PartLeftArm,
    PartRightArm,
    PartLeftLeg,
    PartRightLeg,
    PartCount,
};

static constexpr std::uint32_t kMaxParts = 16;

static constexpr std::uint32_t MaskTorso = 1u << PartTorso;
static constexpr std::uint32_t MaskHead = 1u << PartHead;
static constexpr std::uint32_t MaskLeftArm = 1u << PartLeftArm;
static constexpr std::uint32_t MaskRightArm = 1u << PartRightArm;
static constexpr std::uint32_t MaskLeftLeg = 1u << PartLeftLeg;
static constexpr std::uint32_t MaskRightLeg = 1u << PartRightLeg;
static constexpr std::uint32_t MaskFull = MaskTorso | MaskHead | MaskLeftArm |
                                          MaskRightArm | MaskLeftLeg | MaskRightLeg;
static constexpr std::uint32_t MaskUpper =
    MaskTorso | MaskHead | MaskLeftArm | MaskRightArm;
static constexpr std::uint32_t MaskArms = MaskLeftArm | MaskRightArm;
static constexpr std::uint32_t MaskLower = MaskLeftLeg | MaskRightLeg;

inline std::uint64_t partHash(std::uint32_t part)
{
    switch (part) {
        case PartTorso: return gameHash("torso");
        case PartHead: return gameHash("head");
        case PartLeftArm: return gameHash("leftArm");
        case PartRightArm: return gameHash("rightArm");
        case PartLeftLeg: return gameHash("leftLeg");
        case PartRightLeg: return gameHash("rightLeg");
        default: return 0;
    }
}

inline int partIndexFromHash(std::uint64_t hash)
{
    for (std::uint32_t i = 0; i < PartCount; ++i)
        if (partHash(i) == hash)
            return (int)i;
    return -1;
}

// The body parts every procedural clip requires. A skeleton missing any of
// these fails validation; optional/extra parts are simply ignored.
static constexpr std::uint32_t kRequiredPartCount = PartCount;

inline std::uint64_t requiredPartHash(std::uint32_t index)
{
    return index < PartCount ? partHash(index) : 0;
}

static constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;

// One part of a pose. Rotations are XYZ euler in degrees (converted at the
// skeleton.apply boundary to keep every consumer on one canonical unit).
struct PartPose {
    float trans[3];
    float rot[3];
};

struct Pose {
    PartPose part[PartCount];
    std::uint32_t mask;

    void clear()
    {
        mask = 0;
        for (std::uint32_t i = 0; i < PartCount; ++i) {
            for (int k = 0; k < 3; ++k) {
                part[i].trans[k] = 0.0f;
                part[i].rot[k] = 0.0f;
            }
        }
    }
};

struct Keyframe {
    float t;                  // seconds
    float part[PartCount][6]; // [tx,ty,tz, rx,ry,rz] degrees
};

struct ActionClip {
    float duration;           // seconds
    std::uint8_t loop;        // 1 = loop, 0 = one-shot
    std::uint8_t fullBody;    // 1 = replace whole body, 0 = upper-body overlay
    std::uint32_t mask;       // parts this clip drives
    const Keyframe* frames;   // may be null for procedurally evaluated clips
    std::uint32_t frameCount;
};

inline void lerpPart(PartPose& out, const float a[6], const float b[6], float w)
{
    for (int k = 0; k < 3; ++k) {
        out.trans[k] = a[k] + (b[k] - a[k]) * w;
        out.rot[k] = a[3 + k] + (b[3 + k] - a[3 + k]) * w;
    }
}

// Sample a keyframed clip. Looping wraps the time; one-shots clamp to the last
// frame so a finished action holds its final pose instead of snapping to rest.
inline void sampleClip(const ActionClip& clip, float time, Pose& out)
{
    out.clear();
    out.mask = clip.mask;
    if (!clip.frames || clip.frameCount == 0)
        return;
    if (clip.duration > 1e-5f) {
        if (clip.loop)
            time = time - clip.duration * std::floor(time / clip.duration);
        else {
            // afad20a one-shots clamp to durationTicks - 1, so the JSON and C++
            // samplers hold the same final pose instead of diverging by a tick.
            const float maxT = std::max(0.0f, clip.duration - 1.0f / 60.0f);
            if (time > maxT)
                time = maxT;
        }
    }
    if (time <= clip.frames[0].t) {
        for (std::uint32_t i = 0; i < PartCount; ++i)
            lerpPart(out.part[i], clip.frames[0].part[i], clip.frames[0].part[i], 0.0f);
        return;
    }
    for (std::uint32_t f = 1; f < clip.frameCount; ++f) {
        if (time <= clip.frames[f].t) {
            const Keyframe& a = clip.frames[f - 1];
            const Keyframe& b = clip.frames[f];
            const float span = b.t - a.t;
            const float w = span > 1e-5f ? (time - a.t) / span : 0.0f;
            for (std::uint32_t i = 0; i < PartCount; ++i)
                lerpPart(out.part[i], a.part[i], b.part[i], w);
            return;
        }
    }
    const Keyframe& last = clip.frames[clip.frameCount - 1];
    for (std::uint32_t i = 0; i < PartCount; ++i)
        lerpPart(out.part[i], last.part[i], last.part[i], 0.0f);
}

inline ActionClip actionClip(std::uint64_t actionId);

// ── Procedural locomotion poses (speed-scaled) ──────────────────────

inline void evaluateIdle(float t, bool equipped, Pose& out)
{
    out.clear();
    out.mask = MaskFull;
    const float sway = std::sin(t * 1.6f);
    const float breathe = std::sin(t * 1.1f);
    const float head = std::sin(t * 0.8f);
    // Clear, visible idle sway/breathing on every part.
    out.part[PartTorso].rot[2] = sway * 2.0f;
    out.part[PartTorso].rot[0] = breathe * 1.1f;
    out.part[PartTorso].trans[1] = breathe * 0.03f;
    out.part[PartHead].rot[0] = head * 1.0f;
    out.part[PartHead].rot[1] = sway * 1.1f;
    // rot[0], rot[1], and rot[2] are the local X/Y/Z Euler axes, in degrees.
    // In this model rot[2] is the arm front/back axis. Change the index on
    // both arm assignments below if the model's imported axis convention is
    // changed. Do not animate two rotation axes if the desired motion is a
    // simple front/back swing: animating two axes at once makes a circle.
    const float armX = equipped ? -58.0f : 0.0f;
    // This is the center pose on the other axes. Change -58.0f to alter the
    // equipped carry angle; change 0.0f to give empty hands a fixed tilt.
    out.part[PartLeftArm].rot[0] = armX;
    out.part[PartRightArm].rot[0] = armX;

    // std::sin(...) makes the smooth back-and-forth motion. The value inside
    // sin is time * speed: increase 1.5f to move faster, decrease it to move
    // slower. The multiplier outside sin controls range in degrees: increase
    // 8.0f/10.0f for a larger front/back swing, or decrease it for a smaller
    // swing. The right-arm minus sign makes the arms alternate; remove it if
    // both arms should move toward the same side together.
    const float frontBack = std::sin(t * 0.5f) * (equipped ? 10.0f : 8.0f);
    out.part[PartLeftArm].rot[2] = frontBack;
    out.part[PartRightArm].rot[2] = -frontBack;
    out.part[PartLeftLeg].rot[0] = std::sin(t * 1.5f + 0.6f) * 2.0f;
    out.part[PartRightLeg].rot[0] = std::sin(t * 1.5f + 2.0f) * 2.0f;
}

// Walking is intent-driven and always plays at the same procedural speed and
// amplitude (no velocity scaling); the leg cycle keeps a readable stride.
inline void evaluateWalk(float t, float /*speed01*/, Pose& out)
{
    out.clear();
    out.mask = MaskFull;
    // rot[0], rot[1], rot[2] are local X/Y/Z Euler axes in degrees.  This
    // model uses rot[2] for the visible arm/leg front-back swing. If the
    // imported model uses a different axis, change only the rot[index] on
    // both arm lines below (and the matching leg lines if needed).
    const float freq = 2.0f; // walk-cycle speed: larger = faster, smaller = slower
    const float p = t * freq; // phase: time multiplied by the cycle speed
    const float swing = std::sin(p); // smooth -1..+1 back/forth movement
    const float amp = 38.0f; // leg range in degrees; larger = longer stride
    const float armAmp = 30.0f; // arm range in degrees; larger = more arm movement
    out.part[PartLeftLeg].rot[2] = swing * amp;
    out.part[PartRightLeg].rot[2] = -swing * amp;
    // Arms alternate opposite to the legs. Change the minus sign to plus if
    // both arms should move in the same direction. The old fixed 5/-5 values
    // overwrote the animated arm pose, which is why walking arms barely moved.
    out.part[PartLeftArm].rot[2] = -swing * armAmp;
    out.part[PartRightArm].rot[2] = swing * armAmp;
    out.part[PartTorso].rot[2] = swing * 1.1f;
    out.part[PartTorso].trans[1] = std::fabs(std::sin(p * 2.0f)) * 0.03f;
    out.part[PartHead].rot[2] = -2.0f + std::sin(p * 2.0f) * 2.0f;
}

// Derive the locomotion base action from generic facts so the upper-body
// overlay can compose against it. Single source of truth for the base pose.
inline std::uint64_t locomotionAction(bool grounded, float vy, bool justLanded,
                                      float speed01, bool equipped)
{
    if (!grounded)
        return vy > 0.5f ? HOT_ACTION_JUMP : HOT_ACTION_FALL;
    if (justLanded)
        return HOT_ACTION_LAND;
    if (speed01 > 0.08f)
        return HOT_ACTION_WALK;
    return equipped ? HOT_ACTION_EQUIPPED_IDLE : HOT_ACTION_IDLE;
}

inline ActionClip actionClip(std::uint64_t actionId);
inline bool jsonClipApplied(std::uint64_t actionId);
inline bool afad20aSourceCpp();
inline float afad20aWalkSpeedScale();
inline void afad20aSampleClip(const ActionClip& clip, float timeSeconds,
                              float speedScale, Pose& out);

inline void evaluateAction(std::uint64_t actionId, float time, float speed01,
                           Pose& out)
{
    // JSON/afad20a keyframes win for locomotion; otherwise the procedural
    // evaluators below are the compiled fallback. Both sources sample the same
    // cached frames, so the C++ and JSON implementations agree.
    if ((actionId == HOT_ACTION_IDLE || actionId == HOT_ACTION_EQUIPPED_IDLE ||
         actionId == HOT_ACTION_WALK ||
         actionId == HOT_ACTION_RETURN_TO_IDLE) &&
        jsonClipApplied(actionId)) {
        const ActionClip clip = actionClip(actionId);
        if (afad20aSourceCpp())
            afad20aSampleClip(clip, time, afad20aWalkSpeedScale(), out);
        else
            sampleClip(clip, time, out);
        return;
    }
    switch (actionId) {
        case HOT_ACTION_IDLE: evaluateIdle(time, false, out); return;
        case HOT_ACTION_EQUIPPED_IDLE: evaluateIdle(time, true, out); return;
        case HOT_ACTION_WALK: evaluateWalk(time, speed01, out); return;
        default: break;
    }
    sampleClip(actionClip(actionId), time, out);
}

// ── Masking and blending ────────────────────────────────────────────

inline void applyMask(Pose& dst, const Pose& src, std::uint32_t mask)
{
    for (std::uint32_t i = 0; i < PartCount; ++i) {
        if ((mask & (1u << i)) == 0)
            continue;
        dst.part[i] = src.part[i];
    }
    dst.mask |= (src.mask & mask);
}

// Blend `b` over `a` by weight w in [0,1] for the union of their masks. Parts
// absent from one side fall back to the other so a partial clip never snaps an
// unmasked part to rest.
inline void blendPose(const Pose& a, const Pose& b, float w, Pose& out)
{
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;
    out = b;
    out.mask = a.mask | b.mask;
    for (std::uint32_t i = 0; i < PartCount; ++i) {
        const bool inA = (a.mask & (1u << i)) != 0;
        const bool inB = (b.mask & (1u << i)) != 0;
        if (inA && inB) {
            for (int k = 0; k < 3; ++k) {
                out.part[i].trans[k] = a.part[i].trans[k] +
                    (b.part[i].trans[k] - a.part[i].trans[k]) * w;
                out.part[i].rot[k] = a.part[i].rot[k] +
                    (b.part[i].rot[k] - a.part[i].rot[k]) * w;
            }
        } else if (inA) {
            out.part[i] = a.part[i];
        }
    }
}

// ── Weapon arm overrides ────────────────────────────────────────────
// A carried-weapon pose for locomotion/idle. Action poses (shoot/reload/slash)
// keep their own arms; the weapon override only shapes the carry stance so
// different tools read differently without a per-weapon animation.
// Per-weapon arm rotations can be authored in config/animations.json under
// `weaponArms` (behaviorSource json). The compiled table below is the fallback.
struct WeaponArmV1 {
    float leftT[3] = {0.0f, 0.0f, 0.0f};
    float rightT[3] = {0.0f, 0.0f, 0.0f};
    float leftX = 0.0f;
    float rightX = 0.0f;
    float leftZ = 0.0f;
    float rightZ = 0.0f;
};

inline const std::unordered_map<std::uint64_t, WeaponArmV1>& jsonWeaponArms()
{
    static std::unordered_map<std::uint64_t, WeaponArmV1> map;
    static std::uint64_t lastWrite = 0;
    std::error_code ec;
    const auto ft = std::filesystem::last_write_time("config/animations.json", ec);
    const std::uint64_t write =
        ec ? 0ull : static_cast<std::uint64_t>(ft.time_since_epoch().count());
    if (write != lastWrite) {
        lastWrite = write;
        map.clear();
        std::ifstream file("config/animations.json");
        if (file) {
            try {
                const auto j = nlohmann::json::parse(file, nullptr, true, true);
                if (j.value("behaviorSource", "cpp") == "json" &&
                    j.contains("weaponArms") && j["weaponArms"].is_object()) {
                    for (auto it = j["weaponArms"].begin();
                         it != j["weaponArms"].end(); ++it) {
                        if (!it.value().is_object())
                            continue;
                        WeaponArmV1 a;
                        a.leftX = it.value().value("leftX", 0.0f);
                        a.rightX = it.value().value("rightX", 0.0f);
                        a.leftZ = it.value().value("leftZ", 0.0f);
                        a.rightZ = it.value().value("rightZ", 0.0f);
                        map[gameHash(it.key().c_str())] = a;
                    }
                }
                // afad20a used weapons.<id>.poses.<active_pose>. Keep that
                // authored contract live alongside the newer compact schema.
                if (j.value("behaviorSource", "cpp") == "json" &&
                    j.contains("weapons") && j["weapons"].is_object()) {
                    for (auto it = j["weapons"].begin();
                         it != j["weapons"].end(); ++it) {
                        const auto& weapon = it.value();
                        const std::string poseName =
                            weapon.value("active_pose", "idle");
                        const auto poses = weapon.value("poses", nlohmann::json::object());
                        const auto pit = poses.find(poseName);
                        if (pit == poses.end() || !pit->is_object())
                            continue;
                        WeaponArmV1 a;
                        const auto readArm = [&](const char* name, float t[3],
                                                 float& x, float& z) {
                            const auto arm = pit.value().value(name, nlohmann::json::object());
                            const auto tr = arm.value("translation", nlohmann::json::array());
                            const auto ro = arm.value("rotation", nlohmann::json::array());
                            for (int k = 0; k < 3 && k < (int)tr.size(); ++k)
                                if (tr[k].is_number()) t[k] = tr[k].get<float>();
                            if (ro.size() > 0 && ro[0].is_number()) x = ro[0].get<float>();
                            if (ro.size() > 2 && ro[2].is_number()) z = ro[2].get<float>();
                        };
                        readArm("leftArm", a.leftT, a.leftX, a.leftZ);
                        readArm("rightArm", a.rightT, a.rightX, a.rightZ);
                        map[gameHash(it.key().c_str())] = a;
                    }
                }
            } catch (...) {
                map.clear();
            }
        }
    }
    return map;
}

inline bool weaponCarryArms(std::uint64_t weaponKey, float& outLeftX,
                            float& outRightX, float& outLeftZ, float& outRightZ)
{
    if (weaponKey == 0)
        return false;
    // JSON per-weapon arm pose wins when authored and authoritative.
    const auto& jsonArms = jsonWeaponArms();
    const auto found = jsonArms.find(weaponKey);
    if (found != jsonArms.end()) {
        outLeftX = found->second.leftX;
        outRightX = found->second.rightX;
        outLeftZ = found->second.leftZ;
        outRightZ = found->second.rightZ;
        return true;
    }
    const std::uint64_t sword = gameHash("swordsword");
    const std::uint64_t knife = gameHash("spyknife");
    const std::uint64_t revolver = gameHash("revolver");
    const std::uint64_t shotgun = gameHash("shotgun");
    const std::uint64_t rocket = gameHash("rocket_launcher");
    const std::uint64_t grenade = gameHash("grenade_launcher");
    if (weaponKey == sword) {
        outLeftX = -30.0f; outRightX = -52.0f;
        outLeftZ = 10.0f; outRightZ = -4.0f;
        return true;
    }
    if (weaponKey == knife) {
        outLeftX = -20.0f; outRightX = -40.0f;
        outLeftZ = 8.0f; outRightZ = -6.0f;
        return true;
    }
    if (weaponKey == revolver || weaponKey == shotgun || weaponKey == rocket ||
        weaponKey == grenade) {
        outLeftX = -55.0f; outRightX = -70.0f;
        outLeftZ = 12.0f; outRightZ = -2.0f;
        return true;
    }
    return false;
}

inline void applyWeaponArms(Pose& pose, std::uint64_t weaponKey,
                            std::uint64_t actionId)
{
    if (weaponKey == 0)
        return;
    // Only carry/idle/locomotion actions get the weapon carry override; action
    // poses own the arms while they play.
    switch (actionId) {
        case HOT_ACTION_IDLE:
        case HOT_ACTION_EQUIPPED_IDLE:
        case HOT_ACTION_WALK:
        case HOT_ACTION_JUMP:
        case HOT_ACTION_FALL:
        case HOT_ACTION_LAND:
            break;
        default:
            return;
    }
    float lx = 0.0f, rx = 0.0f, lz = 0.0f, rz = 0.0f;
    if (!weaponCarryArms(weaponKey, lx, rx, lz, rz))
        return;
    const auto& authored = jsonWeaponArms();
    const auto found = authored.find(weaponKey);
    if (pose.mask & MaskLeftArm) {
        if (found != authored.end()) {
            pose.part[PartLeftArm].trans[0] = found->second.leftT[0];
            pose.part[PartLeftArm].trans[1] = found->second.leftT[1];
            pose.part[PartLeftArm].trans[2] = found->second.leftT[2];
        }
        pose.part[PartLeftArm].rot[0] = lx;
        pose.part[PartLeftArm].rot[2] = lz;
    }
    if (pose.mask & MaskRightArm) {
        if (found != authored.end()) {
            pose.part[PartRightArm].trans[0] = found->second.rightT[0];
            pose.part[PartRightArm].trans[1] = found->second.rightT[1];
            pose.part[PartRightArm].trans[2] = found->second.rightT[2];
        }
        pose.part[PartRightArm].rot[0] = rx;
        pose.part[PartRightArm].rot[2] = rz;
    }
}

// ── Procedural keyframe tables ──────────────────────────────────────
// Authored in degrees. Part order: torso, head, leftArm, rightArm, leftLeg,
// rightLeg. Each entry is tx,ty,tz,rx,ry,rz. These are live C++ constants:
// editing them and saving changes the running animation with no restart.

#define HA_PART(tx,ty,tz,rx,ry,rz) {tx,ty,tz,rx,ry,rz}
#define HA_ZERO HA_PART(0,0,0,0,0,0)

inline constexpr Keyframe kJumpFrames[] = {
    {0.00f, {HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO}},
    {0.15f, {HA_PART(0,0,0, 12,0,0), HA_PART(0,0,0, -8,0,0),
             HA_PART(0,0,0, -75,0,12), HA_PART(0,0,0, -75,0,-12),
             HA_PART(0,0,0, -50,0,0), HA_PART(0,0,0, -45,0,0)}},
    {0.45f, {HA_PART(0,0,0, 10,0,0), HA_PART(0,0,0, -6,0,0),
             HA_PART(0,0,0, -70,0,10), HA_PART(0,0,0, -70,0,-10),
             HA_PART(0,0,0, -42,0,0), HA_PART(0,0,0, -38,0,0)}},
};

inline constexpr Keyframe kFallFrames[] = {
    {0.00f, {HA_PART(0,0,0, 5,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, -40,0,35), HA_PART(0,0,0, -40,0,-35),
             HA_PART(0,0,0, 18,0,0), HA_PART(0,0,0, -18,0,0)}},
    {0.45f, {HA_PART(0,0,0, 5,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, -40,0,35), HA_PART(0,0,0, -40,0,-35),
             HA_PART(0,0,0, 18,0,0), HA_PART(0,0,0, -18,0,0)}},
};

inline constexpr Keyframe kLandFrames[] = {
    {0.00f, {HA_PART(0,0,0, 18,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, -35,0,15), HA_PART(0,0,0, -35,0,-15),
             HA_PART(0,0,0, -50,0,0), HA_PART(0,0,0, -50,0,0)}},
    {0.14f, {HA_PART(0,0,0, 8,0,0), HA_PART(0,0,0, -4,0,0),
             HA_PART(0,0,0, -18,0,8), HA_PART(0,0,0, -18,0,-8),
             HA_PART(0,0,0, -20,0,0), HA_PART(0,0,0, -20,0,0)}},
    {0.28f, {HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kDashFrames[] = {
    // HA_PART arguments are tx,ty,tz, rx,ry,rz. These dash rotations are
    // intentionally on Z: the old rx values are now the final rz values.
    // Entry order in every frame: 1=torso, 2=head, 3=left arm,
    // 4=right arm, 5=left leg, 6=right leg.
    // To change a part's Z rotation, edit the LAST number in its HA_PART.
    // The first three numbers are translation; the next two are X/Y rotation.
    // Frame 0.00 values: torso=30, head=-15, left arm=55, right arm=-55,
    // left leg=-25, right leg=35. Frame 0.30 uses the same body-part order.
    {0.00f, {HA_PART(0,0,0, 0,0,-10), HA_PART(0,0,0, 0,0,-15),
             HA_PART(0,0,0, 0,0,55), HA_PART(0,0,0, 0,0,-55),
             HA_PART(0,0,0, 0,0,-25), HA_PART(0,0,0, 0,0,35)}},
    {0.30f, {HA_PART(0,0,0, 0,0,-10), HA_PART(0,0,0, 0,0,-15),
             HA_PART(0,0,0, 0,0,55), HA_PART(0,0,0, 0,0,-55),
             HA_PART(0,0,0, 0,0,-25), HA_PART(0,0,0, 0,0,35)}},
};

inline constexpr Keyframe kDownDashFrames[] = {
    // Down-dash is Z-only: every HA_PART has rx=0 and ry=0.
    {0.00f, {HA_ZERO, HA_PART(0,0,0, 0,0,-15),
             HA_PART(0,0,0, 0,0,-20), HA_PART(0,0,0, 0,0,-20),
             HA_PART(0,0,0, 0,0,-25), HA_PART(0,0,0, 0,0,5)}},
    {0.35f, {HA_ZERO, HA_PART(0,0,0, 0,0,-15),
             HA_PART(0,0,0, 0,0,-20), HA_PART(0,0,0, 0,0,-20),
             HA_PART(0,0,0, 0,0,-25), HA_PART(0,0,0, 0,0,5)}},
};

inline constexpr Keyframe kFreezeFrames[] = {
    {0.00f, {HA_PART(0,0,0, 8,0,0), HA_PART(0,0,0, 8,0,0),
             HA_PART(0,0,0, -85,0,22), HA_PART(0,0,0, -85,0,-22),
             HA_ZERO, HA_ZERO}},
    {0.45f, {HA_PART(0,0,0, 8,0,0), HA_PART(0,0,0, 8,0,0),
             HA_PART(0,0,0, -85,0,22), HA_PART(0,0,0, -85,0,-22),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kEquipFrames[] = {
    {0.00f, {HA_ZERO, HA_PART(0,0,0, -5,0,0),
             HA_PART(0,0,0, -30,0,0), HA_PART(0,0,0, -55,0,-8),
             HA_ZERO, HA_ZERO}},
    {0.30f, {HA_ZERO, HA_PART(0,0,0, -5,0,0),
             HA_PART(0,0,0, -55,0,0), HA_PART(0,0,0, -70,0,-4),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kShootFrames[] = {
    {0.00f, {HA_PART(0,0,0, 4,0,0), HA_PART(0,0,0, -3,0,0),
             HA_PART(0,0,0, -70,0,10), HA_PART(0,0,0, -85,0,-2),
             HA_ZERO, HA_ZERO}},
    {0.06f, {HA_PART(0,0,0, 8,0,0), HA_PART(0,0,0, -6,0,0),
             HA_PART(0,0,0, -72,0,10), HA_PART(0,0,0, -100,0,-2),
             HA_ZERO, HA_ZERO}},
    {0.18f, {HA_PART(0,0,0, 4,0,0), HA_PART(0,0,0, -3,0,0),
             HA_PART(0,0,0, -70,0,10), HA_PART(0,0,0, -85,0,-2),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kReloadFrames[] = {
    {0.00f, {HA_PART(0,0,0, 3,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, 25,0,12), HA_PART(0,0,0, -65,0,0),
             HA_ZERO, HA_ZERO}},
    {0.30f, {HA_PART(0,0,0, 3,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, -30,0,10), HA_PART(0,0,0, -65,0,0),
             HA_ZERO, HA_ZERO}},
    {0.60f, {HA_PART(0,0,0, 3,0,0), HA_PART(0,0,0, -6,0,0),
             HA_PART(0,0,0, 5,0,8), HA_PART(0,0,0, -62,0,0),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kSlashFrames[] = {
    {0.00f, {HA_PART(0,0,0, 0,-10,0), HA_PART(0,0,0, 0,-8,0),
             HA_PART(0,0,0, -20,0,0), HA_PART(0,0,0, -150,-6,0),
             HA_ZERO, HA_ZERO}},
    {0.12f, {HA_PART(0,0,0, 0,20,0), HA_PART(0,0,0, 0,12,0),
             HA_PART(0,0,0, -25,0,0), HA_PART(0,0,0, -30,40,0),
             HA_ZERO, HA_ZERO}},
    {0.30f, {HA_PART(0,0,0, 0,10,0), HA_PART(0,0,0, 0,6,0),
             HA_PART(0,0,0, -15,0,0), HA_PART(0,0,0, 10,20,0),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kLungeFrames[] = {
    {0.00f, {HA_PART(0,0,0, 25,0,0), HA_PART(0,0,0, -5,0,0),
             HA_PART(0,0,0, -30,0,6), HA_PART(0,0,0, -110,0,-4),
             HA_ZERO, HA_ZERO}},
    {0.28f, {HA_PART(0,0,0, 35,0,0), HA_PART(0,0,0, -8,0,0),
             HA_PART(0,0,0, -20,0,6), HA_PART(0,0,0, -125,0,-4),
             HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kHurtFrames[] = {
    {0.00f, {HA_PART(0,0,0, -18,0,0), HA_PART(0,0,0, -22,0,0),
             HA_PART(0,0,0, -20,0,25), HA_PART(0,0,0, -20,0,-25),
             HA_PART(0,0,0, -8,0,0), HA_PART(0,0,0, -6,0,0)}},
    {0.30f, {HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO}},
};

inline constexpr Keyframe kDeathFrames[] = {
    {0.00f, {HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO}},
    {0.30f, {HA_PART(0,0,0, -25,0,0), HA_PART(0,0,0, -15,0,0),
             HA_PART(0,0,0, -30,0,20), HA_PART(0,0,0, -30,0,-20),
             HA_PART(0,0,0, -30,0,0), HA_PART(0,0,0, -35,0,0)}},
    {0.90f, {HA_PART(0,0,0, -80,0,0), HA_PART(0,0,0, -20,0,0),
             HA_PART(0,0,0, -10,0,45), HA_PART(0,0,0, -10,0,-45),
             HA_PART(0,0,0, 25,0,0), HA_PART(0,0,0, 25,0,0)}},
};

inline constexpr Keyframe kRespawnFrames[] = {
    {0.00f, {HA_PART(0,0,0, -80,0,0), HA_PART(0,0,0, -20,0,0),
             HA_PART(0,0,0, -10,0,45), HA_PART(0,0,0, -10,0,-45),
             HA_PART(0,0,0, 25,0,0), HA_PART(0,0,0, 25,0,0)}},
    {0.35f, {HA_PART(0,0,0, -30,0,0), HA_PART(0,0,0, -10,0,0),
             HA_PART(0,0,0, -25,0,15), HA_PART(0,0,0, -25,0,-15),
             HA_PART(0,0,0, -20,0,0), HA_PART(0,0,0, -20,0,0)}},
    {0.60f, {HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO, HA_ZERO}},
};

#undef HA_ZERO
#undef HA_PART

inline ActionClip actionClipBuiltin(std::uint64_t actionId)
{
    ActionClip c{};
    c.duration = 0.4f;
    c.loop = 0;
    c.fullBody = 1;
    c.mask = MaskFull;
    c.frames = nullptr;
    c.frameCount = 0;

    switch (actionId) {
        case HOT_ACTION_JUMP:
            c.duration = 0.45f; c.frames = kJumpFrames;
            c.frameCount = (std::uint32_t)(sizeof(kJumpFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_FALL:
            c.duration = 0.45f; c.frames = kFallFrames;
            c.frameCount = (std::uint32_t)(sizeof(kFallFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_LAND:
            c.duration = 0.28f; c.frames = kLandFrames;
            c.frameCount = (std::uint32_t)(sizeof(kLandFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_DASH:
            c.duration = 0.30f; c.frames = kDashFrames;
            c.frameCount = (std::uint32_t)(sizeof(kDashFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_DOWN_DASH:
            c.duration = 0.35f; c.frames = kDownDashFrames;
            c.frameCount = (std::uint32_t)(sizeof(kDownDashFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_FREEZE:
            c.duration = 0.45f; c.frames = kFreezeFrames;
            c.frameCount = (std::uint32_t)(sizeof(kFreezeFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_EQUIP:
            c.duration = 0.30f; c.mask = MaskUpper; c.fullBody = 0;
            c.frames = kEquipFrames;
            c.frameCount = (std::uint32_t)(sizeof(kEquipFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_SHOOT:
            c.duration = 0.18f; c.mask = MaskUpper; c.fullBody = 0;
            c.frames = kShootFrames;
            c.frameCount = (std::uint32_t)(sizeof(kShootFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_RELOAD:
            c.duration = 0.60f; c.mask = MaskUpper; c.fullBody = 0;
            c.frames = kReloadFrames;
            c.frameCount = (std::uint32_t)(sizeof(kReloadFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_SLASH:
            c.duration = 0.30f; c.mask = MaskUpper; c.fullBody = 0;
            c.frames = kSlashFrames;
            c.frameCount = (std::uint32_t)(sizeof(kSlashFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_LUNGE:
            c.duration = 0.28f; c.mask = MaskUpper; c.fullBody = 0;
            c.frames = kLungeFrames;
            c.frameCount = (std::uint32_t)(sizeof(kLungeFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_HURT:
            c.duration = 0.30f; c.frames = kHurtFrames;
            c.frameCount = (std::uint32_t)(sizeof(kHurtFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_DEATH:
            c.duration = 0.90f; c.frames = kDeathFrames;
            c.frameCount = (std::uint32_t)(sizeof(kDeathFrames) / sizeof(Keyframe));
            return c;
        case HOT_ACTION_RESPAWN:
            c.duration = 0.60f; c.frames = kRespawnFrames;
            c.frameCount = (std::uint32_t)(sizeof(kRespawnFrames) / sizeof(Keyframe));
            return c;
        default:
            return c;
    }
}

inline const char* actionConfigName(std::uint64_t actionId)
{
    if (actionId == HOT_ACTION_IDLE) return "idle";
    if (actionId == HOT_ACTION_EQUIPPED_IDLE) return "equipped_idle";
    if (actionId == HOT_ACTION_WALK) return "walk";
    if (actionId == HOT_ACTION_RETURN_TO_IDLE) return "return_to_idle";
    if (actionId == HOT_ACTION_JUMP) return "jump";
    if (actionId == HOT_ACTION_FALL) return "fall";
    if (actionId == HOT_ACTION_LAND) return "land";
    if (actionId == HOT_ACTION_DASH) return "dash";
    if (actionId == HOT_ACTION_DOWN_DASH) return "down_dash";
    if (actionId == HOT_ACTION_FREEZE) return "freeze";
    if (actionId == HOT_ACTION_EQUIP) return "equip";
    if (actionId == HOT_ACTION_SHOOT) return "shoot";
    if (actionId == HOT_ACTION_RELOAD) return "reload";
    if (actionId == HOT_ACTION_SLASH) return "slash";
    if (actionId == HOT_ACTION_LUNGE) return "lunge";
    if (actionId == HOT_ACTION_HURT) return "hurt";
    if (actionId == HOT_ACTION_DEATH) return "death";
    if (actionId == HOT_ACTION_RESPAWN) return "respawn";
    return "";
}

struct JsonClipCache {
    static constexpr std::uint32_t kActionCount = 18;
    static constexpr std::uint32_t kMaxFrames = 32;
    nlohmann::json root;
    Keyframe frames[kActionCount][kMaxFrames]{};
    std::uint32_t counts[kActionCount]{};
    std::uint32_t masks[kActionCount]{};
    float durations[kActionCount]{};
    bool loops[kActionCount]{};
    bool valid[kActionCount]{};
    bool loaded = false;
    std::filesystem::file_time_type write{};
};

inline int jsonClipIndex(const char* name)
{
    static constexpr const char* names[] = {
        "idle", "equipped_idle", "walk", "jump", "fall", "land", "dash",
        "down_dash", "freeze", "equip", "shoot", "reload", "slash", "lunge",
        "hurt", "death", "respawn", "return_to_idle"};
    for (int i = 0; i < 18; ++i)
        if (std::strcmp(name, names[i]) == 0)
            return i;
    return -1;
}

inline int jsonPartIndex(const std::string& name)
{
    static constexpr const char* names[] = {
        "torso", "head", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    for (int i = 0; i < 6; ++i)
        if (name == names[i])
            return i;
    return -1;
}

inline void loadJsonClipCache(JsonClipCache& cache)
{
    for (std::uint32_t i = 0; i < JsonClipCache::kActionCount; ++i) {
        cache.valid[i] = false;
        cache.counts[i] = 0;
        cache.masks[i] = 0;
        cache.durations[i] = 0.0f;
        cache.loops[i] = false;
    }
    cache.root = nlohmann::json::object();
    std::ifstream file("config/animations.json");
    try {
        cache.root = nlohmann::json::parse(file, nullptr, true, true);
        auto actions = cache.root.value("actions", nlohmann::json::object());
        // afad20a names the same clip layer layers.animations and stores
        // duration in fixed 60 Hz ticks. Normalize it into the hot cache.
        const auto layers = cache.root.value("layers", nlohmann::json::object());
        const auto legacy = layers.value("animations", nlohmann::json::object());
        if (actions.empty() && legacy.is_object())
            actions = legacy;
        for (auto it = actions.begin(); it != actions.end(); ++it) {
            const int index = jsonClipIndex(it.key().c_str());
            if (index < 0 || !it.value().is_object())
                continue;
            const auto& item = it.value();
            if (item.contains("duration") && item["duration"].is_number())
                cache.durations[index] = std::max(0.001f, item["duration"].get<float>());
            if (item.contains("durationTicks") && item["durationTicks"].is_number())
                cache.durations[index] = std::max(0.001f, item["durationTicks"].get<float>() / 60.0f);
            cache.loops[index] = item.value("loop", false);
            const auto frames = item.value("keyframes", nlohmann::json::array());
            if (!frames.is_array() || frames.empty())
                continue;
            const std::uint32_t count = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(frames.size()), JsonClipCache::kMaxFrames);
            std::uint32_t mask = 0;
            for (std::uint32_t f = 0; f < count; ++f) {
                Keyframe& dst = cache.frames[index][f];
                for (std::uint32_t p = 0; p < PartCount; ++p)
                    for (int k = 0; k < 6; ++k)
                        dst.part[p][k] = 0.0f;
                const auto& src = frames[f];
                dst.t = src.value("time", src.value("tick", 0.0f) / 60.0f);
                const auto parts = src.value("parts", nlohmann::json::object());
                for (auto pit = parts.begin(); pit != parts.end(); ++pit) {
                    const int p = jsonPartIndex(pit.key());
                    if (p < 0 || !pit.value().is_object())
                        continue;
                    const auto& part = pit.value();
                    const auto tr = part.value("translation", nlohmann::json::array());
                    const auto ro = part.value("rotation", nlohmann::json::array());
                    for (int k = 0; k < 3 && k < static_cast<int>(tr.size()); ++k)
                        if (tr[k].is_number()) cache.frames[index][f].part[p][k] = tr[k].get<float>();
                    for (int k = 0; k < 3 && k < static_cast<int>(ro.size()); ++k)
                        if (ro[k].is_number()) cache.frames[index][f].part[p][3 + k] = ro[k].get<float>();
                    mask |= 1u << p;
                }
            }
            cache.counts[index] = count;
            cache.masks[index] = mask;
            if (cache.durations[index] <= 0.0f)
                cache.durations[index] = cache.frames[index][count - 1].t;
            cache.valid[index] = mask != 0 && count > 0;
        }
        cache.loaded = true;
    } catch (...) {
        cache.loaded = false;
    }
}

// One shared, mtime-refreshed cache of config/animations.json for both the
// clip lookup and the "is JSON active for this action" query. Loading the same
// file twice (once here, once in tool-visuals) is avoided on this side.
inline JsonClipCache& jsonClipCache()
{
    static JsonClipCache cache;
    std::error_code ec;
    const auto write = std::filesystem::last_write_time("config/animations.json", ec);
    if (!ec && (!cache.loaded || write != cache.write)) {
        loadJsonClipCache(cache);
        cache.write = write;
    }
    return cache;
}

inline bool jsonClipApplied(std::uint64_t actionId)
{
    JsonClipCache& cache = jsonClipCache();
    if (!cache.loaded)
        return false;
    const int index = jsonClipIndex(actionConfigName(actionId));
    return index >= 0 && cache.valid[index];
}

inline ActionClip actionClip(std::uint64_t actionId)
{
    ActionClip clip = actionClipBuiltin(actionId);
    JsonClipCache& cache = jsonClipCache();
    if (!cache.loaded)
        return clip;
    const char* name = actionConfigName(actionId);
    const int index = jsonClipIndex(name);
    if (index < 0)
        return clip;
    if (cache.valid[index]) {
        clip.frames = cache.frames[index];
        clip.frameCount = cache.counts[index];
        clip.mask = cache.masks[index];
        clip.loop = cache.loops[index] ? 1u : 0u;
    }
    if (cache.durations[index] > 0.0f)
        clip.duration = cache.durations[index];
    return clip;
}

// ── afad20a C++ locomotion sampling ─────────────────────────────────────────
// The afad20a animator sampled keyframes on a fixed 60 Hz tick clock, scaled by
// walkFrequency * walkFrequencyMultiplier, wrapped with fmod for loops and
// clamped to durationTicks - 1 for one-shots, then linearly interpolated on
// euler degrees. This is the C++ implementation of that contract; the JSON
// source samples the same cached frames through sampleClip().
inline bool afad20aSourceCpp()
{
    JsonClipCache& cache = jsonClipCache();
    if (!cache.loaded)
        return true;  // compiled default: the C++ afad20a sampler
    return cache.root.value("locomotionSource", std::string("cpp")) == "cpp";
}

inline float afad20aWalkSpeedScale()
{
    JsonClipCache& cache = jsonClipCache();
    float scale = 0.3f;
    if (cache.loaded) {
        const float freq = cache.root.value("walkFrequency", 0.3f);
        const float mult = cache.root.value("walkFrequencyMultiplier", 1.0f);
        scale = std::max(0.01f, freq * mult);
    }
    return scale;
}

inline void afad20aSampleClip(const ActionClip& clip, float timeSeconds,
                              float speedScale, Pose& out)
{
    out.clear();
    out.mask = clip.mask;
    if (!clip.frames || clip.frameCount == 0)
        return;
    const float durationTicks = clip.duration * 60.0f;
    const float duration = durationTicks > 0.0f ? durationTicks : 1.0f;
    const float tickTime = timeSeconds * 60.0f * speedScale;
    float looped = clip.loop ? std::fmod(tickTime, duration)
                             : std::min(tickTime, duration - 1.0f);
    if (looped < 0.0f)
        looped += duration;

    const Keyframe* prev = &clip.frames[0];
    const Keyframe* next = &clip.frames[0];
    for (std::uint32_t i = 0; i < clip.frameCount; ++i) {
        const float ft = clip.frames[i].t * 60.0f;
        if (ft <= looped)
            prev = &clip.frames[i];
        if (ft >= looped) {
            next = &clip.frames[i];
            break;
        }
    }

    float range = 0.0f;
    float t = 0.0f;
    if (prev == next && clip.loop && clip.frameCount > 1) {
        const Keyframe* last = &clip.frames[clip.frameCount - 1];
        prev = last;
        const float wrapped = (duration - last->t * 60.0f) + next->t * 60.0f;
        float current = looped - last->t * 60.0f;
        if (current < 0.0f)
            current += duration;
        range = wrapped;
        t = range > 0.001f ? current / range : 0.0f;
    } else {
        range = next->t * 60.0f - prev->t * 60.0f;
        t = range > 0.001f ? (looped - prev->t * 60.0f) / range : 0.0f;
    }
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    for (std::uint32_t i = 0; i < PartCount; ++i)
        lerpPart(out.part[i], prev->part[i], next->part[i], t);
}

} // namespace HotAnim
