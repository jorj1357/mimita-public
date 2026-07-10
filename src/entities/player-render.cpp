#include "player.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "config.h"
#include "debug/debug-diag.h"
#include "debug/debug-visuals.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "replay/replay-scene.h"
#include "world/texture-store.h"
#include "avatar/avatar.h"

extern TextureStore gTextures;

extern GLuint capsuleVAO;
extern int capsuleVertCount;
extern GLuint playerVAO;

void initCapsuleMesh();
void uploadBodyPartMeshPart(const Mesh& mesh, int partIndex);
void uploadPlayerMeshIfNeeded(const Mesh& mesh);

void Player::render(unsigned int shader,
                    const glm::mat4& view,
                    const glm::mat4& proj,
                    bool hideHead) const
{
    const_cast<Player*>(this)->updateModelWorldTransforms();
    bool flash = spawnFlashTimer > 0.0f;
    renderCurrentPose(shader, view, proj, flash, hideHead);
}

void Player::applyReplayPose(
    const glm::vec3& rootPosition,
    float rootYaw,
    const std::vector<ReplayBodyPartState>& parts)
{
    pos = rootPosition;
    yaw = rootYaw;
    dead = false;
    syncLegacyStateToLayers();

    const glm::mat4 root =
        glm::translate(glm::mat4(1.0f), rootPosition) *
        glm::rotate(glm::mat4(1.0f), glm::radians(rootYaw), glm::vec3(0, 0, 1));
    for (PhysicalBodyPart& bodyPart : physicalBody.parts) {
        auto it = std::find_if(
            parts.begin(), parts.end(),
            [&bodyPart](const ReplayBodyPartState& part) {
                return part.name == bodyPart.name;
            });
        if (it == parts.end())
            continue;

        glm::mat4 local = glm::translate(glm::mat4(1.0f), it->position)
            * glm::mat4_cast(it->rotation)
            * glm::scale(glm::mat4(1.0f), it->scale);
        bodyPart.worldTransform = root * local;
    }
}

