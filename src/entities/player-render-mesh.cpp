#include "player.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "physics/config.h"
#include "debug/debug-diag.h"
#include "debug/gl-debug.h"
#include "world/texture-store.h"

extern TextureStore gTextures;

GLuint capsuleVAO = 0;
GLuint capsuleVBO = 0;
int    capsuleVertCount = 0;

namespace {

// Body geometry is immutable after load and shared by every actor using the
// same model/avatar. Keeping the upload cache keyed by that identity means one
// actor performs the upload and all replicas reuse the same VAO/VBO instead of
// re-uploading their private mesh copies every frame.
struct CombinedBatch {
    GLint first = 0;
    GLsizei count = 0;
    uint16_t shaderPartIndex = 0;
    uint16_t sourcePartIndex = 0;
    uint16_t sourceBatchIndex = 0;
};

struct ModelUploadSet {
    std::vector<GLuint> partVaos;
    std::vector<GLuint> partVbos;
    std::vector<uint64_t> partGenerations;
    GLuint renderVao = 0;
    GLuint renderVbo = 0;
    uint64_t renderGeneration = ~0ull;
    GLuint combinedVao = 0;
    GLuint combinedVbo = 0;
    uint64_t combinedGeneration = ~0ull;
    GLsizei combinedVertexCount = 0;
    int combinedPartCount = 0;
    std::vector<CombinedBatch> combinedBatches;
};

std::unordered_map<std::string, ModelUploadSet> gModelUploads;

ModelUploadSet& uploadsFor(const std::string& modelKey)
{
    static const std::string fallback = "<unkeyed>";
    return gModelUploads[modelKey.empty() ? fallback : modelKey];
}

uint64_t partGeometryGeneration(const Mesh& mesh, int partIndex)
{
    return reinterpret_cast<uint64_t>(mesh.verts.data()) ^
        (static_cast<uint64_t>(mesh.verts.size()) << 1) ^
        (static_cast<uint64_t>(partIndex) << 32);
}

uint64_t meshGeometryGeneration(const Mesh& mesh)
{
    return reinterpret_cast<uint64_t>(mesh.verts.data()) ^
        (static_cast<uint64_t>(mesh.verts.size()) << 1);
}

} // namespace

GLuint bodyPartVAO = 0;
GLuint bodyPartVBO = 0;

void initCapsuleMesh()
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

void uploadBodyPartMeshPart(const Mesh& mesh, int partIndex, const std::string& modelKey)
{
    MIMITA_GL_CLEAR_STAGE("uploadBodyPartMeshPart");
    if (partIndex < 0 || mesh.verts.empty()) return;

    ModelUploadSet& uploads = uploadsFor(modelKey);
    const size_t required = static_cast<size_t>(partIndex) + 1;
    if (uploads.partVaos.size() < required) {
        uploads.partVaos.resize(required, 0);
        uploads.partVbos.resize(required, 0);
        uploads.partGenerations.resize(required, ~0ull);
    }

    const uint64_t expectedGen = partGeometryGeneration(mesh, partIndex);
    if (uploads.partGenerations[partIndex] == expectedGen &&
        uploads.partVaos[partIndex] != 0)
    {
        MIMITA_GL_CALL(glBindVertexArray(uploads.partVaos[partIndex]));
        return;
    }

    if (!uploads.partVaos[partIndex])
        MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.partVaos[partIndex]));
    if (!uploads.partVbos[partIndex])
        MIMITA_GL_CALL(glGenBuffers(1, &uploads.partVbos[partIndex]));

    MIMITA_GL_CALL(glBindVertexArray(uploads.partVaos[partIndex]));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, uploads.partVbos[partIndex]));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));

    uploads.partGenerations[partIndex] = expectedGen;
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

GLuint uploadPlayerMeshIfNeeded(const Mesh& mesh, const std::string& modelKey)
{
    ModelUploadSet& uploads = uploadsFor(modelKey);
    const uint64_t expectedGen = meshGeometryGeneration(mesh);
    if (uploads.renderVao && uploads.renderGeneration == expectedGen)
    {
        MIMITA_GL_CALL(glBindVertexArray(uploads.renderVao));
        return uploads.renderVao;
    }

    MIMITA_GL_CLEAR_STAGE("uploadPlayerMeshIfNeeded");
    if (!uploads.renderVao) MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.renderVao));
    if (!uploads.renderVbo) MIMITA_GL_CALL(glGenBuffers(1, &uploads.renderVbo));

    MIMITA_GL_CALL(glBindVertexArray(uploads.renderVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, uploads.renderVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));

    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));

    uploads.renderGeneration = expectedGen;
    printf("[PLAYER GLB] uploaded model=%s verts=%zu triangles=%zu batches=%zu\n",
           modelKey.c_str(), mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
    return uploads.renderVao;
}

namespace {

struct MergedBodyVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    float partIndex;
};

