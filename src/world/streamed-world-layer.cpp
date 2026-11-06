// 2026-11-05
#include "world/streamed-world-layer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "physics/physical-entity.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;
extern void setUniforms(GLuint shader, const glm::vec3& cameraPos);

namespace {

const glm::vec3 kCubePositions[8] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

const int kCubeFaces[6][4] = {
    {4, 5, 6, 7}, // +X
    {1, 0, 3, 2}, // -X
    {3, 7, 6, 2}, // +Y
    {0, 1, 5, 4}, // -Y
    {3, 2, 1, 0}, // +Z
    {4, 5, 6, 7}, // -Z (unused in normal mapping, kept for indexing)
};

// Face order used for material overrides: +X, -X, +Y, -Y, +Z, -Z.
const glm::vec3 kFaceNormals[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};
const int kFaceOrder[6][4] = {
    {4, 5, 6, 7}, // +X
    {1, 0, 3, 2}, // -X
    {3, 7, 6, 2}, // +Y
    {0, 1, 5, 4}, // -Y
    {7, 6, 2, 3}, // +Z
    {4, 5, 1, 0}, // -Z
};

glm::mat4 partMatrix(const ChunkPart& part)
{
    return glm::translate(glm::mat4(1.0f), part.position) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.z),
                       glm::vec3(0, 0, 1)) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.y),
                       glm::vec3(0, 1, 0)) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.x),
                       glm::vec3(1, 0, 0)) *
           glm::scale(glm::mat4(1.0f), part.size);
}

GLuint resolveMaterial(const std::string& material)
{
    if (material.empty()) return gTextures.get("default");
    if (material.find('/') != std::string::npos ||
        material.find('\\') != std::string::npos ||
        material.find('.') != std::string::npos)
        return gTextures.getPath(material);
    return gTextures.get(material);
}

void appendFace(std::vector<Vertex>& out, const ChunkPart& part, int face,
                const std::string& material, std::vector<Mesh::Batch>& batches,
                const glm::mat4& localTransform)
{
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(localTransform)));
    const size_t first = out.size();
    const int order[6] = {0, 1, 2, 0, 2, 3};
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i : order) {
        const glm::vec3 local = kCubePositions[kFaceOrder[face][i]];
        Vertex v;
        v.pos = glm::vec3(localTransform * glm::vec4(local, 1.0f));
        const glm::vec3 n = glm::normalize(normalMatrix * kFaceNormals[face]);
        v.normal = n;
        v.uv = uv[i];
        out.push_back(v);
    }
    Mesh::Batch batch;
    batch.materialName = material.empty() ? "default" : material;
    batch.texture = resolveMaterial(material);
    batch.first = first;
    batch.count = 6;
    batch.hasBounds = true;
    batch.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    batch.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (size_t i = first; i < out.size(); ++i) {
        batch.boundsMin = glm::min(batch.boundsMin, out[i].pos);
        batch.boundsMax = glm::max(batch.boundsMax, out[i].pos);
    }
    batches.push_back(std::move(batch));
}

} // namespace

StreamedWorldLayer& StreamedWorldLayer::instance()
{
    static StreamedWorldLayer layer;
    return layer;
}

StreamedWorldLayer::~StreamedWorldLayer()
{
    clear();
}

void StreamedWorldLayer::clear()
{
    for (const ChunkRecord& record : mChunks) {
        if (record.collisionEntity)
            PhysicalEntitySystem::instance().remove(record.collisionEntity);
        if (record.vao) glDeleteVertexArrays(1, &record.vao);
        if (record.vbo) glDeleteBuffers(1, &record.vbo);
    }
    mChunks.clear();
    for (Template& templateData : mTemplates) {
        if (templateData.vao) glDeleteVertexArrays(1, &templateData.vao);
        if (templateData.vbo) glDeleteBuffers(1, &templateData.vbo);
        if (templateData.instanceVbo) glDeleteBuffers(1, &templateData.instanceVbo);
    }
    mTemplates.clear();
}

StreamedWorldLayer::Template* StreamedWorldLayer::ensureTemplate(
    const std::string& chunkId)
{
    for (Template& templateData : mTemplates)
        if (templateData.chunkId == chunkId) return &templateData;
    Template templateData;
    templateData.chunkId = chunkId;
    mTemplates.push_back(std::move(templateData));
    return &mTemplates.back();
}

