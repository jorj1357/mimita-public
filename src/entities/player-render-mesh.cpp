#include "player.h"
#include "game/gl-resource-bridge-module.h"

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
#include "render/render-command.h"
#include "render/shader-program.h"

namespace r = mimita::game::render;

extern TextureStore gTextures;

std::uint32_t capsuleVAO = 0;
std::uint32_t capsuleVBO = 0;
int    capsuleVertCount = 0;

namespace {

// Body geometry is immutable after load and shared by every actor using the
// same model/avatar. Keeping the upload cache keyed by that identity means one
// actor performs the upload and all replicas reuse the same VAO/VBO instead of
// re-uploading their private mesh copies every frame.
struct CombinedBatch {
    std::int32_t first = 0;
    std::int32_t count = 0;
    uint16_t shaderPartIndex = 0;
    uint16_t sourcePartIndex = 0;
    uint16_t sourceBatchIndex = 0;
};

struct ModelUploadSet {
    std::vector<std::uint32_t> partVaos;
    std::vector<std::uint32_t> partVbos;
    std::vector<uint64_t> partGenerations;
    std::uint32_t renderVao = 0;
    std::uint32_t renderVbo = 0;
    uint64_t renderGeneration = ~0ull;
    std::uint32_t combinedVao = 0;
    std::uint32_t combinedVbo = 0;
    uint64_t combinedGeneration = ~0ull;
    std::int32_t combinedVertexCount = 0;
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

std::size_t playerModelUploadCacheCount()
{
    return gModelUploads.size();
}

std::uint32_t bodyPartVAO = 0;
std::uint32_t bodyPartVBO = 0;

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
    capsuleVAO = mimita::game::genVertexArray();
    capsuleVBO = mimita::game::genBuffer();

    r::bindVertexArray(capsuleVAO);
    r::bindBuffer(r::BufferTarget::Array, capsuleVBO);
    r::bufferData(r::BufferTarget::Array, capsuleVBO, verts.data(),
                  static_cast<std::uint32_t>(verts.size() * sizeof(V)),
                  r::BufferUsage::Static);

    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(V), 0);

    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(V),
                           static_cast<std::uint32_t>(offsetof(V, uv)));

    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(V),
                           static_cast<std::uint32_t>(offsetof(V, normal)));
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
        r::bindVertexArray(uploads.partVaos[partIndex]);
        return;
    }

    if (!uploads.partVaos[partIndex])
        uploads.partVaos[partIndex] = mimita::game::genVertexArray();
    if (!uploads.partVbos[partIndex])
        uploads.partVbos[partIndex] = mimita::game::genBuffer();

    r::bindVertexArray(uploads.partVaos[partIndex]);
    r::bindBuffer(r::BufferTarget::Array, uploads.partVbos[partIndex]);
    r::bufferData(r::BufferTarget::Array, uploads.partVbos[partIndex],
                  mesh.verts.data(),
                  static_cast<std::uint32_t>(mesh.verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Static);

    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::enableVertexAttribArray(2);

    uploads.partGenerations[partIndex] = expectedGen;
}

