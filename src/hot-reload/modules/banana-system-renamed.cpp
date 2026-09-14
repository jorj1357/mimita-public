// 09 14 2026
/* purpose
* Falsification probe for the generic runtime: a system concept that did not
* exist when mimita.exe started. It self-registers through HotPackageBuilder,
* so creating this file alone (with no EXE edit, no enum, no slot) must make it
* build, discover, register, validate, activate, and execute while the process
* stays running. It also emits a brand-new event type (banana.eaten) defined in
* the sibling split file banana-events.cpp.
* Does NOT own gameplay state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdio>
#include <cstdint>

namespace {

std::uint64_t gBananaTicks = 0;

void MIMITA_GAME_CALL bananaPeelSlipTick(void* host, std::uint64_t tick, float /*dt*/)
{
    ++gBananaTicks;
    static std::uint64_t last = 0;
    if (tick - last >= 300) {
        last = tick;
        std::printf("[HOT_SYSTEM] banana.peel-slip alive count=%llu tick=%llu\n",
                    (unsigned long long)gBananaTicks, (unsigned long long)tick);
    }

    // Emit the brand-new event type through the kernel's generic emit
    // capability (host is the GameplayContextV1 provided by the scheduler).
    static std::uint64_t lastEmit = 0;
    if (tick - lastEmit >= 600) {
        lastEmit = tick;
        auto* ctx = static_cast<GameplayContextV1*>(host);
        if (ctx && ctx->emitEvent) {
            GameEventV1 event{};
            event.typeId = gameHash("banana.eaten");
            event.schemaHash = gameHash("banana.eaten.v1");
            event.payloadVersion = 1;
            event.payloadSize = 0;
            event.tick = tick;
            using EmitFn = void (MIMITA_GAME_CALL *)(GameplayContextV1*,
                                                     const GameEventV1*);
            reinterpret_cast<EmitFn>(ctx->emitEvent)(ctx, &event);
            std::printf("[HOT_EMIT] banana.eaten queued tick=%llu\n",
                        (unsigned long long)tick);
        }
    }
}

const MimitaHotPackage::SystemRegistrar s_banana{
    {gameHash("banana.peel-slip"), GAME_DOMAIN_GAMEPLAY, 500, 0,
     bananaPeelSlipTick, "banana.peel-slip"}};

} // namespace

#endif
