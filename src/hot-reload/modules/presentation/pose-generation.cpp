// 09 16 2026
/* purpose
* Hot production pose generation. A render.frame system reads the generic
* AnimationState.v2 selected by the hot state machine plus generic actor facts
* and generates local bone offsets through the procedural C++ clip library,
* publishing them only as POD via the generic `skeleton.apply` capability. The
* kernel stores them as a POD PoseState; the cold renderer owns
* skeleton/skinning/draw. No Player/Npc/Monster pose type, no pointer into DLL
* memory, and no STL in the applied pose.
* Composition: full-body action pose, or upper-body action overlaid on a
* locomotion base; weapon carry arm overrides; dash/freeze are full-body clips.
* Blends from the previous applied pose during the state machine's blend window.
* Editing this file and saving changes the running client's pose behavior.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-pose.h"
#include "hot-reload/hot-tool-visual.h"

#include <cmath>
#include <cstdint>

namespace {

using SkeletonApplyFn = void (MIMITA_GAME_CALL *)(void*,
                                                  const GameSkeletonPoseV1*);

// TEMPORARY visual-proof switch. Toggled with the `posedebug 1|0` command.
bool g_debugExtreme = false;

void MIMITA_GAME_CALL poseDebugCommand(void* /*host*/, const char* args)
{
    g_debugExtreme = args && args[0] == '1';
    std::printf("[POSE] debug extreme = %d\n", (int)g_debugExtreme);
}

// Map a body action to the tool animation phase it should resolve.
std::uint64_t phaseIdForAction(std::uint64_t actionId)
{
    switch (actionId) {
        case HOT_ACTION_IDLE:
        case HOT_ACTION_EQUIPPED_IDLE:
        case HOT_ACTION_WALK:
        case HOT_ACTION_JUMP:
        case HOT_ACTION_FALL:
        case HOT_ACTION_LAND:
            return TOOL_PHASE_IDLE;
        case HOT_ACTION_SHOOT:
            return TOOL_PHASE_SHOOT;
        case HOT_ACTION_JUST_SHOT:
            return TOOL_PHASE_JUST_SHOT;
        case HOT_ACTION_RELOAD:
            return TOOL_PHASE_RELOAD;
        case HOT_ACTION_EQUIP:
            return TOOL_PHASE_EQUIP;
        case HOT_ACTION_UNEQUIP:
            return TOOL_PHASE_UNEQUIP;
        case HOT_ACTION_SLASH:
            return TOOL_PHASE_SLASH;
        case HOT_ACTION_LUNGE:
            return TOOL_PHASE_LUNGE;
        default:
            return 0;
    }
}

const ToolAnimPhaseV1* findToolPhase(const ToolVisualRecipeV1& tool,
                                     std::uint64_t actionId)
{
    if (!tool.definition.phases || tool.definition.phaseCount == 0)
        return nullptr;
    std::uint64_t want = phaseIdForAction(actionId);
    for (std::uint32_t i = 0; i < tool.definition.phaseCount; ++i)
        if (tool.definition.phases[i].phaseId == want)
            return &tool.definition.phases[i];
    // A tool without a distinct just-shot phase reuses its shoot phase.
    if (actionId == HOT_ACTION_JUST_SHOT) {
        for (std::uint32_t i = 0; i < tool.definition.phaseCount; ++i)
            if (tool.definition.phases[i].phaseId == TOOL_PHASE_SHOOT)
                return &tool.definition.phases[i];
    }
    return nullptr;
}

void addPart(GameSkeletonPoseV1& pose, std::uint64_t part,
             const HotAnim::PartPose& p)
{
    if (pose.count >= GAME_MAX_POSE_PARTS)
        return;
    GamePosePartV1& out = pose.parts[pose.count++];
    out.part = part;
    out.translation[0] = p.trans[0];
    out.translation[1] = p.trans[1];
    out.translation[2] = p.trans[2];
    // Pose library is authored in degrees; the hot boundary unit is radians.
    out.rotationEuler[0] = p.rot[0] * HotAnim::kDeg2Rad;
    out.rotationEuler[1] = p.rot[1] * HotAnim::kDeg2Rad;
    out.rotationEuler[2] = p.rot[2] * HotAnim::kDeg2Rad;
}

