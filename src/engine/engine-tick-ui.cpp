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
#include "gui/gui-editor.h"
#include "ui/hitmarker.h"
#include "crosshair/crosshair-render.h"
#include "crosshair/crosshair-config.h"
#include "devtools/dev-config.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/terminal.h"
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

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;

void engineTickUI(Engine& engine, float dt, bool worldPassRan)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    bool& editorMode = EDITOR_MODE;
    std::string& activeGameMode = ACTIVE_GAME_MODE;
    std::string& activeMapPath = ACTIVE_MAP_PATH;
    int& selectedEditorObject = SELECTED_EDITOR_OBJ;
    bool& freecamEnabled = FREECAM_ENABLED;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& replayWeaponModels = REPLAY_WEAPON_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& G_COMMAND_BINDS = CMD_BINDS;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayFactory = REPLAY_FACTORY;
    auto& gReplayBrowser = REPLAY_BROWSER;
    auto& gReplayTimeline = REPLAY_TIMELINE;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    { Perf::ScopedTimer _ui("UI");
    uiBeginFrame(engine.window(), "game-debug-overlay");

    engineTickUIHUD(engine, dt);

    if (player.spawnFlashTimer <= 0.0f)
        engineTickUIOverlays(engine, dt, worldPassRan);

    if (gReplayPlayer.totalTicks() > 0) {
        const ReplaySceneFrame* cleanupFrame = gReplayPlayer.currentSceneFrame();
        std::vector<std::string> toRemove;
        for (const auto& kv : replayActorModels) {
            if (!cleanupFrame ||
                std::none_of(cleanupFrame->actors.begin(),
                             cleanupFrame->actors.end(),
                             [&](const ReplayActorState& a) { return a.id == kv.first; })) {
                toRemove.push_back(kv.first);
            }
        }
        for (const std::string& id : toRemove)
            replayActorModels.erase(id);
    }

    Perf::state().npcCount = (int)npcSystem.all().size();
    if (Perf::state().npcCount > Perf::state().peakNpcCount)
        Perf::state().peakNpcCount = (float)Perf::state().npcCount;
    Perf::state().playerCount = 1;
    Perf::state().bloodCount = EffectPartSystem::instance().activeCount();
    Perf::state().particleCount = EffectPartSystem::instance().activeCount();
    Perf::state().effectCount = (int)EffectPartSystem::instance().activeCount();
    Perf::state().corpseCount = 0;
    if (gReplayPlayer.isPlaying())
        Perf::state().replayMemoryMb = (double)gReplayPlayer.totalTicks() * sizeof(ReplaySceneFrame) / (1024.0 * 1024.0);
    uiEndFrame();

    if (GuiEditor::instance().isEnabled()) {
        GuiEditor::instance().setActiveLayout("config/gui/hud.json");
        GuiEditor::instance().update(engine.window());
    }
    } // Perf::ScopedTimer UI
}
