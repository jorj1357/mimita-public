// 08 19 2026, 09 50
/* purpose
* Draws gameplay HUD elements shared by live play and replay playback.
* Renders crosshair, player status, ammo, healthbars, chat, and mode text.
* Uses replay actor state when replay playback or export is active.
* Does NOT render the 3D world, replay camera, or encode video frames.
* Does NOT own replay export framebuffer capture or encoder state.
* Does NOT implement replay controls, timeline, or info panels.
*/
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
#include "gui/hud/chat-window.h"
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
#include "config/gameplay-config.h"
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

    // During replay, force spawn flash off so the full HUD is visible.
    if (replayPlaybackActive)
        player.spawnFlashTimer = 0.0f;

    // During replay playback, read from the viewed replay actor instead of
    // the local player so HUD displays correct values at correct positions.
    const ReplayActorState* replayViewedActor = nullptr;
    if (replayPlaybackActive) {
        const ReplaySceneFrame* rf = gReplayPlayer.currentSceneFrame();
        if (rf && !rf->actors.empty())
            replayViewedActor = &rf->actors[0];
    }

    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");

    // Draw all panel elements from hud.json (backgrounds, containers, etc.)
    // This makes JSON-edited panels visible, matching the server browser pattern.
    for (const auto& id : hudLayout.elementIds())
    {
        const GuiElement* el = hudLayout.get(id);
        if (el && el->type == "panel" && el->visible)
            drawGuiElement(engine.window(), *el);
    }

    auto hudText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = hudLayout.get(id);
        if (!el) return;
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        glm::vec4 color = el->getTextColorVec();
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
    };

    auto ammoStateColor = [&](const char* id, glm::vec4 fallback) -> glm::vec4 {
        const GuiElement* el = hudLayout.get(id);
        return el ? el->getTextColorVec() : fallback;
    };

    // Ammo indicator centered below the crosshair. Color + blink state come
    // from JSON elements in hud.json: full=white, 1 bullet=blink white/red
    // every 30 ticks, 0=red, reloading=blue with a fast-ticking countdown.
    auto drawAmmoIndicator = [&](const std::string& weaponName, int ammo, int reserve,
                                 int maxAmmo, bool reloading, float reloadTimer) {
        const GuiElement* el = hudLayout.get("ammoText");
        if (!el || !el->visible) return;
        if (!reloading && maxAmmo <= 0) return;  // melee / no ammo concept
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.5f;
        glm::vec4 color;
        char text[128];
        if (reloading) {
            color = ammoStateColor("ammoColorReload", {0.3f, 0.6f, 1.0f, 1.0f});
            if (reloadTimer > 0.0f)
                snprintf(text, sizeof(text), "reloading %.2f", std::max(0.0f, reloadTimer));
            else
                snprintf(text, sizeof(text), "reloading...");
        } else if (ammo == 1) {
            const bool blinkOn = (gChatUiTickClock.getTick() / 30u) % 2u == 0u;
            color = ammoStateColor(blinkOn ? "ammoColorLowA" : "ammoColorLowB",
                                   blinkOn ? glm::vec4(1.0f)
                                           : glm::vec4(1.0f, 0.15f, 0.15f, 1.0f));
            snprintf(text, sizeof(text), "%s: %d / %d", weaponName.c_str(), ammo, reserve);
        } else if (ammo <= 0) {
            color = ammoStateColor("ammoColorEmpty", {1.0f, 0.15f, 0.15f, 1.0f});
            snprintf(text, sizeof(text), "%s: %d / %d", weaponName.c_str(), ammo, reserve);
        } else {
            color = ammoStateColor("ammoColorFull", {1.0f, 1.0f, 1.0f, 1.0f});
            snprintf(text, sizeof(text), "%s: %d / %d", weaponName.c_str(), ammo, reserve);
        }
        float textW = uiMeasureText(text, scale);
        uiDrawText(text, uiScaleX(el->x) - textW * 0.5f, uiScaleY(el->y), scale, color);
    };

    {
        glm::vec3 vel = replayViewedActor ? replayViewedActor->velocity : player.vel;
        bool grounded = replayViewedActor ? replayViewedActor->grounded : player.ground.onGround;
        bool shooting = replayViewedActor ? replayViewedActor->shooting : weapons.isShooting();
        bool didDash = replayViewedActor ? false : player.dash.didDash;
        if (replayViewedActor || weapons.getCurrentDef(player)) {
            updateCrosshairDynamic(dt, glm::length(glm::vec2(vel)), grounded, didDash, shooting);
            float cx = uiScreenW() * 0.5f, cy = uiScreenH() * 0.5f;
            if (GameplayConfig::instance().aimMode() == GameplayAimMode::Physical &&
                weapons.mPhysicalAimValid &&
                DebugVis::projectToScreen(camera, weapons.mPhysicalAimPoint, cx, cy)) {
                // Crosshair sits on the laser impact instead of screen center.
            }
            drawCrosshair(cx, cy);
        }
    }
    if (player.spawnFlashTimer > 0.0f)
    {
        Debug::logThrottled(Debug::Category::Audio, "spawnflash", 1.0f,
            "[SPAWN FX] hiding GUI for spawn flash (timer=%.0f)\n", player.spawnFlashTimer);
    }
    else
    {
    hudText("playerName", replayViewedActor ? replayViewedActor->name : player.username);
    int hp = replayViewedActor ? replayViewedActor->health : player.currentHp;
    int maxHp = replayViewedActor ? replayViewedActor->maxHealth : player.maxHp;
    bool dead = replayViewedActor ? replayViewedActor->dead : player.dead;
    char hpText[64];
    snprintf(hpText, sizeof(hpText), "HP: %d/%d", hp, maxHp);
    hudText("hpText", hpText);
    if (dead && gDuelManager.phase() != DuelPhase::MatchEnd) {
        if (!gReplayExportRenderMode || ReplayExportUI::showDeathScreen)
        {
        // Draw death overlay from layout JSON
        const GuiElement* doEl = hudLayout.get("deathOverlay");
        if (doEl && doEl->visible) drawGuiElement(engine.window(), *doEl);

        std::string deathText = "you died to " +
            (player.killedBy.empty() ? std::string("unknown") : player.killedBy);
        char respawnBuf[128];
        snprintf(respawnBuf, sizeof(respawnBuf),
                 "respawning automatically in %.3f...", player.respawnTimer);
        hudText("deathText", deathText);
        hudText("respawnText", respawnBuf);
        // Show Space hint only when instant respawn would actually work
        const bool duelBlocksRespawn = gDuelManager.phase() != DuelPhase::Off;
        hudText("respawnHint", duelBlocksRespawn ? "" : "press space to respawn instantly");
        }
    }
    if (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay)
    {
        glm::vec3 totalVel = replayViewedActor ? replayViewedActor->velocity : player.vel;
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
                    (int)player.dash.downDashAvailable);
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
        if (replayViewedActor) {
            const char* weaponName = replayViewedActor->weaponName.empty() ? "?" : replayViewedActor->weaponName.c_str();
            const WeaponDefinition* replayDef = WeaponRegistry::instance().get(weaponName);
            drawAmmoIndicator(weaponName, replayViewedActor->currentAmmo,
                              replayViewedActor->reserveAmmo,
                              replayDef ? replayDef->magazineSize : 0,
                              replayViewedActor->reloading, 0.0f);
        } else {
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
                    drawAmmoIndicator(curDef->displayName, rt.currentAmmo,
                                      std::max(0, rt.reserveAmmo), curDef->magazineSize,
                                      rt.isReloading, rt.reloadTimer);
                }
            }
        }
        if (!replayViewedActor && player.inventoryOpen)
            uiDrawText("INVENTORY: [1] Revolver [2-10] Empty", 24, 260, 0.36f, {0.9f,0.9f,1.0f,1.0f});
    }

    // ── Hotbar from JSON config ────────────────────────────────────
    {
        auto readVal = [&](const char* id, float defaultVal) -> float {
            const GuiElement* el = hudLayout.get(id);
            return el ? el->x : defaultVal;
        };
        auto readCol = [&](const char* id, glm::vec4 defaultVal) -> glm::vec4 {
            const GuiElement* el = hudLayout.get(id);
            return el ? el->getBackgroundColorVec() : defaultVal;
        };
        auto readTextCol = [&](const char* id, glm::vec4 defaultVal) -> glm::vec4 {
            const GuiElement* el = hudLayout.get(id);
            return el ? el->getTextColorVec() : defaultVal;
        };

        float slotSize = readVal("hotbarSlotSize", 44.0f);
        float gap = readVal("hotbarGap", 7.0f);
        float yOffset = readVal("hotbarY", 70.0f);
        int slotCount = (int)readVal("hotbarSlotCount", 10.0f);
        glm::vec4 bgEq = readCol("hotbarBgEquipped", {0.32f,0.32f,0.36f,0.95f});
        glm::vec4 bgNorm = readCol("hotbarBgNormal", {0.12f,0.12f,0.14f,0.92f});
        glm::vec4 borderEq = readTextCol("hotbarBorderEquipped", {1,1,1,1});
        glm::vec4 borderNorm = readTextCol("hotbarBorderNormal", {0.45f,0.45f,0.48f,1});
        glm::vec4 wepCol = readTextCol("hotbarWeaponColor", {0.55f,0.55f,0.58f,1});
        glm::vec4 wepColEq = readTextCol("hotbarWeaponColorEquipped", {1.0f,0.85f,0.35f,1});

        float totalWidth = slotSize * slotCount + gap * (slotCount - 1);
        float x = uiScreenW() * 0.5f - totalWidth * 0.5f;
        float y = uiScreenH() - yOffset;
        for (int slot = 1; slot <= slotCount; ++slot) {
            bool equipped = player.equippedSlot == slot;
            float size = equipped ? slotSize * 1.2f : slotSize;
            float offset = (size - slotSize) * 0.5f;
            UIRect rect{x - offset, y - offset, size, size};
            uiDrawRect(rect, equipped ? bgEq : bgNorm, "hotbar-slot");
            uiDrawRectOutline(rect, equipped ? borderEq : borderNorm, "hotbar-border");
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
                           equipped ? wepColEq : wepCol);
            } else {
                uiDrawText("-", rect.x + 13, rect.y + 34, 0.20f, wepCol);
            }
            x += slotSize + gap;
        }
    }

    // ── Self-healthbar (world-space, reuses NPC healthbar style) ──
    if (!dead && !replayPlaybackActive) {
        Debug::log(Debug::Category::Gui,
            "[HEALTHBAR SELF] calling drawPlayerHealthbar hp=%d/%d pos=(%.2f %.2f %.2f)\n",
            player.currentHp, player.maxHp,
            player.pos.x, player.pos.y, player.pos.z);
        drawPlayerHealthbar(player, camera, "self-hp", "live_world");
    }
    for (const Npc& npc : npcSystem.all()) {
        if (npc.body.dead) {
            continue;
        }
        drawPlayerHealthbar(npc.body, camera, "npc-hp", "live_world");
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
                    kv.second, camera, "network-player-hp", "live_world");
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

        // Server-authoritative NPCs render as network entities (no local
        // NpcSystem mirror), so draw their health bars here too.
        for (const auto& kv : mpContext.remoteNpcs)
        {
            if (kv.second.dead || kv.second.currentHp <= 0) continue;
            drawPlayerHealthbar(kv.second, camera, "network-npc-hp", "live_world");
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

    // ── Chat Window ─────────────────────────────────────
    Debug::logThrottled(Debug::Category::Chat, "chat-trace-render-before", 1.0f,
                        "[CHAT TRACE 3 BEFORE HUD GATE] gpChatHistory=%p chatOpen=%d\n",
                        (void*)gpChatHistory,
                        (int)gChatWindowState.open);
    if (gpChatHistory)
    {
        renderChatWindow(gChatWindowState, engine.window(), gChatHistory,
                         gChatUiTickClock, uiScreenW(), uiScreenH());
        Debug::logThrottled(Debug::Category::Chat, "chat-trace-render-after", 1.0f,
                            "[CHAT TRACE 3 AFTER HUD RENDER] gpChatHistory=%p chatOpen=%d\n",
                            (void*)gpChatHistory,
                            (int)gChatWindowState.open);
    }
    else
    {
        Debug::warn(Debug::Category::Chat,
                    "[CHAT TRACE 3 PROBLEM] gpChatHistory is NULL; renderChatWindow was skipped\n");
    }
}
