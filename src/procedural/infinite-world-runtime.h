// 2026-11-05
/* purpose
* Stream authored primitive chunks into StreamedWorldLayer for generation
* zones. Placement follows the zone/mode grid; the base world and its collision
* broadphase are never rebuilt. The host remains authoritative for accepted
* chunk identity in multiplayer.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

struct World;
struct Player;
struct AuthoredChunk;
struct GenerationZone;

class InfiniteWorldRuntime
{
public:
    static InfiniteWorldRuntime& instance();
    ~InfiniteWorldRuntime();

    void tick(const Player& player, World& world);
    void reset();
    void shutdown();

private:
    InfiniteWorldRuntime() = default;

    uint64_t cellKey(int x, int y, int z) const;
    void placeChunk(int x, int y, int z, const GenerationZone& zone,
                    const std::vector<const AuthoredChunk*>& candidates);
    void clearZone();
    int64_t chunkCoord(const glm::vec3& point, int axis,
                       const GenerationZone& zone) const;
    glm::vec3 cellOrigin(int x, int y, int z,
                         const GenerationZone& zone) const;

    bool mActive = false;
    std::string mZoneId;
    uint64_t mLastConfigRevision = 0;
    bool mHasRevision = false;
    int mLastCellX = 0;
    int mLastCellY = 0;
    int mLastCellZ = 0;
    bool mHasCell = false;
    std::unordered_set<uint64_t> mLoaded;
};

void registerInfiniteWorldCommands();
