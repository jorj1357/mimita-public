// C:\important\quiet\n\mimita-priv-v7\src\render\render-player.cpp
// feb 10 2026
/**
 * purpose
 * rned erplauer
 * render plauer
 * wrapper
 * small wrapper so that the game faster i 
 * think
 */

#include "render-player.h"
#include "entities/player.h"

#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>
#include "camera.h"
#include "renderer/renderer.h"
#include "terminal/terminal-state.h"
#include "avatar/cosmetic-system.h"
#include "config/player-settings.h"
#include "config/player-visuals-config.h"
#include "render/dynamic-light.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/structured-log.h"
#include "perf/perf-spike.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <glad/glad.h>

extern Renderer* gRenderer;

namespace {

struct PlayerTrailPoint {
    glm::vec3 position{0.0f};
    std::chrono::steady_clock::time_point time;
};
struct PlayerTrailHistory {
    std::deque<PlayerTrailPoint> points;
    std::chrono::steady_clock::time_point lastSample{};
    std::chrono::steady_clock::time_point lastSeen{};
};

struct TrailVertex {
    glm::vec3 position;
    glm::vec4 color;
};

std::unordered_map<uint32_t, PlayerTrailHistory> gTrailHistories;
std::vector<TrailVertex> gTrailBatch;

void appendPlayerTrail(const Player& player, const Camera& cam, uint32_t entityId,
                       const PlayerTrailSettings& settings)
{
    if (entityId == 0) return;
    using Clock = std::chrono::steady_clock;
    struct TrailDiagState {
        std::string state;
        std::chrono::steady_clock::time_point emitted{};
    };
    auto emitTrailState = [&](const char* state, float speed, size_t points) {
        static std::unordered_map<uint32_t, TrailDiagState> sLastTrailState;
        const auto now = Clock::now();
        auto& last = sLastTrailState[entityId];
        if (last.state == state) return;
        // Trail point collection naturally oscillates while the actor moves.
        // Keep the first useful evidence and rate-limit repeats instead of
        // writing a journal line whenever the history drops below two points.
        const auto elapsed = last.emitted.time_since_epoch().count() == 0
            ? std::chrono::seconds(999) : now - last.emitted;
        if (last.state == "drawn" && std::string(state) == "collecting" &&
            elapsed < std::chrono::seconds(10))
            return;
        if (std::string(state) == "drawn" && elapsed < std::chrono::seconds(3))
            return;
        last.state = state;
        last.emitted = now;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "player_visuals.trail", std::to_string(entityId), state, 0,
            nlohmann::json{
                {"actor_id", entityId},
                {"enabled", settings.enabled},
                {"state", state},
                {"speed", speed},
                {"min_speed", settings.minSpeed},
                {"points", points},
                {"max_samples", settings.maxSamples},
                {"width", settings.width},
                {"alpha", settings.alpha},
                {"lifetime_seconds", settings.lifetimeSeconds},
                {"dead", player.dead}},
            __FILE__, __LINE__, __FUNCTION__);
    };
    if (!settings.enabled || player.dead) {
        gTrailHistories.erase(entityId);
        emitTrailState(!settings.enabled ? "disabled" : "dead", 0.0f, 0);
        return;
    }
    const auto now = Clock::now();
    auto& history = gTrailHistories[entityId];
    history.lastSeen = now;
    const glm::vec3 trailPosition = player.pos + glm::vec3(0.0f, 0.0f,
        std::max(0.25f, player.sizeScale * 0.9f));
    const float speed = glm::length(player.vel);
    if (speed < settings.minSpeed)
        emitTrailState("below_min_speed", speed, history.points.size());
    const auto interval = std::chrono::duration<float>(settings.sampleIntervalSeconds);
    if (speed >= settings.minSpeed &&
        (history.lastSample.time_since_epoch().count() == 0 || now - history.lastSample >= interval)) {
        if (!history.points.empty()) {
            const float elapsed = std::chrono::duration<float>(now - history.points.back().time).count();
            const float maxStep = std::max(5.0f, speed * elapsed * 2.0f + 1.0f);
            if (glm::distance(history.points.back().position, trailPosition) > maxStep)
                history.points.clear();
        }
        history.points.push_back({trailPosition, now});
        history.lastSample = now;
    }
    const auto lifetime = std::chrono::duration<float>(settings.lifetimeSeconds);
    while (!history.points.empty() && now - history.points.front().time > lifetime)
        history.points.pop_front();
    while (history.points.size() > static_cast<size_t>(settings.maxSamples))
        history.points.pop_front();

