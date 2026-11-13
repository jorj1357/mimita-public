// 2026-11-13
// Transitional EXE-side core-state accessors for pre-cutover game code.

#include "game/resident-state-module.h"

#include "camera.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "gui/hud/chat-history.h"
#include <glm/glm.hpp>
#include "replay/replay.h"
#include "replay/replay-factory.h"
#include "replay/replay-camera.h"
#include "combat/weapon-viewmodel.h"
#include "gui/hud/chat-bubble.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include "combat/weapon-system.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "npc/npc.h"
#include "world/world.h"

extern Player* gpPlayer;
extern World* gpWorld;
extern NpcSystem* gpNpcSystem;
extern WeaponSystem* gpWeapons;
extern MimitaNet::MultiplayerContext* gpMpContext;
extern Camera* gpCamera;
extern GameState* gpGameState;
extern ChatHistory* gpChatHistory;
extern DuelConfig* gpDuelConfig;
extern bool* gpFreecamEnabled;
extern bool* gpEditorMode;
extern int* gpSelectedEditorObject;
extern glm::vec3* gpDeathPosition;
extern ReplayRingBuffer* gpReplayRecorder;
extern ReplayPlayer* gpReplayPlayer;
extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;
extern ReplayFactory* gpReplayFactory;
extern ReplayBrowser* gpReplayBrowser;
extern ReplayTimeline* gpReplayTimeline;
extern ReplayCameraMgr* gpReplayCameraMgr;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern std::unordered_map<int, bool>* gpBindPrev;
extern bool gReplayExportRenderMode;
extern bool gMainmenuDebug;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gRoomCodeShow;

namespace mimita::game {
Player* playerPtr() { return ::gpPlayer; }
World* worldPtr() { return ::gpWorld; }
NpcSystem* npcSystemPtr() { return ::gpNpcSystem; }
WeaponSystem* weaponsPtr() { return ::gpWeapons; }
MimitaNet::MultiplayerContext* mpContextPtr() { return ::gpMpContext; }
Camera* cameraPtr() { return ::gpCamera; }
GameState* gameStatePtr() { return ::gpGameState; }
ChatHistory* chatHistoryPtr() { return ::gpChatHistory; }
DuelConfig* duelConfigPtr() { return ::gpDuelConfig; }
bool& freecamRef() { return *::gpFreecamEnabled; }
bool& editorModeRef() { return *::gpEditorMode; }
int& selectedEditorObjectRef() { return *::gpSelectedEditorObject; }
glm::vec3& deathPositionRef() { return *::gpDeathPosition; }
ReplayRingBuffer* replayRecorderPtr() { return ::gpReplayRecorder; }
ReplayPlayer* replayPlayerPtr() { return ::gpReplayPlayer; }
ReplayFactory* replayFactoryPtr() { return ::gpReplayFactory; }
ReplayBrowser* replayBrowserPtr() { return ::gpReplayBrowser; }
ReplayTimeline* replayTimelinePtr() { return ::gpReplayTimeline; }
ReplayCameraMgr* replayCameraMgrPtr() { return ::gpReplayCameraMgr; }
std::unordered_map<std::string, std::unique_ptr<Player>>* replayActorModelsPtr() { return ::gpReplayActorModels; }
std::unordered_map<std::string, WeaponViewModel>* replayWeaponModelsPtr() { return ::gpReplayWeaponModels; }
std::unordered_map<std::string, ActorChatState>* replayChatStatesPtr() { return ::gpReplayChatStates; }
std::vector<std::string>* replayClipsCachePtr() { return ::gpReplayClipsCache; }
std::unordered_map<int, std::string>* commandBindsPtr() { return ::gpCommandBinds; }
std::unordered_map<int, bool>* bindPrevPtr() { return ::gpBindPrev; }
bool& replayExportRenderModeRef() { return ::gReplayExportRenderMode; }
bool& mainmenuDebugRef() { return ::gMainmenuDebug; }
bool& replayCinematicModeRef() { return ::gReplayCinematicMode; }
bool& netPresentationDebugRef() { return ::gNetPresentationDebug; }
bool& netDebugEntitiesRef() { return ::gNetDebugEntities; }
bool& roomCodeShowRef() { return ::gRoomCodeShow; }
}  // namespace mimita::game
