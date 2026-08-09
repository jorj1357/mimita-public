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
#define THE_PLAYER (*gpPlayer)
#define THE_CAMERA (*gpCamera)
#define THE_WORLD (*gpWorld)
#define THE_NPC_SYSTEM (*gpNpcSystem)
#define THE_WEAPONS (*gpWeapons)
#define FREECAM_ENABLED (*gpFreecamEnabled)
#define DEATH_POSITION (*gpDeathPosition)
#define SELECTED_EDITOR_OBJ (*gpSelectedEditorObject)
#define EDITOR_MODE (*gpEditorMode)
#define ACTIVE_GAME_MODE (*gpActiveGameMode)
#define ACTIVE_MAP_PATH (*gpActiveMapPath)
#define WORLD_LOADED (*gpWorldLoaded)
#define DUEL_CONFIG (*gpDuelConfig)
#define MP_CONTEXT (*gpMpContext)
#define GAME_STATE (*gpGameState)
#define gChatHistory (*gpChatHistory)
