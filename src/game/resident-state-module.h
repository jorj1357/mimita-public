// 2026-11-13
// Resident-owned session state accessors for the game DLL.

#pragma once

#include <string>

#include "host/abi/game-host-abi.h"

struct Player;
struct World;
struct Camera;
struct DuelConfig;
class ChatHistory;
#include "game/game-state.h"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include "combat/weapon-viewmodel.h"
#include "gui/hud/chat-bubble.h"
#include "replay/replay-camera.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
class NpcSystem;
class WeaponSystem;
namespace MimitaNet { class MultiplayerContext; }

namespace mimita::game {
void initializeResidentState(const mimita::host::HostServicesV1* services);
bool& worldLoadedRef();
std::string& activeMapPathRef();
std::string& activeGameModeRef();
Player* playerPtr();
World* worldPtr();
NpcSystem* npcSystemPtr();
WeaponSystem* weaponsPtr();
MimitaNet::MultiplayerContext* mpContextPtr();
Camera* cameraPtr();
GameState* gameStatePtr();
ChatHistory* chatHistoryPtr();
DuelConfig* duelConfigPtr();
bool& freecamRef();
bool& editorModeRef();
int& selectedEditorObjectRef();
glm::vec3& deathPositionRef();
ReplayRingBuffer* replayRecorderPtr();
ReplayPlayer* replayPlayerPtr();
ReplayFactory* replayFactoryPtr();
ReplayBrowser* replayBrowserPtr();
ReplayTimeline* replayTimelinePtr();
ReplayCameraMgr* replayCameraMgrPtr();
std::unordered_map<std::string, std::unique_ptr<Player>>* replayActorModelsPtr();
std::unordered_map<std::string, WeaponViewModel>* replayWeaponModelsPtr();
std::unordered_map<std::string, ActorChatState>* replayChatStatesPtr();
std::vector<std::string>* replayClipsCachePtr();
std::unordered_map<int, std::string>* commandBindsPtr();
std::unordered_map<int, bool>* bindPrevPtr();
bool& replayExportRenderModeRef();
bool& mainmenuDebugRef();
bool& replayCinematicModeRef();
bool& netPresentationDebugRef();
bool& netDebugEntitiesRef();
bool& roomCodeShowRef();
}  // namespace mimita::game
