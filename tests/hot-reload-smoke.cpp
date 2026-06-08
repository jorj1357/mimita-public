#include "hot-reload/hot-reload-system.h"

#include <chrono>
#include <cstdio>
#include <filesystem>

int main()
{
    HotReloadSystem& hotReload = HotReloadSystem::instance();
    if (!hotReload.loadGameDLL())
        return 1;

    GameEffectPartState effect{};
    effect.position[0] = 1.0f;
    effect.velocity[0] = 2.0f;
    effect.maxLifetime = 10.0f;
    effect.alive = 1;

    const GameAPI* api = hotReload.gameAPI();
    api->updateEffects(&hotReload.gameMemory(), &effect, 1, 0.5f);
    if (effect.position[0] < 1.19f || effect.position[0] > 1.21f)
        return 2;

    hotReload.reloadGameDLLIfChanged();
    const auto source =
        std::filesystem::current_path() / "src" / "effects" / "effect-part.cpp";
    const auto originalSourceTime = std::filesystem::last_write_time(source);
    std::filesystem::last_write_time(source, originalSourceTime + std::chrono::seconds(2));
    const bool reloaded = hotReload.reloadGameDLLIfChanged();
    std::filesystem::last_write_time(source, originalSourceTime);
    if (!reloaded)
        return 3;

    api = hotReload.gameAPI();
    api->updateEffects(&hotReload.gameMemory(), &effect, 1, 0.5f);
    if (effect.position[0] < 1.39f || effect.position[0] > 1.41f)
        return 4;

    std::printf("[HOT RELOAD TEST] state survived DLL swap position=%.2f reloads=%u\n",
                effect.position[0], hotReload.gameMemory().reloadCount);
    hotReload.unloadGameDLL();
    return 0;
}
