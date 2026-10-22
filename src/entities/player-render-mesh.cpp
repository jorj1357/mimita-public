#include "player.h"

#include <cstdio>
#include <cstdint>
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
struct ModelUploadSet {
    std::vector<GLuint> partVaos;
    std::vector<GLuint> partVbos;
    std::vector<uint64_t> partGenerations;
    GLuint renderVao = 0;
    GLuint renderVbo = 0;
    uint64_t renderGeneration = ~0ull;
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
