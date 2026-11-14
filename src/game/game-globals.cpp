// 2026-11-13
// Game-generation aliases for terminal/legacy access.
//
// The resident state store remains the owner. These pointers are populated by
// gameInitSubsystems and are only a thin compatibility surface for code that
// still names the historical globals.

#include "camera.h"
#include "combat/weapon-system.h"
#include "combat/weapon-viewmodel.h"
#include "entities/player.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "gui/hud/chat-bubble.h"
#include "gui/hud/chat-history.h"
#include "network/multiplayer-context.h"
#include "npc/npc.h"
#include "replay/replay-camera.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
#include "world/world.h"

#ifdef MIMITA_GAME_DLL
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
std::unordered_map<int, std::string>* gpCommandBinds = nullptr;
std::unordered_map<int, bool>* gpBindPrev = nullptr;

DuelConfig* gpDuelConfig = nullptr;
MimitaNet::MultiplayerContext* gpMpContext = nullptr;
GameState* gpGameState = nullptr;
ChatHistory* gpChatHistory = nullptr;

bool gAiLaunchMode = false;
bool gUnlockCursorOnLaunch = false;
bool gReplayExportRenderMode = false;
bool gReplayCinematicMode = false;
bool gNetPresentationDebug = false;
bool gNetDebugEntities = false;
bool gMainmenuDebug = false;
bool gRoomCodeShow = true;
#endif
