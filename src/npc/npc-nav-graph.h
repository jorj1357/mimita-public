// 10 05 2026
/* purpose
* Lazy, chunked walkable navigation graph built from loaded collision
* triangles. Lets an NPC plan a long-range route across a large or dynamically
* changing map without manually authored waypoints.
* Geometry is voxelized into coarse chunks on demand (only the chunks a route
* needs are built), so no global pass is required at map load and procedural /
* destructible worlds are supported by invalidating and rebuilding chunks.
* Supports multiple equal-cost routes so squads do not walk identical lines.
* Does NOT decide goals, steer, apply physics, or own combat.
* Does NOT require hand-placed navigation points anywhere.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "npc/npc-surface.h"

struct World;

// Tuning for graph generation and querying. All values are metres/seconds.
struct NpcNavGraphSettings
{
    float chunkSize = 32.0f;          // graph chunk edge length (XZ)
    float cellSize = 2.0f;            // coarse node spacing
    float maxWalkableSlopeDot = 0.80f;// cos(max walkable slope); shared rule
    float maxStepHeight = 0.65f;      // rise walked up without a jump
    float maxDropHeight = 14.0f;      // drop the actor may fall
    float jumpHeight = 2.0f;          // legal rise via a jump
    float actorRadius = 0.45f;        // capsule radius for edge clearance
    float actorHeight = 1.8f;         // headroom required above a surface
    bool allowJumps = true;           // create jump links
    int maxSurfacesPerColumn = 3;     // stacked floors per X/Y cell
    int maxRoutes = 3;                // multiple equal-cost route candidates
    int maxActiveChunks = 512;        // bound on lazily built chunks per query
    float maxRouteMeters = 700.0f;    // refuse absurd single-hop queries
};

// One candidate route: an ordered list of navigation points (>= 2) and its
// traversed length. Routes are distinct so an actor can pick one by its id.
struct NpcNavRoute
{
    std::vector<glm::vec3> points;
    float length = 0.0f;
    bool valid() const { return points.size() >= 2; }
};

class NpcNavGraph
{
public:
    NpcNavGraph() = default;

    static NpcNavGraph& instance();

    // Drop all cached chunks (call when world collision geometry changes).
    void invalidate();

    // Plan up to settings.maxRoutes distinct routes from `start` to `goal`.
    // Chunks are built lazily along the start->goal corridor and expanded only
    // if no route is found. `actorSeed` selects which routes are preferred for
    // this actor so squads spread out. Empty result = no route found/known.
    std::vector<NpcNavRoute> findRoutes(const World& world, const glm::vec3& start,
                                        const glm::vec3& goal,
                                        const NpcNavGraphSettings& settings,
                                        uint32_t actorSeed);

    // Diagnostics.
    std::size_t builtChunkCount() const { return mChunks.size(); }
    std::uint64_t buildCount() const { return mBuildCount; }

private:
    struct GraphNode
    {
        glm::vec3 pos{0.0f};
        NavSurfaceKind kind = NavSurfaceKind::Floor;
        std::uint32_t id = 0;
        std::uint16_t cx = 0;
        std::uint16_t cy = 0;
        std::vector<std::uint32_t> neighbors;   // global node ids
        std::vector<std::uint8_t> caps;         // NavCapability per neighbor
        std::vector<float> costs;               // traversal cost per neighbor
    };

    struct GraphChunk
    {
        glm::ivec2 coord{0, 0};
        std::uint32_t idBase = 0;
        std::vector<GraphNode> nodes;
        std::unordered_map<int, std::vector<int>> cells; // cellKey -> local idx
        std::vector<int> triangles;                      // gathered collision tris
        float groundMaxZ = 0.0f;
    };

    int cellKey(int cx, int cy) const { return cy * 64 + cx; }
    int cellsPerChunk(const NpcNavGraphSettings& s) const;

    GraphChunk* buildChunk(const World& world, const NpcNavGraphSettings& s,
                           glm::ivec2 coord);
    void ensureLinks(const World& world, const NpcNavGraphSettings& s,
                     const std::vector<glm::ivec2>& coords);
    void linkPair(const World& world, GraphChunk& a, GraphChunk& b,
                  const NpcNavGraphSettings& s);
    void addEdge(GraphNode& from, std::uint32_t to, std::uint8_t cap, float cost);

    bool findOneRoute(const World& world, const NpcNavGraphSettings& s,
                      const std::vector<glm::ivec2>& coords,
                      const glm::vec3& start, const glm::vec3& goal,
                      const std::unordered_map<std::uint64_t, float>& penalty,
                      NpcNavRoute& out);

    std::unordered_map<std::uint64_t, GraphChunk> mChunks; // key = chunk coord
    std::unordered_map<std::uint64_t, bool> mLinkedPairs;
    std::uint64_t mGeometrySignature = 0;
    std::uint32_t mNextNodeId = 1;
    std::uint64_t mBuildCount = 0;
};

// Build a minimal collision world and prove the graph routes around a wall,
// connects stacked floors, and yields multiple distinct routes.
bool npcNavGraphSelfTest(std::string& report);
