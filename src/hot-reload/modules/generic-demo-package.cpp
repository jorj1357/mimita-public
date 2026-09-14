// 09 14 2026
/* purpose
* Demonstration + package aggregation for the hot DLL generation. Systems,
* events, schemas, and commands self-register through HotPackageBuilder so
* adding a new hot source file registers a new concept without editing this
* file or the EXE.
* This file is part of the hot module glob (src/hot-reload/modules/*.cpp).
* Does NOT own gameplay state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"
#include "hot-reload/hot-package.h"

#include <cstdio>
#include <cstdint>

namespace {

void MIMITA_GAME_CALL demoGameplayTick(void* /*host*/, std::uint64_t tick, float /*dt*/)
{
    static std::uint64_t last = 0;
    if (tick - last >= 300) {
        last = tick;
        std::printf("[GENERIC DEMO] gameplay.60 tick=%llu\n",
                    (unsigned long long)tick);
    }
}

void MIMITA_GAME_CALL demoRenderTick(void* /*host*/, std::uint64_t tick, float /*dt*/)
{
    static std::uint64_t last = 0;
    if (tick - last >= 120) {
        last = tick;
        std::printf("[GENERIC DEMO] render.frame tick=%llu rev=1\n",
                    (unsigned long long)tick);
    }
}

void MIMITA_GAME_CALL demoCommand(void* /*host*/, const char* args)
{
    std::printf("[GENERIC DEMO] hotdemo args='%s'\n", args ? args : "");
}

// Package identity (the aggregation root).
const bool s_packageSet = []() {
    HotPackageBuilder::instance().setPackage(
        gameHash("mimita.core"), gameHash("mimita.core.v1"), "mimita.core");
    return true;
}();

// Self-registered systems, events, schemas, and commands.
const MimitaHotPackage::SystemRegistrar s_demoGameplay{
    {gameHash("demo.gameplay-tick"), GAME_DOMAIN_GAMEPLAY, 1000, 0,
     demoGameplayTick, "demo.gameplay-tick"}};
const MimitaHotPackage::SystemRegistrar s_demoRender{
    {gameHash("demo.render-tick"), GAME_DOMAIN_RENDER, 1000, 0,
     demoRenderTick, "demo.render-tick"}};
const MimitaHotPackage::EventRegistrar s_demoPing{
    {gameHash("demo.ping"), 0, 0, nullptr, "demo.ping"}};
const MimitaHotPackage::SchemaRegistrar s_demoTag{
    {gameHash("DemoTag"), gameHash("DemoTag.v1"), 4, 4, GAME_COPY_AUTHORING, 0,
     "DemoTag"}};
const MimitaHotPackage::CommandRegistrar s_demoCmd{
    {"hotdemo", "hotdemo [text] - generic runtime command demo", 0, demoCommand}};

} // namespace

const GamePackageDescriptorV1* MimitaGetPackageDescriptor()
{
    return HotPackageBuilder::instance().descriptor();
}

#endif
