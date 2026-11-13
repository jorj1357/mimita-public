// 2026-11-13
// Game-owned singleton objects that are not resident host state.

#include "game/duel.h"
#include "game/gamemode-manager.h"
#include "video/frame-pacer.h"

DuelManager gDuelManager;
GamemodeManager gGamemodeManager;
FramePacer gFramePacer;
