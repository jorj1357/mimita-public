// 2026-11-05
/* purpose
* Own streamed infinite-world chunk instances separately from the base World.
* Chunk geometry is generated from primitive parts; collision is composed into
* the World collision mesh tail before the broadphase rebuild so every actor
* and navigation query sees streamed chunks through the same static surface.
* Rendering supports instancing and per-chunk VBO backends.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "map/map_common.h"
#include "physics/physics-types.h"

struct AuthoredChunk;
struct Camera;
struct World;

// Shared host-side chunk collision builder used by the client stream layer and
// the server NPC collision stream. The live chunk-collision module selects the
// collidable parts and composes world-space triangles.
bool buildStreamedChunkCollision(
    const AuthoredChunk& chunk, const glm::mat4& transform,
    std::vector<CollisionTriangle>& out, std::vector<uint32_t>& solidIds);

class StreamedWorldLayer
{
public:
    static StreamedWorldLayer& instance();
    ~StreamedWorldLayer();

    void setInstancing(bool enabled);
    bool instancing() const { return mInstancing; }

    void addChunk(uint64_t key, const AuthoredChunk& chunk,
                  const glm::mat4& transform);
    void addProxy(uint64_t key, const AuthoredChunk& chunk,
                  const glm::mat4& transform, const glm::vec3& boundsMin,
                  const glm::vec3& boundsMax);
    bool isProxy(uint64_t key) const;
    void removeChunk(uint64_t key);
    void setStreamFocus(const glm::vec3& focus, float collisionRadiusM);
    void setCoarseColliders(const std::vector<AABB>& colliders);
    bool hasFullCollisionNear(const glm::vec3& focus, float radius) const;
    // Rebuild visual data and loaded per-chunk VBOs. Collision proxies are
    // rebuilt when geometry changed; material/UV-only refresh can skip it.
    void refreshChunk(const AuthoredChunk& chunk,
                      bool rebuildCollision = true);
    void updateMaterial(const std::string& chunkId, const std::string& partId,
                        int face, const std::string& material);
    void clear();
    size_t chunkCount() const { return mChunks.size(); }

    // Rebuild the streamed collision surface and append it to the World
    // collision mesh tail. Runs the same static face-removal/union owner as
    // authored maps, then rebuilds the broadphase. Call before actor simulation.
    bool flushCollision(World& world);
    bool collisionDirty() const { return mCollisionDirty; }
    uint64_t collisionRevision() const { return mCollisionRevision; }
    uint32_t collidedChunkCount() const { return mCollidedChunkCount; }
    size_t collisionTriangleCount() const { return mCollisionTriangleCount; }
    size_t collisionRemovedFaces() const { return mCollisionRemovedFaces; }
    uint64_t chunksInstalledTotal() const { return mChunksInstalledTotal; }
    uint64_t chunksRemovedTotal() const { return mChunksRemovedTotal; }
    double lastCollisionRebuildMs() const { return mLastCollisionRebuildMs; }
    double maxCollisionRebuildMs() const { return mMaxCollisionRebuildMs; }
    bool collisionCooldownActive() const;
    uint32_t lastLoadedChunkCount() const { return mLastLoadedChunkCount; }
    bool lastCollisionRevisionMatched() const {
        return mLastCollisionRevisionMatched;
    }

    void render(const Camera& camera);

private:
    StreamedWorldLayer() = default;

    struct Template
    {
        std::string chunkId;
        bool proxy = false;
        std::vector<Vertex> verts;      // local space
        std::vector<Mesh::Batch> batches;
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint instanceVbo = 0;
        std::vector<glm::mat4> instances;
        bool vboDirty = true;
    };

    struct ChunkRecord
    {
        uint64_t key = 0;
        std::string chunkId;
        glm::mat4 transform{1.0f};
        bool proxy = false;
        bool collisionActive = true;
        AABB bounds{};
        GLuint vao = 0;
        GLuint vbo = 0;
        std::vector<Mesh::Batch> batches;
        std::vector<CollisionTriangle> collisionTriangles;
        std::vector<uint32_t> collisionSolidIds;
    };

    static std::string templateKey(const std::string& chunkId, bool proxy);
    Template* ensureTemplate(const std::string& chunkId, bool proxy);
    Template* findTemplate(const std::string& chunkId, bool proxy);
    Template* findRecordTemplate(const ChunkRecord& record);
    void rebuildProxyTemplate(Template& templateData,
                              const AuthoredChunk& chunk);
    void rebuildTemplate(Template& templateData, const std::string& chunkId);
    void rebuildTemplateFromChunk(Template& templateData,
                                  const AuthoredChunk& chunk);
    void uploadTemplate(Template& templateData);
    void uploadInstanceBuffer(Template& templateData);
    void rebuildRecordVbo(ChunkRecord& record, const Template& templateData);
    void rebuildInstances();
    GLuint makeChunkVao(GLuint vbo, GLuint instanceVbo);
    void freeChunk(const ChunkRecord& record);

    bool mInstancing = true;
    bool mCollisionDirty = true;
    glm::vec3 mStreamFocus{0.0f};
    float mCollisionRadiusM = 0.0f;
    bool mHasStreamFocus = false;
    std::vector<AABB> mCoarseColliders;
    uint64_t mCollisionRevision = 0;
    uint32_t mCollidedChunkCount = 0;
    size_t mCollisionTriangleCount = 0;
    size_t mCollisionRemovedFaces = 0;
    uint64_t mChunksInstalledTotal = 0;
    uint64_t mChunksRemovedTotal = 0;
    double mLastCollisionRebuildMs = 0.0;
    double mMaxCollisionRebuildMs = 0.0;
    double mCollisionFrameBudgetMs = 8.0;
    uint64_t mCollisionCooldownUntilMs = 0;
    uint32_t mLastLoadedChunkCount = 0;
    size_t mBaseBroadphaseTriangles = 0;
    uint64_t mBaseBroadphaseRevision = UINT64_MAX;
    bool mLastCollisionRevisionMatched = true;
    std::vector<Template> mTemplates;
    std::vector<ChunkRecord> mChunks;
};
