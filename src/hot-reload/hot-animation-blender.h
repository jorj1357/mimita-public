// 09 17 2026
/* purpose
* Blender-authored (imported) 60 Hz exact-pose clip vocabulary, beside the
* procedural C++ clip library. A BlenderClip stores ONE pose per fixed
* simulation tick, actor-relative to `plrOrigin`, in the same units/convention
* as the procedural library (per-part local delta from the rest pose, degrees,
* converted to radians only at the skeleton.apply boundary). Sampling by tick is
* therefore independent of render FPS.
*
* This is the concrete activation of AnimationClipSource::ImportedAsset. The
* source of data is an offline Blender exporter (tools/blender_export_animation.py)
* that emits both a canonical JSON asset and a generated C++ header consumed
* here. The animation runtime never parses JSON and never owns gameplay damage.
* Hot-only header: not a GameAPI context field, no kernel enum.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation-clips.h"

namespace HotAnim {

// ── Explicit animation modes ────────────────────────────────────────
// The existing procedural/hot system stays available as CurrentHot. The
// Blender-driven system is BlenderPhysical + Exact.
enum class AnimationMode : std::uint32_t {
    CurrentHot = 0,
    BlenderPhysical = 1,
};

enum class PoseAuthority : std::uint32_t {
    Wished = 0,   // a softer mode may treat the authored pose as a wish
    Exact = 1,    // the authored tick pose is the required target
};

// ── Coordinate conversion (Blender -> MiMITA) ───────────────────────
// MiMITA is Z-up (the vertical axis is index 2; see movement `vy = linear[2]`).
// Blender is also Z-up. The exporter applies this conversion to positions and
// rotations; it is identity until the Phase 0 rest-pose check proves a forward
// axis difference. Kept as one function so the conversion has one owner.
inline void blenderToMimita(const float in[3], float out[3])
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

// ── Collision markers ───────────────────────────────────────────────
// Optional probe attached to a body part: a local offset + radius the exact-pose
// runtime sweeps through the world. `part` is gameHash(part name); `nameHash`
// identifies the marker (weapon_edge, foot_left, ...).
struct BlenderMarker {
    std::uint64_t part;
    std::uint64_t nameHash;
    float localOffset[3];
    float radius;
    std::uint32_t flags;      // bit0 = this marker is a weapon contact shape
    std::uint32_t reserved;
};

static constexpr std::uint32_t BLENDER_MARKER_WEAPON = 1u << 0;
// A part without an explicit marker sweeps with this default radius.
static constexpr float kDefaultPartRadius = 0.16f;

// ── Clip ────────────────────────────────────────────────────────────
// `frames[t]` is the exact pose AT tick t; `t` must equal its index. durationTicks
// equals frameCount for a one-shot clip (Blender export = one frame per tick).
struct BlenderClip {
    std::uint64_t animationId;    // gameHash("animation.katana_slash")
    std::uint32_t sampleRate;     // 60
    std::uint32_t durationTicks;
    std::uint32_t mask;           // parts this clip drives
    std::uint32_t frameCount;
    const Keyframe* frames;       // tick-indexed, degrees
    const BlenderMarker* markers;
    std::uint32_t markerCount;
    std::uint32_t loop;           // 1 = loop, 0 = one-shot
};

// One owner of the imported-clip registry (DLL-internal; stable pointers to
// static generated data). Keyed by animation id.
inline std::vector<const BlenderClip*>& blenderClipRegistry()
{
    static std::vector<const BlenderClip*> registry;
    return registry;
}

inline bool registerBlenderClip(const BlenderClip& clip)
{
    if (clip.animationId == 0 || !clip.frames || clip.frameCount == 0)
        return false;
    if (clip.sampleRate != 60)
        return false;
    auto& registry = blenderClipRegistry();
    for (const BlenderClip* existing : registry)
        if (existing->animationId == clip.animationId)
            return true;  // idempotent: a reloaded generation re-registers
    registry.push_back(&clip);
    return true;
}

inline const BlenderClip* findBlenderClip(std::uint64_t animationId)
{
    if (animationId == 0)
        return nullptr;
    for (const BlenderClip* clip : blenderClipRegistry())
        if (clip->animationId == animationId)
            return clip;
    return nullptr;
}

// ── Exact tick sampling ─────────────────────────────────────────────
// The authored pose for exactly this tick. Loop wraps; one-shot clamps to the
// final tick. No render-time interpolation lives here (rendering may interpolate
// between resolved ticks, gameplay uses this).
inline void sampleBlenderClipAtTick(const BlenderClip& clip, std::uint32_t tick,
                                    Pose& out)
{
    out.clear();
    out.mask = clip.mask;
    if (!clip.frames || clip.frameCount == 0)
        return;
    std::uint32_t index = tick;
    if (clip.loop) {
        if (clip.durationTicks > 0)
            index = tick % clip.durationTicks;
        if (index >= clip.frameCount)
            index = clip.frameCount - 1;
    } else if (index >= clip.frameCount) {
        index = clip.frameCount - 1;
    }
    const Keyframe& frame = clip.frames[index];
    for (std::uint32_t i = 0; i < PartCount; ++i)
        for (int k = 0; k < 3; ++k) {
            out.part[i].trans[k] = frame.part[i][k];
            out.part[i].rot[k] = frame.part[i][3 + k];
        }
}

// ── Action -> imported clip resolution ──────────────────────────────
// The existing hot state machine owns action selection; this maps a selected
// action + equipped tool to an imported exact clip when one exists. Returning
// null selects the CurrentHot procedural fallback, so partial coverage is safe.
inline const BlenderClip* findBlenderClipForAction(std::uint64_t actionId,
                                                   std::uint64_t weaponKey)
{
    if (weaponKey == gameHash("katana") && actionId == HOT_ACTION_SLASH)
        return findBlenderClip(gameHash("animation.katana_slash"));
    return nullptr;
}

} // namespace HotAnim
