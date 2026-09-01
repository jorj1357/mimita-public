#include "engine/engine-tick-setup.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
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
#include "gui/hud/reward-popup.h"
#include "config/killfeed-config.h"
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
#include "duel/duel-weapon-pool.h"
#include "hot-reload/hot-reload-system.h"
#include "notifications/notifications.h"
#include "gui/gui-layout.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "devtools/terminal.h"
#include "terminal/weapon-commands.h"
#include "render/post-fx.h"
#include "killfeed/killfeed.h"

#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

extern FramePacer gFramePacer;

// ── Hot-reload master gate ─────────────────────────────────
// Reads config/hotreload.json once at startup. When enabled=false,
// zero config files are polled for changes (no filesystem overhead).
// When enabled=true, polling is throttled to every N frames.
static bool sHotReloadEnabled = true;
static int sHotReloadFrameCounter = 0;
static constexpr int HOT_RELOAD_THROTTLE_FRAMES = 10; // check every 10 frames (~6x/sec at 60fps)

static void loadHotReloadConfig()
{
    std::ifstream f("config/hotreload.json");
    if (!f.is_open()) {
        sHotReloadEnabled = true; // default to enabled if file missing
        return;
    }
    try {
        nlohmann::json root;
        f >> root;
        sHotReloadEnabled = root.value("enabled", true);
        printf("[HOTRELOAD] Master gate: %s\n", sHotReloadEnabled ? "ENABLED" : "DISABLED");
    } catch (...) {
        sHotReloadEnabled = true;
    }
}

void engineTickSetup(Engine& engine, float& dt, bool& worldPassRan)
{
    { MIMITA_PERF_SCOPE("Setup::HotReloadDLL");
      if (sHotReloadEnabled)
          HotReloadSystem::instance().reloadGameDLLIfChanged(); }
    // gFramePacer.beginFrame() and Perf::beginFrame() are now called in
    // engineTick() BEFORE the first MIMITA_PERF_SCOPE so the scope stack
    // is empty when perfResetScopes() runs.
    { MIMITA_PERF_SCOPE("Setup::BeginFrame"); dt = engine.beginFrame(); }
    { MIMITA_PERF_SCOPE("Setup::Analytics"); AnalyticsManager::instance().update(dt); }
    { MIMITA_PERF_SCOPE("Setup::PlayerHotReload"); updatePlayerProceduralHotReload(dt); }

    // Load hot-reload config once at startup
    static bool sFirstFrame = true;
    if (sFirstFrame) {
        loadHotReloadConfig();
        RewardPopupConfig::instance().load();
        KillfeedConfig::instance().load();
        KillfeedManager::instance().setMode(KillfeedConfig::instance().data().mode);
        sFirstFrame = false;
    }

    // When hot-reload is disabled, skip all config polling (zero overhead).
    // When enabled, throttle to every N frames.
    const bool shouldPoll = sHotReloadEnabled && (++sHotReloadFrameCounter % HOT_RELOAD_THROTTLE_FRAMES == 0);

    { MIMITA_PERF_SCOPE("Setup::ConfigPolling");
    if (shouldPoll) {
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
        MimitaNet::pollDisagreementReload();
        CamConfig::instance().pollReload();
        SizeScalingConfig::instance().pollReload();
        AvatarSystem::instance().pollHotReload();
        RagdollConfig::instance().pollReload();
        RagdollDeathConfig::instance().pollReload();
        if (NpcDifficultyConfig::instance().pollReload())
        {
            static uint64_t lastNpcDifficultyRevision = 0;
            if (lastNpcDifficultyRevision != NpcDifficultyConfig::instance().revision())
            {
                THE_NPC_SYSTEM.refreshDifficultyTuning();
                lastNpcDifficultyRevision = NpcDifficultyConfig::instance().revision();
            }
        }
        GamemodeRegistry::instance().pollReload();
        DuelMapPool::instance().pollReload();
        DuelWeaponPool::instance().pollReload();
        WeaponHitFxConfig::instance().pollReload();
        ImpactDecalsConfig::instance().pollReload();
        NotificationSystem::instance().pollReload();
        GuiLayoutManager::instance().pollReload();
        NetworkingConfig::instance().pollReload();
        PostFX::instance().pollReload();
        if (CollisionLodConfig::instance().pollHotReload())
            redecimateCollision(THE_WORLD);
        CollisionConfig::instance().pollHotReload();
        RewardPopupConfig::instance().pollReload();
        if (KillfeedConfig::instance().pollReload())
            KillfeedManager::instance().setMode(KillfeedConfig::instance().data().mode);
    }
    }

    { MIMITA_PERF_SCOPE("Setup::DynamicLights"); DynamicLightManager::instance().update(dt); }
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

    { MIMITA_PERF_SCOPE("Setup::Debug");
      DebugVis::update();
      uiSetDebug(DebugVis::ui()); }
}
