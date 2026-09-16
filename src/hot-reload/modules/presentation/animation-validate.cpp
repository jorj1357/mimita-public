// 09 16 2026
/* purpose
* Hot animation skeleton validation. A render.frame system (priority 0, before
* the state machine and pose generation) validates each animated actor's
* required body parts through the generic `skeleton.validate` capability and
* records a local-only result. A model/resource swap consumer uses the result to
* reject a mesh missing required parts WITHOUT replacing the active generation.
* Also registers the `animvalidate` diagnostic command.
* Does NOT pose, draw, or own actor state.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-package.h"

#include <cstdint>
#include <cstdio>

namespace {

using SkeletonValidateFn = bool (MIMITA_GAME_CALL *)(void*,
                                                     GameSkeletonValidateV1*);

void fillRequired(GameSkeletonValidateV1& req, std::uint64_t entity)
{
    req = GameSkeletonValidateV1{};
    req.entity = entity;
    req.requiredCount = HotAnim::kRequiredPartCount;
    for (std::uint32_t i = 0; i < HotAnim::kRequiredPartCount; ++i)
        req.requiredParts[i] = HotAnim::requiredPartHash(i);
}

void MIMITA_GAME_CALL animationValidateTick(void* host, std::uint64_t /*tick*/,
                                            float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicWriteComponent ||
        !ctx->resolveCapability)
        return;
    auto validate = reinterpret_cast<SkeletonValidateFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SKELETON_VALIDATE));
    if (!validate)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        GameSkeletonValidateV1 req;
        fillRequired(req, entities[i]);
        if (!validate(ctx->host, &req))
            continue;
        const std::uint32_t requiredMask =
            (HotAnim::kRequiredPartCount >= 32)
                ? 0xFFFFFFFFu
                : ((1u << HotAnim::kRequiredPartCount) - 1u);
        HotAnimationValidV1 v{};
        v.version = HOT_ANIMATION_VALID_VERSION;
        v.validated = 1;
        v.valid = req.valid;
        v.missingMask = (~req.presentMask) & requiredMask;
        ctx->dynamicWriteComponent(ctx->host, entities[i],
                                   HOT_ANIMATION_VALID_COMPONENT, &v, sizeof(v));
    }
}

void MIMITA_GAME_CALL animValidateCommand(void* host, const char* /*args*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->resolveCapability) {
        std::printf("[ANIMVALIDATE] unavailable\n");
        return;
    }
    auto validate = reinterpret_cast<SkeletonValidateFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SKELETON_VALIDATE));
    if (!validate) {
        std::printf("[ANIMVALIDATE] skeleton.validate unavailable\n");
        return;
    }
    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, 256);
    std::uint32_t validCount = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        GameSkeletonValidateV1 req;
        fillRequired(req, entities[i]);
        validate(ctx->host, &req);
        if (req.valid)
            ++validCount;
        else
            std::printf("[ANIMVALIDATE] entity=%llu valid=0 missing=%u\n",
                        (unsigned long long)entities[i], req.missingCount);
    }
    std::printf("[ANIMVALIDATE] checked=%u valid=%u\n", count, validCount);
}

const MimitaHotPackage::SchemaRegistrar s_animationValidSchema{
    {HOT_ANIMATION_VALID_COMPONENT, gameHash("AnimationValid.v1"),
     sizeof(HotAnimationValidV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "AnimationValid", HOT_ANIMATION_VALID_VERSION, 0}};
const MimitaHotPackage::SystemRegistrar s_animationValidateSystem{
    {gameHash("hot.animation-validate"), GAME_DOMAIN_RENDER, 0, 0,
     animationValidateTick, "hot.animation-validate"}};
const MimitaHotPackage::CommandRegistrar s_animValidateCommand{
    {"animvalidate", "animvalidate - report required body parts on animated actors",
     0, animValidateCommand}};

} // namespace

#endif
