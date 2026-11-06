// 2026-11-05
/* purpose
* Own streamed infinite-world chunk instances separately from the base World.
* Chunk geometry is generated from primitive parts; collision is registered as
* per-chunk static physical entities so add/remove never rebuilds the base
* world broadphase. Rendering supports instancing and per-chunk VBO backends.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "map/map_common.h"

struct AuthoredChunk;
struct Camera;

class StreamedWorldLayer
{
public:
    static StreamedWorldLayer& instance();
    ~StreamedWorldLayer();

    void setInstancing(bool enabled) { mInstancing = enabled; }
    bool instancing() const { return mInstancing; }

    void addChunk(uint64_t key, const AuthoredChunk& chunk,
                  const glm::mat4& transform);
    void removeChunk(uint64_t key);
    void updateMaterial(const std::string& chunkId, const std::string& partId,
                        int face, const std::string& material);
    void clear();
    size_t chunkCount() const { return mChunks.size(); }

    void render(const Camera& camera);

private:
    StreamedWorldLayer() = default;

    struct Template
    {
        std::string chunkId;
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
        GLuint vao = 0;
        GLuint vbo = 0;
        std::vector<Mesh::Batch> batches;
        uint32_t collisionEntity = 0;
    };

    Template* ensureTemplate(const std::string& chunkId);
    void rebuildTemplate(Template& templateData, const std::string& chunkId);
    void uploadTemplate(Template& templateData);
    void uploadInstanceBuffer(Template& templateData);
    GLuint makeChunkVao(GLuint vbo, GLuint instanceVbo);
    void freeChunk(const ChunkRecord& record);

    bool mInstancing = true;
    std::vector<Template> mTemplates;
    std::vector<ChunkRecord> mChunks;
};
