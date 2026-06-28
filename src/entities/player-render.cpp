#include "player.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "config.h"
#include "debug/debug-diag.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "replay/replay-scene.h"
#include "world/texture-store.h"

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
        if (uViewLoc < 0) uViewLoc = glGetUniformLocation(shader, "view");
        if (uProjLoc < 0) uProjLoc = glGetUniformLocation(shader, "projection");
        if (uModelLoc < 0) uModelLoc = glGetUniformLocation(shader, "model");
        if (uUseColorLoc < 0) uUseColorLoc = glGetUniformLocation(shader, "uUseColor");
        if (uColorLoc < 0) uColorLoc = glGetUniformLocation(shader, "uColor");
        if (uTexLoc < 0) uTexLoc = glGetUniformLocation(shader, "uTex");

        MIMITA_GL_CLEAR_STAGE("Player::render body parts");
        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(uViewLoc, 1, 0, &view[0][0]);
        glUniformMatrix4fv(uProjLoc, 1, 0, &proj[0][0]);
        glUniform1i(uUseColorLoc, whiteOverride ? 1 : 0);
        glUniform1i(uTexLoc, 0);

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
                MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
                diagRenderCountPlayerDraw();
            }
        }

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