void StreamedWorldLayer::rebuildTemplate(Template& templateData,
                                         const std::string& chunkId)
{
    const AuthoredChunk* chunk = nullptr;
    for (const auto& candidate : MapConfigRegistry::instance().chunks())
        if (candidate.id == chunkId) { chunk = &candidate; break; }
    if (!chunk) return;

    templateData.verts.clear();
    templateData.batches.clear();
    for (const ChunkPart& part : chunk->parts) {
        if (!part.visible) continue;
        const glm::mat4 local = partMatrix(part);
        for (int face = 0; face < 6; ++face) {
            const std::string material = part.faceMaterials[face].empty()
                ? part.material : part.faceMaterials[face];
            appendFace(templateData.verts, part, face, material,
                       templateData.batches, local);
        }
    }
    templateData.vboDirty = true;
}

void StreamedWorldLayer::uploadTemplate(Template& templateData)
{
    if (templateData.vao == 0) glGenVertexArrays(1, &templateData.vao);
    if (templateData.vbo == 0) glGenBuffers(1, &templateData.vbo);
    glBindVertexArray(templateData.vao);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(templateData.verts.size() * sizeof(Vertex)),
                 templateData.verts.empty() ? nullptr : templateData.verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));

    // Instance matrix attributes (locations 9..12) with divisor 1.
    if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.instanceVbo);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(9 + i);
        glVertexAttribPointer(9 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(9 + i, 1);
    }
    glBindVertexArray(0);
    templateData.vboDirty = false;
}

