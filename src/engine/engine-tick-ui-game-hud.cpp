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
#include "debug/debug-visuals.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;

void engineTickUIGameHUD(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& editorMode = EDITOR_MODE;
    std::string& activeGameMode = ACTIVE_GAME_MODE;
    bool& freecamEnabled = FREECAM_ENABLED;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
    auto hudText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = hudLayout.get(id);
        if (!el) return;
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        glm::vec4 color = el->getTextColorVec();
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
    };

    if (!replayPlaybackActive || gReplayCinematicMode) {
        if (weapons.getCurrentDef(player)) {
            updateCrosshairDynamic(
                dt, glm::length(glm::vec2(player.vel)), player.ground.onGround,
                player.dash.didDash, weapons.isShooting());
            drawCrosshair(uiScreenW() * 0.5f, uiScreenH() * 0.5f);
        }
    }
    if (player.spawnFlashTimer > 0.0f)
    {
        Debug::logThrottled(Debug::Category::Audio, "spawnflash", 1.0f,
            "[SPAWN FX] hiding GUI for spawn flash (timer=%.0f)\n", player.spawnFlashTimer);
    }
    else
    {
    hudText("playerName", player.username);
    char hpText[64];
    snprintf(hpText, sizeof(hpText), "HP: %d/%d", player.currentHp, player.maxHp);
    hudText("hpText", hpText);
    if (player.dead && gDuelManager.phase() != DuelPhase::MatchEnd) {
        if (!gReplayExportRenderMode || ReplayExportUI::showDeathScreen)
        {
        const float centerX = uiScreenW() * 0.5f;
        const float centerY = uiScreenH() * 0.5f;
        std::string deathText = "you died to " +
            (player.killedBy.empty() ? std::string("unknown") : player.killedBy);
        char respawnText[128];
        snprintf(respawnText, sizeof(respawnText),
                 "respawning automatically in %.3f...", player.respawnTimer);
        uiDrawRect(
            {centerX - 270.0f, centerY - 80.0f, 540.0f, 160.0f},
            {0.0f, 0.0f, 0.0f, 0.75f},
            "death-overlay");
        hudText("deathText", deathText);
        hudText("respawnText", respawnText);
        hudText("respawnHint", "press space to respawn instantly");
        }
    }
    if (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay)
    {
        glm::vec3 totalVel = player.vel;
        float speed = glm::length(totalVel);
        char spBuf[64];
        snprintf(spBuf, sizeof(spBuf), "Speed: %.2f m/s", speed);
        hudText("speedText", spBuf);
    }
    if (DebugConfig::DEBUG_PHYSICS && (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay))
    {
        char dbg[512];
        int y = 200;
        snprintf(dbg, sizeof(dbg),
            "gnd:%d stb:%d raw:%d realC:%d wc:%d pos:(%.1f %.1f %.1f) vel.z:%.1f",
            (int)player.ground.onGround, (int)player.ground.stableOnGround,
            (int)player.ground.wasOnGround,
            (int)player.ground.realWorldContactThisFrame, (int)player.ground.hasWorldContact,
            player.pos.x, player.pos.y, player.pos.z,
            player.vel.z);
        uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
        snprintf(dbg, sizeof(dbg),
            "jmpI:%.2f aj:%d da:%d grAv:%d ddAv:%d jc:%d",
            player.jump.jumpIntentTimer, player.jump.airJumpsLeft,
            (int)player.dash.dashAvailable, (int)player.groundReturn.available,
            (int)player.dash.downDashAvailable, (int)player.jump.jumpConsumed);
        uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
        float feetZ = player.pos.z - PLAYER_HEIGHT * 0.5f;
        snprintf(dbg, sizeof(dbg),
            "feetZ:%.2f landCD:%.2f fzAv:%d",
            feetZ, player.ground.landingCooldown, (int)player.freeze.freezeAvailable);
        uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
        snprintf(dbg, sizeof(dbg),
            "didLand:%d wasGnd:%d stbGnd:%d gLost:%.2f airT:%.2f",
            (int)player.ground.didLand, (int)player.ground.wasOnGround,
            (int)player.ground.stableOnGround, player.ground.groundLostTimer, player.ground.airborneTimer);
        uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f});
    }
    if (!gReplayExportRenderMode || ReplayExportUI::showModeText)
    {
        char modeText[128];
        snprintf(modeText, sizeof(modeText), "%s | %s | slot %d",
                 editorMode ? "EDITOR" : "PLAYING", activeGameMode.c_str(), player.equippedSlot);
        hudText("modeText", modeText);
        if (mpContext.active) {
            char mpText[128];
            snprintf(mpText, sizeof(mpText), "MP id=%u players=%zu server=%s",
                     mpContext.localPlayerId,
                     mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1 : 0),
                     mpContext.serverAddress.c_str());
            uiDrawText(mpText, 24, 232, 0.32f, {0.7f, 0.9f, 1.0f, 1.0f});
        }
        {
            const WeaponDefinition* curDef = nullptr;
            for (const auto& pair : WeaponRegistry::instance().all()) {
                if (pair.second.slot == player.equippedSlot) {
                    curDef = &pair.second;
                    break;
                }
            }
            if (curDef) {
                auto it = player.weaponRuntimes.find(curDef->id);
                if (it != player.weaponRuntimes.end()) {
                    const WeaponRuntime& rt = it->second;
                    char ammoText[96];
                    int displayReserve = std::max(0, rt.reserveAmmo);
                    snprintf(ammoText, sizeof(ammoText), "%s: %d / %d",
                             curDef->displayName.c_str(),
                             rt.currentAmmo, displayReserve);
                    hudText("ammoText", ammoText);

                    if (rt.isReloading) {
                        char reloadText[96];
                        snprintf(reloadText, sizeof(reloadText),
                                 "no bullets! reloading... %.2f",
                                 std::max(0.0f, rt.reloadTimer));
                        hudText("reloadText", reloadText);
                    }
                }
            }
        }
        if (player.inventoryOpen)
            uiDrawText("INVENTORY: [1] Revolver [2-10] Empty", 24, 260, 0.36f, {0.9f,0.9f,1.0f,1.0f});
    }
    {
        const float normalSize = 44.0f;
        const float gap = 7.0f;
        const float totalWidth = normalSize * 10.0f + gap * 9.0f;
        float x = uiScreenW() * 0.5f - totalWidth * 0.5f;
        float y = uiScreenH() - 70.0f;
        for (int slot = 1; slot <= 10; ++slot) {
            bool equipped = player.equippedSlot == slot;
            float size = equipped ? normalSize * 1.2f : normalSize;
            float offset = (size - normalSize) * 0.5f;
            UIRect rect{x - offset, y - offset, size, size};
            uiDrawRect(rect, slot == 1 ? glm::vec4(0.32f,0.32f,0.36f,0.95f)
                                       : glm::vec4(0.12f,0.12f,0.14f,0.92f), "hotbar-slot");
            uiDrawRectOutline(rect, equipped ? glm::vec4(1,1,1,1)
                                             : glm::vec4(0.45f,0.45f,0.48f,1), "hotbar-border");
            std::string label = slot == 10 ? "0" : std::to_string(slot);
            uiDrawText(label.c_str(), rect.x + 5, rect.y + 16, 0.30f, {1,1,1,1});
            const WeaponDefinition* slotDef = nullptr;
            for (const auto& pair : WeaponRegistry::instance().all()) {
                if (pair.second.slot == slot) {
                    slotDef = &pair.second;
                    break;
                }
            }
            if (slotDef) {
                std::string shortName = slotDef->id.substr(0, 3);
                std::transform(shortName.begin(), shortName.end(), shortName.begin(), ::toupper);
                uiDrawText(shortName.c_str(), rect.x + 13, rect.y + 34, 0.20f,
                           equipped ? glm::vec4(1,0.85f,0.35f,1) : glm::vec4(0.55f,0.55f,0.58f,1));
            } else {
                uiDrawText("-", rect.x + 13, rect.y + 34, 0.20f, glm::vec4(0.55f,0.55f,0.58f,1));
            }
            x += normalSize + gap;
        }
    }
    if (!replayPlaybackActive) {
    {
        float nameX = 0.0f, nameY = 0.0f;
        if (DebugVis::projectToScreen(camera, player.pos + glm::vec3(0,0,PLAYER_HEIGHT * 0.7f),
                                      nameX, nameY)) {
            float ratio = player.maxHp > 0 ? (float)player.currentHp / player.maxHp : 0.0f;
            uiDrawRect({nameX - 70, nameY - 8, 140, 12}, {0.55f,0.05f,0.05f,0.95f}, "self-hp-bg");
            uiDrawRect({nameX - 70, nameY - 8, 140 * ratio, 12}, {0.05f,0.8f,0.15f,0.95f}, "self-hp-current");
            uiDrawText(player.username.c_str(), nameX - 35, nameY - 32, 0.32f, {1,1,1,1});
            uiDrawText(hpText, nameX - 35, nameY + 8, 0.28f, {1,1,1,1});
        }
    }
        for (const Npc& npc : npcSystem.all()) {
            if (npc.body.dead) {
                continue;
            }
            drawPlayerHealthbar(npc.body, camera, "npc-hp");
        }
    }

    renderChatBubbles(player.chatState, player, camera);
    if (!replayPlaybackActive)
    {
        for (auto& kv : mpContext.remotePlayers)
            renderChatBubbles(kv.second.chatState, kv.second, camera);
    }
    else
    {
        for (const auto& kv : gReplayChatStates)
        {
            auto actorIt = replayActorModels.find(kv.first);
            if (actorIt != replayActorModels.end() && actorIt->second)
                renderChatBubbles(kv.second, *actorIt->second, camera);
        }
    }

    if (mpContext.active)
    {
        static uint64_t lastHealthbarLogMs = 0;
        const uint64_t healthbarNowMs = MimitaNet::nowMs();
        const bool logHealthbars =
            mpContext.showDebugOverlay &&
            healthbarNowMs - lastHealthbarLogMs >= 1000;

        for (const auto& kv : mpContext.remotePlayers)
        {
            const HealthbarRenderResult result =
                drawPlayerHealthbar(
                    kv.second, camera, "network-player-hp");
            if (logHealthbars)
            {
                printf(
                    "[NET HEALTHBAR] entityId=%u owner=remote "
                    "health=%d/%d anchor=%s "
                    "world=(%.2f %.2f %.2f) screen=(%.1f %.1f) "
                    "distance=%.1f rendered=%d cull=%s\n",
                    kv.first,
                    kv.second.currentHp, kv.second.maxHp,
                    result.usedHeadTransform ? "head" : "fallback",
                    result.anchor.x, result.anchor.y, result.anchor.z,
                    result.screen.x, result.screen.y,
                    result.distance, (int)result.rendered,
                    healthbarCullReasonName(result.cullReason));
            }
        }

        if (logHealthbars)
            lastHealthbarLogMs = healthbarNowMs;
    }
    {
        static float hpLogTimer = 0.0f; hpLogTimer -= 0.016f;
        if (hpLogTimer <= 0.0f && !npcSystem.all().empty()) {
            hpLogTimer = 1.0f;
            printf("[PLAYER HP FRAME] hp=%d/%d pos=(%.1f %.1f %.1f)\n",
                   player.currentHp, player.maxHp,
                   player.pos.x, player.pos.y, player.pos.z);
        }
    }
    if (!gReplayExportRenderMode || ReplayExportUI::showNpcDebug)
    {
        char npcText[96];
        snprintf(npcText, sizeof(npcText), "NPCs: %zu", npcSystem.all().size());
        uiDrawText(npcText, 24, 168, 0.32f, {1.0f, 0.82f, 0.38f, 1.0f});
        if (!npcSystem.all().empty()) {
            const Npc& first = npcSystem.all().front();
            char tuneText[256];
            snprintf(tuneText, sizeof(tuneText),
                     "  Diff=%.0f aimErr=%.1fdeg reaction=%.2fs moveVar=%.2f",
                     first.difficulty,
                     NpcCombat::aimErrorDegrees(first.difficulty),
                     first.tuning.reactionDelay,
                     first.tuning.movementPrecision);
            uiDrawText(tuneText, 24, 184, 0.28f, {0.8f, 0.9f, 1.0f, 1.0f});
        }
    }
    if (DebugVis::render() && (!gReplayExportRenderMode || ReplayExportUI::showDebugVis))
    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "dt %.3f grounded %d vel %.2f %.2f %.2f cam %.1f %.1f %.1f",
                 dt, (int)player.ground.onGround, player.vel.x, player.vel.y, player.vel.z,
                 camera.pos.x, camera.pos.y, camera.pos.z);
        uiDrawText(dbg, 24, 184, 0.30f, {1.0f, 0.9f, 0.45f, 1.0f});
    }
    }
}
