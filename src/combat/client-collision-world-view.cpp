// 07 19 2026 1030
/* purpose
* Client-side CollisionWorldView adapter implementation.
* Queries world collision chunks (same spatial grid as the server adapter).
* Includes player capsules from a caller-provided PlayerReplica list.
* Excludes owner projectile during arming distance.
* fill in 3rd line
* Does NOT modify packets, weapon config, or server behavior.
* Does NOT route live client projectiles through the kernel.
* Does NOT affect rendering, effects, or client prediction state.
*/

#include "client-collision-world-view.h"

#include <algorithm>
#include <glm/glm.hpp>

#include "physics/movement/physics-collision-shared.h"

ClientCollisionWorldView::ClientCollisionWorldView(
    const CollisionMeshCache& collisionMesh,
    float collisionChunkSize,
    const std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash>* collisionChunks,
    const std::vector<int>* collisionLargeTriangles,
    uint32_t ownerPlayerId,
    const std::vector<PlayerReplica>& players)
    : mCollisionMesh(collisionMesh)
    , mCollisionChunkSize(collisionChunkSize)
    , mCollisionChunks(collisionChunks)
    , mCollisionLargeTriangles(collisionLargeTriangles)
    , mOwnerPlayerId(ownerPlayerId)
    , mPlayers(players)
{
}

void ClientCollisionWorldView::setPlayers(const std::vector<PlayerReplica>& players)
{
    mPlayers = players;
}

void ClientCollisionWorldView::queryTrianglesSwept(
    const glm::vec3& from, const glm::vec3& to, float radius,
    std::vector<int>& outIndices) const
{
    const auto& mesh = mCollisionMesh;
    if (mesh.empty())
        return;

    AABB queryBounds;
    queryBounds.min = glm::min(from, to) - glm::vec3(radius);
    queryBounds.max = glm::max(from, to) + glm::vec3(radius);

    constexpr int MAX_CELLS = 100;
    glm::ivec3 c0 = collisionChunkCoord(queryBounds.min, mCollisionChunkSize);
    glm::ivec3 c1 = collisionChunkCoord(queryBounds.max, mCollisionChunkSize);

    int64_t cellsX = (int64_t)c1.x - (int64_t)c0.x + 1;
    int64_t cellsY = (int64_t)c1.y - (int64_t)c0.y + 1;
    int64_t cellsZ = (int64_t)c1.z - (int64_t)c0.z + 1;
    if (cellsX > MAX_CELLS) { c1.x = c0.x + MAX_CELLS - 1; }
    if (cellsY > MAX_CELLS) { c1.y = c0.y + MAX_CELLS - 1; }
    if (cellsZ > MAX_CELLS) { c1.z = c0.z + MAX_CELLS - 1; }

    thread_local std::vector<uint32_t> s_gen;
    thread_local uint32_t s_curGen = 0;
    s_curGen++;
    if (s_curGen == 0) { s_gen.assign(mesh.triangles.size(), 0); s_curGen = 1; }
    if (s_gen.size() != mesh.triangles.size())
        s_gen.assign(mesh.triangles.size(), 0);

    if (mCollisionChunks)
    {
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
        {
            auto it = mCollisionChunks->find(glm::ivec3(x, y, z));
            if (it == mCollisionChunks->end())
                continue;

            for (int triIdx : it->second)
            {
                if (triIdx < 0 || triIdx >= (int)mesh.triangles.size())
                    continue;
                if (s_gen[triIdx] == s_curGen)
                    continue;
                s_gen[triIdx] = s_curGen;

                AABB tb;
                const auto& tri = mesh.triangles[triIdx];
                tb.min = glm::min(tri.a, glm::min(tri.b, tri.c));
                tb.max = glm::max(tri.a, glm::max(tri.b, tri.c));
                tb.min -= glm::vec3(radius * 0.1f);
                tb.max += glm::vec3(radius * 0.1f);
                if (overlaps(queryBounds, tb))
                    outIndices.push_back(triIdx);
            }
        }
    }

    if (mCollisionLargeTriangles)
    {
        for (int triIdx : *mCollisionLargeTriangles)
        {
            if (triIdx < 0 || triIdx >= (int)mesh.triangles.size())
                continue;
            if (s_gen[triIdx] == s_curGen)
                continue;
            s_gen[triIdx] = s_curGen;

            AABB tb;
            const auto& tri = mesh.triangles[triIdx];
            tb.min = glm::min(tri.a, glm::min(tri.b, tri.c));
            tb.max = glm::max(tri.a, glm::max(tri.b, tri.c));
            tb.min -= glm::vec3(radius * 0.1f);
            tb.max += glm::vec3(radius * 0.1f);
            if (overlaps(queryBounds, tb))
                outIndices.push_back(triIdx);
        }
    }

    std::sort(outIndices.begin(), outIndices.end());
}

const CollisionTriangle& ClientCollisionWorldView::triangleAt(int index) const
{
    return mCollisionMesh.triangles[(size_t)index];
}

int ClientCollisionWorldView::triangleCount() const
{
    return (int)mCollisionMesh.triangles.size();
}

void ClientCollisionWorldView::queryPlayerCapsulesSwept(
    const glm::vec3& from, const glm::vec3& to, float radius,
    std::vector<SweptPlayerCapsule>& out) const
{
    AABB projectileBounds;
    projectileBounds.min = glm::min(from, to) - glm::vec3(radius);
    projectileBounds.max = glm::max(from, to) + glm::vec3(radius);

    for (const auto& replica : mPlayers)
    {
        if (replica.dead)
            continue;
        if (replica.playerId == mOwnerPlayerId)
            continue;

        float s = std::max(replica.sizeScale, 0.001f);
        float r = std::max(replica.capsuleRadius, 0.001f) * s;
        float h = std::max(replica.capsuleHeight, 0.001f) * s;
        float half = h * 0.5f;
        glm::vec3 center = replica.capsuleCenter;
        if (glm::length(center - replica.pos) > 0.0001f)
            center = replica.pos;

        SweptPlayerCapsule cap;
        cap.playerId = replica.playerId;
        cap.spawnGeneration = replica.spawnGeneration;
        cap.a = center - glm::vec3(0.0f, 0.0f, half - r);
        cap.b = center + glm::vec3(0.0f, 0.0f, half - r);
        cap.radius = r;

        AABB capBounds;
        capBounds.min = glm::min(cap.a, cap.b) - glm::vec3(cap.radius);
        capBounds.max = glm::max(cap.a, cap.b) + glm::vec3(cap.radius);
        if (overlaps(projectileBounds, capBounds))
            out.push_back(cap);
    }

    std::sort(out.begin(), out.end(),
        [](const SweptPlayerCapsule& a, const SweptPlayerCapsule& b) {
            if (a.playerId != b.playerId)
                return a.playerId < b.playerId;
            return a.spawnGeneration < b.spawnGeneration;
        });
}
