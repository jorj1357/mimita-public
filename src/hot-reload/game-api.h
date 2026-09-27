#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define MIMITA_GAME_CALL __cdecl
#if defined(MIMITA_GAME_DLL)
#define MIMITA_GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define MIMITA_GAME_EXPORT extern "C"
#endif
#else
#define MIMITA_GAME_CALL
#define MIMITA_GAME_EXPORT extern "C"
#endif

static constexpr std::uint32_t MIMITA_GAME_API_VERSION = 3;

struct GameEffectPartState {
    float position[3];
    float velocity[3];
    float lifetime;
    float maxLifetime;
    float gravity;
    float drag;
    std::uint8_t alive;
    std::uint8_t sticky;
    std::uint8_t affectedByGravity;
    std::uint8_t reserved;
};

struct GameQuickHitDebugVisualState {
    float position[3];
    float radius;
    float color[4];
    std::uint8_t enabled;
    std::uint8_t wireframe;
    std::uint8_t reserved[2];
};

using GameLogFn = void (MIMITA_GAME_CALL *)(const char* message);

struct GamePlatformAPI {
    std::uint32_t version;
    GameLogFn log;
};

struct GameMemory {
    std::uint32_t apiVersion;
    std::uint32_t reloadCount;
    void* permanentStorage;
    std::size_t permanentStorageSize;
    GamePlatformAPI platform;
};

using GameOnReloadFn = bool (MIMITA_GAME_CALL *)(GameMemory* memory);
using GameBeforeUnloadFn = void (MIMITA_GAME_CALL *)(GameMemory* memory);
using GameUpdateEffectsFn = void (MIMITA_GAME_CALL *)(
    GameMemory* memory,
    GameEffectPartState* effects,
    std::uint32_t effectCount,
    float dt);
using GameUpdateQuickHitDebugVisualFn = void (MIMITA_GAME_CALL *)(
    GameMemory* memory,
    GameQuickHitDebugVisualState* visual);

struct GameAPI {
    std::uint32_t version;
    std::uint32_t structSize;
    GameOnReloadFn onReload;
    GameBeforeUnloadFn beforeUnload;
    GameUpdateEffectsFn updateEffects;
    GameUpdateQuickHitDebugVisualFn updateQuickHitDebugVisual;
};

using GetGameAPIFn = bool (MIMITA_GAME_CALL *)(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);

MIMITA_GAME_EXPORT bool MIMITA_GAME_CALL GetGameAPI(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);
