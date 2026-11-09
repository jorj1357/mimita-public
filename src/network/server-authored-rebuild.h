// 2026-11-08
/* purpose
* build a candidate authored world + NPC collision + Recast navmesh off the
* fixed 60 Hz tick, then swap it atomically at a tick boundary
* this worker never owns or mutates the live world, registry, physics entities,
* module host, logger, infinite-collision global, or Recast singleton
*/
#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "network/server.h"
#include "npc/recast-navigation.h"
#include "world/world.h"

namespace MimitaNet {

struct AuthoredDestructibleSnapshot
{
    std::string objectId;
    int nodeIndex = -1;
    uint32_t entityId = 0;
    size_t triangleFirst = 0;
    uint32_t triangleCount = 0;
    std::vector<std::pair<size_t, uint32_t>> removed;
};

struct AuthoredRebuildRequest
{
    std::string mapId;
    uint64_t signature = 0;
    uint64_t configRevision = 0;
    uint32_t mapVersion = 0;
    uint64_t requestGeneration = 0;

    std::shared_ptr<const HeadlessWorld> baseWorld;
    bool cacheHit = false;
    std::shared_ptr<const HeadlessWorld> cachedWorld;

    std::vector<AuthoredDestructibleSnapshot> activeSnapshots;
    std::vector<std::pair<std::string, int>> rejectedNodes;
    std::vector<CollisionTriangle> destructibleSurfaceTriangles;
    uint32_t lastDestructionMatchId = 0;

    bool recastWanted = false;
    NavigationAgentProfile recastProfile;
};

struct AuthoredRebuildResult
{
    bool ok = false;
    std::string failure;
    AuthoredRebuildRequest input;

    HeadlessWorld authoredWorld;
    HeadlessWorld liveWorld;
    std::unique_ptr<World> npcWorld;
    RecastNavMeshCandidate navMesh;

    double authoredMs = 0.0;
    double npcMs = 0.0;
    double navMs = 0.0;
    double workerMs = 0.0;
};

class AuthoredWorldRebuildWorker
{
public:
    static AuthoredWorldRebuildWorker& instance();

    void submit(AuthoredRebuildRequest request);
    std::shared_ptr<AuthoredRebuildResult> tryTakeResult();
    void retire(std::shared_ptr<AuthoredRebuildResult> result);
    bool busy() const;
    void cancelAndWait();
    void shutdown();

private:
    AuthoredWorldRebuildWorker();
    ~AuthoredWorldRebuildWorker();
    AuthoredWorldRebuildWorker(const AuthoredWorldRebuildWorker&) = delete;
    AuthoredWorldRebuildWorker& operator=(const AuthoredWorldRebuildWorker&) = delete;

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace MimitaNet
