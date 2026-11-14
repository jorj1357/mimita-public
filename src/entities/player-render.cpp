#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <chrono>

#include "config.h"
#include "config/camera-config.h"
#include "debug/debug-diag.h"
#include "debug/debug-visuals.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/gl-debug.h"
#include "replay/replay-scene.h"
#include "world/texture-store.h"
#include "avatar/avatar.h"
#include "entities/player-avatar-animations.h"
#include "hot-reload/module-host.h"
#include "modules/api/actor-rendering-api.h"
#include "render/render-command.h"
#include "render/shader-program.h"

namespace r = mimita::game::render;

#include <glm/gtx/matrix_decompose.hpp>

extern TextureStore gTextures;

static glm::mat4 renderPartTransform(const Player& player, const PhysicalBodyPart& part)
{
    const AnimationRuntimeSettings& settings = animationRuntimeSettings();
    const float alpha = animationRenderAlpha();
    mimita_actor_rendering::PoseV1 request{};
    std::memcpy(request.current, &part.worldTransform[0][0], sizeof(request.current));
    std::memcpy(request.previous, &part.previousWorldTransform[0][0], sizeof(request.previous));
    request.alpha = alpha;
    request.interpolate = settings.renderPoseInterpolation ? 1u : 0u;
    request.hasPrevious = part.hasPreviousWorldTransform ? 1u : 0u;
    if (ModuleHost::instance().invokeEvent(mimita_module::EVENT_ACTOR_RENDER_POSE,
                                           &request, sizeof(request))) {
        if (request.hasPrevious && request.interpolate && alpha < 0.999f &&
            request.stepMeters > 8.0f) {
            static std::unordered_map<std::string, std::uint64_t> lastLogMs;
            const auto nowMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
            auto& previousMs = lastLogMs[player.username];
            if (nowMs - previousMs >= 1000) {
                previousMs = nowMs;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Rendering, StructuredLevel::Important,
                    "actor.pose.interpolation", player.username,
                    "large previous-to-current part displacement rejected by actor module", 0,
                    {{"actor", player.username}, {"avatar", player.avatarName()},
                     {"part", part.name}, {"step_m", request.stepMeters},
                     {"alpha", alpha}, {"used_previous", request.usedPrevious != 0},
                     {"module_generation", ModuleHost::instance().activeGeneration("actor-rendering")},
                     {"current", {part.worldTransform[3].x,part.worldTransform[3].y,part.worldTransform[3].z}},
                     {"previous", {part.previousWorldTransform[3].x,part.previousWorldTransform[3].y,part.previousWorldTransform[3].z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
        glm::mat4 result(1.0f);
        std::memcpy(&result[0][0], request.result, sizeof(request.result));
        return result;
    }
    if (!settings.renderPoseInterpolation || alpha >= 0.999f ||
        !part.hasPreviousWorldTransform)
        return part.worldTransform;

    // A respawn, avatar swap, or teleport can leave previousWorldTransform at
    // the old location. Interpolating between that point and the current pose
    // stretches every triangle across the map, which the outline pass exposes
    // as huge red/green sheets. Only interpolate a plausible fixed-tick step.
    const glm::vec3 currentPosition(part.worldTransform[3]);
    const glm::vec3 previousPosition(part.previousWorldTransform[3]);
    const float step = glm::length(currentPosition - previousPosition);
    if (!std::isfinite(step) || step > 8.0f)
        return part.worldTransform;

    glm::vec3 p0(0.0f), p1(0.0f), s0(1.0f), s1(1.0f), skew0(0.0f), skew1(0.0f);
    glm::quat q0(1.0f, 0.0f, 0.0f, 0.0f), q1(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 perspective0(0.0f), perspective1(0.0f);
    glm::decompose(part.previousWorldTransform, p0, q0, s0, skew0, perspective0);
    glm::decompose(part.worldTransform, p1, q1, s1, skew1, perspective1);
    const glm::vec3 position = glm::mix(p0, p1, alpha);
    const glm::quat rotation = glm::slerp(q0, q1, alpha);
    const glm::vec3 scale = glm::mix(s0, s1, alpha);
    return glm::translate(glm::mat4(1.0f), position) *
           glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.0f), scale);
}

extern std::uint32_t capsuleVAO;
extern int capsuleVertCount;

void initCapsuleMesh();
void uploadBodyPartMeshPart(const Mesh& mesh, int partIndex, const std::string& modelKey);
std::uint32_t uploadPlayerMeshIfNeeded(const Mesh& mesh, const std::string& modelKey);
bool drawCombinedBodyParts(const Player& player, mimita::game::render::ShaderProgram& shader, bool outlinePass);

void Player::render(mimita::game::render::ShaderProgram& shader,
                    const glm::mat4& view,
                    const glm::mat4& proj,
                    bool hideHead,
                    const glm::vec3& playerTint) const
{
    const_cast<Player*>(this)->finalizeModelIfReady();
    const_cast<Player*>(this)->updateModelWorldTransforms();
    bool flash = spawnFlashTimer > 0.0f;
    renderCurrentPose(shader, view, proj, flash, hideHead, false, 0.0f,
                      glm::vec4(1.0f), playerTint);
}

void Player::applyReplayPose(
    const glm::vec3& rootPosition,
    float rootYaw,
    const ReplayBodyPartState* parts,
    uint8_t partCount)
{
    pos = rootPosition;
    yaw = rootYaw;
    dead = false;
    syncLegacyStateToLayers();

    const glm::mat4 root =
        glm::translate(glm::mat4(1.0f), rootPosition) *
        glm::rotate(glm::mat4(1.0f), glm::radians(rootYaw), glm::vec3(0, 0, 1));
    std::array<bool, ReplayActorState::MAX_BODY_PARTS> applied{};
    for (int pass = 0; pass < 2; ++pass) {
    for (PhysicalBodyPart& bodyPart : physicalBody.parts) {
        const ReplayBodyPartState* found = nullptr;
        for (uint8_t i = 0; i < partCount; ++i) {
            if (parts[i].partId == partIdFromName(bodyPart.name.c_str())) {
                found = &parts[i];
                break;
            }
        }
        if (!found) continue;

        glm::mat4 parentWorld = root;
        if (found->parentPartId != 0xFF) {
            bool parentApplied = false;
            for (size_t i = 0; i < physicalBody.parts.size(); ++i) {
                const PhysicalBodyPart& parent = physicalBody.parts[i];
                if (i < applied.size() &&
                    partIdFromName(parent.name.c_str()) == found->parentPartId && applied[i]) {
                    parentWorld = parent.worldTransform;
                    parentApplied = true;
                    break;
                }
            }
            if (!parentApplied)
                continue;
        }
        glm::mat4 local = glm::translate(glm::mat4(1.0f), found->position)
            * glm::mat4_cast(found->rotation)
            * glm::scale(glm::mat4(1.0f), found->scale);
        bodyPart.worldTransform = parentWorld * local;
        const size_t bodyPartIndex = (size_t)(&bodyPart - physicalBody.parts.data());
        if (bodyPartIndex < applied.size())
            applied[bodyPartIndex] = true;

        // Left/right leg diagnostic: log quaternion at apply time
        uint8_t pid = partIdFromName(bodyPart.name.c_str());
        if (pid == 4 || pid == 5) {
            static int legDiagCounter = 0;
            if (legDiagCounter++ % 60 == 0) {
                const char* legName = (pid == 4) ? "leftLeg" : "rightLeg";
                glm::vec3 appliedEuler = glm::degrees(glm::eulerAngles(found->rotation));
                Debug::log(Debug::Category::Replay,
                    "[LEG DIAG] apply part=%s localRot=(%.4f %.4f %.4f %.4f) euler=(%.1f %.1f %.1f) pos=(%.3f %.3f %.3f)\n",
                    legName,
                    found->rotation.w, found->rotation.x, found->rotation.y, found->rotation.z,
                    appliedEuler.x, appliedEuler.y, appliedEuler.z,
                    found->position.x, found->position.y, found->position.z);
            }
        }
    }
    }
}

void Player::renderCurrentPose(mimita::game::render::ShaderProgram& shader,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               bool whiteOverride,
                               bool hideHead,
                               bool outlinePass,
                               float outlineThickness,
                               const glm::vec4& outlineColor,
                               const glm::vec3& playerTint) const
{
    auto logRagdollPath = [&](uint32_t stage, const char* event,
                              const char* reason, const nlohmann::json& fields) {
        if (!dead || !ragdollModeActive) return;
        static std::unordered_map<const Player*, uint32_t> loggedStages;
        uint32_t& seen = loggedStages[this];
        if ((seen & stage) != 0) return;
        seen |= stage;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Ragdoll, StructuredLevel::Important,
            event, username, reason,
            static_cast<uint32_t>(movementSimulationTick),
            fields, __FILE__, __LINE__, __FUNCTION__);
    };

    {
        static bool perfModelLogged = false;
        if (DebugConfig::DEBUG_PERF_MODEL && !perfModelLogged) {
            perfModelLogged = true;
            int totalTris = 0, totalBatches = 0, totalVerts = 0;
            for (size_t pi = 0; pi < physicalBody.partMeshes.size(); ++pi) {
                const Mesh& pm = physicalBody.partMeshes[pi];
                int tris = (int)pm.verts.size() / 3;
                totalTris += tris;
                totalVerts += (int)pm.verts.size();
                totalBatches += (int)pm.batches.size();
                printf("[PERF MODEL] part=%s verts=%zu tris=%d batches=%zu\n",
                       physicalBody.parts[pi].name.c_str(), pm.verts.size(), tris, pm.batches.size());
            }
            printf("[PERF MODEL] total: %d vertices, %d triangles, %d batches across %zu parts\n",
                   totalVerts, totalTris, totalBatches, physicalBody.partMeshes.size());
        } else if (!DebugConfig::DEBUG_PERF_MODEL) {
            perfModelLogged = false;
        }
    }

    const bool renderPhysicalParts =
        !physicalBody.parts.empty() &&
        physicalBody.partMeshes.size() == physicalBody.parts.size();
    const bool renderExtraParts =
        !physicalBody.extraParts.empty() &&
        physicalBody.extraPartMeshes.size() == physicalBody.extraParts.size();
    if (modelLoaded && (renderPhysicalParts || renderExtraParts))
    {
        size_t drawableParts = 0;
        size_t drawableBatches = 0;
        size_t drawableVertices = 0;
        static std::int32_t uViewLoc = -1, uProjLoc = -1, uModelLoc = -1;
        static std::int32_t uUseColorLoc = -1, uColorLoc = -1, uTexLoc = -1, uTintLoc = -1;
        static std::int32_t uOutlinePassLoc = -1, uOutlineThicknessLoc = -1;
        static std::int32_t uAlphaCutoffLoc = -1, uDebugViewLoc = -1;
        static std::int32_t uCosmeticTextureEnabledLoc = -1;
        static std::int32_t uCosmeticUvOffsetLoc = -1, uCosmeticUvScaleLoc = -1;
        static std::int32_t uCosmeticUvRotationLoc = -1, uCosmeticBrightnessLoc = -1;
        static std::int32_t uCosmeticOpacityLoc = -1;
        static std::int32_t uCameraFadeEnabledLoc = -1, uCameraFadeStartLoc = -1;
        static std::int32_t uCameraFadeEndLoc = -1, uCameraPositionLoc = -1;
        if (uViewLoc < 0) uViewLoc = shader.uniform( "view");
        if (uProjLoc < 0) uProjLoc = shader.uniform( "projection");
        if (uModelLoc < 0) uModelLoc = shader.uniform( "model");
        if (uOutlinePassLoc < 0) uOutlinePassLoc = shader.uniform( "uOutlinePass");
        if (uOutlineThicknessLoc < 0) uOutlineThicknessLoc = shader.uniform( "uOutlineThickness");
        if (uUseColorLoc < 0) uUseColorLoc = shader.uniform( "uUseColor");
        if (uColorLoc < 0) uColorLoc = shader.uniform( "uColor");
        if (uTexLoc < 0) uTexLoc = shader.uniform( "uTex");
        if (uTintLoc < 0) uTintLoc = shader.uniform( "uTint");
        if (uAlphaCutoffLoc < 0) uAlphaCutoffLoc = shader.uniform( "uAlphaCutoff");
        if (uDebugViewLoc < 0) uDebugViewLoc = shader.uniform( "uDebugView");
        if (uCosmeticTextureEnabledLoc < 0)
            uCosmeticTextureEnabledLoc = shader.uniform( "uCosmeticTextureEnabled");
        if (uCosmeticUvOffsetLoc < 0)
            uCosmeticUvOffsetLoc = shader.uniform( "uCosmeticUvOffset");
        if (uCosmeticUvScaleLoc < 0)
            uCosmeticUvScaleLoc = shader.uniform( "uCosmeticUvScale");
        if (uCosmeticUvRotationLoc < 0)
            uCosmeticUvRotationLoc = shader.uniform( "uCosmeticUvRotation");
        if (uCosmeticBrightnessLoc < 0)
            uCosmeticBrightnessLoc = shader.uniform( "uCosmeticBrightness");
        if (uCosmeticOpacityLoc < 0)
            uCosmeticOpacityLoc = shader.uniform( "uCosmeticOpacity");
        if (uCameraFadeEnabledLoc < 0)
            uCameraFadeEnabledLoc = shader.uniform( "uCameraProximityFadeEnabled");
        if (uCameraFadeStartLoc < 0)
            uCameraFadeStartLoc = shader.uniform( "uCameraProximityFadeStart");
        if (uCameraFadeEndLoc < 0)
            uCameraFadeEndLoc = shader.uniform( "uCameraProximityFadeEnd");
        if (uCameraPositionLoc < 0)
            uCameraPositionLoc = shader.uniform( "uCameraPos");

        MIMITA_GL_CLEAR_STAGE("Player::render body parts");
        r::useProgram(shader.program());
        const CameraConfigData& cameraConfig = CamConfig::instance().data();
        const glm::vec3 cameraPosition = glm::vec3(glm::inverse(view)[3]);
        if (uCameraFadeEnabledLoc >= 0)
            r::uniform1i(uCameraFadeEnabledLoc, cameraConfig.proximityFadeEnabled ? 1 : 0);
        if (uCameraFadeStartLoc >= 0)
            r::uniform1f(uCameraFadeStartLoc, cameraConfig.proximityFadeStartDistanceMeters);
        if (uCameraFadeEndLoc >= 0)
            r::uniform1f(uCameraFadeEndLoc, cameraConfig.proximityFadeEndDistanceMeters);
        if (uCameraPositionLoc >= 0)
            r::uniform3f(uCameraPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
        r::uniformMatrix4fv(uViewLoc, 1, &view[0][0]);
        r::uniformMatrix4fv(uProjLoc, 1, &proj[0][0]);
        if (uOutlinePassLoc >= 0) r::uniform1i(uOutlinePassLoc, outlinePass ? 1 : 0);
        if (uOutlineThicknessLoc >= 0) r::uniform1f(uOutlineThicknessLoc, outlineThickness);
        r::uniform1i(uUseColorLoc, whiteOverride ? 1 : 0);
        r::uniform1i(uTexLoc, 0);
        if (uTintLoc >= 0)
            r::uniform3f(uTintLoc, playerTint.r, playerTint.g, playerTint.b);
        if (uCosmeticTextureEnabledLoc >= 0)
            r::uniform1i(uCosmeticTextureEnabledLoc, 0);
        if (uCosmeticUvOffsetLoc >= 0)
            r::uniform2f(uCosmeticUvOffsetLoc, 0.0f, 0.0f);
        if (uCosmeticUvScaleLoc >= 0)
            r::uniform2f(uCosmeticUvScaleLoc, 1.0f, 1.0f);
        if (uCosmeticUvRotationLoc >= 0)
            r::uniform1f(uCosmeticUvRotationLoc, 0.0f);
        if (uCosmeticBrightnessLoc >= 0)
            r::uniform1f(uCosmeticBrightnessLoc, 1.0f);
        if (uCosmeticOpacityLoc >= 0)
            r::uniform1f(uCosmeticOpacityLoc, 1.0f);
        if (DebugConfig::DEBUG_WEAPON_VIEWMODEL) {
            printf("[PLAYER SHADER RESET] entityId=%s bodyColor=(1.00,1.00,1.00) "
                   "useTexture=1 useColor=%d\n",
                   username.c_str(), whiteOverride ? 1 : 0);
        }

        // Set debug view (UV checker etc.) for player too
        r::uniform1i(uDebugViewLoc, DebugVis::shaderDebugView());

        // Compute death animation alpha override. The body stays FULLY visible
        // while it falls over (the renderer hides it the instant the animation
        // ends), so there is no gradual fade.
        float deathAlpha = 1.0f;
        if (dead && deathAnim.active) {
            deathAlpha = 1.0f;
        }

        // Set alpha cutoff from current avatar instance (NOT from singleton)
        float alphaCutoff = 0.0f;
        int alphaBlendMode = 2; // 0=opaque, 1=cutout, 2=blend
        if (deathAlpha < 1.0f) {
            alphaBlendMode = 2; // force alpha blend during death fade
        } else if (avatarInstance) {
            const auto& av = avatarInstance->definition;
            if (av.textureMode == "uv_atlas") {
                if (av.alphaMode == "cutout") {
                    alphaCutoff = av.alphaCutoff;
                    alphaBlendMode = 1;
                } else if (av.alphaMode == "opaque") {
                    alphaBlendMode = 0;
                } else {
                    alphaBlendMode = 2; // blend
                }
            }
        }
        r::uniform1f(uAlphaCutoffLoc, alphaCutoff);

        // Save and restore GL state around the player render.
        r::pushState();
        if (alphaBlendMode == 0 || alphaBlendMode == 1) {
            r::disable(r::Capability::Blend);
        } else {
            r::enable(r::Capability::Blend);
            r::blendFunc(r::BlendFactor::SrcAlpha, r::BlendFactor::OneMinusSrcAlpha);
        }

        r::activeTexture(0);
        static thread_local std::vector<mimita_actor_rendering::PartV1> planParts;
        static thread_local std::vector<std::uint8_t> planMask;
        planParts.resize(physicalBody.parts.size());
        planMask.resize(physicalBody.parts.size());
        for (size_t i = 0; i < physicalBody.parts.size(); ++i) {
            const auto& part = physicalBody.parts[i];
            const ActorBodyPart kind = actorBodyPartFromName(part.name);
            planParts[i] = {
                (part.nodeIndex >= 0 && part.nodeIndex < (int)perfectPoseSkeleton.nodes.size()) ? 1u : 0u,
                i < physicalBody.partMeshes.size() ? (std::uint32_t)physicalBody.partMeshes[i].verts.size() : 0u,
                part.name == "head" ? 1u : 0u,
                (std::uint32_t)(kind == ActorBodyPart::Unknown ? ActorBodyLayer::Skin : body.layer(kind))};
        }
        mimita_actor_rendering::DrawPlanV1 drawPlan{};
        drawPlan.parts = planParts.data();
        drawPlan.partDrawMask = planMask.data();
        drawPlan.partCount = (std::uint32_t)planParts.size();
        drawPlan.partCapacity = (std::uint32_t)planMask.size();
        drawPlan.hideHead = hideHead;
        drawPlan.outlinePass = outlinePass;
        drawPlan.whiteOverride = whiteOverride;
        drawPlan.debugCull = DebugConfig::DEBUG_CULL;
        const bool moduleDrawPlan = ModuleHost::instance().invokeEvent(
            mimita_module::EVENT_ACTOR_RENDER_DRAW_PLAN, &drawPlan, sizeof(drawPlan));
        bool combinedDrawn = false;
        bool allPartsSkin = true;
        if (!moduleDrawPlan) {
            for (const PhysicalBodyPart& part : physicalBody.parts)
            {
                const ActorBodyPart actorPart = actorBodyPartFromName(part.name);
                if (actorPart == ActorBodyPart::Unknown)
                    continue;
                if (body.layer(actorPart) != ActorBodyLayer::Skin)
                {
                    allPartsSkin = false;
                    break;
                }
            }
        }
        if (moduleDrawPlan && drawPlan.combineAllowed)
            combinedDrawn = drawCombinedBodyParts(*this, shader, outlinePass);
        if (!moduleDrawPlan && allPartsSkin && !hideHead && !outlinePass && !whiteOverride && !DebugConfig::DEBUG_CULL) {
            bool validPartSet = true;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.nodeIndex >= 0 &&
                    part.nodeIndex < static_cast<int>(perfectPoseSkeleton.nodes.size()))
                    continue;
                validPartSet = false;
                break;
            }
            if (validPartSet)
                combinedDrawn = drawCombinedBodyParts(*this, shader, outlinePass);
        }
        if (!combinedDrawn) {
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            if (moduleDrawPlan && !planMask[i]) continue;
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;
            if (!moduleDrawPlan && hideHead && part.name == "head")
                continue;
            const ActorBodyPart actorPart = actorBodyPartFromName(part.name);
            const ActorBodyLayer layer = actorPart == ActorBodyPart::Unknown
                ? ActorBodyLayer::Skin : body.layer(actorPart);
            if (!moduleDrawPlan && layer == ActorBodyLayer::Destroyed)
                continue;
            const Mesh* drawMesh = &mesh;
            glm::vec3 layerTint(1.0f);
            if (layer == ActorBodyLayer::Meat || layer == ActorBodyLayer::Bone)
            {
                const Mesh& layerMesh =
                    part.layerMeshes[(size_t)layer];
                if (!layerMesh.verts.empty())
                    drawMesh = &layerMesh;
                if (layer == ActorBodyLayer::Bone)
                    layerTint = glm::vec3(0.86f, 0.82f, 0.70f);
            }

            ++drawableParts;
            drawableBatches += drawMesh->batches.size();
            drawableVertices += drawMesh->verts.size();

            const std::string uploadKey =
                renderUploadKey() + "|layer" + std::to_string((int)layer);
            uploadBodyPartMeshPart(*drawMesh, (int)i, uploadKey);

            const glm::mat4 model = renderPartTransform(*this, part);
            r::uniformMatrix4fv(uModelLoc, 1, &model[0][0]);

            // Per-part color tint from outfit.json
            glm::vec3 tint = layerTint;
            if (!whiteOverride && i < outfitPartColors.size() &&
                layer == ActorBodyLayer::Skin)
                tint = outfitPartColors[i];
            r::uniform4f(uColorLoc, outlinePass ? outlineColor.r : tint.r,
                        outlinePass ? outlineColor.g : tint.g,
                        outlinePass ? outlineColor.b : tint.b,
                        outlinePass ? outlineColor.a : deathAlpha);

            for (const Mesh::Batch& batch : drawMesh->batches)
            {
                r::bindTexture(r::TextureTarget::Texture2D,
                               batch.texture ? batch.texture : gTextures.get("default"));

                // Cull debug mode: enable culling + solid color to expose winding issues
                if (DebugConfig::DEBUG_CULL) {
                    if (batch.doubleSided)
                        r::disable(r::Capability::CullFace);
                    else {
                        r::enable(r::Capability::CullFace);
                        r::cullFace(r::CullFaceMode::Back);
                    }
                    r::uniform1i(uUseColorLoc, 1);
                    r::uniform4f(uColorLoc, 1.0f, 0.5f, 0.0f, 1.0f);
                    r::drawArrays(r::Primitive::Triangles,
                                  static_cast<std::int32_t>(batch.first),
                                  static_cast<std::int32_t>(batch.count));
                    r::uniform1i(uUseColorLoc, whiteOverride ? 1 : 0);
                } else {
                    // Character meshes are thin-shell — always render both sides
                    if (outlinePass) {
                        r::enable(r::Capability::CullFace);
                        r::cullFace(r::CullFaceMode::Front);
                    } else {
                        r::disable(r::Capability::CullFace);
                    }
                    r::drawArrays(r::Primitive::Triangles,
                                  static_cast<std::int32_t>(batch.first),
                                  static_cast<std::int32_t>(batch.count));
                }
                diagRenderCountPlayerDraw();

                // Head debug: log once for the head batch
                if (part.name == "head" && DebugConfig::DEBUG_HEAD) {
                    static bool headLogged = false;
                    if (!headLogged) {
                        headLogged = true;
                        glm::mat4 modelMtx = part.worldTransform;
                        float det = glm::determinant(glm::mat3(modelMtx));
                        printf("\n[HEAD DEBUG]\n");
                        printf("  node=%s\n", part.name.c_str());
                        printf("  nodeIndex=%d\n", part.nodeIndex);
                        printf("  batches=%zu\n", mesh.batches.size());
                        printf("  batch=%zu first=%zu count=%zu\n", (size_t)(&batch - &mesh.batches[0]), batch.first, batch.count);
                        printf("  vertices=%zu\n", mesh.verts.size());
                        printf("  texture=%u\n", batch.texture);
                        printf("  double_sided=%d\n", (int)batch.doubleSided);
                        printf("  model_determinant=%.4f\n", det);
                        printf("  culling=%s\n", "disabled (engine override)");
                        printf("  front_face=CCW (default)\n");
                        if (!mesh.verts.empty()) {
                            float minU=1e10f, maxU=-1e10f, minV=1e10f, maxV=-1e10f;
                            for (const auto& v : mesh.verts) {
                                minU = std::min(minU, v.uv.x); maxU = std::max(maxU, v.uv.x);
                                minV = std::min(minV, v.uv.y); maxV = std::max(maxV, v.uv.y);
                            }
                            printf("  uv_range=(%.4f,%.4f)..(%.4f,%.4f)\n", minU, minV, maxU, maxV);
                            printf("  first_uv=(%.4f,%.4f)\n", mesh.verts[0].uv.x, mesh.verts[0].uv.y);
                            printf("  first_pos=(%.2f,%.2f,%.2f)\n", mesh.verts[0].pos.x, mesh.verts[0].pos.y, mesh.verts[0].pos.z);
                        }
                        printf("\n");
                    }
                }
            }
        }

        // Render-only rig parts (wings/stinger/tail). Drawn after physical
        // parts with their own node world transform; never simulated.
        for (size_t i = 0; i < physicalBody.extraParts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.extraParts[i];
            const Mesh& mesh = physicalBody.extraPartMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;

            ++drawableParts;
            drawableBatches += mesh.batches.size();
            drawableVertices += mesh.verts.size();

            uploadBodyPartMeshPart(mesh, (int)(physicalBody.parts.size() + i), renderUploadKey());

            const glm::mat4 model = renderPartTransform(*this, part);
            r::uniformMatrix4fv(uModelLoc, 1, &model[0][0]);
            r::uniform4f(uColorLoc, outlinePass ? outlineColor.r : 1.0f,
                        outlinePass ? outlineColor.g : 1.0f,
                        outlinePass ? outlineColor.b : 1.0f,
                        outlinePass ? outlineColor.a : deathAlpha);

            // Evidence: what the renderer actually submits for a rig part.
            static uint32_t sRollRenderLog = 0;
            if ((sRollRenderLog++ % 120u) == 0u) {
                const glm::quat q =
                    glm::normalize(glm::quat_cast(glm::mat3(part.worldTransform)));
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Important,
                    "roll.render", username, "rendered rig-part transform", 0,
                    {{"part", part.name},
                     {"pos", {part.worldTransform[3].x,
                              part.worldTransform[3].y,
                              part.worldTransform[3].z}},
                     {"quat", {q.w, q.x, q.y, q.z}},
                     {"model_root_active", modelRootRotationActive},
                     {"model_root_quat", {modelRootRotation.w,
                                          modelRootRotation.x,
                                          modelRootRotation.y,
                                          modelRootRotation.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }

            for (const Mesh::Batch& batch : mesh.batches)
            {
                r::bindTexture(r::TextureTarget::Texture2D,
                               batch.texture ? batch.texture : gTextures.get("default"));
                if (outlinePass) {
                    r::enable(r::Capability::CullFace);
                    r::cullFace(r::CullFaceMode::Front);
                } else {
                    r::disable(r::Capability::CullFace);
                }
                r::drawArrays(r::Primitive::Triangles,
                              static_cast<std::int32_t>(batch.first),
                              static_cast<std::int32_t>(batch.count));
                diagRenderCountPlayerDraw();
            }
        }
        } // !combinedDrawn

        r::popState();

        // Reset avatar-only shader uniforms to prevent leaking into world/subsequent renders
        if (uAlphaCutoffLoc >= 0) r::uniform1f(uAlphaCutoffLoc, 0.0f);
        if (uOutlinePassLoc >= 0) r::uniform1i(uOutlinePassLoc, 0);
        if (uOutlineThicknessLoc >= 0) r::uniform1f(uOutlineThicknessLoc, 0.0f);
        if (uCosmeticTextureEnabledLoc >= 0) r::uniform1i(uCosmeticTextureEnabledLoc, 0);
        if (uCosmeticBrightnessLoc >= 0) r::uniform1f(uCosmeticBrightnessLoc, 1.0f);
        if (uCosmeticOpacityLoc >= 0) r::uniform1f(uCosmeticOpacityLoc, 1.0f);
        if (uTintLoc >= 0) r::uniform3f(uTintLoc, 1.0f, 1.0f, 1.0f);

        if (dead && ragdollModeActive)
            logRagdollPath(
                1u, "ragdoll.render.path", "physical body mesh path completed",
                {{"actor_id", username}, {"path", "physical_body"},
                 {"drawable_parts", drawableParts}, {"drawable_batches", drawableBatches},
                 {"drawable_vertices", drawableVertices}, {"shader_program", shader.program()},
                 {"position", {pos.x, pos.y, pos.z}}});

        return;
    }

    if (modelLoaded && !renderMesh.verts.empty())
    {
        if (dead && ragdollModeActive)
            logRagdollPath(
                2u, "ragdoll.render.path", "fallback render mesh path selected",
                {{"actor_id", username}, {"path", "render_mesh"},
                 {"vertices", renderMesh.verts.size()}, {"batches", renderMesh.batches.size()},
                 {"shader_program", shader.program()}, {"position", {pos.x, pos.y, pos.z}}});
        const std::uint32_t meshVao = uploadPlayerMeshIfNeeded(renderMesh, renderUploadKey());

        glm::mat4 model =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

        MIMITA_GL_CLEAR_STAGE("Player::render mesh");
        r::useProgram(shader.program());
        r::uniformMatrix4fv(shader.uniform("view"),1,&view[0][0]);
        r::uniformMatrix4fv(shader.uniform("projection"),1,&proj[0][0]);
        r::uniformMatrix4fv(shader.uniform("model"),1,&model[0][0]);
        const std::int32_t meshOutlinePassLoc = shader.uniform( "uOutlinePass");
        const std::int32_t meshOutlineThicknessLoc = shader.uniform( "uOutlineThickness");
        if (meshOutlinePassLoc >= 0) r::uniform1i(meshOutlinePassLoc, outlinePass ? 1 : 0);
        if (meshOutlineThicknessLoc >= 0)
            r::uniform1f(meshOutlineThicknessLoc, outlinePass ? outlineThickness : 0.0f);
        const std::int32_t meshUseColorLoc = shader.uniform( "uUseColor");
        const std::int32_t meshColorLoc = shader.uniform( "uColor");
        r::uniform1i(meshUseColorLoc, (whiteOverride || outlinePass) ? 1 : 0);
        if (outlinePass)
            r::uniform4f(meshColorLoc, outlineColor.r, outlineColor.g,
                        outlineColor.b, outlineColor.a);
        else if (whiteOverride)
            r::uniform4f(meshColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        r::uniform1i(shader.uniform("uTex"),0);
        const std::int32_t tintLoc = shader.uniform( "uTint");
        r::uniform3f(tintLoc, playerTint.r, playerTint.g, playerTint.b);

        // The single-mesh fallback is used by avatars that do not expose
        // physical body parts (for example the girl body and normal zombie).
        // It must still honor the outline pass or those actors never receive
        // the mode-authored silhouette.
        if (outlinePass) {
            r::enable(r::Capability::CullFace);
            r::cullFace(r::CullFaceMode::Front);
        }
        r::activeTexture(0);
        r::bindVertexArray(meshVao);
        for (const Mesh::Batch& batch : renderMesh.batches)
        {
            r::bindTexture(r::TextureTarget::Texture2D,
                           batch.texture ? batch.texture : gTextures.get("default"));
            r::drawArrays(r::Primitive::Triangles,
                          static_cast<std::int32_t>(batch.first),
                          static_cast<std::int32_t>(batch.count));
            diagRenderCountPlayerDraw();
        }
        if (outlinePass) {
            if (meshOutlinePassLoc >= 0) r::uniform1i(meshOutlinePassLoc, 0);
            if (meshOutlineThicknessLoc >= 0) r::uniform1f(meshOutlineThicknessLoc, 0.0f);
            r::disable(r::Capability::CullFace);
        }
        r::uniform3f(tintLoc, 1.0f, 1.0f, 1.0f);
        return;
    }

    if (dead && ragdollModeActive)
        logRagdollPath(
            4u, "ragdoll.render.path", "fallback capsule path selected",
            {{"actor_id", username}, {"path", "capsule"},
             {"shader_program", shader.program()}, {"position", {pos.x, pos.y, pos.z}}});

    initCapsuleMesh();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    MIMITA_GL_CLEAR_STAGE("Player::render capsule");
    r::useProgram(shader.program());
    r::uniformMatrix4fv(shader.uniform("view"),1,&view[0][0]);
    r::uniformMatrix4fv(shader.uniform("projection"),1,&proj[0][0]);
    r::uniformMatrix4fv(shader.uniform("model"),1,&model[0][0]);
    const std::int32_t capsuleOutlinePassLoc = shader.uniform( "uOutlinePass");
    const std::int32_t capsuleOutlineThicknessLoc = shader.uniform( "uOutlineThickness");
    if (capsuleOutlinePassLoc >= 0) r::uniform1i(capsuleOutlinePassLoc, outlinePass ? 1 : 0);
    if (capsuleOutlineThicknessLoc >= 0)
        r::uniform1f(capsuleOutlineThicknessLoc, outlinePass ? outlineThickness : 0.0f);
    const std::int32_t capsuleUseColorLoc = shader.uniform( "uUseColor");
    const std::int32_t capsuleColorLoc = shader.uniform( "uColor");
    r::uniform1i(capsuleUseColorLoc, (whiteOverride || outlinePass) ? 1 : 0);
    if (outlinePass)
        r::uniform4f(capsuleColorLoc, outlineColor.r, outlineColor.g,
                    outlineColor.b, outlineColor.a);
    else if (whiteOverride)
        r::uniform4f(capsuleColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    r::uniform1i(shader.uniform("uTex"),0);
    r::uniform3f(shader.uniform("uTint"), playerTint.r, playerTint.g, playerTint.b);

    if (outlinePass) {
        r::enable(r::Capability::CullFace);
        r::cullFace(r::CullFaceMode::Front);
    }
    r::activeTexture(0);
    r::bindTexture(r::TextureTarget::Texture2D, gTextures.get("greenwirev1"));

    r::bindVertexArray(capsuleVAO);
    r::drawArrays(r::Primitive::Triangles, 0, capsuleVertCount);
    if (outlinePass) {
        if (capsuleOutlinePassLoc >= 0) r::uniform1i(capsuleOutlinePassLoc, 0);
        if (capsuleOutlineThicknessLoc >= 0) r::uniform1f(capsuleOutlineThicknessLoc, 0.0f);
        r::disable(r::Capability::CullFace);
    }
    r::uniform3f(shader.uniform( "uTint"), 1.0f, 1.0f, 1.0f);
    diagRenderCountPlayerDraw();
}

void Player::renderDepth(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj) const
{
    const_cast<Player*>(this)->finalizeModelIfReady();
    const_cast<Player*>(this)->updateModelWorldTransforms();

    r::useProgram(shadowShader.program());
    std::int32_t loc = shadowShader.uniform("uLightMVP");

    const bool havePhysicalParts =
        !physicalBody.parts.empty() &&
        physicalBody.partMeshes.size() == physicalBody.parts.size();
    const bool haveExtraParts =
        !physicalBody.extraParts.empty() &&
        physicalBody.extraPartMeshes.size() == physicalBody.extraParts.size();
    if (modelLoaded && (havePhysicalParts || haveExtraParts))
    {
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;

            uploadBodyPartMeshPart(mesh, (int)i, renderUploadKey());

            const glm::mat4 model = renderPartTransform(*this, part);
            glm::mat4 mvp = lightViewProj * model;
            if (loc >= 0)
                r::uniformMatrix4fv(loc, 1, &mvp[0][0]);

            for (const Mesh::Batch& batch : mesh.batches)
                r::drawArrays(r::Primitive::Triangles,
                              static_cast<std::int32_t>(batch.first),
                              static_cast<std::int32_t>(batch.count));
        }
        r::useProgram(0);
        return;
    }

    if (modelLoaded && !renderMesh.verts.empty())
    {
        const std::uint32_t meshVao = uploadPlayerMeshIfNeeded(renderMesh, renderUploadKey());

        glm::mat4 modelMat =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

        glm::mat4 mvp = lightViewProj * modelMat;
        if (loc >= 0)
            r::uniformMatrix4fv(loc, 1, &mvp[0][0]);

        r::bindVertexArray(meshVao);
        for (const Mesh::Batch& batch : renderMesh.batches)
            r::drawArrays(r::Primitive::Triangles,
                          static_cast<std::int32_t>(batch.first),
                          static_cast<std::int32_t>(batch.count));

        r::useProgram(0);
        return;
    }

    initCapsuleMesh();

    glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 mvp = lightViewProj * modelMat;
    if (loc >= 0)
        r::uniformMatrix4fv(loc, 1, &mvp[0][0]);

    r::bindVertexArray(capsuleVAO);
    r::drawArrays(r::Primitive::Triangles, 0, capsuleVertCount);
    r::useProgram(0);
}
