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

static GLuint capsuleVAO = 0;
static GLuint capsuleVBO = 0;
static int    capsuleVertCount = 0;
static GLuint playerVAO = 0;
static GLuint playerVBO = 0;
static size_t playerUploadedVertCount = (size_t)-1;
static GLuint* bodyPartVAOs = nullptr;
static GLuint* bodyPartVBOs = nullptr;
static int bodyPartCount = 0;
static uint64_t* bodyPartUploadGenerations = nullptr;

static GLuint bodyPartVAO = 0;
static GLuint bodyPartVBO = 0;

static void initCapsuleMesh()
{
    if (capsuleVAO) return;

    struct V {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    constexpr int slices = 16;
    constexpr int stacks = 8;
    constexpr float PI = 3.1415926535f;

    float r = PLAYER_RADIUS;
    float h = PLAYER_HEIGHT;

    float cylinderHalf = h * 0.5f - r;

    std::vector<V> verts;

    // ===============================
    // CYLINDER
    // ===============================

    for (int i = 0; i < slices; i++)
    {
        float u0 = float(i) / slices;
        float u1 = float(i+1) / slices;

        float a0 = u0 * 2 * PI;
        float a1 = u1 * 2 * PI;

        glm::vec3 p0(r*cos(a0), r*sin(a0), -cylinderHalf);
        glm::vec3 p1(r*cos(a1), r*sin(a1), -cylinderHalf);
        glm::vec3 p2(r*cos(a0), r*sin(a0),  cylinderHalf);
        glm::vec3 p3(r*cos(a1), r*sin(a1),  cylinderHalf);

        verts.push_back({p0,{u0,0}, glm::normalize(glm::vec3(p0.x,p0.y,0.0f))});
        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});

        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p3,{u1,1}, glm::normalize(glm::vec3(p3.x,p3.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});
    }

    // ===============================
    // TOP HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});
        }
    }

    // ===============================
    // BOTTOM HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                -r * sin(phi0) - cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                -r * sin(phi1) - cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,-cylinderHalf))});
        }
    }

    capsuleVertCount = (int)verts.size();

    MIMITA_GL_CLEAR_STAGE("initCapsuleMesh");
    MIMITA_GL_CALL(glGenVertexArrays(1,&capsuleVAO));
    MIMITA_GL_CALL(glGenBuffers(1,&capsuleVBO));

    MIMITA_GL_CALL(glBindVertexArray(capsuleVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER,capsuleVBO));

    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER,
        verts.size()*sizeof(V),
        verts.data(),
        GL_STATIC_DRAW));

    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0));

    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv)));

    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
    MIMITA_GL_CALL(glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,normal)));
}

void uploadBodyPartMeshPart(const Mesh& mesh, int partIndex)
{
    MIMITA_GL_CLEAR_STAGE("uploadBodyPartMeshPart");
    if (!bodyPartVAOs) {
        int count = 8;
        bodyPartVAOs = new GLuint[count]();
        bodyPartVBOs = new GLuint[count]();
        bodyPartUploadGenerations = new uint64_t[count]();
        bodyPartCount = count;
    }
    if (partIndex < 0 || partIndex >= bodyPartCount) return;

    uint64_t expectedGen = reinterpret_cast<uint64_t>(mesh.verts.data()) ^ mesh.verts.size();
    if (bodyPartUploadGenerations[partIndex] == expectedGen)
    {
        MIMITA_GL_CALL(glBindVertexArray(bodyPartVAOs[partIndex]));
        return;
    }

    if (!bodyPartVAOs[partIndex])
        MIMITA_GL_CALL(glGenVertexArrays(1, &bodyPartVAOs[partIndex]));
    if (!bodyPartVBOs[partIndex])
        MIMITA_GL_CALL(glGenBuffers(1, &bodyPartVBOs[partIndex]));

    MIMITA_GL_CALL(glBindVertexArray(bodyPartVAOs[partIndex]));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, bodyPartVBOs[partIndex]));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));

    bodyPartUploadGenerations[partIndex] = expectedGen;
}

void uploadBodyPartMesh(const Mesh& mesh)
{
    MIMITA_GL_CLEAR_STAGE("uploadBodyPartMesh");
    if (!bodyPartVAO) MIMITA_GL_CALL(glGenVertexArrays(1, &bodyPartVAO));
    if (!bodyPartVBO) MIMITA_GL_CALL(glGenBuffers(1, &bodyPartVBO));

    MIMITA_GL_CALL(glBindVertexArray(bodyPartVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, bodyPartVBO));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_DYNAMIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
}

static void uploadPlayerMeshIfNeeded(const Mesh& mesh)
{
    if (playerVAO && playerUploadedVertCount == mesh.verts.size())
        return;

    MIMITA_GL_CLEAR_STAGE("uploadPlayerMeshIfNeeded");
    if (!playerVAO) MIMITA_GL_CALL(glGenVertexArrays(1, &playerVAO));
    if (!playerVBO) MIMITA_GL_CALL(glGenBuffers(1, &playerVBO));

    MIMITA_GL_CALL(glBindVertexArray(playerVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, playerVBO));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));

    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));

    playerUploadedVertCount = mesh.verts.size();
    printf("[PLAYER GLB] uploaded verts=%zu triangles=%zu batches=%zu\n",
           mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
}

void Player::render(unsigned int shader,
                    const glm::mat4& view,
                    const glm::mat4& proj) const
{
    const_cast<Player*>(this)->updateModelWorldTransforms();
    bool flash = spawnFlashTimer > 0.0f;
    renderCurrentPose(shader, view, proj, flash);
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
                               bool whiteOverride) const
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
        if (whiteOverride)
            glUniform4f(uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        glUniform1i(uTexLoc, 0);

        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;

            uploadBodyPartMeshPart(mesh, (int)i);

            const glm::mat4& model = part.worldTransform;
            glUniformMatrix4fv(uModelLoc, 1, 0, &model[0][0]);

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
