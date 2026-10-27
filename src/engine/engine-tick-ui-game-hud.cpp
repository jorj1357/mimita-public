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
#include <algorithm>
#include "terminal/terminal-state.h"
#include <cstdio>
#include <vector>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "network/multiplayer-context.h"
#include "network/community-match-client.h"
#include "procedural/procedural-world-client.h"
#include "network/community-server-config.h"
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
#include "gui/hud/healthbar-config.h"
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
#include "perf/perf-spike.h"
#include "video/frame-pacer.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "render/post-fx.h"
#include "render/lighting-config.h"
#include "audio/music-manager.h"
#include "game/duel.h"
#include "duel/duel-queue.h"
#include "game/gamemode-manager.h"
#include "gamemode/gamemode.h"
#include "game/game-state.h"
#include "network/multiplayer-context.h"

#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config/player-settings.h"
#include "config/gameplay-config.h"
#include "npc/npc-combat.h"
#include "network/server.h"
#include "debug/debug-visuals.h"
#include "vip/vip-name-render.h"
#include "interaction/interaction-system.h"
#include "vehicles/vehicle-hud.h"

extern DuelManager gDuelManager;
extern GamemodeManager gGamemodeManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;

void engineTickUIGameHUD(Engine& engine, float dt)
{
    clientProceduralTeleportShieldRenderUi();
    Player& player = THE_PLAYER;
    auto& communityMatch = MimitaNet::CommunityMatchClient::instance();
    const auto enemyHealthbarForActor = [&](uint32_t actorId) {
        if (!communityMatch.active()) return false;
        const uint8_t localTeam = communityMatch.localTeam(MP_CONTEXT.localPlayerId);
        const uint8_t actorTeam = communityMatch.teamForActor(actorId);
        return localTeam == 0xFF || actorTeam == 0xFF || localTeam != actorTeam;
    };
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
    { MIMITA_PERF_SCOPE("HUD::Panels");
    for (const auto& id : hudLayout.elementIds())
    {
        const GuiElement* el = hudLayout.get(id);
        if (el && el->type == "panel" && el->visible && id != "deathOverlay")
            drawGuiElement(engine.window(), *el);
    } }

    auto hudText = [&](const std::string& id, const std::string& text,
                       float alpha = 1.0f) {
        const GuiElement* el = hudLayout.get(id);
        if (!el) return;
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        glm::vec4 color = el->getTextColorVec();
        color.a *= std::clamp(alpha, 0.0f, 1.0f);
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
        float hitmarkerX = uiScreenW() * 0.5f;
        float hitmarkerY = uiScreenH() * 0.5f;
        glm::vec3 vel = replayViewedActor ? replayViewedActor->velocity : player.vel;
        bool grounded = replayViewedActor ? replayViewedActor->grounded : player.ground.onGround;
        bool shooting = replayViewedActor ? replayViewedActor->shooting : weapons.isShooting();
        bool didDash = replayViewedActor ? false : player.dash.didDash;
        const WeaponDefinition* currentDef = weapons.getCurrentDef(player);
        const bool currentWeaponHasCrosshair = replayViewedActor
            ? replayViewedActor->weaponName != "nothing"
            : (currentDef && currentDef->id != "nothing");
        if (currentWeaponHasCrosshair) {
            updateCrosshairDynamic(dt, glm::length(glm::vec2(vel)), grounded, didDash, shooting);
            float cx = uiScreenW() * 0.5f, cy = uiScreenH() * 0.5f;
            if (GameplayConfig::instance().aimMode() == GameplayAimMode::Physical &&
                weapons.mPhysicalAimValid &&
                DebugVis::projectToScreen(camera, weapons.mPhysicalAimPoint, cx, cy)) {
                // Crosshair sits on the laser impact instead of screen center.
            }
            const GuiElement* dryFireEl = hudLayout.get("dryFireIndicator");
            bool dryFireEnabled = dryFireEl && dryFireEl->visible;
            std::string modeId = communityMatch.mode();
            if (modeId.empty()) modeId = activeGameMode;
            if (modeId == "bomb_tag") modeId = "bombtag";
            else if (modeId == "team_deathmatch") modeId = "tdm";
            else if (modeId == "free_for_all") modeId = "ffa";
            if (dryFireEl && !modeId.empty() &&
                GamemodeRegistry::instance().has(modeId)) {
                const auto& mode = GamemodeRegistry::instance().get(modeId);
                if (mode.presentation.hasDryFireIndicator)
                    dryFireEnabled = mode.presentation.dryFireIndicator;
            }
            const float dryFireFade = dryFireEnabled && dryFireEl
                ? dryFireFeedbackFade(gChatUiTickClock.getTick(), dryFireEl->fadeTicks)
                : 0.0f;
            drawCrosshair(cx, cy, 1.0f, dryFireFade > 0.0f);
            if (dryFireFade > 0.0f && dryFireEl && !dryFireEl->text.empty()) {
                glm::vec4 color = dryFireEl->getTextColorVec();
                color.a *= dryFireEl->opacity * dryFireFade;
                const float scale = dryFireEl->fontSize > 0.0f
                    ? dryFireEl->fontSize : 0.32f;
                const float markerX = cx + crosshairRightExtent() + uiScaleX(dryFireEl->x);
                const float markerY = cy + uiScaleY(dryFireEl->y);
                uiDrawText(dryFireEl->text.c_str(),
                           markerX, markerY, scale, color);
                static bool loggedDryFireDraw = false;
                static uint64_t lastDryFireDrawTick = 0;
                const uint64_t uiTick = gChatUiTickClock.getTick();
                if (!loggedDryFireDraw || uiTick >= lastDryFireDrawTick + 60) {
                    loggedDryFireDraw = true;
                    lastDryFireDrawTick = uiTick;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Weapons, StructuredLevel::Important,
                        "weapon.dry_fire_hud_drawn", modeId,
                        "dry-fire marker submitted to gameplay HUD", uiTick,
                        {{"mode", modeId}, {"text", dryFireEl->text},
                         {"alpha", color.a}, {"x", markerX}, {"y", markerY},
                         {"crosshair_x", cx}, {"crosshair_y", cy},
                         {"crosshair_tinted", true}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
            }
            hitmarkerX = cx;
            hitmarkerY = cy;
        }
        // Keep hit feedback attached to the same projected aim point as the
        // physical-aim crosshair, rather than using a second center-screen
        // coordinate calculation in the hitmarker renderer.
        drawHitmarker(dt, hitmarkerX, hitmarkerY);
    }
    if (player.spawnFlashTimer > 0.0f)
    {
        Debug::logThrottled(Debug::Category::Audio, "spawnflash", 1.0f,
            "[SPAWN FX] hiding GUI for spawn flash (timer=%.0f)\n", player.spawnFlashTimer);
    }
    else
    {
    {
        const GuiElement* nameEl = hudLayout.get("playerName");
        if (nameEl)
        {
            float scale = nameEl->fontSize > 0.0f ? nameEl->fontSize : 0.32f;
            const std::string displayName = replayViewedActor ? replayViewedActor->name : player.username;
            if (replayViewedActor)
            {
                glm::vec4 color = nameEl->getTextColorVec();
                uiDrawText(displayName.c_str(), uiScaleX(nameEl->x), uiScaleY(nameEl->y), scale, color);
            }
            else
            {
                VipNameDrawOptions nameOpts;
                nameOpts.scale = scale;
                nameOpts.alpha = 1.0f;
                nameOpts.phase = 0.0f;
                nameOpts.drawBadge = false;
                nameOpts.detail = &player.vipStyleDetail;
                vipDrawStyledNameCentered(displayName, player.vipAppearance,
                                          uiScaleX(nameEl->x + nameEl->w * 0.5f), uiScaleY(nameEl->y), nameOpts);
            }
        }
    }
    int hp = replayViewedActor ? replayViewedActor->health : player.currentHp;
    int maxHp = replayViewedActor ? replayViewedActor->maxHealth : player.maxHp;
    bool dead = replayViewedActor ? replayViewedActor->dead : player.dead;
    constexpr uint64_t DEATH_POPUP_DURATION_TICKS = 600;
    static bool deathPopupStarted = false;
    static uint64_t deathPopupStartTick = 0;
    bool drawDeathPopup = false;
    float deathPopupAlpha = 0.0f;
    if (dead) {
        if (!deathPopupStarted) {
            deathPopupStarted = true;
            deathPopupStartTick = gChatUiTickClock.getTick();
        }
        const uint64_t elapsedTicks =
            gChatUiTickClock.getElapsedTicks(deathPopupStartTick);
        if (elapsedTicks < DEATH_POPUP_DURATION_TICKS) {
            drawDeathPopup = true;
            deathPopupAlpha = 1.0f -
                static_cast<float>(elapsedTicks) /
                static_cast<float>(DEATH_POPUP_DURATION_TICKS);
        }
    } else {
        deathPopupStarted = false;
    }
    char hpText[64];
    snprintf(hpText, sizeof(hpText), "HP: %d/%d", hp, maxHp);
    hudText("hpText", hpText);
    if (drawDeathPopup && gDuelManager.phase() != DuelPhase::MatchEnd && !DuelQueue::instance().matchOver()) {
        if (!gReplayExportRenderMode || ReplayExportUI::showDeathScreen())
        {
        // Draw death overlay from layout JSON
        const GuiElement* doEl = hudLayout.get("deathOverlay");
        if (doEl && doEl->visible) {
            GuiElement fadedOverlay = *doEl;
            fadedOverlay.opacity *= deathPopupAlpha;
            drawGuiElement(engine.window(), fadedOverlay);
        }

        std::string deathText = "you died to " +
            (player.killedBy.empty() ? std::string("unknown") : player.killedBy);
        char respawnBuf[128];
        snprintf(respawnBuf, sizeof(respawnBuf),
                 "respawning automatically in %.3f...", player.respawnTimer);
        hudText("deathText", deathText, deathPopupAlpha);
        hudText("respawnText", respawnBuf, deathPopupAlpha);
        // Show Space hint only when instant respawn would actually work.
        // Local/offline duel only. Network duels are controlled by DuelQueue + server DuelStatePacket.
        const bool duelBlocksRespawn = gDuelManager.enabled() && gDuelManager.phase() != DuelPhase::Off;
        hudText("respawnHint", duelBlocksRespawn ? "" : "press space to respawn instantly actually trolled it doesnt work as of 2026 10 21 lolllolloloolo -jorj1357 Hehhehehs",
                deathPopupAlpha);
        }
    }
    if (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay())
    {
        glm::vec3 totalVel = replayViewedActor ? replayViewedActor->velocity : player.vel;
        float speed = glm::length(totalVel);
        char spBuf[64];
        snprintf(spBuf, sizeof(spBuf), "Speed: %.2f m/s", speed);
        hudText("speedText", spBuf);
    }
    if (DebugConfig::DEBUG_PHYSICS && (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay()))
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
    if (!gReplayExportRenderMode || ReplayExportUI::showModeText())
    {
        char modeText[128];
        snprintf(modeText, sizeof(modeText), "%s | %s | slot %d",
                 editorMode ? "EDITOR" : "PLAYING", activeGameMode.c_str(), player.equippedSlot);
        hudText("modeText", modeText);
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
    { MIMITA_PERF_SCOPE("HUD::Hotbar");
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
        struct HotbarEntry {
            int logicalSlot = 0;
            const WeaponDefinition* definition = nullptr;
            std::string label;
            std::string shortName;
        };
        static std::vector<HotbarEntry> cachedEntries;
        static int cachedWeaponSetId = -1;
        static uint64_t cachedRegistryRevision = ~0ull;
        static int cachedConfiguredSlotCount = -1;
        static size_t cachedWeaponCount = 0;
        int configuredSlotCount = (int)readVal("hotbarSlotCount", 10.0f);
        auto& cfg = MimitaNet::CommunityServerConfig::instance();
        if (cfg.weaponSets().empty()) cfg.load();
        const int weaponSetId = MP_CONTEXT.communityWeaponSetId > 0
            ? MP_CONTEXT.communityWeaponSetId : 1;
        const auto* set = cfg.weaponSetById(weaponSetId);
        const uint64_t registryRevision = WeaponRegistry::instance().revision();
        const size_t weaponCount = set ? set->weapons.size() : 0;
        if (cachedWeaponSetId != weaponSetId ||
            cachedRegistryRevision != registryRevision ||
            cachedConfiguredSlotCount != configuredSlotCount ||
            cachedWeaponCount != weaponCount) {
            cachedWeaponSetId = weaponSetId;
            cachedRegistryRevision = registryRevision;
            cachedConfiguredSlotCount = configuredSlotCount;
            cachedWeaponCount = weaponCount;
            cachedEntries.clear();
            if (set && !(set->weapons.size() == 1 && set->weapons.front() == "*")) {
                for (int logicalSlot = 1; logicalSlot <= static_cast<int>(set->weapons.size()); ++logicalSlot) {
                    const auto* id = cfg.weaponForSlot(weaponSetId, logicalSlot);
                    const WeaponDefinition* def = id ? WeaponRegistry::instance().get(*id) : nullptr;
                    if (def && def->id != "nothing" && def->behaviorType != WeaponBehaviorType::None)
                        cachedEntries.push_back({logicalSlot, def,
                            logicalSlot == 10 ? "0" : std::to_string(logicalSlot), {}});
                }
            }
            if (cachedEntries.empty()) {
                for (int nativeSlot = 1; nativeSlot <= configuredSlotCount; ++nativeSlot) {
                    const WeaponDefinition* def = nullptr;
                    for (const auto& pair : WeaponRegistry::instance().all()) {
                        if (pair.second.slot == nativeSlot) {
                            def = &pair.second;
                            break;
                        }
                    }
                    if (def && def->id != "nothing" && def->behaviorType != WeaponBehaviorType::None)
                        cachedEntries.push_back({nativeSlot, def,
                            nativeSlot == 10 ? "0" : std::to_string(nativeSlot), {}});
                }
            }
            for (HotbarEntry& entry : cachedEntries) {
                entry.shortName = entry.definition->id.substr(0, 3);
                std::transform(entry.shortName.begin(), entry.shortName.end(),
                               entry.shortName.begin(), ::toupper);
            }
        }
        glm::vec4 bgEq = readCol("hotbarBgEquipped", {0.32f,0.32f,0.36f,0.95f});
        glm::vec4 bgNorm = readCol("hotbarBgNormal", {0.12f,0.12f,0.14f,0.92f});
        glm::vec4 borderEq = readTextCol("hotbarBorderEquipped", {1,1,1,1});
        glm::vec4 borderNorm = readTextCol("hotbarBorderNormal", {0.45f,0.45f,0.48f,1});
        glm::vec4 wepCol = readTextCol("hotbarWeaponColor", {0.55f,0.55f,0.58f,1});
        glm::vec4 wepColEq = readTextCol("hotbarWeaponColorEquipped", {1.0f,0.85f,0.35f,1});

        const int slotCount = static_cast<int>(cachedEntries.size());
        if (slotCount > 0) {
            float totalWidth = slotSize * slotCount + gap * (slotCount - 1);
            float x = uiScreenW() * 0.5f - totalWidth * 0.5f;
            float y = uiScreenH() - yOffset;
            for (const HotbarEntry& entry : cachedEntries) {
                const WeaponDefinition* slotDef = entry.definition;
                bool equipped = player.equippedSlot == slotDef->slot;
                float size = equipped ? slotSize * 1.2f : slotSize;
                float offset = (size - slotSize) * 0.5f;
                UIRect rect{x - offset, y - offset, size, size};
                uiDrawRect(rect, equipped ? bgEq : bgNorm, "hotbar-slot");
                uiDrawRectOutline(rect, equipped ? borderEq : borderNorm, "hotbar-border");
                uiDrawText(entry.label.c_str(), rect.x + 5, rect.y + 16, 0.30f, {1,1,1,1});
                uiDrawText(entry.shortName.c_str(), rect.x + 13, rect.y + 34, 0.20f,
                           equipped ? wepColEq : wepCol);
                x += slotSize + gap;
            }
        }
    }

    // ── Self-healthbar (world-space, reuses NPC healthbar style) ──
    { MIMITA_PERF_SCOPE("HUD::WorldOverlays");
    const bool worldHealthbarsVisible =
        HealthbarConfig::instance().worldHealthbarsVisible();
    if (worldHealthbarsVisible) {
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
            drawPlayerHealthbar(npc.body, camera, "npc-hp", "live_world",
                                enemyHealthbarForActor(npc.id));
        }
    }

    if (!gReplayExportRenderMode || ReplayExportUI::showChatBubbles()) {
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
    }

    // Typing indicators above players
    if (!gReplayExportRenderMode) {
        renderTypingIndicator(player, camera, true);
        if (!replayPlaybackActive)
        {
            for (const auto& kv : mpContext.remotePlayers)
                renderTypingIndicator(kv.second, camera, false);
        }
    }

    if (mpContext.active)
    {
        static uint64_t lastHealthbarLogMs = 0;
        const uint64_t healthbarNowMs = MimitaNet::nowMs();
        const bool logHealthbars =
            mpContext.showDebugOverlay &&
            healthbarNowMs - lastHealthbarLogMs >= 1000;

        if (worldHealthbarsVisible) {
        for (const auto& kv : mpContext.remotePlayers)
        {
            const HealthbarRenderResult result =
                drawPlayerHealthbar(
                    kv.second, camera, "network-player-hp", "live_world",
                    enemyHealthbarForActor(kv.first));
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
            drawPlayerHealthbar(kv.second, camera, "network-npc-hp", "live_world",
                                enemyHealthbarForActor(kv.first));
        }
        } // worldHealthbarsVisible

        if (logHealthbars)
            lastHealthbarLogMs = healthbarNowMs;
    }
    } // HUD::WorldOverlays
    {
        static float hpLogTimer = 0.0f; hpLogTimer -= 0.016f;
        if (hpLogTimer <= 0.0f && !npcSystem.all().empty()) {
            hpLogTimer = 1.0f;
            printf("[PLAYER HP FRAME] hp=%d/%d pos=(%.1f %.1f %.1f)\n",
                   player.currentHp, player.maxHp,
                   player.pos.x, player.pos.y, player.pos.z);
        }
    }
    if (DebugConfig::DEBUG_MOVEMENT_HUD &&
        (!gReplayExportRenderMode || ReplayExportUI::showDebugVis()))
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
    if (gpChatHistory && (!gReplayExportRenderMode || ReplayExportUI::showChat()))
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

    { MIMITA_PERF_SCOPE("HUD::Vehicle"); drawVehicleHud(); }
    { MIMITA_PERF_SCOPE("HUD::Interaction");
      InteractionSystem::instance().draw(camera); }
}
