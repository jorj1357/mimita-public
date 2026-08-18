#include "engine/engine-tick-setup.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"
#include "perf/perf.h"
#include "analytics/analytics-manager.h"
#include "avatar/avatar.h"
#include "ragdoll/ragdoll-config.h"

#include "crosshair/crosshair-config.h"
#include "config/gameplay-config.h"
#include "config/movement-config.h"
#include "config/networking-config.h"
#include "config/size-scaling-config.h"
#include "config/collision-lod-config.h"
#include "config/collision-config.h"
#include "map/map-loader-collision.h"
#include "gui/hud/healthbar-config.h"
#include "effects/hit-effects.h"
#include "effects/muzzle-flash-config.h"
#include "render/dynamic-light-config.h"
#include "render/dynamic-light.h"
#include "network/disagreement-visuals.h"
#include "config/camera-config.h"
#include "config/weapon-hitfx-config.h"
#include "config/impact-decals-config.h"
#include "config/weapon-tracers-config.h"
#include "config/ragdoll-death-config.h"
#include "npc/npc-difficulty-config.h"
#include "gamemode/gamemode.h"
#include "duel/duel-map-pool.h"
#include "hot-reload/hot-reload-system.h"
#include "notifications/notifications.h"
#include "gui/gui-layout.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "devtools/terminal.h"
#include "terminal/weapon-commands.h"

extern FramePacer gFramePacer;

void engineTickSetup(Engine& engine, float& dt, bool& worldPassRan)
{
    HotReloadSystem::instance().reloadGameDLLIfChanged();
    gFramePacer.beginFrame();
    Perf::beginFrame();
    dt = engine.beginFrame();
    AnalyticsManager::instance().update(dt);
    updatePlayerProceduralHotReload(dt);
    GameplayConfig::instance().pollReload();
    MovementJsonConfig::instance().pollReload();
    CrosshairConfig::instance().pollReload();
    pollWorldCrosshairConfig();
    pollCoolShotLineConfig();
    WeaponTracersConfig::instance().pollReload();
    HealthbarConfig::instance().pollReload();
    HitEffects::pollReload();
    MuzzleFlashConfig::instance().pollReload();
    DynamicLightConfig::instance().pollReload();
    DynamicLightManager::instance().update(dt);
    MimitaNet::pollDisagreementReload();
    CamConfig::instance().pollReload();
    SizeScalingConfig::instance().pollReload();
    AvatarSystem::instance().pollHotReload();
    RagdollConfig::instance().pollReload();
    RagdollDeathConfig::instance().pollReload();
    NpcDifficultyConfig::instance().pollReload();
    GamemodeRegistry::instance().pollReload();
    DuelMapPool::instance().pollReload();
    WeaponHitFxConfig::instance().pollReload();
    ImpactDecalsConfig::instance().pollReload();
    NotificationSystem::instance().pollReload();
    GuiLayoutManager::instance().pollReload();
    NetworkingConfig::instance().pollReload();
    if (CollisionLodConfig::instance().pollHotReload())
        redecimateCollision(THE_WORLD);
    CollisionConfig::instance().pollHotReload();
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