void StreamedWorldLayer::uploadInstanceBuffer(Template& templateData)
{
    if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(templateData.instances.size() * sizeof(glm::mat4)),
                 templateData.instances.empty() ? nullptr
                                                : templateData.instances.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLuint StreamedWorldLayer::makeChunkVao(GLuint vbo, GLuint /*instanceVbo*/)
{
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glBindVertexArray(0);
    return vao;
}

void StreamedWorldLayer::addChunk(uint64_t key, const AuthoredChunk& chunk,
                                  const glm::mat4& transform)
{
    Template* templateData = ensureTemplate(chunk.id);
    if (templateData->verts.empty())
        rebuildTemplate(*templateData, chunk.id);
    if (templateData->vboDirty)
        uploadTemplate(*templateData);

    // Collision proxy: world-space triangles registered as a static entity.
    std::vector<CollisionTriangle> worldTriangles;
    for (const ChunkPart& part : chunk.parts) {
        if (!part.collision) continue;
        std::vector<CollisionTriangle> local;
        buildBoxCollisionTriangles(local, glm::vec3(0.0f), part.size * 0.5f);
        const glm::mat4 localToWorld = transform * partMatrix(part);
        const glm::mat3 normalMatrix =
            glm::transpose(glm::inverse(glm::mat3(localToWorld)));
        for (const CollisionTriangle& t : local) {
            CollisionTriangle world;
            world.a = glm::vec3(localToWorld * glm::vec4(t.a, 1.0f));
            world.b = glm::vec3(localToWorld * glm::vec4(t.b, 1.0f));
            world.c = glm::vec3(localToWorld * glm::vec4(t.c, 1.0f));
            world.normal = glm::normalize(normalMatrix * t.normal);
            worldTriangles.push_back(world);
        }
    }

    ChunkRecord record;
    record.key = key;
    record.chunkId = chunk.id;
    record.transform = transform;

    if (mInstancing) {
        templateData->instances.push_back(transform);
        uploadInstanceBuffer(*templateData);
    } else {
        // Bake chunk-local geometry to world space and upload a per-chunk VBO.
        std::vector<Vertex> worldVerts;
        worldVerts.reserve(templateData->verts.size());
        for (const Vertex& v : templateData->verts) {
            Vertex copy = v;
            copy.pos = glm::vec3(transform * glm::vec4(v.pos, 1.0f));
            worldVerts.push_back(copy);
        }
        record.batches = templateData->batches;
        glGenBuffers(1, &record.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, record.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(worldVerts.size() * sizeof(Vertex)),
                     worldVerts.data(), GL_STATIC_DRAW);
        record.vao = makeChunkVao(record.vbo, 0);
    }

    if (!worldTriangles.empty()) {
        record.collisionEntity = PhysicalEntitySystem::instance().add(
            worldTriangles, glm::mat4(1.0f), PhysicalEntityMotion::Static, 0);
        if (PhysicalEntity* entity =
                PhysicalEntitySystem::instance().find(record.collisionEntity)) {
            entity->renderEnabled = false;
            entity->collidesWithActors = true;
        }
    }

    mChunks.push_back(std::move(record));
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.chunk.applied", chunk.id,
        "streamed chunk installed", 0,
        {{"chunk", chunk.id}, {"key", key}, {"chunks", mChunks.size()},
         {"triangles", worldTriangles.size()},
         {"backend", mInstancing ? "instancing" : "vbo"}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::removeChunk(uint64_t key)
{
    for (size_t i = 0; i < mChunks.size(); ++i) {
        if (mChunks[i].key != key) continue;
        ChunkRecord record = std::move(mChunks[i]);
        mChunks.erase(mChunks.begin() + i);

        if (record.collisionEntity)
            PhysicalEntitySystem::instance().remove(record.collisionEntity);
        if (!mInstancing) {
            if (record.vao) glDeleteVertexArrays(1, &record.vao);
            if (record.vbo) glDeleteBuffers(1, &record.vbo);
        } else {
            for (Template& templateData : mTemplates) {
                if (templateData.chunkId != record.chunkId) continue;
                auto found = std::find(templateData.instances.begin(),
                                       templateData.instances.end(),
                                       record.transform);
                if (found != templateData.instances.end())
                    templateData.instances.erase(found);
                uploadInstanceBuffer(templateData);
                break;
            }
        }
        return;
    }
}

void StreamedWorldLayer::updateMaterial(const std::string& chunkId,
                                        const std::string& partId, int face,
                                        const std::string& material)
{
    for (auto& chunk : MapConfigRegistry::instance().chunksMutable()) {
        if (chunk.id != chunkId) continue;
        for (auto& part : chunk.parts) {
            if (part.id != partId) continue;
            if (face < 0)
                part.material = material;
            else if (face < 6)
                part.faceMaterials[face] = material;
        }
        MapConfigRegistry::instance().markChanged();
        break;
    }
    for (Template& templateData : mTemplates) {
        if (templateData.chunkId == chunkId) {
            rebuildTemplate(templateData, chunkId);
            uploadTemplate(templateData);
            break;
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "chunk.part.material-changed", chunkId,
        "chunk part material changed live", 0,
        {{"chunk", chunkId}, {"part", partId}, {"face", face},
         {"material", material}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::render(const Camera& camera)
{
    if (!gRenderer || !gRenderer->shaderProgram || mChunks.empty())
        return;

    glUseProgram(gRenderer->shaderProgram);
    const glm::mat4 view = camera.getView();
    const glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth,
                                          (float)gRenderer->sceneHeight);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"),
                       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"),
                       1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUsePartModels"), 0);
    setUniforms(gRenderer->shaderProgram, camera.pos);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glActiveTexture(GL_TEXTURE0);

    if (mInstancing) {
        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseInstanceModel"), 1);
        const glm::mat4 identity(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                           1, GL_FALSE, &identity[0][0]);
        for (Template& templateData : mTemplates) {
            if (templateData.instances.empty()) continue;
            glBindVertexArray(templateData.vao);
            for (const Mesh::Batch& batch : templateData.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture);
                glDrawArraysInstanced(GL_TRIANGLES, (GLint)batch.first,
                                      (GLsizei)batch.count,
                                      (GLsizei)templateData.instances.size());
            }
        }
    } else {
        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseInstanceModel"), 0);
        const glm::mat4 identity(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                           1, GL_FALSE, &identity[0][0]);
        for (const ChunkRecord& record : mChunks) {
            if (record.vao == 0) continue;
            glBindVertexArray(record.vao);
            for (const Mesh::Batch& batch : record.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture);
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first,
                             (GLsizei)batch.count);
            }
        }
    }
    glBindVertexArray(0);
    glUseProgram(0);
}
