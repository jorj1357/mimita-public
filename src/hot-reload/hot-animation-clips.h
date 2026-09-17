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

#include <cmath>
#include <cstdint>

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
        else if (time > clip.duration)
            time = clip.duration;
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
    out.part[PartTorso].rot[1] = sway * 5.0f;
    out.part[PartTorso].rot[0] = breathe * 2.0f;
    out.part[PartTorso].trans[1] = breathe * 0.03f;
    out.part[PartHead].rot[0] = head * 6.0f;
    out.part[PartHead].rot[1] = sway * 4.0f;
    const float armX = equipped ? -58.0f : 0.0f;
    const float armSpread = equipped ? 10.0f : 8.0f;
    out.part[PartLeftArm].rot[0] = armX + std::sin(t * 1.5f) * 8.0f;
    out.part[PartRightArm].rot[0] = armX + std::sin(t * 1.5f + 1.4f) * 8.0f;
    out.part[PartLeftArm].rot[2] = armSpread + sway * 3.0f;
    out.part[PartRightArm].rot[2] = -armSpread - sway * 3.0f;
    out.part[PartLeftLeg].rot[0] = std::sin(t * 1.5f + 0.6f) * 2.0f;
    out.part[PartRightLeg].rot[0] = std::sin(t * 1.5f + 2.0f) * 2.0f;
}

// Walking is intent-driven and always plays at the same procedural speed and
// amplitude (no velocity scaling); the leg cycle keeps a readable stride.
inline void evaluateWalk(float t, float /*speed01*/, Pose& out)
{
    out.clear();
    out.mask = MaskFull;
    const float freq = 8.0f;
    const float p = t * freq;
    const float swing = std::sin(p);
    const float amp = 38.0f;
    out.part[PartLeftLeg].rot[0] = swing * amp;
    out.part[PartRightLeg].rot[0] = -swing * amp;
    out.part[PartLeftArm].rot[0] = -swing * amp * 0.7f;
    out.part[PartRightArm].rot[0] = swing * amp * 0.7f;
    out.part[PartLeftArm].rot[2] = 5.0f;
    out.part[PartRightArm].rot[2] = -5.0f;
    out.part[PartTorso].rot[1] = swing * 5.0f;
    out.part[PartTorso].trans[1] = std::fabs(std::sin(p * 2.0f)) * 0.03f;
    out.part[PartHead].rot[0] = -2.0f + std::sin(p * 2.0f) * 2.0f;
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

inline void evaluateAction(std::uint64_t actionId, float time, float speed01,
                           Pose& out)
{
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
inline bool weaponCarryArms(std::uint64_t weaponKey, float& outLeftX,
                            float& outRightX, float& outLeftZ, float& outRightZ)
{
    if (weaponKey == 0)
        return false;
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
    if (pose.mask & MaskLeftArm) {
        pose.part[PartLeftArm].rot[0] = lx;
        pose.part[PartLeftArm].rot[2] = lz;
    }
    if (pose.mask & MaskRightArm) {
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
    {0.00f, {HA_PART(0,0,0, 30,0,0), HA_PART(0,0,0, -15,0,0),
             HA_PART(0,0,0, 55,0,8), HA_PART(0,0,0, 55,0,-8),
             HA_PART(0,0,0, -25,0,0), HA_PART(0,0,0, 35,0,0)}},
    {0.30f, {HA_PART(0,0,0, 30,0,0), HA_PART(0,0,0, -15,0,0),
             HA_PART(0,0,0, 55,0,8), HA_PART(0,0,0, 55,0,-8),
             HA_PART(0,0,0, -25,0,0), HA_PART(0,0,0, 35,0,0)}},
};

inline constexpr Keyframe kDownDashFrames[] = {
    {0.00f, {HA_ZERO, HA_PART(0,0,0, 15,0,0),
             HA_PART(0,0,0, -20,0,6), HA_PART(0,0,0, -20,0,-6),
             HA_PART(0,0,0, -5,0,0), HA_PART(0,0,0, 5,0,0)}},
    {0.35f, {HA_ZERO, HA_PART(0,0,0, 15,0,0),
             HA_PART(0,0,0, -20,0,6), HA_PART(0,0,0, -20,0,-6),
             HA_PART(0,0,0, -5,0,0), HA_PART(0,0,0, 5,0,0)}},
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

inline ActionClip actionClip(std::uint64_t actionId)
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

} // namespace HotAnim
