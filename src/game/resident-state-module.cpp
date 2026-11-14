// 2026-11-13
// Resolve resident session state on every access instead of caching a pointer
// copied once into the game generation.

#include "game/resident-state-module.h"

#include "host/abi/global-capability.h"
#include "game/state-bridge-module.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "network/multiplayer-context.h"
#include "camera.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "gui/hud/chat-history.h"
#include <glm/glm.hpp>
#include <new>
#include <utility>
#include "replay/replay-camera.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
#include "combat/weapon-viewmodel.h"
#include "gui/hud/chat-bubble.h"

namespace mimita::game {
namespace {
const mimita::host::GlobalCapabilityV1* gGlobals = nullptr;
void* gHostData = nullptr;
bool fallbackWorldLoaded = false;
std::string fallbackMapPath;
std::string fallbackGameMode;

template <typename T>
T* pointer(mimita::host::GlobalId id) {
    if (!gGlobals || !gGlobals->get_pointer) return nullptr;
    return reinterpret_cast<T*>(gGlobals->get_pointer(gHostData, id));
}

template <typename T, typename... Args>
T* resident(mimita::host::StateObjectId id, Args&&... args) {
    bool created = false;
    void* object = mimita::game::ensureResidentObject(
        id, sizeof(T), alignof(T), &created);
    if (!object) return nullptr;
    auto* value = static_cast<T*>(object);
    if (created) new (value) T(std::forward<Args>(args)...);
    return value;
}

bool& worldSlot() {
    if (auto* menu = mimita::game::menuState()) return menu->worldLoaded;
    if (gGlobals && gGlobals->get_pointer) {
        if (auto* value = reinterpret_cast<bool*>(
                gGlobals->get_pointer(gHostData, mimita::host::GlobalId::WorldLoaded))) {
            return *value;
        }
    }
    return fallbackWorldLoaded;
}

std::string& mapSlot() {
    if (auto* value = resident<std::string>(
            mimita::host::StateObjectId::ActiveMapPath)) {
        return *value;
    }
    if (gGlobals && gGlobals->get_pointer) {
        if (auto* value = reinterpret_cast<std::string*>(
                gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ActiveMapPath))) {
            return *value;
        }
    }
    return fallbackMapPath;
}

std::string& modeSlot() {
    if (auto* value = resident<std::string>(
            mimita::host::StateObjectId::ActiveGameMode, "sandbox")) {
        return *value;
    }
    if (gGlobals && gGlobals->get_pointer) {
        if (auto* value = reinterpret_cast<std::string*>(
                gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ActiveGameMode))) {
            return *value;
        }
    }
    return fallbackGameMode;
}
}  // namespace

