// 09 15 2026
/* purpose
* Hot pose generation. A render.frame system reads the generic AnimationState
* (+ playback time) and generates local bone offsets, publishing them through
* the generic `skeleton.apply` capability. The kernel stores them as a POD
* PoseState on the entity; the cold renderer owns skeleton/skinning/draw. No
* Player/Npc/Monster pose type and no pointer into DLL memory is stored.
* Editing this file and saving changes the running client's pose behavior.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-pose.h"

#include <cmath>
#include <cstdint>

namespace {

const std::uint64_t kTorso = gameHash("torso");
const std::uint64_t kHead = gameHash("head");
const std::uint64_t kLeftArm = gameHash("leftArm");
const std::uint64_t kRightArm = gameHash("rightArm");
const std::uint64_t kLeftLeg = gameHash("leftLeg");
const std::uint64_t kRightLeg = gameHash("rightLeg");

using SkeletonApplyFn = void (MIMITA_GAME_CALL *)(void*,
                                                  const GameSkeletonPoseV1*);

void addPart(GameSkeletonPoseV1& pose, std::uint64_t part, float tx, float ty,
             float tz, float rx, float ry, float rz)
{
    if (pose.count >= GAME_MAX_POSE_PARTS)
        return;
    GamePosePartV1& p = pose.parts[pose.count++];
    p.part = part;
    p.translation[0] = tx; p.translation[1] = ty; p.translation[2] = tz;
    p.rotationEuler[0] = rx; p.rotationEuler[1] = ry; p.rotationEuler[2] = rz;
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
        HotAnimationStateV1 anim{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_ANIMATION_STATE_COMPONENT, &anim,
                                       sizeof(anim)))
            continue;

        const float t = anim.playbackTime;
        GameSkeletonPoseV1 pose{};
        pose.entity = entities[i];
        pose.flags = 1;  // pose version

        if (anim.clipId == HOT_ANIM_DEAD) {
            addPart(pose, kTorso, 0.0f, 0.0f, 0.0f, -1.5708f, 0.0f, 0.0f);
            addPart(pose, kHead, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f);
            addPart(pose, kLeftArm, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f);
            addPart(pose, kRightArm, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.4f);
        } else if (anim.clipId == HOT_ANIM_MOVE) {
            const float swing = std::sin(t * 8.0f);
            addPart(pose, kLeftLeg, 0.0f, 0.0f, 0.0f, swing * 0.6f, 0.0f, 0.0f);
            addPart(pose, kRightLeg, 0.0f, 0.0f, 0.0f, -swing * 0.6f, 0.0f, 0.0f);
            addPart(pose, kLeftArm, 0.0f, 0.0f, 0.0f, -swing * 0.4f, 0.0f, 0.0f);
            addPart(pose, kRightArm, 0.0f, 0.0f, 0.0f, swing * 0.4f, 0.0f, 0.0f);
        } else {
            const float sway = std::sin(t * 1.5f);
            addPart(pose, kLeftArm, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, sway * 0.08f);
            addPart(pose, kRightArm, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -sway * 0.08f);
            addPart(pose, kTorso, 0.0f, 0.0f, 0.0f, 0.0f, sway * 0.05f, 0.0f);
        }

        apply(ctx->host, &pose);
    }
}

const MimitaHotPackage::SchemaRegistrar s_poseStateSchema{
    {HOT_POSE_STATE_COMPONENT, gameHash("PoseState.v1"), sizeof(HotPoseStateV1), 8,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "PoseState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_poseSystem{
    {gameHash("hot.pose-generation"), GAME_DOMAIN_RENDER, 2, 0,
     poseGenerationTick, "hot.pose-generation"}};

} // namespace

#endif