    if (history.points.size() < 2) {
        emitTrailState("collecting", speed, history.points.size());
        return;
    }

    const glm::vec3 rgb = settings.color / 255.0f;
    const float halfWidth = settings.width * std::max(0.25f, player.sizeScale) * 0.5f;
    for (size_t i = 1; i < history.points.size(); ++i) {
        const float age = std::chrono::duration<float>(now - history.points[i].time).count();
        const float alpha = settings.alpha * std::clamp(1.0f - age / settings.lifetimeSeconds, 0.0f, 1.0f);
        if (alpha <= 0.01f) continue;
        const glm::vec4 color(rgb, alpha);
        const glm::vec3 start = history.points[i - 1].position;
        const glm::vec3 end = history.points[i].position;
        const glm::vec3 segment = end - start;
        if (glm::dot(segment, segment) <= 0.000001f) continue;
        const glm::vec3 direction = glm::normalize(segment);
        // Keep the ribbon's front face toward the camera. The previous order
        // faced away from the camera, so inherited back-face culling could make
        // trails disappear as the view angle changed.
        glm::vec3 side = glm::cross(cam.front, direction);
        if (glm::dot(side, side) <= 0.000001f) side = glm::cross(cam.up, direction);
        if (glm::dot(side, side) <= 0.000001f) side = cam.right;
        side = glm::normalize(side) * halfWidth;
        const glm::vec3 startLeft = start - side;
        const glm::vec3 startRight = start + side;
        const glm::vec3 endLeft = end - side;
        const glm::vec3 endRight = end + side;
        gTrailBatch.push_back({startLeft, color});
        gTrailBatch.push_back({startRight, color});
        gTrailBatch.push_back({endRight, color});
        gTrailBatch.push_back({startLeft, color});
        gTrailBatch.push_back({endRight, color});
        gTrailBatch.push_back({endLeft, color});
    }
    emitTrailState("batched", speed, history.points.size());

    static auto lastPrune = Clock::now();
    if (now - lastPrune >= std::chrono::seconds(1)) {
        lastPrune = now;
        for (auto it = gTrailHistories.begin(); it != gTrailHistories.end();) {
            if (now - it->second.lastSeen > std::chrono::seconds(5)) it = gTrailHistories.erase(it);
            else ++it;
        }
    }
}

void logRagdollRenderStage(const Player& player, uint32_t stage,
                           const char* event, const char* reason,
                           const nlohmann::json& fields)
{
    if (!player.dead || !player.ragdollModeActive) return;
    static std::unordered_map<const Player*, uint32_t> loggedStages;
    uint32_t& seen = loggedStages[&player];
    if ((seen & stage) != 0) return;
    seen |= stage;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Ragdoll, StructuredLevel::Important,
        event, player.username, reason,
        static_cast<uint32_t>(player.movementSimulationTick),
        fields, __FILE__, __LINE__, __FUNCTION__);
}

