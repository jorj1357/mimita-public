// 2026-11-13
// Game-DLL-side resident-global bridge.
//
// The bootstrap owns the real process globals. This module keeps the game-side
// symbols resolvable and refreshes pointer/scalar values from the host after
// bootstrap initialization (first fixed tick). It does not create a second
// process-global store for resident-owned state.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera.h"
#include "combat/weapon-system.h"
#include "combat/weapon-viewmodel.h"
#include "entities/player.h"
#include "game/duel.h"
#include "game/gamemode-manager.h"
#include "game/game-state.h"
#include "gui/hud/chat-bubble.h"
#include "gui/hud/chat-history.h"
#include "host/abi/global-capability.h"
#include "network/multiplayer-context.h"
#include "npc/npc.h"
#include "replay/replay-camera.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
#include "video/frame-pacer.h"
#include "world/world.h"

DuelManager gDuelManager;
GamemodeManager gGamemodeManager;
FramePacer gFramePacer;

Player* gpPlayer = nullptr;
Camera* gpCamera = nullptr;
World* gpWorld = nullptr;
NpcSystem* gpNpcSystem = nullptr;
WeaponSystem* gpWeapons = nullptr;
bool* gpFreecamEnabled = nullptr;
glm::vec3* gpDeathPosition = nullptr;
int* gpSelectedEditorObject = nullptr;
bool* gpEditorMode = nullptr;
std::string* gpActiveGameMode = nullptr;
std::string* gpActiveMapPath = nullptr;
bool* gpWorldLoaded = nullptr;

ReplayRingBuffer* gpReplayRecorder = nullptr;
ReplayPlayer* gpReplayPlayer = nullptr;
std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels = nullptr;
std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels = nullptr;
ReplayFactory* gpReplayFactory = nullptr;
ReplayBrowser* gpReplayBrowser = nullptr;
ReplayTimeline* gpReplayTimeline = nullptr;
ReplayCameraMgr* gpReplayCameraMgr = nullptr;
std::unordered_map<std::string, ActorChatState>* gpReplayChatStates = nullptr;
std::vector<std::string>* gpReplayClipsCache = nullptr;

bool gReplayExportRenderMode = false;
std::unordered_map<int, std::string>* gpCommandBinds = nullptr;
std::unordered_map<int, bool>* gpBindPrev = nullptr;

DuelConfig* gpDuelConfig = nullptr;
MimitaNet::MultiplayerContext* gpMpContext = nullptr;
GameState* gpGameState = nullptr;
ChatHistory* gpChatHistory = nullptr;

bool gMainmenuDebug = false;
bool gReplayCinematicMode = false;
bool gNetPresentationDebug = false;
bool gNetDebugEntities = false;
bool gRoomCodeShow = true;

namespace mimita::game {
namespace {
const mimita::host::GlobalCapabilityV1* gGlobals = nullptr;
void* gHostData = nullptr;

template <typename T>
T* pointer(mimita::host::GlobalId id) {
    return reinterpret_cast<T*>(gGlobals->get_pointer(gHostData, id));
}
}  // namespace

void initializeHostGlobals(const mimita::host::HostServicesV1* services) {
    gGlobals = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::GlobalCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Globals),
            mimita::host::kGlobalCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kGlobalCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::GlobalCapabilityV1)) {
        gGlobals = capability;
    }
}

void syncHostGlobals() {
    if (!gGlobals || !gGlobals->get_pointer) return;
    gpPlayer = pointer<Player>(mimita::host::GlobalId::Player);
    gpCamera = pointer<Camera>(mimita::host::GlobalId::Camera);
    gpWorld = pointer<World>(mimita::host::GlobalId::World);
    gpNpcSystem = pointer<NpcSystem>(mimita::host::GlobalId::NpcSystem);
    gpWeapons = pointer<WeaponSystem>(mimita::host::GlobalId::Weapons);
    gpFreecamEnabled = pointer<bool>(mimita::host::GlobalId::FreecamEnabled);
    gpDeathPosition = pointer<glm::vec3>(mimita::host::GlobalId::DeathPosition);
    gpSelectedEditorObject = pointer<int>(mimita::host::GlobalId::SelectedEditorObject);
    gpEditorMode = pointer<bool>(mimita::host::GlobalId::EditorMode);
    gpActiveGameMode = pointer<std::string>(mimita::host::GlobalId::ActiveGameMode);
    gpActiveMapPath = pointer<std::string>(mimita::host::GlobalId::ActiveMapPath);
    gpWorldLoaded = pointer<bool>(mimita::host::GlobalId::WorldLoaded);
    gpReplayRecorder = pointer<ReplayRingBuffer>(mimita::host::GlobalId::ReplayRecorder);
    gpReplayPlayer = pointer<ReplayPlayer>(mimita::host::GlobalId::ReplayPlayer);
    gpReplayActorModels = reinterpret_cast<
        std::unordered_map<std::string, std::unique_ptr<Player>>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ReplayActorModels));
    gpReplayWeaponModels = reinterpret_cast<
        std::unordered_map<std::string, WeaponViewModel>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ReplayWeaponModels));
    gpReplayFactory = pointer<ReplayFactory>(mimita::host::GlobalId::ReplayFactory);
    gpReplayBrowser = pointer<ReplayBrowser>(mimita::host::GlobalId::ReplayBrowser);
    gpReplayTimeline = pointer<ReplayTimeline>(mimita::host::GlobalId::ReplayTimeline);
    gpReplayCameraMgr = pointer<ReplayCameraMgr>(mimita::host::GlobalId::ReplayCameraMgr);
    gpReplayChatStates = reinterpret_cast<
        std::unordered_map<std::string, ActorChatState>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ReplayChatStates));
    gpReplayClipsCache = reinterpret_cast<std::vector<std::string>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ReplayClipsCache));
    gpCommandBinds = reinterpret_cast<std::unordered_map<int, std::string>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::CommandBinds));
    gpBindPrev = reinterpret_cast<std::unordered_map<int, bool>*>(
        gGlobals->get_pointer(gHostData, mimita::host::GlobalId::BindPrev));
    gpDuelConfig = pointer<DuelConfig>(mimita::host::GlobalId::DuelConfig);
    gpMpContext = pointer<MimitaNet::MultiplayerContext>(mimita::host::GlobalId::MpContext);
    gpGameState = pointer<GameState>(mimita::host::GlobalId::GameState);
    gpChatHistory = pointer<ChatHistory>(mimita::host::GlobalId::ChatHistory);
    if (gGlobals->get_bool) {
        gReplayExportRenderMode =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::ReplayExportRenderMode) != 0;
        gMainmenuDebug =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::MainmenuDebug) != 0;
        gReplayCinematicMode =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::ReplayCinematicMode) != 0;
        gNetPresentationDebug =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::NetPresentationDebug) != 0;
        gNetDebugEntities =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::NetDebugEntities) != 0;
        gRoomCodeShow =
            gGlobals->get_bool(gHostData, mimita::host::GlobalId::RoomCodeShow) != 0;
    }
}

}  // namespace mimita::game