// Read the previously applied pose (stored by skeleton.apply as PoseState) and
// convert it back to the library's degrees unit for blending.
bool readPreviousPose(GameplayContextV1* ctx, std::uint64_t entity,
                      HotAnim::Pose& out)
{
    HotPoseStateV1 prev{};
    if (!ctx->dynamicReadComponent(ctx->host, entity,
                                   HOT_POSE_STATE_COMPONENT, &prev,
                                   sizeof(prev)))
        return false;
    out.clear();
    const std::uint32_t n = prev.count < HOT_POSE_MAX_PARTS ? prev.count
                                                            : HOT_POSE_MAX_PARTS;
    for (std::uint32_t i = 0; i < n; ++i) {
        const int idx = HotAnim::partIndexFromHash(prev.part[i]);
        if (idx < 0)
            continue;
        const float kRad2Deg = 1.0f / HotAnim::kDeg2Rad;
        for (int k = 0; k < 3; ++k) {
            out.part[idx].trans[k] = prev.translation[i][k];
            out.part[idx].rot[k] = prev.rotationEuler[i][k] * kRad2Deg;
        }
        out.mask |= (1u << (std::uint32_t)idx);
    }
    return true;
}

void MIMITA_GAME_CALL poseGenerationTick(void* host, std::uint64_t /*tick*/,
                                         float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->resolveCapability)
        return;
    auto apply = reinterpret_cast<SkeletonApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SKELETON_APPLY));
    if (!apply)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t entity = entities[i];
        HotAnimationStateV2 anim{};
        if (!ctx->dynamicReadComponent(ctx->host, entity,
                                       HOT_ANIMATION_STATE_COMPONENT, &anim,
                                       sizeof(anim)))
            continue;

        HotActorActionStateV1 action{};
        const bool hasAction = ctx->dynamicReadComponent(
            ctx->host, entity, HOT_ACTOR_ACTION_COMPONENT, &action,
            sizeof(action));
        const std::uint64_t weaponKey = hasAction ? action.weaponKey : 0;

        HotAnimationMemoryV2 mem{};
        const bool hasMem = ctx->dynamicReadComponent(
            ctx->host, entity, HOT_ANIMATION_MEMORY_COMPONENT, &mem,
            sizeof(mem));
        const bool justLanded = hasMem && mem.prevGrounded == 0;

        // Generic facts for locomotion base composition.
        bool grounded = hasAction &&
                        (action.flags & HOT_ACTION_FLAG_GROUNDED) != 0;
        float vy = 0.0f;
        float speed = hasAction ? action.speed : 0.0f;
        GameVelocityComponentV1 vel{};
        if (ctx->readComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &vel,
                               sizeof(vel))) {
            vy = vel.linear[2];
            const float planar =
                std::sqrt(vel.linear[0] * vel.linear[0] +
                          vel.linear[1] * vel.linear[1]);
            if (planar > speed)
                speed = planar;
        }
        GameMovementRuntimeStateComponentV1 runtime{};
        if (ctx->readComponent(ctx->host, entity,
                               GAME_COMPONENT_MOVEMENT_RUNTIME_STATE, &runtime,
                               sizeof(runtime)))
            grounded = runtime.grounded != 0;

        constexpr float kWalkSpeedRef = 6.0f;
        float speed01 = speed / kWalkSpeedRef;
        if (speed01 < 0.0f) speed01 = 0.0f;
        if (speed01 > 1.5f) speed01 = 1.5f;

        // Resolve the equipped tool's per-phase arm clip. During unequip the
        // tool is already gone, so the remembered key names the departing tool.
        std::uint64_t toolKey = hasAction ? action.weaponKey : 0;
        if (anim.actionId == HOT_ACTION_UNEQUIP && hasMem &&
            mem.departingWeaponKey != 0)
            toolKey = mem.departingWeaponKey;
        const ToolVisualRecipeV1* tool = findToolVisual(toolKey);
        const ToolAnimPhaseV1* toolPhase =
            tool ? findToolPhase(*tool, anim.actionId) : nullptr;

        const HotAnim::ActionClip clip = HotAnim::actionClip(anim.actionId);
        HotAnim::Pose target;
        if (toolPhase && toolPhase->frames && toolPhase->frameCount > 0) {
            // Tool phases drive the arms; compose them over the locomotion base
            // so the legs keep moving while the tool shoots/reloads/equips.
            const float locoTime =
                hasMem ? mem.locomotionTime : anim.playbackTime;
            const std::uint64_t baseAction = HotAnim::locomotionAction(
                grounded, vy, justLanded, speed01, true);
            HotAnim::Pose base;
            HotAnim::evaluateAction(baseAction, locoTime, speed01, base);
            HotAnim::ActionClip pc{};
            pc.duration = toolPhase->duration;
            pc.loop = static_cast<std::uint8_t>(toolPhase->loop ? 1 : 0);
            pc.fullBody = 0;
            pc.mask = toolPhase->mask;
            pc.frames = toolPhase->frames;
            pc.frameCount = toolPhase->frameCount;
            HotAnim::Pose overlay;
            HotAnim::sampleClip(pc, anim.playbackTime, overlay);
            HotAnim::applyMask(base, overlay, toolPhase->mask);
            target = base;
        } else if (clip.fullBody == 0) {
            // Upper-body action over a locomotion base (generic fallback).
            const float locoTime = hasMem ? mem.locomotionTime : anim.playbackTime;
            const std::uint64_t baseAction = HotAnim::locomotionAction(
                grounded, vy, justLanded, speed01, weaponKey != 0);
            HotAnim::Pose base;
            HotAnim::evaluateAction(baseAction, locoTime, speed01, base);
            HotAnim::Pose overlay;
            HotAnim::evaluateAction(anim.actionId, anim.playbackTime, speed01,
                                    overlay);
            HotAnim::applyMask(base, overlay, clip.mask);
            target = base;
        } else {
            HotAnim::evaluateAction(anim.actionId, anim.playbackTime, speed01,
                                    target);
        }
        if (!toolPhase)
            HotAnim::applyWeaponArms(target, weaponKey, anim.actionId);

        // Blend from the previous pose during the state machine's blend window.
        float w = (anim.flags & HOT_ANIM_FLAG_BLENDING) ? anim.blendWeight : 1.0f;
        HotAnim::Pose finalPose = target;
        HotAnim::Pose prev;
        if (w < 1.0f && readPreviousPose(ctx, entity, prev))
            HotAnim::blendPose(prev, target, w, finalPose);

        GameSkeletonPoseV1 pose{};
        pose.entity = entity;
        pose.flags = 2;  // pose contract version 2 (radians)
        for (std::uint32_t p = 0; p < HotAnim::PartCount; ++p) {
            if ((finalPose.mask & (1u << p)) == 0)
                continue;
            addPart(pose, HotAnim::partHash(p), finalPose.part[p]);
        }

        if (g_debugExtreme) {
            pose.count = 0;
            HotAnim::PartPose lp{}, rp{}, tp{}, hp{};
            lp.rot[2] = 90.0f;
            rp.rot[2] = -90.0f;
            tp.rot[0] = 35.0f;
            hp.rot[1] = 45.0f;
            addPart(pose, HotAnim::partHash(HotAnim::PartLeftArm), lp);
            addPart(pose, HotAnim::partHash(HotAnim::PartRightArm), rp);
            addPart(pose, HotAnim::partHash(HotAnim::PartTorso), tp);
            addPart(pose, HotAnim::partHash(HotAnim::PartHead), hp);
        }

        apply(ctx->host, &pose);
    }
}

const MimitaHotPackage::SchemaRegistrar s_poseStateSchema{
    {HOT_POSE_STATE_COMPONENT, gameHash("PoseState.v1"), sizeof(HotPoseStateV1),
     8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "PoseState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_poseSystem{
    {gameHash("hot.pose-generation"), GAME_DOMAIN_RENDER, 2, 0,
     poseGenerationTick, "hot.pose-generation"}};
const MimitaHotPackage::CommandRegistrar s_poseDebugCommand{
    {"posedebug", "posedebug 1|0 - temporary extreme pose for visual proof", 0,
     poseDebugCommand}};

} // namespace

#endif
