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

// v3 introduces versioned data envelopes, named module tables, and a
// deterministic self-test hook. The replaceable DLL still ships as one coarse
// module collection so function-level or per-module DLLs can arrive later
// without changing the public contract.
static constexpr std::uint32_t MIMITA_GAME_API_VERSION = 3;
static constexpr std::uint32_t MIMITA_GAME_MAX_MODULES = 8;
static constexpr std::size_t MIMITA_GAME_SELFTEST_MESSAGE = 128;

// State type identifiers carried by GameEnvelope::stateType. Later phases add
// the concrete plain-data structs; the identifier space is reserved here.
enum GameStateType : std::uint32_t {
    GAME_STATE_NONE = 0,
    GAME_STATE_ACTOR = 1,
    GAME_STATE_MOVEMENT = 2,
    GAME_STATE_PROJECTILE = 3,
    GAME_STATE_EFFECT = 4,
    GAME_STATE_UI = 5,
    GAME_STATE_CAMERA = 6,
};

// Versioned data envelope. Hot code receives state and returns decisions or
// results through this stable plain-data contract. It must never carry STL
// containers, owning pointers, live engine objects, or unstable class layouts.
struct GameEnvelope {
    std::uint32_t stateType;
    std::uint32_t stateVersion;
    std::uint32_t byteSize;
    std::uint32_t flags;
    void* data;
    std::uint64_t tick;
    std::uint64_t codeGeneration;
};

// Result of a candidate's deterministic smoke validation.
struct GameSelfTestResult {
    std::uint32_t structSize;
    std::uint32_t passed;
    std::uint64_t checksum;
    char message[MIMITA_GAME_SELFTEST_MESSAGE];
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
using GameSelfTestFn = bool (MIMITA_GAME_CALL *)(GameSelfTestResult* out);

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

using GameUpdateEffectsFn = void (MIMITA_GAME_CALL *)(
    GameMemory* memory,
    GameEffectPartState* effects,
    std::uint32_t effectCount,
    float dt);

// A named, ABI-versioned function table inside the coarse replaceable DLL.
// A null function table means the module is declared but not implemented yet.
struct GameModuleDescriptor {
    const char* name;
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    const void* functions;
};

// Effects module function table. Expanded effect/visual entry points arrive in
// a later phase; updateEffects keeps the existing behavior.
struct GameEffectModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    GameUpdateEffectsFn updateEffects;
};

struct GameAPI {
    std::uint32_t version;
    std::uint32_t structSize;
    // Filled by the EXE after load; the DLL leaves these zero.
    std::uint32_t generation;
    std::uint32_t reserved;
    std::uint64_t codeHash;
    GameOnReloadFn onReload;
    GameBeforeUnloadFn beforeUnload;
    GameUpdateEffectsFn updateEffects;
    GameSelfTestFn selfTest;
    std::uint32_t moduleCount;
    std::uint32_t reserved2;
    GameModuleDescriptor modules[MIMITA_GAME_MAX_MODULES];
};

using GetGameAPIFn = bool (MIMITA_GAME_CALL *)(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);

MIMITA_GAME_EXPORT bool MIMITA_GAME_CALL GetGameAPI(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);
