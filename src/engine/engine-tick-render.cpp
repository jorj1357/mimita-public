// 08 19 2026, 09 41
/* purpose
* Renders the active world, replay actors, weapons, effects, and post-processing.
* Produces the completed world pass in the default framebuffer for later UI.
* Reports replay-export world-pass completion through centralized diagnostics.
* Does NOT draw the centralized gameplay HUD or capture encoder pixels.
* Does NOT advance replay export job state or encode video frames.
* Does NOT own menus, terminal rendering, or UI input.
*/
#include "engine/engine-tick-render.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <cstdint>
#include <memory>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "avatar/avatar.h"
#include "avatar/cosmetic-system.h"
#include "world/world.h"
#include "npc/npc.h"
#include "render/render-world.h"
#include "render/skybox.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-config.h"
#include "render/lighting-config.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "ragdoll/ragdoll.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/death-ghost.h"
#include "pobjects/persistent-physics.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "debug/debug-log.h"
#include "debug/transform-debug.h"
#include "network/badconn/badconn.h"
#include "network/multiplayer-context.h"
#include "config/networking-config.h"
#include "engine/engine-tick-net.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-factory.h"
#include "world/texture-store.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "devtools/dev-npc-selection.h"
#include "config/player-settings.h"
#include "game/duel.h"
#include "video/frame-pacer.h"

extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;

// Cinematic impact frame state for replay export
struct KillImpactFrame {
    bool active = false;
    int remainingTicks = 0;
    glm::vec3 victimPosition{0.0f};
    std::string victimId;
};
static KillImpactFrame gKillImpactFrame;
static std::unordered_map<std::string, bool> gActorPrevDead;
static std::unordered_map<std::string, std::string> gReplayActorAvatarNames;
static std::unordered_map<std::string, bool> gReplayActorPrevDead;