bool ensureCombinedBodyParts(const Player& player)
{
    const std::string uploadKey = player.renderUploadKey();
    ModelUploadSet& uploads = uploadsFor(uploadKey);
    const auto& parts = player.physicalBody.partMeshes;
    const auto& extras = player.physicalBody.extraPartMeshes;
    // Geometry and UV space are immutable per upload key; every actor using
    // this model + UV mode has a private mesh copy but identical layout.
    // Never rebuild per actor.
    const uint64_t gen = static_cast<uint64_t>(std::hash<std::string>{}(uploadKey));
    if (uploads.combinedVao && uploads.combinedGeneration == gen)
        return true;

    std::vector<MergedBodyVertex> verts;
    std::vector<CombinedBatch> batches;
    int shaderPartIndex = 0;
    auto append = [&](const std::vector<Mesh>& meshes, uint16_t sourceBase) {
        for (size_t i = 0; i < meshes.size(); ++i) {
            const Mesh& mesh = meshes[i];
            if (mesh.verts.empty())
                continue;
            const GLint base = static_cast<GLint>(verts.size());
            for (const Vertex& v : mesh.verts)
                verts.push_back({v.pos, v.normal, v.uv,
                                 static_cast<float>(shaderPartIndex)});
            for (size_t b = 0; b < mesh.batches.size(); ++b) {
                const Mesh::Batch& batch = mesh.batches[b];
                if (batch.count == 0)
                    continue;
                batches.push_back({base + static_cast<GLint>(batch.first),
                                   static_cast<GLsizei>(batch.count),
                                   static_cast<uint16_t>(shaderPartIndex),
                                   static_cast<uint16_t>(sourceBase + i),
                                   static_cast<uint16_t>(b)});
            }
            ++shaderPartIndex;
        }
    };
    append(parts, 0);
    append(extras, static_cast<uint16_t>(parts.size()));
    if (verts.empty() || shaderPartIndex > 32)
        return false;

    if (!uploads.combinedVao)
        MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.combinedVao));
    if (!uploads.combinedVbo)
        MIMITA_GL_CALL(glGenBuffers(1, &uploads.combinedVbo));

    MIMITA_GL_CALL(glBindVertexArray(uploads.combinedVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, uploads.combinedVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(MergedBodyVertex),
                                verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MergedBodyVertex), (void*)offsetof(MergedBodyVertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MergedBodyVertex), (void*)offsetof(MergedBodyVertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MergedBodyVertex), (void*)offsetof(MergedBodyVertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
    MIMITA_GL_CALL(glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(MergedBodyVertex), (void*)offsetof(MergedBodyVertex, partIndex)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(8));

    uploads.combinedGeneration = gen;
    uploads.combinedVertexCount = static_cast<GLsizei>(verts.size());
    uploads.combinedPartCount = shaderPartIndex;
    uploads.combinedBatches = std::move(batches);
    printf("[PLAYER GLB] combined body model=%s parts=%d verts=%zu batches=%zu\n",
           uploadKey.c_str(), shaderPartIndex, verts.size(), uploads.combinedBatches.size());
    return true;
}

GLuint combinedBatchTexture(const Player& player, const CombinedBatch& batch)
{
    const auto& parts = player.physicalBody.partMeshes;
    const auto& extras = player.physicalBody.extraPartMeshes;
    if (batch.sourcePartIndex < parts.size()) {
        const Mesh& mesh = parts[batch.sourcePartIndex];
        if (batch.sourceBatchIndex < mesh.batches.size())
            return mesh.batches[batch.sourceBatchIndex].texture;
    } else {
        const size_t extraIndex = batch.sourcePartIndex - parts.size();
        if (extraIndex < extras.size()) {
            const Mesh& mesh = extras[extraIndex];
            if (batch.sourceBatchIndex < mesh.batches.size())
                return mesh.batches[batch.sourceBatchIndex].texture;
        }
    }
    return 0;
}

} // namespace

bool drawCombinedBodyParts(const Player& player, unsigned int shader)
{
    if (!ensureCombinedBodyParts(player))
        return false;

    ModelUploadSet& uploads = uploadsFor(player.renderUploadKey());
    if (uploads.combinedPartCount <= 0 ||
        uploads.combinedPartCount > 32 ||
        uploads.combinedBatches.empty())
        return false;

    std::array<glm::mat4, 32> partModels{};
    int partCount = 0;
    auto appendMatrices = [&](const std::vector<PhysicalBodyPart>& sourceParts,
                              const std::vector<Mesh>& meshes) {
        for (size_t i = 0; i < meshes.size() && partCount < 32; ++i) {
            if (meshes[i].verts.empty())
                continue;
            partModels[partCount++] = sourceParts[i].worldTransform;
        }
    };
    appendMatrices(player.physicalBody.parts, player.physicalBody.partMeshes);
    appendMatrices(player.physicalBody.extraParts, player.physicalBody.extraPartMeshes);
    if (partCount != uploads.combinedPartCount)
        return false;

    static GLint usePartModelsLoc = -1;
    static GLint partModelsLoc = -1;
    if (usePartModelsLoc < 0)
        usePartModelsLoc = glGetUniformLocation(shader, "uUsePartModels");
    if (partModelsLoc < 0)
        partModelsLoc = glGetUniformLocation(shader, "uPartModels");
    if (usePartModelsLoc < 0 || partModelsLoc < 0)
        return false;

    glUniform1i(usePartModelsLoc, 1);
    glUniformMatrix4fv(partModelsLoc, partCount, GL_FALSE, &partModels[0][0][0]);
    glBindVertexArray(uploads.combinedVao);

    const GLuint firstTexture = combinedBatchTexture(player, uploads.combinedBatches.front());
    bool singleTexture = true;
    for (const CombinedBatch& batch : uploads.combinedBatches) {
        if (combinedBatchTexture(player, batch) != firstTexture) {
            singleTexture = false;
            break;
        }
    }

    glDisable(GL_CULL_FACE);
    if (singleTexture) {
        glBindTexture(GL_TEXTURE_2D, firstTexture ? firstTexture : gTextures.get("default"));
        glDrawArrays(GL_TRIANGLES, 0, uploads.combinedVertexCount);
    } else {
        for (const CombinedBatch& batch : uploads.combinedBatches) {
            const GLuint texture = combinedBatchTexture(player, batch);
            glBindTexture(GL_TEXTURE_2D, texture ? texture : gTextures.get("default"));
            glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
        }
    }
    glUniform1i(usePartModelsLoc, 0);
    return true;
}
