// 2026-11-13
// Game-owned forceMainMenu command. The bootstrap keeps its CLI copy for the
// current EXE; the game DLL uses this generation-local definition.

#include <chrono>
#include <cstdio>
#include <string>

#include "combat/weapon-runtime.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "game/render-device-bridge-module.h"
#include "gui/menus/online-menu.h"
#include "network/multiplayer-context.h"
#include "npc/npc.h"
#include "physics/runtime-objects.h"
#include "replay/replay-export.h"
#include "replay/replay.h"
#include "terminal/terminal-state.h"

extern DuelManager gDuelManager;
extern bool gMainmenuDebug;

void forceMainMenu()
{
    auto startTime = std::chrono::steady_clock::now();
    Debug::log(Debug::Category::General, "[MAINMENU] requested");
    printf("[MAINMENU] entering\n");

    auto logPhase = [&](const char* phase) {
        if (!gMainmenuDebug) return;
        auto now = std::chrono::steady_clock::now();
        double ms = (double)std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000.0;
        Debug::log(Debug::Category::General, "[MAINMENU] %s: %.2fms", phase, ms);
    };

    Debug::log(Debug::Category::General, "[MAINMENU] currentState=%s",
        gDuelManager.phase() != DuelPhase::Off ? "DUEL" :
        REPLAY_PLAYER.isPlaying() ? "REPLAY" :
        MP_CONTEXT.active ? "MULTIPLAYER" : "GAMEPLAY");
    Debug::log(Debug::Category::General, "[MAINMENU] transitioning");

    if (REPLAY_PLAYER.isPlaying()) {
        printf("[MAINMENU] cleaning replay\n");
        REPLAY_PLAYER.stopPlayback();
        logPhase("Replay Cleanup");
    }

    printf("[MAINMENU] clearing replay models\n");
    if (gpReplayActorModels) gpReplayActorModels->clear();
    if (gpReplayWeaponModels) gpReplayWeaponModels->clear();
    if (gpReplayChatStates) gpReplayChatStates->clear();
    logPhase("Replay Models Clear");

    if (REPLAY_RECORDER.isRecording()) {
        REPLAY_RECORDER.stopRecording();
        logPhase("Replay Recording Stop");
    }

    if (gDuelManager.phase() != DuelPhase::Off) {
        printf("[MAINMENU] cleaning duel\n");
        gDuelManager.stopDuel();
        logPhase("Duel Cleanup");
    }

    THE_NPC_SYSTEM.destroyAll();
    logPhase("NPC Cleanup");
    clearRuntimeObjects("main-menu");

    if (MP_CONTEXT.active) {
        printf("[MAINMENU] cleaning network\n");
        Debug::log(Debug::Category::General, "[MAINMENU] disconnecting multiplayer");
        MimitaNet::mpShutdown(MP_CONTEXT);
        onlineMenuSetServerCode("");
        onlineMenuSetServerRunning(false);
        logPhase("Network Cleanup");
        Debug::log(Debug::Category::General, "[MAINMENU] client disconnected");
    }

    FREECAM_ENABLED = false;
    resetAllWeaponRuntimesForSpawn(THE_PLAYER, "forceMainMenu");
    THE_PLAYER.dead = false;
    THE_PLAYER.currentHp = THE_PLAYER.maxHp;
    THE_PLAYER.vel = glm::vec3(0.0f);
    THE_PLAYER.externalImpulse = glm::vec3(0.0f);
    THE_PLAYER.proceduralFrozen = false;
    THE_PLAYER.respawnTimer = 0.0f;
    THE_PLAYER.killedBy.clear();

    cancelReplayExport();
    mimita::game::setHostCursorVisible(true);
    printf("[MAINMENU] switching scene\n");
    GAME_STATE = GAME_MENU;
    logPhase("GUI Load");
    Debug::log(Debug::Category::General, "[MAINMENU] success");
}