// Renders the replay scene's actors (and cinematic kill-impact overlay) into the
// currently bound framebuffer. Shared by the visible window path and the
// offscreen export FBO so the captured clip always matches what is displayed.
static void renderReplayActors(
    const ReplaySceneFrame& replayFrame,
    const Camera& cam,
    const glm::mat4& view,
    const glm::mat4& proj,
    Engine& engine,
    float dt,
    std::unordered_map<std::string, std::unique_ptr<Player>>& replayActorModels,
    std::unordered_map<std::string, WeaponViewModel>& replayWeaponModels)
{
    for (const ReplayActorState& actorState : replayFrame.actors) {
        std::unique_ptr<Player>& actor = replayActorModels[actorState.id];

        // Detect life transition: was dead last tick, alive this tick = new life
        bool& wasDead = gReplayActorPrevDead[actorState.id];
        const bool isNewLife = wasDead && !actorState.dead;
        const bool firstCreation = !actor;
        wasDead = actorState.dead;

        if (firstCreation) {
            // Create player without loading current account's character
            actor = std::make_unique<Player>(false);
            // Load the recorded character (not the current account's)
            if (!actorState.characterName.empty())
                actor->loadCharacter(actorState.characterName);
            else if (!actorState.modelPath.empty())
                actor->loadModel(actorState.modelPath.c_str());

            printf("[RPLX AVATAR] Replay player created\n");
            printf("[RPLX AVATAR] Loading avatar via AvatarSystem::applyToPlayer\n");
            printf("[RPLX AVATAR] Current avatar in system: %s\n",
                   AvatarSystem::instance().hasAvatar()
                       ? AvatarSystem::instance().currentName().c_str()
                       : "(none)");

            // Recorded avatar identity wins over the exporting account's avatar.
            bool avatarApplied = false;
            if (!actorState.avatarName.empty())
                avatarApplied = AvatarSystem::instance().loadAvatar(actorState.avatarName) &&
                    AvatarSystem::instance().applyToPlayer(*actor, true);
            else
                avatarApplied = AvatarSystem::instance().applyToPlayer(*actor);
            if (avatarApplied) {
                printf("[RPLX AVATAR] Avatar applied via gameplay avatar pipeline\n");
                printf("[RPLX AVATAR] Avatar initialization complete\n");
            } else {
                printf("[RPLX AVATAR] No avatar loaded in system, applying outfit texture directly\n");
                const std::string& outfitToUse =
                    !actorState.outfitPath.empty()
                        ? actorState.outfitPath
                        : REPLAY_PLAYER.outfitPath();
                if (!outfitToUse.empty()) {
                    GLuint tex = gTextures.getPath(outfitToUse);
                    if (tex) {
                        for (auto& mesh : actor->physicalBody.partMeshes)
                            for (auto& batch : mesh.batches)
                                batch.texture = tex;
                        actor->bodyPartMeshes = actor->physicalBody.partMeshes;
                    }
                }
            }
            printf("[REPLAY] loaded actor '%s' character=%s model=%s\n",
                   actorState.id.c_str(),
                   actorState.characterName.c_str(),
                   actorState.modelPath.c_str());
        }
        if (actor && isNewLife && !actorState.avatarName.empty()) {
            if (AvatarSystem::instance().loadAvatar(actorState.avatarName))
                AvatarSystem::instance().applyToPlayer(*actor, true);
            gReplayActorAvatarNames[actorState.id] = actorState.avatarName;
        }
        actor->username = actorState.name;
        actor->currentHp = actorState.health;
        actor->maxHp = actorState.maxHealth;
        actor->dead = actorState.dead;
        actor->sizeScale = actorState.sizeScale;
        actor->vel = actorState.velocity;
        actor->ground.onGround = actorState.grounded;
        actor->equippedWeaponId = actorState.weaponName;
        actor->applyReplayPose(
            actorState.position,
            actorState.rotation.z,
            actorState.bodyParts.data(),
            actorState.bodyPartCount);

        const bool hideFirstPersonActor =
            REPLAY_PLAYER.cameraController().mode() ==
                ReplayCameraMode::FirstPerson &&
            actorState.id == REPLAY_PLAYER.killerId();
        if (!hideFirstPersonActor) {
            const bool isImpactVictim = gKillImpactFrame.active &&
                actorState.id == gKillImpactFrame.victimId;
            if (isImpactVictim) {
                GLuint shader = engine.renderer->shaderProgram;
                glUniform1i(glGetUniformLocation(shader, "uUseColor"), 1);
                glUniform4f(glGetUniformLocation(shader, "uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
                actor->renderCurrentPose(shader, view, proj);
                CosmeticSystem::instance().renderCosmetics(*actor);
                glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
            } else {
                actor->renderCurrentPose(
                    engine.renderer->shaderProgram,
                    view, proj);
                CosmeticSystem::instance().renderCosmetics(*actor);
            }
        }

        const WeaponDefinition* definition =
            WeaponRegistry::instance().get(
                actorState.weaponName);
        if (definition && !actorState.weaponModelPath.empty()) {
            WeaponDefinition replayDefinition = *definition;
            replayDefinition.modelPath = actorState.weaponModelPath;
            actor->equippedSlot = replayDefinition.slot;
            const std::string weaponKey =
                actorState.id + ":" + replayDefinition.id;
            WeaponViewModel& viewModel =
                replayWeaponModels[weaponKey];
            viewModel.update(
                cam, *actor, dt, &replayDefinition, false,
                nullptr, true);

            if (gReplayExportVerbose &&
                (!viewModel.vao || viewModel.heldMesh.verts.empty())) {
                Debug::warn(Debug::Category::Replay,
                    "[REPLAY WEAPON] not drawable actor=%s weapon=%s path=%s vao=%u verts=%zu slot=%d\n",
                    actorState.id.c_str(), actorState.weaponName.c_str(),
                    actorState.weaponModelPath.c_str(), viewModel.vao,
                    viewModel.heldMesh.verts.size(), actor->equippedSlot);
            }
            viewModel.render(
                cam, *actor, replayDefinition.slot);
        } else if (gReplayExportVerbose && !actorState.weaponName.empty() &&
                   actorState.weaponName != "none") {
            Debug::warn(Debug::Category::Replay,
                "[REPLAY WEAPON] skipped actor=%s weapon=%s registry=%d recordedPath=%s\n",
                actorState.id.c_str(), actorState.weaponName.c_str(),
                definition ? 1 : 0, actorState.weaponModelPath.c_str());
        }
    }
    if (gKillImpactFrame.active) {
        // Red sphere at death position
        const auto& deCfg = HitEffects::config().deathEllipsoid;
        EffectPartSystem::instance().spawnDeathEllipsoid(
            gKillImpactFrame.victimPosition,
            glm::vec3(0.0f, 0.0f, 1.0f),
            deCfg.length, deCfg.radius, deCfg.lifetime);
        // Full-screen black overlay (impact frame)
        static GLuint impactVao = 0, impactVbo = 0;
        if (!impactVao) {
            float verts[] = { -1,-1,0, 3,-1,0, -1,3,0 };
            glGenVertexArrays(1, &impactVao);
            glGenBuffers(1, &impactVbo);
            glBindVertexArray(impactVao);
            glBindBuffer(GL_ARRAY_BUFFER, impactVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        }
        GLuint shader = engine.renderer->shaderProgram;
        glDisable(GL_DEPTH_TEST);
        glUseProgram(shader);
        glm::mat4 id(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &id[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &id[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &id[0][0]);
        glUniform1i(glGetUniformLocation(shader, "uUseColor"), 1);
        glUniform4f(glGetUniformLocation(shader, "uColor"), 0.0f, 0.0f, 0.0f, 0.6f);
        glBindVertexArray(impactVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);
        glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    }
}

void engineTickRender(Engine& engine, float dt, bool& worldPassRan)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& replayWeaponModels = REPLAY_WEAPON_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayPlayer = REPLAY_PLAYER;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();
    const bool isExporting = getReplayExportJob().state == ReplayExportJob::Capturing;
    bool replayRenderActive = replayPlaybackActive ||
        (isExporting && gReplayPlayer.totalTicks() > 0);
    if (isExporting) {
        replayRenderActive = true;
    }

    // During replay export: detect kills and apply cinematic impact frame
    if (isExporting && replayRenderActive) {
        if (gKillImpactFrame.active) {
            gKillImpactFrame.remainingTicks--;
            if (gKillImpactFrame.remainingTicks <= 0)
                gKillImpactFrame.active = false;
        }
        if (const ReplaySceneFrame* rf = gReplayPlayer.currentSceneFrame()) {
            for (const ReplayActorState& as : rf->actors) {
                bool& wasDead = gActorPrevDead[as.id];
                if (!wasDead && as.dead) {
                    gKillImpactFrame.active = true;
                    gKillImpactFrame.remainingTicks = 3;
                    gKillImpactFrame.victimPosition = as.position;
                    gKillImpactFrame.victimId = as.id;
                }
                wasDead = as.dead;
            }
        }
    } else {
        gKillImpactFrame.active = false;
        gKillImpactFrame.remainingTicks = 0;
        gKillImpactFrame.victimId.clear();
        gActorPrevDead.clear();
        gReplayActorPrevDead.clear();
        gReplayActorAvatarNames.clear();
    }

    // Update skybox animations and check for hot-reload
    gSkybox.pollReload();
    gSkybox.update(dt);

    { MIMITA_PERF_SCOPE("Rendering");
    diagRenderFrameBegin(dt);
    { MIMITA_PERF_SCOPE("Rendering::Shadows"); renderShadowMap(world, camera.pos); }
    glViewport(0, 0, engine.renderer->width, engine.renderer->height);
    PostFX::instance().bindFBO();
    diagRenderStage(1);
    // Skybox renders first (if loaded), otherwise fall back to mesh-based sky
    { MIMITA_PERF_SCOPE("Rendering::World");
    if (gSkybox.isEnabled()) {
        gSkybox.render(camera);
    } else {
        renderSky(world, camera);
    }
    renderWorld(world, camera);
    }
    PostFX::instance().consumeMagentaTest();
    diagRenderStage(2);
    {
        static uint64_t renderLogFrame = 0;
        if (gReplayExportVerbose &&
            (renderLogFrame++ % 60 == 0 || getReplayExportJob().state == ReplayExportJob::Capturing)) {
            Debug::log(Debug::Category::Replay, "[EXPORTTRACE] RENDER: replayRenderActive=%d hasSceneFrame=%d exportState=%d",
                   (int)replayRenderActive,
                   gReplayPlayer.currentSceneFrame() ? 1 : 0,
                   (int)getReplayExportJob().state);
        }
    }
    gReplayExportRenderMode = isReplayExportActive();

    if (gReplayExportVerbose && isExporting)
        Debug::logThrottled(Debug::Category::Replay, "replay-export-ui-world", 1.0f,
            "[replay-export-ui] frame=%u world pass begin renderMode=%d viewport=%dx%d\n",
            getReplayExportJob().capturedTicks, (int)gReplayExportRenderMode,
            engine.renderer->width, engine.renderer->height);
    if (replayRenderActive) {
        gReplayPlayer.pollPoseInvariant();
        const double tRender0 = replayExportNowSec();
        if (const ReplaySceneFrame* replayFrame =
                gReplayPlayer.currentSceneFrame()) {
            const glm::mat4 replayView = camera.getView();
            const glm::mat4 replayProj = camera.getProj(
                (float)engine.renderer->width,
                (float)engine.renderer->height);
            renderReplayActors(
                *replayFrame, camera, replayView, replayProj,
                engine, dt, replayActorModels, replayWeaponModels);
        }
        gExportFrameTimings.renderMs += (replayExportNowSec() - tRender0) * 1000.0;
    } else {
        { MIMITA_PERF_SCOPE("Rendering::Actors");
        if (player.spawnFlashTimer > 0.0f) {
            static GLuint spawnFlashVao = 0, spawnFlashVbo = 0;
            if (!spawnFlashVao) {
                float verts[] = { -1,-1,0, 3,-1,0, -1,3,0 };
                glGenVertexArrays(1, &spawnFlashVao);
                glGenBuffers(1, &spawnFlashVbo);
                glBindVertexArray(spawnFlashVao);
                glBindBuffer(GL_ARRAY_BUFFER, spawnFlashVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
            }
            GLuint shader = engine.renderer->shaderProgram;
            glDisable(GL_DEPTH_TEST);
            glUseProgram(shader);
            glm::mat4 id(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &id[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &id[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &id[0][0]);
            glUniform1i(glGetUniformLocation(shader, "uUseColor"), 1);
            glUniform4f(glGetUniformLocation(shader, "uColor"), 0.0f, 0.0f, 0.0f, 1.0f);
            glBindVertexArray(spawnFlashVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEnable(GL_DEPTH_TEST);
        }
        renderPlayer(player, camera);
        // Draw the local weapon now while the shared viewmodel still contains
        // the local transform calculated during WeaponSystem::update().
        // This prevents renderRemoteWeapon (below) from overwriting the transform
        // before the local weapon renders.
        if (!replayPlaybackActive)
            { Perf::ScopedTimer _wr("WeaponRender"); weapons.render(camera, player); }
        if (mpContext.active) {
            for (auto& kv : mpContext.remotePlayers) {
                renderNetworkPlayer(kv.second, camera, kv.first, false);
                weapons.renderRemoteWeapon(kv.first, kv.second, camera, dt);
            }
            for (auto& kv : mpContext.remoteNpcs) {
                renderNetworkPlayer(kv.second, camera, kv.first, false);
                weapons.renderRemoteWeapon(kv.first, kv.second, camera, dt);
            }
            MimitaNet::mpRenderNetworkProjectiles(mpContext, camera);
            // Render server position ghost if enabled
            engineRenderGhost(player, camera);
        }
        npcSystem.render(camera);
        if (!replayPlaybackActive) {
            for (const Npc& npc : npcSystem.all()) {
                if (npc.body.dead || npc.body.currentHp <= 0)
                    continue;
                weapons.renderRemoteWeapon(npc.id, npc.body, camera, dt);
            }
        }
        } // Rendering::Actors
    }
    diagRenderStage(3);
    {   static float rlogTimer = 0.0f; rlogTimer -= dt;
        if (rlogTimer <= 0.0f && replayPlaybackActive) {
            rlogTimer = 1.0f;
            const auto* rframe = gReplayPlayer.currentSceneFrame();
            printf("[REPLAY] cameraMode=%s freecam=%d paused=%d "
                   "viewingActor=%s tick=%u actorCount=%zu effectCount=%zu\n",
                   gReplayPlayer.cameraController().modeName(),
                   (int)(gReplayPlayer.cameraController().mode() == ReplayCameraMode::Freecam),
                   (int)gReplayPlayer.isPaused(),
                   rframe && !rframe->actors.empty() ? rframe->actors[0].name.c_str() : "none",
                   gReplayPlayer.currentTick(),
                   rframe ? rframe->actors.size() : 0,
                   gReplayPlayer.totalEffectCount());
        }
    }
    if (mpContext.active && !replayPlaybackActive)
    {
        static uint64_t lastReplicaRenderLogMs = 0;
        const uint64_t renderLogNow = MimitaNet::nowMs();
        if (renderLogNow - lastReplicaRenderLogMs >= 1000)
        {
            for (const auto& kv : mpContext.remotePlayers)
                printf("[CLIENT RENDER ENTITY] entityId=%u type=Player visible=%d mesh=%s "
                       "position=(%.2f,%.2f,%.2f)\n",
                       kv.first, (int)!kv.second.dead,
                       kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                       kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            for (const auto& kv : mpContext.remoteNpcs)
                printf("[CLIENT RENDER ENTITY] entityId=%u type=NPC visible=%d mesh=%s "
                       "position=(%.2f,%.2f,%.2f)\n",
                       kv.first, (int)!kv.second.dead,
                       kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                       kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            lastReplicaRenderLogMs = renderLogNow;
        }
    }
    if (!replayPlaybackActive) {
        { Perf::ScopedTimer _dr("EffectRender"); DeathSystem::instance().render(camera); }
        // [RAGDOLL RENDER REMOVED] — DeadBody render handles corpses
        // Local weapon render moved earlier (before renderRemoteWeapon) to prevent
        // shared WeaponViewModel transform from being overwritten by remote players.
    }
    diagRenderStage(4);

    if (gNetPresentationDebug && mpContext.active)
    {
        float debugY = 120.0f;
        for (const auto& kv : mpContext.remotePlayers)
        {
            const Player& rp = kv.second;
            auto it = mpContext.remotePlayerInterpolation.find(kv.first);
            const MimitaNet::EntityInterpolationState* interp =
                it != mpContext.remotePlayerInterpolation.end() ? &it->second : nullptr;

            char buf[256];
            snprintf(buf, sizeof(buf),
                "REMOTE id=%u  weapon=%s  hp=%d  dead=%d  ground=%d  "
                "dashSer=%u  aim=(%.2f,%.2f)",
                kv.first, rp.equippedWeaponId.c_str(),
                rp.currentHp, (int)rp.dead, (int)rp.ground.onGround,
                (unsigned)rp.networkLastDashSerial,
                rp.aimDirection.x, rp.aimDirection.y);
            uiDrawText(buf, 10.0f, debugY, 0.32f,
                rp.dead ? glm::vec4(1,0,0,1) : glm::vec4(0.3f,1,0.5f,1));
            debugY += 22.0f;

            if (interp)
            {
                snprintf(buf, sizeof(buf),
                    "  pos=(%.1f,%.1f,%.1f)  vel=(%.1f,%.1f,%.1f)  yaw=%.1f",
                    rp.pos.x, rp.pos.y, rp.pos.z,
                    rp.vel.x, rp.vel.y, rp.vel.z, rp.yaw);
                uiDrawText(buf, 10.0f, debugY, 0.28f, {0.6f,0.7f,0.9f,1});
                debugY += 20.0f;
            }
        }
    }

    if (mpContext.active)
    {
        const NetworkingConfigData& netCfg = NetworkingConfig::instance().data();
        if (netCfg.debug.showBufferSize || netCfg.debug.showInterpolatedPosition ||
            MimitaNet::gNetInterpDebug)
        {
            float debugY = 300.0f;
            char buf[256];
            snprintf(buf, sizeof(buf), "NETINTERP renderTick=%.1f clockStarted=%d delayMs=%.0f",
                     mpContext.interpolationRenderTick,
                     (int)mpContext.interpolationClockStarted,
                     NetworkingConfig::instance().effectiveRemoteInterpolationDelaySeconds() * 1000.0);
            uiDrawText(buf, 10.0f, debugY, 0.28f, {0.9f, 0.9f, 0.4f, 1.0f});
            debugY += 18.0f;
            for (const auto& kv : mpContext.remotePlayerInterpolation)
            {
                snprintf(buf, sizeof(buf), "  id=%u buffer=%zu newestTick=%u pos=(%.1f,%.1f,%.1f)",
                         kv.first, kv.second.buffer.size(),
                         kv.second.hasTarget ? kv.second.target.serverTick : 0u,
                         kv.second.hasTarget ? kv.second.target.position.x : 0.0f,
                         kv.second.hasTarget ? kv.second.target.position.y : 0.0f,
                         kv.second.hasTarget ? kv.second.target.position.z : 0.0f);
                uiDrawText(buf, 10.0f, debugY, 0.24f, {0.7f, 0.8f, 1.0f, 1.0f});
                debugY += 16.0f;
            }
        }
    }

    if (gNetDebugEntities && mpContext.active)
    {
        float debugY = 120.0f;
        auto drawEntityLine = [&](const char* label, uint32_t id,
            const Player& entity, const glm::vec4& color) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                "%s id=%u  hp=%d  dead=%d  weapon=%s  "
                "pos=(%.1f,%.1f,%.1f)  yaw=%.1f",
                label, id, entity.currentHp, (int)entity.dead,
                entity.equippedWeaponId.c_str(),
                entity.pos.x, entity.pos.y, entity.pos.z, entity.yaw);
            uiDrawText(buf, 10.0f, debugY, 0.28f, color);
            debugY += 18.0f;
        };

        for (const auto& kv : mpContext.remotePlayers)
            drawEntityLine("PLAYER", kv.first, kv.second,
                kv.second.dead ? glm::vec4(1,0,0,1) : glm::vec4(0.3f,1,0.5f,1));

        for (const auto& kv : mpContext.remoteNpcs)
            drawEntityLine("NPC", kv.first, kv.second,
                kv.second.dead ? glm::vec4(1,0.3f,0,1) : glm::vec4(0.2f,0.8f,1,1));
    }

    { MIMITA_PERF_SCOPE("Rendering::Particles"); EffectPartSystem::instance().render(camera); }
    { MIMITA_PERF_SCOPE("Rendering::Particles"); DeathGhostSystem::instance().render(camera); }
    { MIMITA_PERF_SCOPE("Rendering::Particles"); PersistentPhysicsSystem::instance().render(camera); }
    { MIMITA_PERF_SCOPE("Rendering::Decals"); HitEffects::renderHitBursts(camera); }
    { MIMITA_PERF_SCOPE("Rendering::DebugVis");
    DebugVis::flushTris(camera);
    DebugVis::flushWeaponLines(camera);
    }
    diagRenderStage(5);
    } // MIMITA_PERF_SCOPE Rendering

    PostFX::instance().unbindFBO();
    diagRenderStage(6);
    PostFX::instance().advanceTime(dt);
    // PostFX hot-reload gated by master hot-reload config
    { Perf::ScopedTimer _pfx("PostFX"); PostFX::instance().render(); }
    renderShadowMapOverlay(engine.renderer->width, engine.renderer->height);
    diagRenderStage(7);

    if (gReplayExportVerbose && isExporting)
        Debug::logThrottled(Debug::Category::Replay, "replay-export-ui-world-done", 1.0f,
            "[replay-export-ui] frame=%u world pass rendered\n",
            getReplayExportJob().capturedTicks);

    worldPassRan = true;

    if (gDebugVisEnabled) {
        npcSystem.drawDebug(camera);
        drawDebugStuff(player, camera, world);
    }

    if (mpContext.active && mpContext.showDebugOverlay)
    {
        const glm::vec4 predictedColor{0.1f, 1.0f, 0.25f, 1.0f};
        const glm::vec4 serverColor{1.0f, 0.15f, 0.1f, 1.0f};
        const glm::vec4 remoteColor{0.15f, 0.55f, 1.0f, 1.0f};
        const glm::vec4 npcColor{0.1f, 1.0f, 0.9f, 1.0f};
        const auto drawReplicaCapsule =
            [&camera](const Player& replica, const glm::vec4& color)
            {
                const Capsule capsule = replica.getCapsule();
                const float giantRadius = capsule.r * 1.8f;
                DebugVis::drawDiagnosticWireSphere(
                    camera, capsule.a, giantRadius, color);
                DebugVis::drawDiagnosticWireSphere(
                    camera, capsule.b, giantRadius, color);
                DebugVis::drawDiagnosticLine(
                    camera, capsule.a, capsule.b, color);
            };

        DebugVis::drawWireSphere(camera, player.pos, 0.72f, predictedColor);
        DebugVis::drawWorldLabel(
            player.pos + glm::vec3(0.0f, 0.0f, 2.0f),
            "LOCAL PREDICTED", predictedColor);
        if (mpContext.hasLocalServerPosition)
        {
            DebugVis::drawWireSphere(
                camera, mpContext.localServerPosition, 0.78f, serverColor);
            DebugVis::drawLine(
                camera, player.pos, mpContext.localServerPosition, serverColor);
            char serverLabel[128];
            snprintf(serverLabel, sizeof(serverLabel),
                     "SERVER id=%u error=%.2fm",
                     mpContext.localPlayerId,
                     glm::length(player.pos - mpContext.localServerPosition));
            DebugVis::drawWorldLabel(
                mpContext.localServerPosition + glm::vec3(0.0f, 0.0f, 2.3f),
                serverLabel, serverColor);
        }

        if (badconn::active())
        {
            const badconn::BadConnMetrics& bc = badconn::metrics();
            char badConnLabel[256];
            snprintf(badConnLabel, sizeof(badConnLabel),
                     "BADCONN %s '%s' delay=%llu drop=%llu reorder=%llu "
                     "blackout=%d%s",
                     badconn::activePresetId().c_str(),
                     badconn::activePresetName().c_str(),
                     (unsigned long long)bc.packetsDelayed,
                     (unsigned long long)bc.packetsDropped,
                     (unsigned long long)bc.packetsReordered,
                     (int)bc.blackoutActive,
                     bc.blackoutActive ? " ACTIVE" : "");
            DebugVis::drawWorldLabel(
                player.pos + glm::vec3(0.0f, 0.0f, 3.4f),
                badConnLabel, glm::vec4(1.0f, 0.4f, 0.2f, 1.0f));
        }

        for (const auto& kv : mpContext.remotePlayers)
        {
            drawReplicaCapsule(kv.second, remoteColor);
            bool usedHeadTransform = false;
            const glm::vec3 healthbarAnchor =
                playerHealthbarAnchor(
                    kv.second, &usedHeadTransform);
            DebugVis::drawDiagnosticWireSphere(
                camera, healthbarAnchor, 0.16f,
                usedHeadTransform
                    ? glm::vec4(1.0f, 0.75f, 0.1f, 1.0f)
                    : glm::vec4(1.0f, 0.2f, 0.8f, 1.0f));
            const auto interpolation = mpContext.remotePlayerInterpolation.find(kv.first);
            if (interpolation != mpContext.remotePlayerInterpolation.end() &&
                interpolation->second.hasTarget)
            {
                DebugVis::drawDiagnosticWireSphere(
                    camera, interpolation->second.target.position, 0.36f, serverColor);
                DebugVis::drawDiagnosticLine(
                    camera, kv.second.pos, interpolation->second.target.position, serverColor);
            }
            char label[128];
            char interpBuf[32];
            if (interpolation != mpContext.remotePlayerInterpolation.end() &&
                interpolation->second.hasTarget)
            {
                if (NetworkingConfig::instance().data().remotePlayers.directRender)
                    snprintf(interpBuf, sizeof(interpBuf), "direct");
                else
                    snprintf(interpBuf, sizeof(interpBuf), "%.0fms",
                             (double)(MimitaNet::nowMs() -
                                      interpolation->second.target.receivedMs));
            }
            else
            {
                snprintf(interpBuf, sizeof(interpBuf), "n/a");
            }
            snprintf(
                label, sizeof(label),
                "REMOTE PLAYER id=%u HP=%d anchor=%s interp=%s",
                kv.first, kv.second.currentHp,
                usedHeadTransform ? "head" : "fallback", interpBuf);
            DebugVis::drawDiagnosticWorldLabel(
                healthbarAnchor + glm::vec3(0.0f, 0.0f, 0.25f),
                label, remoteColor);
        }
        for (const auto& kv : mpContext.remoteNpcs)
        {
            drawReplicaCapsule(kv.second, npcColor);
            const auto interpolation = mpContext.remoteNpcInterpolation.find(kv.first);
            if (interpolation != mpContext.remoteNpcInterpolation.end() &&
                interpolation->second.hasTarget)
            {
                DebugVis::drawDiagnosticWireSphere(
                    camera, interpolation->second.target.position, 0.36f, serverColor);
                DebugVis::drawDiagnosticLine(
                    camera, kv.second.pos, interpolation->second.target.position, serverColor);
            }
            char label[128];
            char interpBuf[32];
            if (interpolation != mpContext.remoteNpcInterpolation.end() &&
                interpolation->second.hasTarget)
            {
                if (NetworkingConfig::instance().data().remotePlayers.directRender)
                    snprintf(interpBuf, sizeof(interpBuf), "direct");
                else
                    snprintf(interpBuf, sizeof(interpBuf), "%.0fms",
                             (double)(MimitaNet::nowMs() -
                                      interpolation->second.target.receivedMs));
            }
            else
            {
                snprintf(interpBuf, sizeof(interpBuf), "n/a");
            }
            snprintf(label, sizeof(label), "NPC id=%u HP=%d interp=%s",
                     kv.first, kv.second.currentHp, interpBuf);
            DebugVis::drawDiagnosticWorldLabel(
                kv.second.pos + glm::vec3(0.0f, 0.0f, 2.0f),
                label, npcColor);
        }
    }

    if (DebugVis::enabled()) {
        NpcSelectionManager::instance().drawSelection(npcSystem, camera);
    }
}