void logPlayerRenderState(const Player& player, uint32_t entityId, bool isLocal,
                          const char* renderGate)
{
    if (entityId == 0 || !StructuredLogger::instance().shouldLog(
            StructuredCategory::Rendering, StructuredLevel::Important)) return;
    static std::unordered_map<uint64_t, std::string> lastStateByActor;
    const uint64_t key = (static_cast<uint64_t>(entityId) << 1) | (isLocal ? 1u : 0u);
    const bool avatarLoadPending = !player.avatarName().empty() &&
        AvatarSystem::instance().isAvatarLoadPending(player.avatarName());
    nlohmann::json fields{{"actor_id", entityId}, {"is_local", isLocal},
        {"dead", player.dead}, {"health", player.currentHp},
        {"spawn_generation", player.spawnGeneration}, {"render_gate", renderGate},
        {"avatar", player.avatarName()}, {"avatar_load_pending", avatarLoadPending},
        {"avatar_bound", player.avatarInstance != nullptr},
        {"model_loaded", player.modelLoaded}, {"lazy_load_requested", player.mLazyLoadRequested},
        {"body_mesh_count", player.physicalBody.partMeshes.size()},
        {"fallback_batch_count", player.renderMesh.batches.size()}};
    const std::string state = fields.dump();
    auto previous = lastStateByActor.find(key);
    if (previous != lastStateByActor.end() && previous->second == state) return;
    if (previous == lastStateByActor.end() && lastStateByActor.size() >= 256)
        lastStateByActor.erase(lastStateByActor.begin());
    lastStateByActor[key] = state;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "client.actor.render_state", std::to_string(entityId),
        "player render gate or avatar readiness changed",
        static_cast<uint32_t>(player.movementSimulationTick), fields,
        __FILE__, __LINE__, __FUNCTION__);
}
void renderPlayerInternal(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal,
    int localTeam,
    int actorTeam)
{
    Player& p = const_cast<Player&>(player);
    AvatarSystem& av = AvatarSystem::instance();

    // The local player must adopt the saved avatar identity on first render,
    // otherwise spawn leaves it as the default fallback model (the "green
    // capsule") until an avatar is loaded manually.
    if (isLocal && p.avatarName().empty()) {
        const std::string& savedAvatar = GetPlayerSettings().avatarName;
        // Never leave the player as the default fallback model: fall back to
        // the jason avatar when no avatar has been chosen.
        p.setAvatarName(savedAvatar.empty() ? "jason" : savedAvatar);
    }

    // NPCs carry their own per-life avatar identity. Bind it before queuing
    // the shared async model load; this keeps avatar work out of spawn/update.
    if (!p.avatarName().empty() && !av.isAvatarLoadPending(p.avatarName()) &&
        (!p.avatarInstance || p.avatarInstance->name != p.avatarName()))
        av.applyAvatarToPlayer(p, p.avatarName());

    // Kick async load once per player
    if (!p.modelLoaded && !p.mLazyLoadRequested &&
        (av.hasAvatar() || p.avatarInstance)) {
        p.mLazyLoadRequested = true;
        if (!p.avatarInstance)
            av.requestModelLoad(p);
    }
    p.finalizeModelIfReady();
    av.finalizeAtlasIfReady(p);
    const char* renderGate = !gRenderer ? "renderer_unavailable"
        : (player.dead && isLocal ? "local_player_dead"
        : (player.dead && !player.deathAnim.active && !player.ragdollModeActive &&
           !player.authoredDeathActive
            ? "dead_without_death_presentation" : "passed"));
    logPlayerRenderState(player, networkEntityId, isLocal, renderGate);

    static std::unordered_map<uint32_t, uint64_t> lastLogMs;
    const uint64_t nowMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool logDraw =
        networkEntityId != 0 && nowMs - lastLogMs[networkEntityId] >= 1000;

    if (!gRenderer) {
        logRagdollRenderStage(
            player, 1u, "ragdoll.render.no_renderer", "renderer unavailable",
            {{"actor_id", player.username}, {"position", {player.pos.x, player.pos.y, player.pos.z}}});
        printf("[RENDER] renderer missing\n");
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=no-renderer\n",
                   networkEntityId, (int)isLocal);
        return;
    }

    // Remote dead bodies are still drawn while their fall-over death animation
    // plays (frozen in place, rotating over totalTicks); only once it completes
    // (deathAnim.active false) is the body hidden instantly. The local player's
    // own body is never drawn, dead or alive.
    if (player.dead && isLocal) {
        logRagdollRenderStage(
            player, 2u, "ragdoll.render.skipped", "local dead-player render gate",
            {{"actor_id", player.username}, {"gate", "local_dead"},
             {"position", {player.pos.x, player.pos.y, player.pos.z}}});
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=local-dead worldPos=(%.2f,%.2f,%.2f)\n",
                   networkEntityId, (int)isLocal,
                   player.pos.x, player.pos.y, player.pos.z);
        if (logDraw)
            lastLogMs[networkEntityId] = nowMs;
        return;
    }
    // Dead bodies without a scripted fall-over are hidden, except ragdoll
    // corpses which render their physically simulated pose.
    if (player.dead && !player.deathAnim.active && !player.ragdollModeActive &&
        !player.authoredDeathActive) {
        logRagdollRenderStage(
            player, 4u, "ragdoll.render.skipped", "dead-player render gate",
            {{"actor_id", player.username}, {"gate", "dead_without_ragdoll"},
             {"position", {player.pos.x, player.pos.y, player.pos.z}}});
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=dead worldPos=(%.2f,%.2f,%.2f)\n",
                   networkEntityId, (int)isLocal,
                   player.pos.x, player.pos.y, player.pos.z);
        if (logDraw)
            lastLogMs[networkEntityId] = nowMs;
        return;
    }

    if (player.dead && player.ragdollModeActive)
        logRagdollRenderStage(
            player, 8u, "ragdoll.render.player_entry", "passed player render gates",
            {{"actor_id", player.username}, {"shader_program", gRenderer->shaderProgram},
             {"model_loaded", player.modelLoaded},
             {"physical_part_count", player.physicalBody.parts.size()},
             {"physical_mesh_count", player.physicalBody.partMeshes.size()},
             {"render_mesh_vertex_count", player.renderMesh.verts.size()},
             {"render_mesh_batch_count", player.renderMesh.batches.size()},
             {"dead", player.dead}, {"ragdoll_mode_active", player.ragdollModeActive},
             {"position", {player.pos.x, player.pos.y, player.pos.z}}});

    // printf("[RENDER] shaderProgram=%u\n", gRenderer->shaderProgram);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->sceneWidth, (float)gRenderer->sceneHeight);

    if (logDraw)
    {
        printf("[REMOTE PLAYER RENDER] entityId=%u modelLoaded=%d meshCount=%zu visible=1 "
               "position=(%.2f,%.2f,%.2f)\n",
               networkEntityId, (int)player.modelLoaded,
               player.physicalBody.partMeshes.size(),
               player.pos.x, player.pos.y, player.pos.z);
        printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=1 mesh=%s "
               "bodyParts=%zu fallbackBatches=%zu worldPos=(%.2f,%.2f,%.2f)\n",
               networkEntityId, (int)isLocal,
               player.modelLoaded ? "player-glb" : "fallback-capsule",
               player.physicalBody.partMeshes.size(),
               player.renderMesh.batches.size(),
               player.pos.x, player.pos.y, player.pos.z);
        lastLogMs[networkEntityId] = nowMs;
    }

    // Apply ghost rendering for server_showghost
    if (player.renderGhost)
    {
        GLuint prog = gRenderer->shaderProgram;
        GLint colorLoc = glGetUniformLocation(prog, "uColor");
        if (colorLoc >= 0)
        {
            // Semi-transparent dark teal tint for ghost
            glUniform4f(colorLoc, 0.0f, 0.5f, 0.5f, 0.4f);
        }
    }

    const PlayerVisualsData& visuals = PlayerVisualsConfig::instance().data();
    const PlayerVisualsConfig& visualsConfig = PlayerVisualsConfig::instance();
    const PlayerOutlineSettings* outline = &visuals.self;
    const PlayerCapsuleSettings* capsule = &visuals.selfCapsule;
    const PlayerWireframeSettings* wireframe = &visuals.selfWireframe;
    const PlayerContrastSettings* contrast = &visuals.selfContrast;
    const PlayerTrailSettings* trail = nullptr;
    const PlayerPointLightSettings* pointLight = nullptr;
    int resolvedActorTeam = -1;
    bool teammate = false;
    if (!isLocal)
    {
        const bool modeOwnsTeamIdentity =
            visualsConfig.hasPlayerOutlinesOverride();
        resolvedActorTeam = actorTeam >= 0 ? actorTeam
            : (modeOwnsTeamIdentity ? -1 : player.matchTeam);
        teammate = localTeam >= 0 && resolvedActorTeam >= 0 && localTeam == resolvedActorTeam;
        outline = teammate ? &visuals.teammate : &visuals.enemy;
        capsule = teammate ? &visuals.teammateCapsule : &visuals.enemyCapsule;
        wireframe = teammate ? &visuals.teammateWireframe : &visuals.enemyWireframe;
        contrast = teammate ? &visuals.teammateContrast : &visuals.enemyContrast;
        trail = teammate ? &visuals.teammateTrail : &visuals.enemyTrail;
        pointLight = teammate ? &visuals.teammatePointLight : &visuals.enemyPointLight;
    }
    PlayerTrailSettings effectiveTrailSettings;
    if (trail) {
        effectiveTrailSettings = visualsConfig.effectiveTrail(teammate, *trail);
        trail = &effectiveTrailSettings;
    }
    const bool teamIdentityKnown = isLocal ||
        (localTeam >= 0 && resolvedActorTeam >= 0);
    // Do not guess an unknown actor is hostile for a gamemode-forced outline.
    // Configured layers remain independent; only this forced outline waits for
    // authoritative team identity.
    const bool modeForcesTeamOutline = !isLocal && teamIdentityKnown &&
        visualsConfig.hasPlayerOutlinesOverride() && visualsConfig.playerOutlinesEnabled() &&
        (!teammate || visualsConfig.friendlyOutlinesEnabled());
    const bool modeForcesTeamWireframe = !isLocal && teamIdentityKnown &&
        visualsConfig.hasPlayerWireframesOverride() && visualsConfig.playerWireframesEnabled() &&
        (!teammate || visualsConfig.friendlyWireframesEnabled());

    const bool visualLayersAllowed = !visualsConfig.hasPlayerOutlinesOverride();
    const bool outlineAllowed = visualLayersAllowed || modeForcesTeamOutline ||
        (isLocal && visualsConfig.hasPlayerOutlinesOverride() &&
         visualsConfig.playerOutlinesEnabled() && visualsConfig.friendlyOutlinesEnabled());
    if (!isLocal && pointLight) {
        const glm::vec3 lightPosition = player.pos + glm::vec3(0.0f, 0.0f, pointLight->heightOffset);
        DynamicLightManager::instance().updatePlayerVisualLight(
            networkEntityId, pointLight->enabled && !player.dead, lightPosition,
            pointLight->color / 255.0f, pointLight->intensity, pointLight->radius);
    }

    glm::vec3 playerTint(1.0f);
    if (contrast->enabled) {
        const glm::vec3 tintColor = contrast->tint / 255.0f;
        playerTint = glm::mix(glm::vec3(1.0f), tintColor, contrast->tintStrength) * contrast->brightness;
        playerTint = glm::clamp(playerTint, glm::vec3(0.0f), glm::vec3(4.0f));
    }

    // One-time per network actor: prove that the render path is about to submit
    // real geometry, not only that a model name was loaded. This separates a
    // replication/spawn problem from a body-draw or tint problem.
    {
        static std::unordered_set<uint32_t> drawEvidenceLogged;
        if (networkEntityId != 0 && !isLocal && player.modelLoaded &&
            drawEvidenceLogged.insert(networkEntityId).second) {
            uint32_t validParts = 0;
            uint32_t validBatches = 0;
            for (size_t i = 0; i < player.physicalBody.parts.size() &&
                               i < player.physicalBody.partMeshes.size(); ++i) {
                const PhysicalBodyPart& part = player.physicalBody.parts[i];
                const Mesh& mesh = player.physicalBody.partMeshes[i];
                if (part.nodeIndex < 0 ||
                    part.nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size() ||
                    mesh.verts.empty())
                    continue;
                ++validParts;
                validBatches += (uint32_t)mesh.batches.size();
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "client.actor.draw-submission", std::to_string(networkEntityId),
                "remote actor body submission", 0,
                {{"actor_id", networkEntityId},
                 {"is_local", isLocal},
                 {"alive", !player.dead},
                 {"model_loaded", player.modelLoaded},
                 {"avatar", player.avatarName()},
                 {"part_count", player.physicalBody.parts.size()},
                 {"valid_parts", validParts},
                 {"valid_batches", validBatches},
                 {"fallback_batches", player.renderMesh.batches.size()},
                 {"tint", {playerTint.r, playerTint.g, playerTint.b}},
                 {"position", {player.pos.x, player.pos.y, player.pos.z}},
                 {"camera_distance", glm::length(player.pos - cam.pos)}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    const bool hideHead = isLocal && !cam.thirdPerson;
    { MIMITA_PERF_SCOPE("Rendering::Actors::PlayerBody");
      player.render(gRenderer->shaderProgram, view, proj, hideHead, playerTint); }
    auto drawTrail = [&]() {
        if (trail && !isLocal) appendPlayerTrail(player, cam, networkEntityId, *trail);
    };

    const bool outlineEnabled = modeForcesTeamOutline || outline->enabled;
    const bool friendlyActor = isLocal || teammate;
    const float outlineAlpha = !friendlyActor && visualsConfig.hasEnemyOutlineAlphaOverride()
        ? visualsConfig.effectiveEnemyOutlineAlpha(outline->alpha) : outline->alpha;
    auto drawOutline = [&]() {
    const bool drawable = outlineAllowed && outlineEnabled &&
        outline->thickness > 0.0f && outlineAlpha != 0.0f &&
        !(player.dead && outline->disappearOnDeath);
    // The mode-authored outline must be visible on the server-client path. The
    // old path passed 0.0 world thickness into the shader, which made the
    // expanded shell identical to the body and effectively invisible. Use a
    // small world-space shell scaled from the authored thickness, and render
    // the expanded back faces instead of a driver-dependent 1 px line.
    const float worldThickness = std::clamp(
        outline->thickness * 0.015f, 0.01f, 0.15f);
    {
        static std::unordered_map<uint32_t, std::string> sLastOutlineState;
        if (!isLocal && networkEntityId != 0 &&
            visualsConfig.hasPlayerOutlinesOverride()) {
            const std::string state = drawable
                ? (std::string("drawn:") + std::to_string(outline->thickness))
                : (std::string("skipped:") +
                   (!teamIdentityKnown ? "team_identity_unknown" :
                    !outlineAllowed ? "layers_disabled" :
                    !outlineEnabled ? "mode_layer_off" :
                    outline->thickness <= 0.0f ? "zero_thickness" :
                    outlineAlpha == 0.0f ? "zero_alpha" :
                    player.dead && outline->disappearOnDeath ? "dead" :
                    "unknown"));
            auto it = sLastOutlineState.find(networkEntityId);
            if (it == sLastOutlineState.end() || it->second != state) {
                sLastOutlineState[networkEntityId] = state;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "player_visuals.outline", std::to_string(networkEntityId),
                    drawable ? "mode_outline_drawn" : state.c_str(), 0,
                    nlohmann::json{
                        {"actor_id", networkEntityId},
                        {"is_local", isLocal},
                        {"teammate", teammate},
                        {"team_identity_known", teamIdentityKnown},
                        {"local_team", localTeam},
                        {"actor_team", resolvedActorTeam},
                        {"model_loaded", player.modelLoaded},
                        {"mode_forced", modeForcesTeamOutline},
                        {"outline_enabled", outlineEnabled},
                        {"outline_allowed", outlineAllowed},
                        {"thickness", outline->thickness},
                        {"world_thickness", worldThickness},
                        {"alpha", outlineAlpha},
                        {"visible_through_walls", outline->visibleThroughWalls},
                        {"drawn", drawable},
                        {"reason", state}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
    }
    if (drawable) {
        const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWas = glIsEnabled(GL_BLEND);
        const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        GLint depthFuncWas = GL_LESS;
        GLint cullModeWas = GL_BACK;
        glGetIntegerv(GL_DEPTH_FUNC, &depthFuncWas);
        if (cullWas) glGetIntegerv(GL_CULL_FACE_MODE, &cullModeWas);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (outline->visibleThroughWalls) glDisable(GL_DEPTH_TEST); else glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        const glm::vec3 outlineRgb = PlayerVisualsConfig::instance().effectiveOutlineColor(friendlyActor, outline->color);
        const glm::vec4 color(outlineRgb.r / 255.0f, outlineRgb.g / 255.0f,
                              outlineRgb.b / 255.0f, outlineAlpha);
        { MIMITA_PERF_SCOPE("Rendering::Actors::PlayerOutline");
          player.renderCurrentPose(gRenderer->shaderProgram, view, proj, true, hideHead,
                                   true, worldThickness, color); }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthMask(GL_TRUE);
        glDepthFunc(depthFuncWas);
        if (cullWas) { glEnable(GL_CULL_FACE); glCullFace(cullModeWas); }
        else glDisable(GL_CULL_FACE);
        if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (blendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        Debug::logThrottled(Debug::Category::Render, "player_visuals_outline", 1.0f,
            "[PLAYER VISUALS] outline entity=%u category=%s throughWalls=%d thickness=%.2f world=%.3f",
            networkEntityId, isLocal ? "self" : (teammate ? "teammate" : "enemy"),
            outline->visibleThroughWalls ? 1 : 0, outline->thickness, worldThickness);
    }
    };

    auto drawCapsule = [&]() {
    if (visualLayersAllowed && capsule->enabled && capsule->alpha != 0.0f &&
        !(player.dead && outline->disappearOnDeath)) {
        const Capsule c = player.getCapsule();
        const glm::vec4 color(capsule->color.r / 255.0f, capsule->color.g / 255.0f,
                              capsule->color.b / 255.0f, capsule->alpha);
        const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        GLboolean depthWriteWas = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWas);
        const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        GLint cullModeWas = GL_BACK;
        if (cullWas) glGetIntegerv(GL_CULL_FACE_MODE, &cullModeWas);
        if (capsule->visibleThroughWalls || !capsule->depthTest) glDisable(GL_DEPTH_TEST);
        else glEnable(GL_DEPTH_TEST);
        if (capsule->frontFaceCull || capsule->backFaceCull) {
            glEnable(GL_CULL_FACE);
            glCullFace(capsule->frontFaceCull && capsule->backFaceCull ? GL_FRONT_AND_BACK
                : (capsule->frontFaceCull ? GL_FRONT : GL_BACK));
        } else glDisable(GL_CULL_FACE);
        glDepthMask(capsule->depthWrite ? GL_TRUE : GL_FALSE);
        const glm::vec3 axis = c.b - c.a;
        const float length = glm::length(axis);
        const glm::vec3 center = (c.a + c.b) * 0.5f;
        if (length > 0.001f)
            DebugVis::drawFilledCylinder(cam, center, axis, c.r * capsule->scale, length, color);
        DebugVis::drawFilledSphere(cam, c.a, c.r * capsule->scale, color);
        DebugVis::drawFilledSphere(cam, c.b, c.r * capsule->scale, color);
        glDepthMask(depthWriteWas);
        if (cullWas) { glEnable(GL_CULL_FACE); glCullFace(cullModeWas); }
        else glDisable(GL_CULL_FACE);
        if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
    };

    auto drawWireframe = [&]() {
    if ((visualLayersAllowed || modeForcesTeamWireframe) &&
        (modeForcesTeamWireframe || wireframe->enabled) && wireframe->alpha != 0.0f &&
        !(player.dead && wireframe->disappearOnDeath) &&
        player.modelLoaded && !player.avatarName().empty()) {
        // Reject a stale/exploded pose rather than drawing a giant floating
        // outline. Normal avatars, including very large monsters, stay well
        // inside this bound; a failed avatar transition can produce part
        // transforms hundreds of metres from the actor.
        const float saneRadius = std::max(64.0f, std::abs(player.sizeScale) * 64.0f);
        bool boundsSane = true;
        for (const PhysicalBodyPart& part : player.physicalBody.parts) {
            const glm::vec3 offset = glm::vec3(part.worldTransform[3]) - player.pos;
            if (!std::isfinite(offset.x) || !std::isfinite(offset.y) ||
                !std::isfinite(offset.z) || glm::length(offset) > saneRadius) {
                boundsSane = false;
                break;
            }
        }
        if (!boundsSane) {
            static uint64_t lastWireframeRejectMs = 0;
            const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (now - lastWireframeRejectMs >= 1000) {
                lastWireframeRejectMs = now;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Important,
                    "wireframe.bounds_rejected", player.avatarName(),
                    "outline skipped because the posed part transform is outside the sane actor bounds",
                    (uint32_t)player.movementSimulationTick,
                    nlohmann::json{{"avatar", player.avatarName()},
                                   {"actor", player.username},
                                   {"radius", saneRadius},
                                   {"size_scale", player.sizeScale},
                                   {"model_loaded", player.modelLoaded}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            return;
        }
        const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        if (wireframe->visibleThroughWalls) glDisable(GL_DEPTH_TEST); else glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(wireframe->lineWidth);
        const glm::vec3 wireRgb = visualsConfig.effectiveWireframeColor(friendlyActor, wireframe->color);
        const glm::vec4 wireColor(wireRgb.r / 255.0f, wireRgb.g / 255.0f,
                                  wireRgb.b / 255.0f, wireframe->alpha);
        player.renderCurrentPose(gRenderer->shaderProgram, view, proj, true, hideHead,
                                 true, 0.0f, wireColor);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthMask(GL_TRUE);
        if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
    };

    std::array<bool, 4> drawn{};
    const auto drawLayer = [&](const std::string& name) {
        if (name == "trail" && !drawn[0]) { drawn[0] = true; drawTrail(); }
        else if (name == "capsule" && !drawn[1]) { drawn[1] = true; drawCapsule(); }
        else if (name == "outline" && !drawn[2]) { drawn[2] = true; drawOutline(); }
        else if (name == "wireframe" && !drawn[3]) { drawn[3] = true; drawWireframe(); }
    };
    for (const std::string& name : visuals.renderOrder) drawLayer(name);
    for (const char* name : {"trail", "capsule", "outline", "wireframe"}) drawLayer(name);

    // Render attached cosmetic meshes (hats etc.) on top of the body.
    { MIMITA_PERF_SCOPE("Rendering::Actors::Cosmetics");
      CosmeticSystem::instance().renderCosmetics(player); }

    // Reset ghost color
    if (player.renderGhost)
    {
        GLuint prog = gRenderer->shaderProgram;
        GLint colorLoc = glGetUniformLocation(prog, "uColor");
        if (colorLoc >= 0)
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    }
}

} // namespace

void beginPlayerTrailBatch()
{
    gTrailBatch.clear();
    if (gTrailBatch.capacity() < 6000)
        gTrailBatch.reserve(6000);
}

void drawPlayerTrailBatch(const Camera& cam)
{
    if (gTrailBatch.empty() || !gRenderer || !gRenderer->shaderProgram)
        return;

    static GLuint vao = 0, vbo = 0;
    if (!vao) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }
    GLint oldVao = 0, oldBuffer = 0, oldProgram = 0;
    GLint oldUseColor = 0;
    GLint oldBlendSrcRgb = GL_ONE, oldBlendDstRgb = GL_ZERO;
    GLint oldBlendSrcAlpha = GL_ONE, oldBlendDstAlpha = GL_ZERO;
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST), blendWas = glIsEnabled(GL_BLEND);
    GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    GLboolean depthWriteWas = GL_TRUE;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldBuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldBlendDstAlpha);
    const GLint useColorLoc = glGetUniformLocation(gRenderer->shaderProgram, "uUseColor");
    if (useColorLoc >= 0) glGetUniformiv(gRenderer->shaderProgram, useColorLoc, &oldUseColor);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWas);
    glUseProgram(gRenderer->shaderProgram);
    const glm::mat4 model(1.0f), view = cam.getView();
    const glm::mat4 proj = cam.getProj((float)gRenderer->sceneWidth,
                                       (float)gRenderer->sceneHeight);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 2);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, gTrailBatch.size() * sizeof(TrailVertex),
                 gTrailBatch.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));
    glEnableVertexAttribArray(3);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)gTrailBatch.size());
    static auto lastTrailBatchEvent = std::chrono::steady_clock::time_point{};
    const auto trailBatchNow = std::chrono::steady_clock::now();
    if (lastTrailBatchEvent == std::chrono::steady_clock::time_point{} ||
        trailBatchNow - lastTrailBatchEvent >= std::chrono::seconds(1)) {
        lastTrailBatchEvent = trailBatchNow;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "player_visuals.trail.batch", "player_trails",
            "camera-facing trail vertices submitted with culling disabled", 0,
            nlohmann::json{{"vertex_count", gTrailBatch.size()},
                           {"draw_submitted", true},
                           {"face_culling", false}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    glBindVertexArray((GLuint)oldVao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)oldBuffer);
    if (useColorLoc >= 0) glUniform1i(useColorLoc, oldUseColor);
    glBlendFuncSeparate((GLenum)oldBlendSrcRgb, (GLenum)oldBlendDstRgb,
                        (GLenum)oldBlendSrcAlpha, (GLenum)oldBlendDstAlpha);
    glDepthMask(depthWriteWas);
    if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glUseProgram((GLuint)oldProgram);
}

void renderPlayer(const Player& player, const Camera& cam)
{
    renderPlayerInternal(player, cam, 0, true, -1, -1);
}

void renderNetworkPlayer(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal,
    int localTeam,
    int actorTeam)
{
    renderPlayerInternal(player, cam, networkEntityId, isLocal, localTeam, actorTeam);
}
