// 2026-11-12
// Purpose: bounded logging capability for one game.dll generation.
//
// The host owns the canonical StructuredLogger and events.jsonl sink. The game
// generation sends plain values and a bounded UTF-8 JSON fields string; it does
// not link the resident logger implementation or write a second journal file.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kLogCapabilityVersion = 1u;
inline constexpr std::uint32_t kMaxLogPayloadBytes = 16u * 1024u;

enum class LogCategory : std::int32_t {
    General = 0,
    Glb,
    Replay,
    Camera,
    Audio,
    Physics,
    Performance,
    Collision,
    NpcCombat,
    NpcMovement,
    Ragdoll,
    Weapons,
    Animation,
    Gui,
    Avatar,
    Network,
    World,
    Duel,
    Auth,
    Chat,
    Vip,
    Rendering,
    GlbModels,
    Executable,
    GrenadeLauncher,
    Healthbar,
    Skybox,
    ChatLayout,
    Count,
};

enum class LogLevel : std::int32_t {
    Off = 0,
    Errors,
    Important,
    Verbose,
    Trace,
};

struct LogCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Free-form text at a category/level.
    void (*text)(void* hostUserData, std::int32_t category, std::int32_t level,
                 const char* message);

    // One structured events.jsonl record. fieldsJson is a bounded UTF-8 JSON
    // object string; source fields may be null/zero.
    void (*event)(void* hostUserData, std::int32_t category, std::int32_t level,
                  const char* eventName, const char* correlationId,
                  const char* reason, std::uint32_t tick, const char* fieldsJson,
                  const char* sourceFile, std::int32_t sourceLine,
                  const char* functionName);

    // Host-side category/level filter.
    std::int32_t (*should_log)(void* hostUserData, std::int32_t category,
                               std::int32_t level);
};

}  // namespace mimita::host
