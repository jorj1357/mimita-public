// 09 14 2026
/* purpose
* Falsification consumer: requires `banana.launch` and resolves it through the
* generic resolveCapability doorway, proving a hot package can depend on and
* invoke a capability it does not define. The provider lives in
* banana-provider.cpp and can be replaced/deleted live.
* Does NOT own gameplay state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>
#include <cstdio>

namespace {

using BananaLaunchFn = std::uint32_t (MIMITA_GAME_CALL *)(void*, std::uint32_t);

GameplayContextV1* gCtx = nullptr;

void MIMITA_GAME_CALL bananaConsumerTick(void* host, std::uint64_t tick, float /*dt*/)
{
    gCtx = static_cast<GameplayContextV1*>(host);
    if (!gCtx || !gCtx->resolveCapability)
        return;
    auto fn = reinterpret_cast<BananaLaunchFn>(
        gCtx->resolveCapability(gCtx->host, gameHash("banana.launch")));
    if (!fn)
        return;
    (void)fn(gCtx->host, (std::uint32_t)(tick & 0xffffu));
}

void MIMITA_GAME_CALL bananaCommand(void*, const char* /*args*/)
{
    if (!gCtx || !gCtx->resolveCapability) {
        std::printf("[BANANA] consumer has no context yet (run a tick first)\n");
        return;
    }
    auto fn = reinterpret_cast<BananaLaunchFn>(
        gCtx->resolveCapability(gCtx->host, gameHash("banana.launch")));
    if (!fn) {
        std::printf("[BANANA] banana.launch missing (provider not active)\n");
        return;
    }
    const std::uint32_t result = fn(gCtx->host, 0);
    std::printf("[BANANA] launched result=%u\n", result);
}

const MimitaHotPackage::SystemRegistrar s_bananaConsumer{
    {gameHash("banana.consumer"), GAME_DOMAIN_POST_MOVEMENT, 100, 0,
     bananaConsumerTick, "banana.consumer"}};

const MimitaHotPackage::CapabilityRequirementRegistrar s_bananaRequirement{
    gameHash("banana.launch"), gameHash("sig.banana.launch.v1"), 0};

const MimitaHotPackage::CommandRegistrar s_bananaCmd{
    {"bananal", "bananal - resolve+invoke banana.launch", 0, bananaCommand}};

} // namespace

#endif
