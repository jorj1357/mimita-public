// 2026-11-12
// Purpose: versioned C ABI between the resident host (currently mimita.exe) and
// one replaceable whole-game game.dll generation.
//
// Rules:
// - Plain data, fixed-width values, and stable handles only.
// - No STL containers, exceptions, RTTI-dependent objects, allocator-owned
//   objects, or pointers into short-lived game-DLL memory cross this boundary.
// - The host owns persistent state; the game generation owns behavior.
// - Every structure starts with a version and a byte size so a mismatch is a
//   clean rejection, never an out-of-bounds read.
//
// This header declares the boundary only. It is not wired into the EXE yet.

#pragma once

#include <cstddef>
#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kGameHostAbiVersion = 1u;
inline constexpr std::uint32_t kHostServicesVersion = 2u;
inline constexpr std::uint32_t kGameModuleVersion = 1u;

// Capability ids returned by HostServicesV1::get_capability.
enum class HostCapabilityId : std::uint32_t {
    Log = 1,
    Transport = 2,
    Modules = 3,
    Audio = 4,
    Effects = 5,
    Entity = 6,
    Render = 7,
    Jobs = 8,
    Callbacks = 9,
    Resources = 10,
};

// Stable entity identity. The low bits are generation-specific; the host owns
// the mapping from handle to live record and revalidates it on every access.
using EntityHandle = std::uint64_t;
inline constexpr EntityHandle kInvalidEntity = 0u;

inline constexpr std::uint32_t kCodeHashHexLength = 65u;  // 64 hex chars + NUL
inline constexpr std::uint32_t kModuleNameLength = 64u;

enum class HostResult : std::int32_t {
    Ok = 0,
    Busy = 1,
    InvalidArgument = 2,
    Unsupported = 3,
    NotFound = 4,
    MigrationFailed = 5,
    QuiesceFailed = 6,
    InternalError = 7,
};

enum class QuiesceMode : std::int32_t {
    Swap = 0,      // generation may continue in a new module
    Shutdown = 1,  // process or module is stopping
};

// A bounded, generation-neutral view of one entity record. The host fills this
// from its resident store; the game generation reads it and requests changes by
// writing back through the same structure. Fields are intentionally small and
// additive: new fields are appended and the structSize is bumped.
struct EntitySnapshotV1 {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    EntityHandle handle;
    std::int32_t kind;
    std::int32_t team;
    float position[3];
    float velocity[3];
    float health;
    float radius;
    std::uint64_t flags;
    std::uint32_t playerFlags;
    std::uint32_t maxHealth;
    char displayName[64];
};

// Resident services the game generation may call. Functions are synchronous and
// must be safe to call from the owning simulation thread. A generation that
// needs a different thread must submit through the host job facility.
struct HostServicesV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    void* hostUserData;

    void (*log)(void* hostUserData, std::int32_t level, const char* message);

    // Read/write one entity snapshot. Returns NotFound for a stale handle.
    HostResult (*entity_read)(void* hostUserData, EntityHandle handle,
                              EntitySnapshotV1* out);
    HostResult (*entity_write)(void* hostUserData, EntityHandle handle,
                               const EntitySnapshotV1* in);

    // Authoritative fixed tick and wall clock, owned by the host.
    std::uint64_t (*tick)(void* hostUserData);
    double (*time_seconds)(void* hostUserData);

    // Generation-scoped scratch allocation owned by the host. The host frees
    // every block tagged with the generation at unload, even if the generation
    // forgot to release it. Alignment must be a power of two.
    void* (*reserve)(void* hostUserData, std::size_t bytes, std::size_t alignment,
                     const char* tag);
    void (*release)(void* hostUserData, void* pointer, const char* tag);

    // One generic lookup for broad versioned capability families. Returns a
    // pointer to a plain-data struct with version/structSize, or null.
    using GetCapabilityFn = void* (*)(void* hostUserData, std::uint32_t capability,
                                      std::uint32_t version);
    GetCapabilityFn get_capability;
};

// The single export every game.dll generation must return. Validated before any
// function pointer is used:
//   name matches, abiVersion == kGameHostAbiVersion, structSize == sizeof.
struct GameModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    std::uint32_t version;       // candidate implementation version
    std::uint32_t stateSchema;
    std::uint64_t generation;
    char codeHash[kCodeHashHexLength];  // SHA-256 of the source revision
    char name[kModuleNameLength];       // "game"

    // Must finish before the generation is dispatched to.
    HostResult (*initialize)(const HostServicesV1* services);

    // Must stop accepting new work and report BUSY while any call, callback, or
    // job can still re-enter this generation. The host refuses to unload on
    // anything other than Ok.
    HostResult (*quiesce)(QuiesceMode mode);

    // Release generation-owned resources that must be freed with the correct
    // context current. Called after quiesce returns Ok and before unload.
    HostResult (*shutdown)();

    HostResult (*tick)(double deltaSeconds);
    HostResult (*render)(void* renderContext);

    // Event payloads are plain, size-checked structures. The pointer is mutable
    // so an in/out request can carry the result back without a second call.
    HostResult (*on_event)(std::uint32_t eventId, void* payload,
                           std::size_t payloadSize, std::int32_t* handled);

    // State handoff is optional for the first code-only proof. export_state
    // returns the required byte count; if capacity is too small no bytes are
    // written. import_state must leave the previous state intact on failure.
    std::uint32_t (*export_state)(void* buffer, std::uint32_t capacity);
    HostResult (*import_state)(const void* buffer, std::uint32_t size);
};

using GetGameModuleFn = const GameModuleV1* (*)(std::uint32_t abiVersion);

inline constexpr const char* kGetGameModuleSymbol = "GetMimitaGameModuleV1";

inline bool is_valid_host_result(HostResult result) {
    const auto value = static_cast<std::int32_t>(result);
    return value >= static_cast<std::int32_t>(HostResult::Ok) &&
           value <= static_cast<std::int32_t>(HostResult::InternalError);
}

}  // namespace mimita::host
