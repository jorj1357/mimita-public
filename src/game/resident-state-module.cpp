// 2026-11-13
// Resolve resident session state on every access instead of caching a pointer
// copied once into the game generation.

#include "game/resident-state-module.h"

#include "host/abi/global-capability.h"
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

bool& worldSlot() {
    if (gGlobals && gGlobals->get_pointer) {
        if (auto* value = reinterpret_cast<bool*>(
                gGlobals->get_pointer(gHostData, mimita::host::GlobalId::WorldLoaded))) {
            return *value;
        }
    }
    return fallbackWorldLoaded;
}

std::string& mapSlot() {
    if (gGlobals && gGlobals->get_pointer) {
        if (auto* value = reinterpret_cast<std::string*>(
                gGlobals->get_pointer(gHostData, mimita::host::GlobalId::ActiveMapPath))) {
            return *value;
        }
    }
    return fallbackMapPath;
}

std::string& modeSlot() {
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

Player* playerPtr() { return pointer<Player>(mimita::host::GlobalId::Player); }
World* worldPtr() { return pointer<World>(mimita::host::GlobalId::World); }
NpcSystem* npcSystemPtr() { return pointer<NpcSystem>(mimita::host::GlobalId::NpcSystem); }
WeaponSystem* weaponsPtr() { return pointer<WeaponSystem>(mimita::host::GlobalId::Weapons); }
MimitaNet::MultiplayerContext* mpContextPtr() { return pointer<MimitaNet::MultiplayerContext>(mimita::host::GlobalId::MpContext); }
Camera* cameraPtr() { return pointer<Camera>(mimita::host::GlobalId::Camera); }
GameState* gameStatePtr() { return pointer<GameState>(mimita::host::GlobalId::GameState); }
ChatHistory* chatHistoryPtr() { return pointer<ChatHistory>(mimita::host::GlobalId::ChatHistory); }
DuelConfig* duelConfigPtr() { return pointer<DuelConfig>(mimita::host::GlobalId::DuelConfig); }
bool& freecamRef() { static bool fallback = false; bool* p = pointer<bool>(mimita::host::GlobalId::FreecamEnabled); return p ? *p : fallback; }
bool& editorModeRef() { static bool fallback = false; bool* p = pointer<bool>(mimita::host::GlobalId::EditorMode); return p ? *p : fallback; }
int& selectedEditorObjectRef() { static int fallback = -1; int* p = pointer<int>(mimita::host::GlobalId::SelectedEditorObject); return p ? *p : fallback; }
glm::vec3& deathPositionRef() { static glm::vec3 fallback{}; glm::vec3* p = pointer<glm::vec3>(mimita::host::GlobalId::DeathPosition); return p ? *p : fallback; }
ReplayRingBuffer* replayRecorderPtr() { return pointer<ReplayRingBuffer>(mimita::host::GlobalId::ReplayRecorder); }
ReplayPlayer* replayPlayerPtr() { return pointer<ReplayPlayer>(mimita::host::GlobalId::ReplayPlayer); }
ReplayFactory* replayFactoryPtr() { return pointer<ReplayFactory>(mimita::host::GlobalId::ReplayFactory); }
ReplayBrowser* replayBrowserPtr() { return pointer<ReplayBrowser>(mimita::host::GlobalId::ReplayBrowser); }
ReplayTimeline* replayTimelinePtr() { return pointer<ReplayTimeline>(mimita::host::GlobalId::ReplayTimeline); }
ReplayCameraMgr* replayCameraMgrPtr() { return pointer<ReplayCameraMgr>(mimita::host::GlobalId::ReplayCameraMgr); }
std::unordered_map<std::string, std::unique_ptr<Player>>* replayActorModelsPtr() { return pointer<std::unordered_map<std::string, std::unique_ptr<Player>>>(mimita::host::GlobalId::ReplayActorModels); }
std::unordered_map<std::string, WeaponViewModel>* replayWeaponModelsPtr() { return pointer<std::unordered_map<std::string, WeaponViewModel>>(mimita::host::GlobalId::ReplayWeaponModels); }
std::unordered_map<std::string, ActorChatState>* replayChatStatesPtr() { return pointer<std::unordered_map<std::string, ActorChatState>>(mimita::host::GlobalId::ReplayChatStates); }
std::vector<std::string>* replayClipsCachePtr() { return pointer<std::vector<std::string>>(mimita::host::GlobalId::ReplayClipsCache); }
std::unordered_map<int, std::string>* commandBindsPtr() { return pointer<std::unordered_map<int, std::string>>(mimita::host::GlobalId::CommandBinds); }
std::unordered_map<int, bool>* bindPrevPtr() { return pointer<std::unordered_map<int, bool>>(mimita::host::GlobalId::BindPrev); }
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
