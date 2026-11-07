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
#include "config/collision-lod-config.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "map/map-static-union.h"
#include "modules/api/chunk-collision-api.h"
#include "physics/physical-entity.h"
#include "render/world-box-mesh.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;
extern void setUniforms(GLuint shader, const glm::vec3& cameraPos);

namespace {

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

void appendBoxFaces(std::vector<Vertex>& out, const ChunkPart& part,
                    std::vector<Mesh::Batch>& batches,
                    const glm::mat4& localTransform)
{
    mimita_world_rendering::BoxMeshV1 request{};
    const float* transformValues = &localTransform[0][0];
    for (int i = 0; i < 16; ++i)
        request.transform[i] = transformValues[i];
    request.size[0] = part.size.x;
    request.size[1] = part.size.y;
    request.size[2] = part.size.z;
    request.uvScale[0] = part.uvScale.x > 0.0f ? part.uvScale.x : 1.0f;
    request.uvScale[1] = part.uvScale.y > 0.0f ? part.uvScale.y : 1.0f;
    for (int face = 0; face < 6; ++face) {
        const std::string& material = part.faceMaterials[face].empty()
            ? part.material : part.faceMaterials[face];
        std::snprintf(request.materials[face], sizeof(request.materials[face]),
                      "%s", material.c_str());
        request.faceUvScale[face][0] = part.faceUvScale[face].x;
        request.faceUvScale[face][1] = part.faceUvScale[face].y;
    }

    mimita_world_rendering::BoxMeshV1 result{};
    if (!buildBoxMesh(request, result))
        return;

    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int face = 0; face < 6; ++face) {
        const mimita_world_rendering::MeshFaceV1& faceMesh = result.faces[face];
        const size_t first = out.size();
        for (int i : order) {
            Vertex vertex;
            vertex.pos = glm::vec3(faceMesh.positions[i][0],
                                   faceMesh.positions[i][1],
                                   faceMesh.positions[i][2]);
            vertex.normal = glm::vec3(faceMesh.normal[0], faceMesh.normal[1],
                                      faceMesh.normal[2]);
            vertex.uv = glm::vec2(faceMesh.uv[i][0], faceMesh.uv[i][1]);
            out.push_back(vertex);
        }
        Mesh::Batch batch;
        const std::string material = faceMesh.material;
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
}

void buildChunkCollision(const AuthoredChunk& chunk, const glm::mat4& transform,
                         std::vector<CollisionTriangle>& out,
                         std::vector<uint32_t>& solidIds,
                         StaticUnionStats* unionStats)
{
    out.clear();
    solidIds.clear();
    std::vector<mimita_chunk_collision::CollisionPartV1> parts;
    parts.reserve(chunk.parts.size());
    for (const ChunkPart& part : chunk.parts) {
        if (!part.collision)
            continue;
        mimita_chunk_collision::CollisionPartV1 view{};
        std::snprintf(view.id, sizeof(view.id), "%s", part.id.c_str());
        view.position[0] = part.position.x;
        view.position[1] = part.position.y;
        view.position[2] = part.position.z;
        view.size[0] = part.size.x;
        view.size[1] = part.size.y;
        view.size[2] = part.size.z;
        view.rotation[0] = part.rotationEuler.x;
        view.rotation[1] = part.rotationEuler.y;
        view.rotation[2] = part.rotationEuler.z;
        view.collision = part.collision ? 1 : 0;
        parts.push_back(view);
    }
    if (parts.empty())
        return;

    std::vector<mimita_chunk_collision::TriangleV1> triangles(
        mimita_chunk_collision::MAX_OUTPUT_TRIANGLES);
    std::vector<uint32_t> triangleSolidIds(
        mimita_chunk_collision::MAX_OUTPUT_TRIANGLES);
    mimita_chunk_collision::BuildChunkV1 request{};
    std::snprintf(request.chunkId, sizeof(request.chunkId), "%s",
                  chunk.id.c_str());
    const float* transformValues = &transform[0][0];
    for (int i = 0; i < 16; ++i)
        request.chunkTransform[i] = transformValues[i];
    request.parts = parts.data();
    request.partCount = static_cast<uint32_t>(parts.size());
    request.outTriangles = triangles.data();
    request.outSolidIds = triangleSolidIds.data();
    request.capacity = static_cast<uint32_t>(triangles.size());

    bool built = ModuleHost::instance().chunkCollisionBuild(&request) &&
        request.result && request.outCount > 0;
    if (!built) {
        // Safety fallback before the module is loaded: use the shared box
        // triangle helper with the same world transform.
        for (const ChunkPart& part : chunk.parts) {
            if (!part.collision)
                continue;
            std::vector<CollisionTriangle> local;
            buildBoxCollisionTriangles(local, glm::vec3(0.0f),
                                       part.size * 0.5f);
            const glm::mat4 localToWorld = transform * partMatrix(part);
            const glm::mat3 normalMatrix =
                glm::transpose(glm::inverse(glm::mat3(localToWorld)));
            const uint32_t solidId = static_cast<uint32_t>(solidIds.size() / 12) + 1;
            for (const CollisionTriangle& source : local) {
                CollisionTriangle world;
                world.a = glm::vec3(localToWorld * glm::vec4(source.a, 1.0f));
                world.b = glm::vec3(localToWorld * glm::vec4(source.b, 1.0f));
                world.c = glm::vec3(localToWorld * glm::vec4(source.c, 1.0f));
                world.normal = glm::normalize(normalMatrix * source.normal);
                out.push_back(world);
                solidIds.push_back(solidId);
            }
        }
    } else {
        out.reserve(request.outCount);
        solidIds.reserve(request.outCount);
        for (uint32_t i = 0; i < request.outCount; ++i) {
            const auto& source = triangles[i];
            CollisionTriangle triangle;
            triangle.a = glm::vec3(source.a[0], source.a[1], source.a[2]);
            triangle.b = glm::vec3(source.b[0], source.b[1], source.b[2]);
            triangle.c = glm::vec3(source.c[0], source.c[1], source.c[2]);
            triangle.normal =
                glm::vec3(source.normal[0], source.normal[1], source.normal[2]);
            out.push_back(triangle);
            solidIds.push_back(triangleSolidIds[i]);
        }
    }

    if (!out.empty() && CollisionLodConfig::instance().staticSolidUnion()) {
        unionStaticSolidGroups(
            out, solidIds,
            CollisionLodConfig::instance().staticSolidUnionMaxTriangles(),
            unionStats,
            CollisionLodConfig::instance().staticSolidUnionMaxSolids());
        if (CollisionLodConfig::instance().staticFaceRemoval())
            removeFullyCoveredStaticTriangles(out, solidIds);
    }
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
        if (!part.visible)
            continue;
        appendBoxFaces(templateData.verts, part, templateData.batches,
                       partMatrix(part));
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
    // The live chunk-collision module selects collidable parts and applies the
    // chunk/part transforms; the host owns the static-union and registration.
    std::vector<CollisionTriangle> worldTriangles;
    std::vector<uint32_t> solidIds;
    StaticUnionStats unionStats;
    buildChunkCollision(chunk, transform, worldTriangles, solidIds,
                        &unionStats);

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
         {"solid_groups", static_cast<uint32_t>(std::count_if(
              chunk.parts.begin(), chunk.parts.end(),
              [](const ChunkPart& part) { return part.collision; }))},
         {"union_solids", unionStats.unionSolids},
         {"union_input_triangles", unionStats.unionInputTriangles},
         {"union_output_triangles", unionStats.unionOutputTriangles},
         {"union_fallback", unionStats.unionFallback},
         {"union_fallback_reason", unionStats.unionFallbackReason},
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

void StreamedWorldLayer::refreshChunk(const AuthoredChunk& chunk,
                                      bool rebuildCollision)
{
    Template* templateData = findTemplate(chunk.id);
    if (!templateData) return;
    rebuildTemplateFromChunk(*templateData, chunk);
    uploadTemplate(*templateData);

    size_t records = 0;
    size_t collisionTriangles = 0;
    StaticUnionStats unionStats;
    for (ChunkRecord& record : mChunks) {
        if (record.chunkId != chunk.id) continue;
        if (!mInstancing)
            rebuildRecordVbo(record, *templateData);
        ++records;
        if (!rebuildCollision)
            continue;
        if (record.collisionEntity) {
            PhysicalEntitySystem::instance().remove(record.collisionEntity);
            record.collisionEntity = 0;
        }
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> solidIds;
        buildChunkCollision(chunk, record.transform, triangles, solidIds,
                            &unionStats);
        collisionTriangles += triangles.size();
        if (!triangles.empty()) {
            record.collisionEntity = PhysicalEntitySystem::instance().add(
                triangles, glm::mat4(1.0f), PhysicalEntityMotion::Static, 0);
            if (PhysicalEntity* entity = PhysicalEntitySystem::instance().find(
                    record.collisionEntity)) {
                entity->renderEnabled = false;
                entity->collidesWithActors = true;
            }
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.template.updated", chunk.id,
        rebuildCollision ? "loaded chunk template and collision refreshed"
                         : "loaded chunk template refreshed without collision rebuild",
        0,
        {{"chunk", chunk.id},
         {"verts", templateData->verts.size()},
         {"batches", templateData->batches.size()},
         {"records", records},
         {"backend", mInstancing ? "instancing" : "vbo"},
         {"collision_rebuilt", rebuildCollision},
         {"collision_triangles", collisionTriangles},
         {"union_solids", unionStats.unionSolids},
         {"union_fallback", unionStats.unionFallback},
         {"union_fallback_reason", unionStats.unionFallbackReason}},
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
    if (changed) refreshChunk(*changed, false);
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
