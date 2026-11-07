// 2026-11-05
/* purpose
* Stream authored primitive chunks into StreamedWorldLayer for generation
* zones. Placement follows the zone/mode grid; the base world and its collision
* broadphase are never rebuilt. Server chunk identity and collision authority
* still need to be integrated for multiplayer.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "gamemode/map-config.h"

struct World;
struct Player;

class InfiniteWorldRuntime
{
public:
    static InfiniteWorldRuntime& instance();
    ~InfiniteWorldRuntime();

    void tick(const Player& player, World& world);
    void reset();
    void shutdown();

    // Cache invalidation after an authored chunk-library edit.
    void invalidateModeChunks();

private:
    InfiniteWorldRuntime() = default;

    struct ResolvedCell
    {
        bool attempted = false;
        bool valid = false;
        bool logged = false;
        int depth = 0;
        std::string chunkId;
        glm::mat4 transform{1.0f};
    };

    uint64_t cellKey(int x, int y, int z) const;
    bool placeChunk(int x, int y, int z, const GenerationZone& zone,
                    const std::vector<const AuthoredChunk*>& candidates,
                    bool independent);
    void clearZone();
    int64_t chunkCoord(const glm::vec3& point, int axis,
                       const GenerationZone& zone) const;
    glm::vec3 cellOrigin(int x, int y, int z,
                         const GenerationZone& zone) const;
    std::vector<const AuthoredChunk*> resolveCandidates(
        const GenerationZone& zone);
    uint64_t generationSignature(
        const GenerationZone& zone,
        const std::vector<const AuthoredChunk*>& candidates) const;
    const ResolvedCell& resolveCell(
        int x, int y, int z, const GenerationZone& zone,
        const std::vector<const AuthoredChunk*>& candidates,
        bool independent);

    bool mActive = false;
    std::string mZoneId;
    uint64_t mLastConfigRevision = 0;
    bool mHasRevision = false;
    uint64_t mGenerationSignature = 0;
    bool mHasGeneration = false;
    bool mSignatureDirty = true;
    int mAnchorX = 0;
    int mAnchorY = 0;
    int mAnchorZ = 0;
    int mLastCellX = 0;
    int mLastCellY = 0;
    int mLastCellZ = 0;
    bool mHasCell = false;
    glm::ivec3 mLastRadius{-1};
    std::unordered_set<uint64_t> mDesired;
    std::vector<glm::ivec3> mPendingOffsets;
    std::unordered_set<uint64_t> mLoaded;
    std::unordered_set<uint64_t> mRejected;
    std::unordered_map<uint64_t, ResolvedCell> mResolved;
    std::vector<AuthoredChunk> mModeChunks;
    std::string mModeCacheId;
    GenerationZone mModeZone;
};

struct WorldConnectorMatch
{
    std::string chunkId;
    std::string connectorId;
    std::string side;
    std::string role;
    bool explicitLink = false;
};

std::vector<WorldConnectorMatch> previewCompatibleConnectors(
    const std::string& chunkId, const std::string& connectorId,
    std::string* error);

void registerInfiniteWorldCommands();