void Player::renderCurrentPose(unsigned int shader,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               bool whiteOverride,
                               bool hideHead) const
{
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

    if (modelLoaded && !physicalBody.parts.empty() && physicalBody.partMeshes.size() == physicalBody.parts.size())
    {
        static GLint uViewLoc = -1, uProjLoc = -1, uModelLoc = -1;
        static GLint uUseColorLoc = -1, uColorLoc = -1, uTexLoc = -1;
        static GLint uAlphaCutoffLoc = -1, uDebugViewLoc = -1;
        if (uViewLoc < 0) uViewLoc = glGetUniformLocation(shader, "view");
        if (uProjLoc < 0) uProjLoc = glGetUniformLocation(shader, "projection");
        if (uModelLoc < 0) uModelLoc = glGetUniformLocation(shader, "model");
        if (uUseColorLoc < 0) uUseColorLoc = glGetUniformLocation(shader, "uUseColor");
        if (uColorLoc < 0) uColorLoc = glGetUniformLocation(shader, "uColor");
        if (uTexLoc < 0) uTexLoc = glGetUniformLocation(shader, "uTex");
        if (uAlphaCutoffLoc < 0) uAlphaCutoffLoc = glGetUniformLocation(shader, "uAlphaCutoff");
        if (uDebugViewLoc < 0) uDebugViewLoc = glGetUniformLocation(shader, "uDebugView");

        MIMITA_GL_CLEAR_STAGE("Player::render body parts");
        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(uViewLoc, 1, 0, &view[0][0]);
        glUniformMatrix4fv(uProjLoc, 1, 0, &proj[0][0]);
        glUniform1i(uUseColorLoc, whiteOverride ? 1 : 0);
        glUniform1i(uTexLoc, 0);

        // Set debug view (UV checker etc.) for player too
        glUniform1i(uDebugViewLoc, DebugVis::shaderDebugView());

        // Set alpha cutoff from current avatar mode
        float alphaCutoff = 0.0f;
        int alphaBlendMode = 2; // 0=opaque, 1=cutout, 2=blend
        if (AvatarSystem::instance().hasAvatar()) {
            const auto& av = AvatarSystem::instance().current();
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
        glUniform1f(uAlphaCutoffLoc, alphaCutoff);

        // Save and explicitly set culling state for player rendering
        GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        GLint cullModeWas = GL_BACK;
        if (cullWas) glGetIntegerv(GL_CULL_FACE_MODE, &cullModeWas);

        // Set GL blend state based on alpha mode
        GLboolean blendWas = glIsEnabled(GL_BLEND);
        if (alphaBlendMode == 0) {
            glDisable(GL_BLEND);
        } else if (alphaBlendMode == 1) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;
            if (hideHead && part.name == "head")
                continue;

            uploadBodyPartMeshPart(mesh, (int)i);

            const glm::mat4& model = part.worldTransform;
            glUniformMatrix4fv(uModelLoc, 1, 0, &model[0][0]);

            // Per-part color tint from outfit.json
            glm::vec3 tint(1.0f);
            if (!whiteOverride && i < outfitPartColors.size())
                tint = outfitPartColors[i];
            glUniform4f(uColorLoc, tint.r, tint.g, tint.b, 1.0f);

            for (const Mesh::Batch& batch : mesh.batches)
            {
                MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default")));

                // Cull debug mode: enable culling + solid color to expose winding issues
                if (DebugConfig::DEBUG_CULL) {
                    if (batch.doubleSided)
                        glDisable(GL_CULL_FACE);
                    else {
                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_BACK);
                    }
                    glUniform1i(uUseColorLoc, 1);
                    glUniform4f(uColorLoc, 1.0f, 0.5f, 0.0f, 1.0f);
                    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
                    glUniform1i(uUseColorLoc, whiteOverride ? 1 : 0);
                } else {
                    // Character meshes are thin-shell — always render both sides
                    glDisable(GL_CULL_FACE);
                    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
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
                        printf("  blend=%s\n", glIsEnabled(GL_BLEND) ? "enabled" : "disabled");
                        GLboolean depthWrite = GL_FALSE;
                        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
                        printf("  depth_write=%s\n", depthWrite ? "true" : "false");
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

        // Restore culling state
        if (cullWas) {
            glEnable(GL_CULL_FACE);
            glCullFace(cullModeWas);
        } else {
            glDisable(GL_CULL_FACE);
        }

        // Restore blend state
        if (blendWas)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        return;
    }

    if (modelLoaded && !renderMesh.verts.empty())
    {
        uploadPlayerMeshIfNeeded(renderMesh);

        glm::mat4 model =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

        MIMITA_GL_CLEAR_STAGE("Player::render mesh");
        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
        glUniform1i(glGetUniformLocation(shader,"uUseColor"), whiteOverride ? 1 : 0);
        if (whiteOverride)
            glUniform4f(glGetUniformLocation(shader,"uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1i(glGetUniformLocation(shader,"uTex"),0);

        MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
        MIMITA_GL_CALL(glBindVertexArray(playerVAO));
        for (const Mesh::Batch& batch : renderMesh.batches)
        {
            MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default")));
            MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
            diagRenderCountPlayerDraw();
        }
        return;
    }

    initCapsuleMesh();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    MIMITA_GL_CLEAR_STAGE("Player::render capsule");
    MIMITA_GL_CALL(glUseProgram(shader));
    glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
    glUniform1i(glGetUniformLocation(shader,"uUseColor"), whiteOverride ? 1 : 0);
    if (whiteOverride)
        glUniform4f(glGetUniformLocation(shader,"uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(shader,"uTex"),0);

    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, gTextures.get("greenwirev1")));

    MIMITA_GL_CALL(glBindVertexArray(capsuleVAO));
    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, capsuleVertCount));
    diagRenderCountPlayerDraw();
}

void Player::renderDepth(unsigned int shadowShader, const glm::mat4& lightViewProj) const
{
    const_cast<Player*>(this)->updateModelWorldTransforms();

    glUseProgram(shadowShader);
    GLint loc = glGetUniformLocation(shadowShader, "uLightMVP");

    if (modelLoaded && !physicalBody.parts.empty() && physicalBody.partMeshes.size() == physicalBody.parts.size())
    {
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;

            uploadBodyPartMeshPart(mesh, (int)i);

            const glm::mat4& model = part.worldTransform;
            glm::mat4 mvp = lightViewProj * model;
            if (loc >= 0)
                glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);

            for (const Mesh::Batch& batch : mesh.batches)
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }
        glUseProgram(0);
        return;
    }

    if (modelLoaded && !renderMesh.verts.empty())
    {
        uploadPlayerMeshIfNeeded(renderMesh);

        glm::mat4 modelMat =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

        glm::mat4 mvp = lightViewProj * modelMat;
        if (loc >= 0)
            glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);

        glBindVertexArray(playerVAO);
        for (const Mesh::Batch& batch : renderMesh.batches)
            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);

        glUseProgram(0);
        return;
    }

    initCapsuleMesh();

    glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 mvp = lightViewProj * modelMat;
    if (loc >= 0)
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);

    glBindVertexArray(capsuleVAO);
    glDrawArrays(GL_TRIANGLES, 0, capsuleVertCount);
    glUseProgram(0);
}
