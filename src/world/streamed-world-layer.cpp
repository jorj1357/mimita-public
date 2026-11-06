// 2026-11-05
#include "world/streamed-world-layer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_set>
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

// Local axes used for each face's U and V texture coordinates.
const int kFaceUvAxis[6][2] = {
    {2, 1}, // +X: z, y
    {2, 1}, // -X
    {0, 2}, // +Y: x, z
    {0, 2}, // -Y
    {0, 1}, // +Z: x, y
    {0, 1}, // -Z
};

glm::vec2 faceUvScale(const ChunkPart& part, int face)
{
    glm::vec2 scale = part.uvScale;
    if (scale.x <= 0.0f) scale.x = 1.0f;
    if (scale.y <= 0.0f) scale.y = 1.0f;
    const glm::vec2& overrideScale = part.faceUvScale[face];
    if (overrideScale.x > 0.0f) scale.x = overrideScale.x;
    if (overrideScale.y > 0.0f) scale.y = overrideScale.y;
    return scale;
}

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

std::string aliasedMaterial(const std::string& material)
{
    if (material == "stone_floor") return "floorv2";
    if (material == "stone_wall") return "wallv2";
    if (material == "stone") return "stonev2";
    return material;
}

GLuint resolveMaterial(const std::string& material)
{
    if (material.empty()) return gTextures.get("default");
    std::string resolved = aliasedMaterial(material);
    if (resolved != material) {
        static std::unordered_set<std::string> logged;
        if (logged.insert(material).second) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "world-stream.material.alias", material,
                "legacy chunk material resolved to an existing texture", 0,
                {{"requested", material}, {"resolved", resolved}},
                __FILE__, __LINE__, __func__);
        }
    }
    if (resolved.find('/') != std::string::npos ||
        resolved.find('\\') != std::string::npos ||
        resolved.find('.') != std::string::npos)
        return gTextures.getPath(resolved);
    return gTextures.get(resolved);
}

void appendFace(std::vector<Vertex>& out, const ChunkPart& part, int face,
                const std::string& material, std::vector<Mesh::Batch>& batches,
                const glm::mat4& localTransform)
{
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(localTransform)));
    const size_t first = out.size();
    const int order[6] = {0, 1, 2, 0, 2, 3};
    // One texture repeat per world unit by default, then per-part and per-face
    // scale overrides. UVs are derived from local face extents, not 0..1, so a
    // large primitive keeps the same texel density as a small one.
    const int uAxis = kFaceUvAxis[face][0];
    const int vAxis = kFaceUvAxis[face][1];
    const glm::vec2 scale = faceUvScale(part, face);
    const float extentU = part.size[uAxis];
    const float extentV = part.size[vAxis];
    for (int i : order) {
        const glm::vec3 local = kCubePositions[kFaceOrder[face][i]];
        Vertex v;
        v.pos = glm::vec3(localTransform * glm::vec4(local, 1.0f));
        const glm::vec3 n = glm::normalize(normalMatrix * kFaceNormals[face]);
        v.normal = n;
        v.uv = glm::vec2(
            (local[uAxis] + 0.5f) * extentU * scale.x,
            (local[vAxis] + 0.5f) * extentV * scale.y);
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
    if (Template* existing = findTemplate(chunkId)) return existing;
    Template templateData;
    templateData.chunkId = chunkId;
    mTemplates.push_back(std::move(templateData));
    return &mTemplates.back();
}

StreamedWorldLayer::Template* StreamedWorldLayer::findTemplate(
    const std::string& chunkId)
{
    for (Template& templateData : mTemplates)
        if (templateData.chunkId == chunkId) return &templateData;
    return nullptr;
}

void StreamedWorldLayer::rebuildTemplate(Template& templateData,
                                         const std::string& chunkId)
{
    const AuthoredChunk* chunk = nullptr;
    for (const auto& candidate : MapConfigRegistry::instance().chunks())
        if (candidate.id == chunkId) { chunk = &candidate; break; }
    if (!chunk) return;
    rebuildTemplateFromChunk(templateData, *chunk);
}

