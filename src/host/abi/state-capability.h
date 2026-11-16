// 2026-11-13
// B-lite resident menu/session state.
//
// This is a fixed-layout host-owned scalar record plus opaque object storage.
// It is deliberately smaller than the final generic schema store: direct
// access and no per-tick reflection. Opaque objects are game-typed and may
// contain ordinary C++ members. The maximal general form is deferred.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kStateCapabilityVersion = 3u;

struct MenuStateV1 {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;

    std::int32_t gameState;
    std::int32_t selectedEditorObject;
    bool freecamEnabled;
    bool editorMode;
    bool worldLoaded;
};

// Opaque persistent objects are allocated by the host and constructed by the
// game once. Their byte layout is owned by the game type, not the host ABI.
enum class StateObjectId : std::uint32_t {
    Camera = 1,
    ActiveMapPath = 2,
    ActiveGameMode = 3,
    Player = 4,
    World = 5,
    NpcSystem = 6,
    Weapons = 7,
    ChatHistory = 8,
    DuelConfig = 9,
    MpContext = 10,
    ReplayRecorder = 11,
    ReplayPlayer = 12,
    ReplayFactory = 13,
    ReplayBrowser = 14,
    ReplayTimeline = 15,
    ReplayCameraMgr = 16,
    ReplayChatStates = 17,
    ReplayClipsCache = 18,
    CommandBinds = 19,
    BindPrev = 20,
    ReplayActorModels = 21,
    ReplayWeaponModels = 22,
    ReplaySaveWorker = 23,
    ServerGamemode = 24,
    MigrationProbe = 25,
    PlayerCore = 26,
};

// Called by the host when a resident record must grow. The game owns the
// layout, so it constructs the new block, moves the known fields from the old
// block, and destroys the old object. The host frees the old raw memory.
// Returns true when the new block is valid and may be committed. A false
// result leaves the old block and old generation state active.
using StateMigrateFn = bool (*)(void* oldBlock, std::uint32_t oldSize,
                                void* newBlock, std::uint32_t newSize);

struct StateCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    MenuStateV1* (*menu_state)(void* hostUserData);
    void* (*ensure_object)(void* hostUserData, StateObjectId id,
                           std::uint32_t size, std::uint32_t alignment,
                           std::int32_t* created);
    void* (*ensure_object_migrating)(void* hostUserData, StateObjectId id,
                                     std::uint32_t size,
                                     std::uint32_t alignment,
                                     StateMigrateFn migrate,
                                     std::int32_t* created);
};

}  // namespace mimita::host
