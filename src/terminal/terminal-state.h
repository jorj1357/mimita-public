#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>

#include "entities/player.h"
#include "camera.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "replay/replay.h"
#include "replay/replay-factory.h"
#include "replay/replay-browser.h"
#include "replay/replay-timeline.h"
#include "network/multiplayer-context.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "gui/hud/chat-bubble.h"
#include "combat/weapon-viewmodel.h"

extern Player gPlayer;
extern Camera gCamera;
extern World gWorld;
extern NpcSystem gNpcSystem;
extern WeaponSystem gWeapons;
extern bool gFreecamEnabled;
extern glm::vec3 gDeathPosition;
extern int gSelectedEditorObject;
extern bool gEditorMode;
extern std::string gActiveGameMode;
extern std::string gActiveMapPath;
extern bool gWorldLoaded;

extern ReplayRingBuffer gReplayRecorder;
extern ReplayPlayer gReplayPlayer;
extern ReplayClipSaver gReplayClipSaver;
extern ReplayFactory gReplayFactory;
extern ReplayBrowser gReplayBrowser;
extern ReplayTimeline gReplayTimeline;
extern std::unordered_map<std::string, ActorChatState> gReplayChatStates;
extern std::vector<std::string> G_REPLAY_CLIPS_CACHE;
extern std::unordered_map<int, std::string> G_COMMAND_BINDS;
extern std::unordered_map<int, bool> G_BIND_PREV;
extern std::mt19937 gRng;

extern DuelConfig gDuelConfig;
extern DuelManager gDuelManager;
extern MimitaNet::MultiplayerContext gMpContext;

extern GameState gGameState;
extern GameState gPrevState;

