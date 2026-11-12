// 2026-11-12
// Purpose: generic, versioned module-host capability for a game.dll generation.
//
// This is the first partial family bridge: only the small, frequently used
// ModuleHost queries/control methods cross the boundary. Typed policy requests
// remain UNRESOLVED until their operations are added. No silent stubs.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kModuleCapabilityVersion = 1u;

enum class ModuleOp : std::uint32_t {
    ActiveGeneration = 1,
    ModuleLoaded = 2,
    MapEditorLoaded = 3,
    NpcPerceptionLoaded = 4,
    NpcBehaviorLoaded = 5,
    BeginFixedTick = 6,
    EndFixedTick = 7,
    Startup = 8,
    Poll = 9,
};

struct ModuleCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Returns 1 when handled, 0 when unsupported/failed. input/output are plain
    // bounded values valid only for the call; the host must not retain them.
    std::int32_t (*invoke)(void* hostUserData, ModuleOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
