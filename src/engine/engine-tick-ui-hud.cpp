#include "engine/engine-tick-ui.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/hud/chat-bubble.h"
#include "gui/hud/match-leaderboard.h"
#include "gui/hud/match-timer.h"
#include "gui/gui-editor.h"
#include "ui/hitmarker.h"
#include "crosshair/crosshair-render.h"
#include "crosshair/crosshair-config.h"
#include "devtools/dev-config.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-overlay.h"
#include "perf/perf.h"
#include "video/frame-pacer.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "render/post-fx.h"
#include "render/lighting-config.h"
#include "audio/music-manager.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/game-state.h"
#include "network/multiplayer-context.h"

#include "debug/debug-log.h"
#include "config/player-settings.h"
#include "npc/npc-combat.h"
#include "network/server.h"
#include "killfeed/killfeed.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;

void engineTickUIHUD(Engine& engine, float dt)
{
    engineTickUIReplayHUD(engine, dt);
    engineTickUIGameHUD(engine, dt);

    KillfeedManager& kf = KillfeedManager::instance();

    if (gpReplayPlayer && gpReplayPlayer->isPlaying()) {
        static thread_local std::vector<ReplayKillfeedEvent> killEvents;
        gpReplayPlayer->takeTriggeredKillfeedEvents(killEvents);
        for (const ReplayKillfeedEvent& ev : killEvents) {
            kf.onKill(ev.killerName.empty() ? ev.killerId : ev.killerName,
                      ev.victimName.empty() ? ev.victimId : ev.victimName,
                      ev.weaponName.empty() ? "unknown" : ev.weaponName,
                      true);
        }
    }

    kf.update(dt);
    kf.render();

    // Online match HUD: leaderboard + timer
    MatchLeaderboard::instance().update(dt);
    MatchLeaderboard::instance().render();
    MatchTimer::instance().update(dt);
    if (MatchTimer::instance().isActive()) {
        std::string timer = MatchTimer::instance().formatElapsed();
        float timerW = uiMeasureText(timer.c_str(), 0.40f);
        uiDrawText(timer.c_str(), uiScreenW() * 0.5f - timerW * 0.5f, 18.0f, 0.40f,
                  {1.0f, 1.0f, 1.0f, 1.0f});
    }
}
