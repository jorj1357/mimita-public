// 2026-11-13
// Versioned resident-global capability.
//
// The host owns the address of each process-global pointer/scalar. Game code
// queries values by stable id instead of duplicating EXE globals.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kGlobalCapabilityVersion = 1u;

enum class GlobalId : std::uint32_t {
    Player = 1,
    Camera = 2,
    World = 3,
    NpcSystem = 4,
    Weapons = 5,
    FreecamEnabled = 6,
    DeathPosition = 7,
    SelectedEditorObject = 8,
    EditorMode = 9,
    ActiveGameMode = 10,
    ActiveMapPath = 11,
    WorldLoaded = 12,
    ReplayRecorder = 13,
    ReplayPlayer = 14,
    ReplayActorModels = 15,
    ReplayWeaponModels = 16,
    ReplayFactory = 17,
    ReplayBrowser = 18,
    ReplayTimeline = 19,
    ReplayCameraMgr = 20,
    ReplayChatStates = 21,
    ReplayClipsCache = 22,
    CommandBinds = 23,
    BindPrev = 24,
    DuelConfig = 25,
    MpContext = 26,
    GameState = 27,
    ChatHistory = 28,
    ReplayExportRenderMode = 29,
    MainmenuDebug = 30,
    ReplayCinematicMode = 31,
    NetPresentationDebug = 32,
    NetDebugEntities = 33,
    RoomCodeShow = 34,
};

struct GlobalCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uintptr_t (*get_pointer)(void* hostUserData, GlobalId id);
    std::int32_t (*get_bool)(void* hostUserData, GlobalId id);
};

}  // namespace mimita::host
