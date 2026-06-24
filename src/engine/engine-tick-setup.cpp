#include "engine/engine-tick-setup.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"
#include "perf/perf.h"
#include "analytics/analytics-manager.h"
#include "avatar/avatar.h"
#include "crosshair/crosshair-config.h"
#include "hot-reload/hot-reload-system.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "devtools/terminal.h"

extern FramePacer gFramePacer;

void engineTickSetup(Engine& engine, float& dt, bool& worldPassRan)
{
    HotReloadSystem::instance().reloadGameDLLIfChanged();
    gFramePacer.beginFrame();
    Perf::beginFrame();
    dt = engine.beginFrame();
    AnalyticsManager::instance().update(dt);
    updatePlayerProceduralHotReload(dt);
    CrosshairConfig::instance().pollReload();
    AvatarSystem::instance().pollHotReload();
    worldPassRan = false;

    {
        static uint32_t lastReloadCount = 0;
        uint32_t currentCount = HotReloadSystem::instance().gameMemory().reloadCount;
        if (currentCount != lastReloadCount) {
            printf("[AVATAR UI] Hot reload re-register complete (count=%u)\n", currentCount);
            Terminal::instance().addLog("[AVATAR UI] Hot reload re-register complete");
            lastReloadCount = currentCount;
        }
    }

    DebugVis::update();
    uiSetDebug(DebugVis::ui());
}