void uploadBodyPartMesh(const Mesh& mesh)
{
    MIMITA_GL_CLEAR_STAGE("uploadBodyPartMesh");
    if (!bodyPartVAO) bodyPartVAO = mimita::game::genVertexArray();
    if (!bodyPartVBO) bodyPartVBO = mimita::game::genBuffer();

    r::bindVertexArray(bodyPartVAO);
    r::bindBuffer(r::BufferTarget::Array, bodyPartVBO);
    r::bufferData(r::BufferTarget::Array, bodyPartVBO, mesh.verts.data(),
                  static_cast<std::uint32_t>(mesh.verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Dynamic);

    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::enableVertexAttribArray(2);
}

std::uint32_t uploadPlayerMeshIfNeeded(const Mesh& mesh, const std::string& modelKey)
{
    ModelUploadSet& uploads = uploadsFor(modelKey);
    const uint64_t expectedGen = meshGeometryGeneration(mesh);
    if (uploads.renderVao && uploads.renderGeneration == expectedGen)
    {
        r::bindVertexArray(uploads.renderVao);
        return uploads.renderVao;
    }

    MIMITA_GL_CLEAR_STAGE("uploadPlayerMeshIfNeeded");
    if (!uploads.renderVao) uploads.renderVao = mimita::game::genVertexArray();
    if (!uploads.renderVbo) uploads.renderVbo = mimita::game::genBuffer();

    r::bindVertexArray(uploads.renderVao);
    r::bindBuffer(r::BufferTarget::Array, uploads.renderVbo);
    r::bufferData(r::BufferTarget::Array, uploads.renderVbo, mesh.verts.data(),
                  static_cast<std::uint32_t>(mesh.verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Static);

    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(0);

    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::enableVertexAttribArray(1);

    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::enableVertexAttribArray(2);

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
            const std::int32_t base = static_cast<std::int32_t>(verts.size());
            for (const Vertex& v : mesh.verts)
                verts.push_back({v.pos, v.normal, v.uv,
                                 static_cast<float>(shaderPartIndex)});
            for (size_t b = 0; b < mesh.batches.size(); ++b) {
                const Mesh::Batch& batch = mesh.batches[b];
                if (batch.count == 0)
                    continue;
                batches.push_back({base + static_cast<std::int32_t>(batch.first),
                                   static_cast<std::int32_t>(batch.count),
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
        uploads.combinedVao = mimita::game::genVertexArray();
    if (!uploads.combinedVbo)
        uploads.combinedVbo = mimita::game::genBuffer();

    r::bindVertexArray(uploads.combinedVao);
    r::bindBuffer(r::BufferTarget::Array, uploads.combinedVbo);
    r::bufferData(r::BufferTarget::Array, uploads.combinedVbo, verts.data(),
                  static_cast<std::uint32_t>(verts.size() * sizeof(MergedBodyVertex)),
                  r::BufferUsage::Static);

    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(MergedBodyVertex),
                           static_cast<std::uint32_t>(offsetof(MergedBodyVertex, pos)));
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(1, 2, r::VertexType::Float, false, sizeof(MergedBodyVertex),
                           static_cast<std::uint32_t>(offsetof(MergedBodyVertex, uv)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(2, 3, r::VertexType::Float, false, sizeof(MergedBodyVertex),
                           static_cast<std::uint32_t>(offsetof(MergedBodyVertex, normal)));
    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(8, 1, r::VertexType::Float, false, sizeof(MergedBodyVertex),
                           static_cast<std::uint32_t>(offsetof(MergedBodyVertex, partIndex)));
    r::enableVertexAttribArray(8);

    uploads.combinedGeneration = gen;
    uploads.combinedVertexCount = static_cast<std::int32_t>(verts.size());
    uploads.combinedPartCount = shaderPartIndex;
    uploads.combinedBatches = std::move(batches);
    printf("[PLAYER GLB] combined body model=%s parts=%d verts=%zu batches=%zu\n",
           uploadKey.c_str(), shaderPartIndex, verts.size(), uploads.combinedBatches.size());
    return true;
}

std::uint32_t combinedBatchTexture(const Player& player, const CombinedBatch& batch)
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

bool drawCombinedBodyParts(const Player& player, mimita::game::render::ShaderProgram& shader, bool outlinePass)
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

    static std::int32_t usePartModelsLoc = -1;
    static std::int32_t partModelsLoc = -1;
    if (usePartModelsLoc < 0)
        usePartModelsLoc = shader.uniform("uUsePartModels");
    if (partModelsLoc < 0)
        partModelsLoc = shader.uniform("uPartModels");
    if (usePartModelsLoc < 0 || partModelsLoc < 0)
        return false;

    r::uniform1i(usePartModelsLoc, 1);
    r::uniformMatrix4fv(partModelsLoc, partCount, &partModels[0][0][0]);
    r::bindVertexArray(uploads.combinedVao);

    const std::uint32_t firstTexture = combinedBatchTexture(player, uploads.combinedBatches.front());
    bool singleTexture = true;
    for (const CombinedBatch& batch : uploads.combinedBatches) {
        if (combinedBatchTexture(player, batch) != firstTexture) {
            singleTexture = false;
            break;
        }
    }

      // The combined path must use the same front-face culling as the
      // per-part outline path. Rendering both faces of the expanded shell
      // filled the actor with the outline color instead of drawing a
      // silhouette, which is the large red/green "outline" seen in TDM.
      if (outlinePass) {
          r::enable(r::Capability::CullFace);
          r::cullFace(r::CullFaceMode::Front);
      } else {
          r::disable(r::Capability::CullFace);
      }
      if (singleTexture) {
          r::bindTexture(r::TextureTarget::Texture2D,
                         firstTexture ? firstTexture : gTextures.get("default"));
        r::drawArrays(r::Primitive::Triangles, 0, uploads.combinedVertexCount);
    } else {
        for (const CombinedBatch& batch : uploads.combinedBatches) {
            const std::uint32_t texture = combinedBatchTexture(player, batch);
            r::bindTexture(r::TextureTarget::Texture2D,
                           texture ? texture : gTextures.get("default"));
            r::drawArrays(r::Primitive::Triangles,
                          static_cast<std::int32_t>(batch.first),
                          static_cast<std::int32_t>(batch.count));
        }
    }
    r::uniform1i(usePartModelsLoc, 0);
    return true;
}