void StreamedWorldLayer::rebuildTemplateFromChunk(
    Template& templateData, const AuthoredChunk& chunk)
{
    templateData.verts.clear();
    templateData.batches.clear();
    for (const ChunkPart& part : chunk.parts) {
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

void StreamedWorldLayer::rebuildRecordVbo(ChunkRecord& record,
                                          const Template& templateData)
{
    record.batches = templateData.batches;
    std::vector<Vertex> worldVerts;
    worldVerts.reserve(templateData.verts.size());
    for (const Vertex& v : templateData.verts) {
        Vertex copy = v;
        copy.pos = glm::vec3(record.transform * glm::vec4(v.pos, 1.0f));
        worldVerts.push_back(copy);
    }
    if (record.vbo == 0) glGenBuffers(1, &record.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, record.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(worldVerts.size() * sizeof(Vertex)),
                 worldVerts.empty() ? nullptr : worldVerts.data(),
                 GL_STATIC_DRAW);
    if (record.vao == 0) record.vao = makeChunkVao(record.vbo, 0);
}

void StreamedWorldLayer::rebuildInstances()
{
    for (Template& templateData : mTemplates)
        templateData.instances.clear();
    for (const ChunkRecord& record : mChunks) {
        Template* templateData = findTemplate(record.chunkId);
        if (!templateData) continue;
        templateData->instances.push_back(record.transform);
    }
    for (Template& templateData : mTemplates)
        uploadInstanceBuffer(templateData);
}

void StreamedWorldLayer::setInstancing(bool enabled)
{
    if (mInstancing == enabled) return;
    mInstancing = enabled;
    size_t rebuilt = 0;
    if (mInstancing) {
        for (ChunkRecord& record : mChunks) {
            if (record.vao) glDeleteVertexArrays(1, &record.vao);
            if (record.vbo) glDeleteBuffers(1, &record.vbo);
            record.vao = 0;
            record.vbo = 0;
            record.batches.clear();
        }
        for (Template& templateData : mTemplates) {
            if (templateData.vao == 0 || templateData.vboDirty)
                uploadTemplate(templateData);
        }
        rebuildInstances();
    } else {
        for (Template& templateData : mTemplates) {
            templateData.instances.clear();
            uploadInstanceBuffer(templateData);
        }
        for (ChunkRecord& record : mChunks) {
            Template* templateData = findTemplate(record.chunkId);
            if (!templateData) continue;
            if (templateData->vao == 0 || templateData->vboDirty)
                uploadTemplate(*templateData);
            rebuildRecordVbo(record, *templateData);
            ++rebuilt;
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.backend.changed", enabled ? "instancing" : "vbo",
        "streamed chunk backend switched without reloading chunks", 0,
        {{"backend", enabled ? "instancing" : "vbo"},
         {"chunks", mChunks.size()},
         {"templates", mTemplates.size()},
         {"rebuilt", rebuilt}},
        __FILE__, __LINE__, __func__);
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
        rebuildRecordVbo(record, *templateData);
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

void StreamedWorldLayer::refreshChunk(const AuthoredChunk& chunk)
{
    Template* templateData = findTemplate(chunk.id);
    if (!templateData) return;
    rebuildTemplateFromChunk(*templateData, chunk);
    uploadTemplate(*templateData);

    size_t records = 0;
    if (!mInstancing) {
        for (ChunkRecord& record : mChunks) {
            if (record.chunkId != chunk.id) continue;
            rebuildRecordVbo(record, *templateData);
            ++records;
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.template.updated", chunk.id,
        "loaded chunk template refreshed without collision rebuild", 0,
        {{"chunk", chunk.id},
         {"verts", templateData->verts.size()},
         {"batches", templateData->batches.size()},
         {"vbo_records", records},
         {"backend", mInstancing ? "instancing" : "vbo"},
         {"collision_rebuilt", false}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::updateMaterial(const std::string& chunkId,
                                        const std::string& partId, int face,
                                        const std::string& material)
{
    AuthoredChunk* changed = nullptr;
    for (auto& chunk : MapConfigRegistry::instance().chunksMutable()) {
        if (chunk.id != chunkId) continue;
        for (auto& part : chunk.parts) {
            if (part.id != partId) continue;
            if (face < 0)
                part.material = material;
            else if (face < 6)
                part.faceMaterials[face] = material;
        }
        changed = &chunk;
        MapConfigRegistry::instance().markChanged();
        break;
    }
    if (changed) refreshChunk(*changed);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "chunk.part.material-changed", chunkId,
        "chunk part material changed live", 0,
        {{"chunk", chunkId}, {"part", partId}, {"face", face},
         {"material", material}, {"found", changed != nullptr}},
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
    // Shared-shader state: leaving uUseInstanceModel enabled corrupts the next
    // frame's base-world and actor draws because their VAOs have no instance
    // matrix attributes.
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram,
                                     "uUseInstanceModel"), 0);
    glBindVertexArray(0);
    glUseProgram(0);
}
