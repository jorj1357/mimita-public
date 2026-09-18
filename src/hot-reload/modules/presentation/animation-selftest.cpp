// 09 16 2026
/* purpose
* Implements the hot animation candidate self-test declared in
* hot-animation-selftest.h. Verifies clip invariants, interpolation, one-shot
* clamping, determinism, masking, blending, and the versioned state contract.
* A failure rejects the candidate before activation so the previous animation
* generation keeps running. Does NOT own runtime animation policy.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-animation-selftest.h"

#include "hot-reload/hot-animation-blender.h"
#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-animation-physical.h"
#include "hot-reload/hot-animation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const std::uint64_t kTableActions[] = {
    HOT_ACTION_JUMP,      HOT_ACTION_FALL,     HOT_ACTION_LAND,
    HOT_ACTION_DASH,      HOT_ACTION_DOWN_DASH, HOT_ACTION_FREEZE,
    HOT_ACTION_EQUIP,     HOT_ACTION_SHOOT,    HOT_ACTION_RELOAD,
    HOT_ACTION_SLASH,     HOT_ACTION_LUNGE,    HOT_ACTION_HURT,
    HOT_ACTION_DEATH,     HOT_ACTION_RESPAWN,
};

bool fail(char* message, std::uint32_t size, const char* reason)
{
    if (message && size)
        std::snprintf(message, size, "%s", reason);
    return false;
}

bool finitePose(const HotAnim::Pose& p)
{
    for (std::uint32_t i = 0; i < HotAnim::PartCount; ++i)
        for (int k = 0; k < 3; ++k)
            if (!std::isfinite(p.part[i].trans[k]) ||
                !std::isfinite(p.part[i].rot[k]))
                return false;
    return true;
}

} // namespace

bool runAnimationSelfTest(char* message, std::uint32_t messageSize)
{
    using namespace HotAnim;

    // Extension point default must be the procedural source.
    if (static_cast<std::uint32_t>(AnimationClipSource::ProceduralCpp) != 0u)
        return fail(message, messageSize, "clip source default invalid");

    // Required body parts are distinct and non-zero (validation depends on it).
    for (std::uint32_t i = 0; i < kRequiredPartCount; ++i) {
        const std::uint64_t h = requiredPartHash(i);
        if (h == 0)
            return fail(message, messageSize, "required part hash zero");
        for (std::uint32_t j = i + 1; j < kRequiredPartCount; ++j)
            if (requiredPartHash(j) == h)
                return fail(message, messageSize, "required part hash collision");
    }

    // Clip table invariants: positive duration, non-empty mask, ordered finite
    // frames where a table exists.
    for (std::uint64_t actionId : kTableActions) {
        const ActionClip clip = actionClip(actionId);
        if (clip.duration <= 0.0f)
            return fail(message, messageSize, "clip duration not positive");
        if (clip.mask == 0)
            return fail(message, messageSize, "clip mask empty");
        if (!clip.frames || clip.frameCount < 2)
            return fail(message, messageSize, "clip frames missing");
        for (std::uint32_t f = 1; f < clip.frameCount; ++f) {
            if (clip.frames[f].t <= clip.frames[f - 1].t)
                return fail(message, messageSize, "clip frames unordered");
            for (std::uint32_t i = 0; i < PartCount; ++i)
                for (int k = 0; k < 6; ++k)
                    if (!std::isfinite(clip.frames[f].part[i][k]))
                        return fail(message, messageSize, "clip frame not finite");
        }
    }

    // Deterministic sampling: identical input -> identical, finite output.
    Pose s1{}, s2{};
    sampleClip(actionClip(HOT_ACTION_SHOOT), 0.045f, s1);
    sampleClip(actionClip(HOT_ACTION_SHOOT), 0.045f, s2);
    if (!finitePose(s1))
        return fail(message, messageSize, "sampled pose not finite");
    if (std::memcmp(&s1, &s2, sizeof(Pose)) != 0)
        return fail(message, messageSize, "sampled pose not deterministic");

    // One-shot clamps to its final frame (holds instead of snapping to rest).
    Pose holdA{}, holdB{};
    sampleClip(actionClip(HOT_ACTION_DEATH), actionClip(HOT_ACTION_DEATH).duration, holdA);
    sampleClip(actionClip(HOT_ACTION_DEATH), 9.0f, holdB);
    if (std::memcmp(&holdA, &holdB, sizeof(Pose)) != 0)
        return fail(message, messageSize, "one-shot did not hold final frame");

    // Procedural locomotion is deterministic and finite.
    Pose w1{}, w2{};
    evaluateAction(HOT_ACTION_WALK, 0.37f, 0.8f, w1);
    evaluateAction(HOT_ACTION_WALK, 0.37f, 0.8f, w2);
    if (!finitePose(w1))
        return fail(message, messageSize, "walk pose not finite");
    if (std::memcmp(&w1, &w2, sizeof(Pose)) != 0)
        return fail(message, messageSize, "walk pose not deterministic");

    // Masking copies only masked parts; blending is bounded and deterministic.
    Pose base{}, overlay{}, masked{}, blendA{}, blendB{};
    evaluateAction(HOT_ACTION_WALK, 0.2f, 1.0f, base);
    evaluateAction(HOT_ACTION_SHOOT, 0.05f, 1.0f, overlay);
    masked = base;
    applyMask(masked, overlay, actionClip(HOT_ACTION_SHOOT).mask);
    if (masked.part[PartRightArm].rot[0] != overlay.part[PartRightArm].rot[0])
        return fail(message, messageSize, "applyMask did not copy masked part");
    if (masked.part[PartLeftLeg].rot[0] != base.part[PartLeftLeg].rot[0])
        return fail(message, messageSize, "applyMask changed unmasked part");
    blendPose(base, masked, 0.5f, blendA);
    blendPose(base, masked, 0.5f, blendB);
    if (!finitePose(blendA))
        return fail(message, messageSize, "blended pose not finite");
    if (std::memcmp(&blendA, &blendB, sizeof(Pose)) != 0)
        return fail(message, messageSize, "blend not deterministic");

    // Versioned state contract sanity.
    if (HOT_ANIMATION_STATE_VERSION != 2)
        return fail(message, messageSize, "animation state version unexpected");
    if (sizeof(HotAnimationStateV2) <= sizeof(HotAnimationStateV1))
        return fail(message, messageSize, "v2 contract did not grow");

    // ── BlenderPhysical exact-pose runtime contract ─────────────────────
    const BlenderClip* katana =
        findBlenderClip(gameHash("animation.katana_slash"));
    if (!katana)
        return fail(message, messageSize, "imported katana clip not registered");
    if (katana->sampleRate != 60)
        return fail(message, messageSize, "imported clip is not 60 Hz");
    if (katana->durationTicks == 0 ||
        katana->frameCount != katana->durationTicks)
        return fail(message, messageSize, "imported clip is not tick-exact");
    if (katana->mask != MaskFull)
        return fail(message, messageSize, "imported clip misses a body part");
    for (std::uint32_t f = 0; f < katana->frameCount; ++f)
        if (katana->frames[f].t != (float)f)
            return fail(message, messageSize, "imported frames are not tick-indexed");

    // Exact sampling is deterministic and independent of render FPS: the same
    // tick always yields the same pose.
    Pose tickA{}, tickB{};
    sampleBlenderClipAtTick(*katana, 7, tickA);
    sampleBlenderClipAtTick(*katana, 7, tickB);
    if (std::memcmp(&tickA, &tickB, sizeof(Pose)) != 0)
        return fail(message, messageSize, "exact tick sampling not deterministic");
    if (tickA.mask != MaskFull)
        return fail(message, messageSize, "sampled pose misses a required part");

    // One-shot holds its final tick rather than snapping to rest.
    Pose lastA{}, lastB{};
    sampleBlenderClipAtTick(*katana, katana->durationTicks - 1, lastA);
    sampleBlenderClipAtTick(*katana, katana->durationTicks + 50, lastB);
    if (std::memcmp(&lastA, &lastB, sizeof(Pose)) != 0)
        return fail(message, messageSize, "imported one-shot did not hold final tick");

    // A fast arm movement yields a finite, non-zero fixed-tick velocity.
    Pose armStart{}, armEnd{};
    sampleBlenderClipAtTick(*katana, 4, armStart);
    sampleBlenderClipAtTick(*katana, 8, armEnd);
    const float deltaDeg =
        std::fabs(armEnd.part[PartRightArm].rot[0] -
                  armStart.part[PartRightArm].rot[0]);
    if (!(deltaDeg > 1.0f))
        return fail(message, messageSize, "arm did not move across ticks");
    const float armVelocity = deltaDeg / (4.0f * (1.0f / 60.0f));
    if (!std::isfinite(armVelocity) || armVelocity <= 0.0f)
        return fail(message, messageSize, "fixed-tick arm velocity invalid");

    // Tool action resolution selects the correct imported clip.
    if (findBlenderClipForAction(HOT_ACTION_SLASH, gameHash("katana")) != katana)
        return fail(message, messageSize, "katana slash did not select its clip");
    if (findBlenderClipForAction(HOT_ACTION_SLASH, gameHash("swordsword")) != nullptr)
        return fail(message, messageSize, "non-katana slash selected katana clip");
    if (findBlenderClipForAction(HOT_ACTION_WALK, gameHash("katana")) != nullptr)
        return fail(message, messageSize, "non-melee action selected katana clip");

    // The weapon contact marker exists and carries a positive sweep radius.
    const BlenderMarker* weaponMarker = nullptr;
    for (std::uint32_t i = 0; katana->markers && i < katana->markerCount; ++i)
        if ((katana->markers[i].flags & BLENDER_MARKER_WEAPON) != 0)
            weaponMarker = &katana->markers[i];
    if (!weaponMarker || weaponMarker->radius <= 0.0f)
        return fail(message, messageSize, "weapon contact marker missing");

    // Coordinate conversion has one owner and is finite.
    const float source[3] = {1.0f, 2.0f, 3.0f};
    float converted[3];
    blenderToMimita(source, converted);
    if (converted[0] != 1.0f || converted[1] != 2.0f || converted[2] != 3.0f)
        return fail(message, messageSize, "coordinate conversion changed a vector");

    // Versioned physical state contract and phase-0 baseline convention: the
    // target and resolved poses are distinct storage (sway/reaction is additive,
    // never a replacement of the authored exact target).
    if (HotPhys::PHYSICAL_ANIMATION_STATE_VERSION != 1)
        return fail(message, messageSize, "physical state version unexpected");
    if (offsetof(HotPhys::PhysicalAnimationStateV1, target) ==
        offsetof(HotPhys::PhysicalAnimationStateV1, resolved))
        return fail(message, messageSize, "target and resolved pose overlap");
    if (sizeof(HotPhys::PhysicalAnimationStateV1) <=
        sizeof(HotPhys::ActorPoseV1))
        return fail(message, messageSize, "physical state does not own its poses");

    // Force-transfer recipes: exact physical actions transfer force; static
    // world can never be pushed (only the animating actor receives a reaction).
    const HotPhys::PhysicalAnimationRecipeV1 slashRecipe =
        HotPhys::physicalRecipeForAction(HOT_ACTION_SLASH);
    if (!slashRecipe.enabled || !slashRecipe.reactToStaticWorld ||
        !slashRecipe.affectDynamicActors || !slashRecipe.collideWeapon)
        return fail(message, messageSize, "slash recipe cannot transfer force");
    if (HotPhys::physicalRecipeForAction(HOT_ACTION_WALK).enabled)
        return fail(message, messageSize, "locomotion action must not transfer force");
    if (static_cast<std::uint32_t>(HotPhys::PhysicalContactResponse::Slide) != 0u ||
        static_cast<std::uint32_t>(HotPhys::PhysicalContactResponse::Bounce) != 1u ||
        static_cast<std::uint32_t>(HotPhys::PhysicalContactResponse::PushDynamicActor) != 2u ||
        static_cast<std::uint32_t>(HotPhys::PhysicalContactResponse::RedirectToAnimatingActor) != 3u)
        return fail(message, messageSize, "contact response vocabulary changed");

    if (message && messageSize)
        std::snprintf(message, messageSize, "%s", "animation invariants ok");
    return true;
}

#endif