void initializeResidentState(const mimita::host::HostServicesV1* services) {
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

Player* playerPtr() {
    if (auto* value = resident<Player>(mimita::host::StateObjectId::Player))
        return value;
    return pointer<Player>(mimita::host::GlobalId::Player);
}
World* worldPtr() {
    if (auto* value = resident<World>(mimita::host::StateObjectId::World))
        return value;
    return pointer<World>(mimita::host::GlobalId::World);
}
NpcSystem* npcSystemPtr() {
    if (auto* value = resident<NpcSystem>(mimita::host::StateObjectId::NpcSystem))
        return value;
    return pointer<NpcSystem>(mimita::host::GlobalId::NpcSystem);
}
WeaponSystem* weaponsPtr() {
    if (auto* value = resident<WeaponSystem>(mimita::host::StateObjectId::Weapons))
        return value;
    return pointer<WeaponSystem>(mimita::host::GlobalId::Weapons);
}
MimitaNet::MultiplayerContext* mpContextPtr() {
    if (auto* value = resident<MimitaNet::MultiplayerContext>(
            mimita::host::StateObjectId::MpContext))
        return value;
    return pointer<MimitaNet::MultiplayerContext>(mimita::host::GlobalId::MpContext);
}
Camera* cameraPtr() {
    if (auto* value = resident<Camera>(mimita::host::StateObjectId::Camera))
        return value;
    return pointer<Camera>(mimita::host::GlobalId::Camera);
}
GameState* gameStatePtr() {
    if (auto* menu = mimita::game::menuState())
        return reinterpret_cast<GameState*>(&menu->gameState);
    return pointer<GameState>(mimita::host::GlobalId::GameState);
}
ChatHistory* chatHistoryPtr() {
    if (auto* value = resident<ChatHistory>(mimita::host::StateObjectId::ChatHistory))
        return value;
    return pointer<ChatHistory>(mimita::host::GlobalId::ChatHistory);
}
DuelConfig* duelConfigPtr() {
    if (auto* value = resident<DuelConfig>(mimita::host::StateObjectId::DuelConfig))
        return value;
    return pointer<DuelConfig>(mimita::host::GlobalId::DuelConfig);
}
bool& freecamRef() {
    if (auto* menu = mimita::game::menuState()) return menu->freecamEnabled;
    static bool fallback = false; bool* p = pointer<bool>(mimita::host::GlobalId::FreecamEnabled); return p ? *p : fallback;
}
bool& editorModeRef() {
    if (auto* menu = mimita::game::menuState()) return menu->editorMode;
    static bool fallback = false; bool* p = pointer<bool>(mimita::host::GlobalId::EditorMode); return p ? *p : fallback;
}
int& selectedEditorObjectRef() {
    if (auto* menu = mimita::game::menuState()) return menu->selectedEditorObject;
    static int fallback = -1; int* p = pointer<int>(mimita::host::GlobalId::SelectedEditorObject); return p ? *p : fallback;
}
glm::vec3& deathPositionRef() { static glm::vec3 fallback{}; glm::vec3* p = pointer<glm::vec3>(mimita::host::GlobalId::DeathPosition); return p ? *p : fallback; }
ReplayRingBuffer* replayRecorderPtr() {
    if (auto* value = resident<ReplayRingBuffer>(
            mimita::host::StateObjectId::ReplayRecorder))
        return value;
    return pointer<ReplayRingBuffer>(mimita::host::GlobalId::ReplayRecorder);
}
ReplayPlayer* replayPlayerPtr() {
    if (auto* value = resident<ReplayPlayer>(mimita::host::StateObjectId::ReplayPlayer))
        return value;
    return pointer<ReplayPlayer>(mimita::host::GlobalId::ReplayPlayer);
}
ReplayFactory* replayFactoryPtr() {
    if (auto* ring = replayRecorderPtr()) {
        if (auto* value = resident<ReplayFactory>(
                mimita::host::StateObjectId::ReplayFactory, *ring))
            return value;
    }
    return pointer<ReplayFactory>(mimita::host::GlobalId::ReplayFactory);
}
ReplayBrowser* replayBrowserPtr() {
    if (auto* value = resident<ReplayBrowser>(
            mimita::host::StateObjectId::ReplayBrowser))
        return value;
    return pointer<ReplayBrowser>(mimita::host::GlobalId::ReplayBrowser);
}
ReplayTimeline* replayTimelinePtr() {
    if (auto* value = resident<ReplayTimeline>(
            mimita::host::StateObjectId::ReplayTimeline))
        return value;
    return pointer<ReplayTimeline>(mimita::host::GlobalId::ReplayTimeline);
}
ReplayCameraMgr* replayCameraMgrPtr() {
    if (auto* value = resident<ReplayCameraMgr>(
            mimita::host::StateObjectId::ReplayCameraMgr))
        return value;
    return pointer<ReplayCameraMgr>(mimita::host::GlobalId::ReplayCameraMgr);
}
std::unordered_map<std::string, std::unique_ptr<Player>>* replayActorModelsPtr() {
    if (auto* value = resident<std::unordered_map<std::string, std::unique_ptr<Player>>>(
            mimita::host::StateObjectId::ReplayActorModels))
        return value;
    return pointer<std::unordered_map<std::string, std::unique_ptr<Player>>>(
        mimita::host::GlobalId::ReplayActorModels);
}
std::unordered_map<std::string, WeaponViewModel>* replayWeaponModelsPtr() {
    if (auto* value = resident<std::unordered_map<std::string, WeaponViewModel>>(
            mimita::host::StateObjectId::ReplayWeaponModels))
        return value;
    return pointer<std::unordered_map<std::string, WeaponViewModel>>(
        mimita::host::GlobalId::ReplayWeaponModels);
}
std::unordered_map<std::string, ActorChatState>* replayChatStatesPtr() {
    if (auto* value = resident<std::unordered_map<std::string, ActorChatState>>(
            mimita::host::StateObjectId::ReplayChatStates))
        return value;
    return pointer<std::unordered_map<std::string, ActorChatState>>(
        mimita::host::GlobalId::ReplayChatStates);
}
std::vector<std::string>* replayClipsCachePtr() {
    if (auto* value = resident<std::vector<std::string>>(
            mimita::host::StateObjectId::ReplayClipsCache))
        return value;
    return pointer<std::vector<std::string>>(
        mimita::host::GlobalId::ReplayClipsCache);
}
std::unordered_map<int, std::string>* commandBindsPtr() {
    if (auto* value = resident<std::unordered_map<int, std::string>>(
            mimita::host::StateObjectId::CommandBinds))
        return value;
    return pointer<std::unordered_map<int, std::string>>(
        mimita::host::GlobalId::CommandBinds);
}
std::unordered_map<int, bool>* bindPrevPtr() {
    if (auto* value = resident<std::unordered_map<int, bool>>(
            mimita::host::StateObjectId::BindPrev))
        return value;
    return pointer<std::unordered_map<int, bool>>(
        mimita::host::GlobalId::BindPrev);
}
bool& replayExportRenderModeRef() { static bool fallback=false; bool* p=pointer<bool>(mimita::host::GlobalId::ReplayExportRenderMode); return p?*p:fallback; }
bool& mainmenuDebugRef() { static bool fallback=false; bool* p=pointer<bool>(mimita::host::GlobalId::MainmenuDebug); return p?*p:fallback; }
bool& replayCinematicModeRef() { static bool fallback=false; bool* p=pointer<bool>(mimita::host::GlobalId::ReplayCinematicMode); return p?*p:fallback; }
bool& netPresentationDebugRef() { static bool fallback=false; bool* p=pointer<bool>(mimita::host::GlobalId::NetPresentationDebug); return p?*p:fallback; }
bool& netDebugEntitiesRef() { static bool fallback=false; bool* p=pointer<bool>(mimita::host::GlobalId::NetDebugEntities); return p?*p:fallback; }
bool& roomCodeShowRef() { static bool fallback=true; bool* p=pointer<bool>(mimita::host::GlobalId::RoomCodeShow); return p?*p:fallback; }

bool& worldLoadedRef() { return worldSlot(); }
std::string& activeMapPathRef() { return mapSlot(); }
std::string& activeGameModeRef() { return modeSlot(); }
}  // namespace mimita::game
