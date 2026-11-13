#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "entities/player.h"
#include "camera.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "replay/replay.h"
#include "replay/replay-factory.h"
#include "replay/replay-camera.h"
#include "network/multiplayer-context.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "gui/hud/chat-bubble.h"
#include "gui/hud/chat-history.h"
#include "combat/weapon-viewmodel.h"

#define GP_ACCESS(Name, Field, Type) \
    inline Type& g##Name() { return *gp##Name; }

// Declared as extern pointers (set by main.cpp)
extern Player* gpPlayer;
extern Camera* gpCamera;
extern World* gpWorld;
extern NpcSystem* gpNpcSystem;
extern WeaponSystem* gpWeapons;
extern bool* gpFreecamEnabled;
extern glm::vec3* gpDeathPosition;
extern int* gpSelectedEditorObject;
extern bool* gpEditorMode;
extern std::string* gpActiveGameMode;
extern std::string* gpActiveMapPath;
extern bool* gpWorldLoaded;

extern ReplayRingBuffer* gpReplayRecorder;
extern ReplayPlayer* gpReplayPlayer;
extern ReplayFactory* gpReplayFactory;
extern ReplayBrowser* gpReplayBrowser;
extern ReplayTimeline* gpReplayTimeline;
extern ReplayCameraMgr* gpReplayCameraMgr;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern std::unordered_map<int, bool>* gpBindPrev;

extern DuelConfig* gpDuelConfig;
extern MimitaNet::MultiplayerContext* gpMpContext;

extern GameState* gpGameState;
extern ChatHistory* gpChatHistory;

// Convenience accessors (non-null after main() init)
#ifdef MIMITA_GAME_DLL
#define REPLAY_RECORDER (*mimita::game::replayRecorderPtr())
#define REPLAY_PLAYER (*mimita::game::replayPlayerPtr())
#define REPLAY_FACTORY (*mimita::game::replayFactoryPtr())
#define REPLAY_BROWSER (*mimita::game::replayBrowserPtr())
#define REPLAY_TIMELINE (*mimita::game::replayTimelinePtr())
#define REPLAY_CAMERA_MGR (*mimita::game::replayCameraMgrPtr())
#define REPLAY_CHAT_STATES (*mimita::game::replayChatStatesPtr())
#define REPLAY_CLIPS_CACHE (*mimita::game::replayClipsCachePtr())
#define REPLAY_ACTOR_MODELS (*mimita::game::replayActorModelsPtr())
#define REPLAY_WEAPON_MODELS (*mimita::game::replayWeaponModelsPtr())
#define CMD_BINDS (*mimita::game::commandBindsPtr())
#define BIND_PREV (*mimita::game::bindPrevPtr())
#else
#define REPLAY_RECORDER (*gpReplayRecorder)
#define REPLAY_PLAYER (*gpReplayPlayer)
#define REPLAY_FACTORY (*gpReplayFactory)
#define REPLAY_BROWSER (*gpReplayBrowser)
#define REPLAY_TIMELINE (*gpReplayTimeline)
#define REPLAY_CAMERA_MGR (*gpReplayCameraMgr)
#define REPLAY_CHAT_STATES (*gpReplayChatStates)
#define REPLAY_CLIPS_CACHE (*gpReplayClipsCache)
#define REPLAY_ACTOR_MODELS (*gpReplayActorModels)
#define REPLAY_WEAPON_MODELS (*gpReplayWeaponModels)
#define CMD_BINDS (*gpCommandBinds)
#define BIND_PREV (*gpBindPrev)
#endif
#ifdef MIMITA_GAME_DLL
#define THE_PLAYER (*mimita::game::playerPtr())
#define THE_CAMERA (*mimita::game::cameraPtr())
#define THE_WORLD (*mimita::game::worldPtr())
#define THE_NPC_SYSTEM (*mimita::game::npcSystemPtr())
#define THE_WEAPONS (*mimita::game::weaponsPtr())
#else
#define THE_PLAYER (*gpPlayer)
#define THE_CAMERA (*gpCamera)
#define THE_WORLD (*gpWorld)
#define THE_NPC_SYSTEM (*gpNpcSystem)
#define THE_WEAPONS (*gpWeapons)
#endif
#ifdef MIMITA_GAME_DLL
#define FREECAM_ENABLED (mimita::game::freecamRef())
#define DEATH_POSITION (mimita::game::deathPositionRef())
#define SELECTED_EDITOR_OBJ (mimita::game::selectedEditorObjectRef())
#define EDITOR_MODE (mimita::game::editorModeRef())
#else
#define FREECAM_ENABLED (*gpFreecamEnabled)
#define DEATH_POSITION (*gpDeathPosition)
#define SELECTED_EDITOR_OBJ (*gpSelectedEditorObject)
#define EDITOR_MODE (*gpEditorMode)
#endif
#ifdef MIMITA_GAME_DLL
namespace mimita::game {
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
}
#define ACTIVE_GAME_MODE (mimita::game::activeGameModeRef())
#define ACTIVE_MAP_PATH (mimita::game::activeMapPathRef())
#define WORLD_LOADED (mimita::game::worldLoadedRef())
#else
#define ACTIVE_GAME_MODE (*gpActiveGameMode)
#define ACTIVE_MAP_PATH (*gpActiveMapPath)
#define WORLD_LOADED (*gpWorldLoaded)
#endif
#ifdef MIMITA_GAME_DLL
#define DUEL_CONFIG (*mimita::game::duelConfigPtr())
#else
#define DUEL_CONFIG (*gpDuelConfig)
#endif
#ifdef MIMITA_GAME_DLL
#define MP_CONTEXT (*mimita::game::mpContextPtr())
#else
#define MP_CONTEXT (*gpMpContext)
#endif
#ifdef MIMITA_GAME_DLL
#define GAME_STATE (*mimita::game::gameStatePtr())
#else
#define GAME_STATE (*gpGameState)
#endif
#ifdef MIMITA_GAME_DLL
#define gChatHistory (*mimita::game::chatHistoryPtr())
#else
#define gChatHistory (*gpChatHistory)
#endif
