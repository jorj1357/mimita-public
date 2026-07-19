// 07 19 2026 1030
/* purpose
* Client-side CollisionWorldView adapter for the shared projectile kernel.
* Provides world triangles and player capsules to simulateProjectileTick().
* fill in 2nd line
* fill in 3rd line
* Does NOT modify packets, weapon config, or server behavior.
* Does NOT route live client projectiles through the kernel.
* Does NOT affect rendering, effects, or client prediction state.
*/

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "combat/projectile-simulation.h"
#include "physics/physics-types.h"
#include "world/world.h"

class ClientCollisionWorldView final : public CollisionWorldView
{
public:
    struct PlayerReplica {
        uint32_t playerId = 0;
        uint32_t spawnGeneration = 0;
        glm::vec3 pos{0.0f};
        bool dead = false;
        float capsuleRadius = 0.5f;
        float capsuleHeight = 3.6f;
        float sizeScale = 1.0f;
        glm::vec3 capsuleCenter{0.0f};
    };

    ClientCollisionWorldView(
        const CollisionMeshCache& collisionMesh,
        float collisionChunkSize,
        const std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash>* collisionChunks,
        const std::vector<int>* collisionLargeTriangles,
        uint32_t ownerPlayerId = 0,
        const std::vector<PlayerReplica>& players = {});

    void setPlayers(const std::vector<PlayerReplica>& players);

    void queryTrianglesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<int>& outIndices) const override;

    const CollisionTriangle& triangleAt(int index) const override;

    int triangleCount() const override;

    void queryPlayerCapsulesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<SweptPlayerCapsule>& out) const override;

private:
    const CollisionMeshCache& mCollisionMesh;
    float mCollisionChunkSize;
    const std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash>* mCollisionChunks;
    const std::vector<int>* mCollisionLargeTriangles;
    uint32_t mOwnerPlayerId;
    std::vector<PlayerReplica> mPlayers;
};
