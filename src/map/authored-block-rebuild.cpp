// 2026-11-27
/* purpose
* Decide whether an authored geometry signature change is block-only, coalesce
* editor drags, and patch the live NPC collision mirror in place so a block edit
* does not rebuild the whole world on the fixed tick.
* This file DOES NOT own map data, rendering, navmesh baking, or the editor.
*/
#include "map/authored-block-rebuild.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "map/map-loader-collision.h"
#include "network/server.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace {

// Coalesce drag/edit bursts. The authoritative world may trail the editor by at
// most this interval; it is three to six fixed ticks, not tens of seconds.
constexpr uint64_t kMinBlockApplyIntervalMs = 100;

struct BlockAuthorityState
{
    uint64_t appliedNonBlockSignature = 0;
    uint64_t lastApplyMs = 0;
};

struct NpcBlockRange
{
    std::string id;
    uint64_t key = 0;
    size_t first = 0;
    size_t count = 0;
};

struct NpcBlockState
{
    bool valid = false;
    std::vector<NpcBlockRange> ranges;
    size_t hiddenTriangles = 0;
    glm::vec3 dirtyMin{0.0f};
    glm::vec3 dirtyMax{0.0f};
    bool dirtyValid = false;
};

struct PendingRange
{
    NpcBlockRange range;
    size_t headlessFirst = 0;
    bool isNew = false;
};

BlockAuthorityState& state()
{
    static BlockAuthorityState value;
    return value;
}

NpcBlockState& npcState()
{
    static NpcBlockState value;
    return value;
}

std::string& npcFailure()
{
    static std::string value;
    return value;
}

uint64_t hashMix(uint64_t hash, uint64_t value)
{
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash;
}

uint64_t hashFloat(uint64_t hash, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return hashMix(hash, bits);
}

uint64_t authoredBlockKey(const AuthoredBlock& block)
{
    uint64_t hash = 1469598103934665603ull;
    hash = hashFloat(hash, block.position.x);
    hash = hashFloat(hash, block.position.y);
    hash = hashFloat(hash, block.position.z);
    hash = hashFloat(hash, block.size.x);
    hash = hashFloat(hash, block.size.y);
    hash = hashFloat(hash, block.size.z);
    hash = hashFloat(hash, block.rotationEuler.x);
    hash = hashFloat(hash, block.rotationEuler.y);
    hash = hashFloat(hash, block.rotationEuler.z);
    hash = hashMix(hash, block.collision ? 1u : 0u);
    hash = hashMix(hash, block.enabled ? 1u : 0u);
    hash = hashMix(hash, static_cast<uint64_t>(block.shape.size()));
    for (unsigned char character : block.shape)
        hash = hashMix(hash, character);
    hash = hashFloat(hash, block.arcDegrees);
    hash = hashMix(hash, static_cast<uint64_t>(block.segments));
    hash = hashMix(hash, block.convex ? 1u : 0u);
    hash = hashMix(hash, static_cast<uint64_t>(block.profileAxis));
    hash = hashFloat(hash, block.radiusTop);
    hash = hashMix(hash, block.profileFlip ? 1u : 0u);
    hash = hashMix(hash, static_cast<uint64_t>(block.profile.size()));
    for (const glm::vec2& point : block.profile) {
        hash = hashFloat(hash, point.x);
        hash = hashFloat(hash, point.y);
    }
    return hash;
}

bool nearVec(const glm::vec3& a, const glm::vec3& b)
{
    return std::abs(a.x - b.x) <= 1e-3f && std::abs(a.y - b.y) <= 1e-3f &&
           std::abs(a.z - b.z) <= 1e-3f;
}

bool sameTriangle(const CollisionTriangle& a, const CollisionTriangle& b)
{
    return nearVec(a.a, b.a) && nearVec(a.b, b.b) && nearVec(a.c, b.c);
}

void ensureHiddenSized(World& world)
{
    auto& mesh = world.collisionMesh;
    if (mesh.hidden.size() != mesh.triangles.size())
        mesh.hidden.assign(mesh.triangles.size(), 0);
}

void hideNpcRange(World& world, size_t first, size_t count)
{
    auto& mesh = world.collisionMesh;
    ensureHiddenSized(world);
    for (size_t i = first; i < first + count && i < mesh.hidden.size(); ++i)
        mesh.hidden[i] = 1;
    for (size_t i = first; i < first + count && i < mesh.solidIds.size(); ++i)
        mesh.solidIds[i] = kRemovedCollisionSolidId;
}

void addTriangleToChunks(World& world, size_t index, const AABB& bounds)
{
    const float chunkSize = world.collisionChunkSize > 0.001f
        ? world.collisionChunkSize : 6.0f;
    world.collisionChunkSize = chunkSize;
    const glm::ivec3 c0 = collisionChunkCoord(bounds.min, chunkSize);
    const glm::ivec3 c1 = collisionChunkCoord(bounds.max, chunkSize);
    const int64_t cells = static_cast<int64_t>(c1.x - c0.x + 1) *
                          static_cast<int64_t>(c1.y - c0.y + 1) *
                          static_cast<int64_t>(c1.z - c0.z + 1);
    if (cells > kMaxChunksPerTriangle) {
        world.collisionLargeTriangles.push_back(static_cast<int>(index));
        return;
    }
    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
        world.collisionChunks[glm::ivec3(x, y, z)].push_back(
            static_cast<int>(index));
}

void appendHeadlessRange(World& world, const MimitaNet::HeadlessWorld& headless,
                         size_t headlessFirst, size_t count, NpcBlockRange& out,
                         glm::vec3& dirtyMin, glm::vec3& dirtyMax,
                         bool& dirtyAny)
{
    auto& mesh = world.collisionMesh;
    const size_t first = mesh.triangles.size();
    mesh.triangles.reserve(first + count);
    mesh.solidIds.reserve(first + count);
    mesh.hidden.reserve(first + count);
    mesh.triangleAABBs.reserve(first + count);
    mesh.triangleOBBs.reserve(first + count);
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t source = headlessFirst + offset;
        const CollisionTriangle& triangle = headless.triangles[source];
        const uint32_t solidId = source < headless.solidIds.size()
            ? headless.solidIds[source] : 0u;
        mesh.triangles.push_back(triangle);
        mesh.solidIds.push_back(solidId == kRemovedCollisionSolidId
                                    ? 0u : solidId);
        mesh.hidden.push_back(0);
        AABB bounds;
        CollisionTriangleOBB obb;
        buildCollisionTrianglePrimitive(triangle, bounds, obb);
        mesh.triangleAABBs.push_back(bounds);
        mesh.triangleOBBs.push_back(obb);
        addTriangleToChunks(world, first + offset, bounds);
        dirtyMin = glm::min(dirtyMin, bounds.min);
        dirtyMax = glm::max(dirtyMax, bounds.max);
        dirtyAny = true;
    }
    out.first = first;
    out.count = count;
}

void rebuildNpcBaseTree(World& world)
{
    auto& mesh = world.collisionMesh;
    world.collisionTree.clear();
    if (mesh.triangles.empty() ||
        mesh.triangleAABBs.size() != mesh.triangles.size())
        return;
    std::vector<int> ids(mesh.triangles.size());
    for (size_t i = 0; i < ids.size(); ++i)
        ids[i] = static_cast<int>(i);
    world.collisionTree.build(ids, mesh.triangleAABBs);
}

void clearSmoothTopology(World& world)
{
    auto& mesh = world.collisionMesh;
    mesh.smoothEdgeIndex.clear();
    mesh.smoothVertexIndex.clear();
    mesh.smoothEdges.clear();
    mesh.smoothVertices.clear();
}

}  // namespace

void authoredBlockAuthorityReset()
{
    state() = BlockAuthorityState{};
    authoredNpcBlockStateInvalidate();
}

void authoredBlockAuthorityMarkApplied(uint64_t nonBlockSignature,
                                       uint64_t nowMs)
{
    BlockAuthorityState& s = state();
    s.appliedNonBlockSignature = nonBlockSignature;
    s.lastApplyMs = nowMs;
}

bool authoredBlockAuthorityCanApply(uint64_t nonBlockSignature)
{
    const BlockAuthorityState& s = state();
    return s.appliedNonBlockSignature != 0 &&
           s.appliedNonBlockSignature == nonBlockSignature;
}

bool authoredBlockAuthorityThrottleAllows(uint64_t nowMs)
{
    const BlockAuthorityState& s = state();
    return s.lastApplyMs == 0 ||
           nowMs >= s.lastApplyMs + kMinBlockApplyIntervalMs;
}

void authoredNpcBlockStateInvalidate()
{
    npcState() = NpcBlockState{};
    npcFailure().clear();
}

const std::string& authoredNpcBlockStateLastFailure()
{
    return npcFailure();
}

void authoredNpcBlockStateCapture(
    const World& npcWorld, const MimitaNet::HeadlessWorld& headless,
    const std::vector<AuthoredBlock>& blocks)
{
    NpcBlockState next;
    npcFailure().clear();
    size_t total = 0;
    for (const AuthoredBlock& block : blocks)
        total += authoredBlockTriangleCount(block);

    size_t headlessFirst = 0;
    size_t headlessCount = 0;
    if (total == 0) {
        // A full build from an empty block list has no authored tail to patch.
        next.valid = true;
        npcState() = std::move(next);
        return;
    }
    if (!authoredHeadlessBlockTail(headlessFirst, headlessCount) ||
        headlessCount != total ||
        headlessFirst + headlessCount > headless.triangles.size()) {
        npcFailure() = "capture-tail first=" + std::to_string(headlessFirst) +
            " count=" + std::to_string(headlessCount) +
            " total=" + std::to_string(total) +
            " headless=" + std::to_string(headless.triangles.size());
        npcState() = NpcBlockState{};
        return;
    }

    // The NPC copy preserves order and skips removed headless triangles, then
    // appends destructible surfaces at the end. Map the tail through skips.
    size_t removedBefore = 0;
    const size_t limit = std::min(headlessFirst, headless.solidIds.size());
    for (size_t i = 0; i < limit; ++i)
        if (headless.solidIds[i] == kRemovedCollisionSolidId)
            ++removedBefore;
    if (headlessFirst < removedBefore) {
        npcFailure() = "capture-removed-before";
        npcState() = NpcBlockState{};
        return;
    }
    size_t headlessCursor = headlessFirst;
    size_t npcCursor = headlessFirst - removedBefore;
    for (const AuthoredBlock& block : blocks) {
        const size_t count = authoredBlockTriangleCount(block);
        if (count == 0)
            continue;
        if (npcCursor + count > npcWorld.collisionMesh.triangles.size() ||
            headlessCursor + count > headless.triangles.size()) {
            npcFailure() = "capture-range npc=" +
                std::to_string(npcWorld.collisionMesh.triangles.size()) +
                " npc_cursor=" + std::to_string(npcCursor) +
                " count=" + std::to_string(count);
            npcState() = NpcBlockState{};
            return;
        }
        if (!sameTriangle(npcWorld.collisionMesh.triangles[npcCursor],
                          headless.triangles[headlessCursor])) {
            npcFailure() = "capture-triangle-mismatch cursor=" +
                std::to_string(npcCursor);
            npcState() = NpcBlockState{};
            return;
        }
        next.ranges.push_back(
            {block.id, authoredBlockKey(block), npcCursor, count});
        npcCursor += count;
        headlessCursor += count;
    }
    next.valid = true;
    next.hiddenTriangles = 0;
    next.dirtyValid = false;
    npcState() = std::move(next);
}

bool authoredNpcBlockDeltaApply(
    World& npcWorld, const MimitaNet::HeadlessWorld& headless,
    const std::vector<AuthoredBlock>& blocks, std::string& failure)
{
    npcFailure().clear();
    NpcBlockState& current = npcState();
    current.dirtyValid = false;
    if (!current.valid) {
        failure = "state-invalid";
        npcFailure() = failure;
        return false;
    }
    auto& mesh = npcWorld.collisionMesh;
    if (mesh.triangles.size() != mesh.solidIds.size()) {
        failure = "solid-id-mismatch";
        npcFailure() = failure;
        return false;
    }
    if ((!mesh.triangleAABBs.empty() &&
         mesh.triangleAABBs.size() != mesh.triangles.size()) ||
        (!mesh.triangleOBBs.empty() &&
         mesh.triangleOBBs.size() != mesh.triangles.size())) {
        failure = "primitive-cache-mismatch";
        npcFailure() = failure;
        return false;
    }
    if (mesh.triangleAABBs.empty() || mesh.triangleOBBs.empty()) {
        failure = "primitive-cache-missing";
        npcFailure() = failure;
        return false;
    }

    size_t total = 0;
    for (const AuthoredBlock& block : blocks)
        total += authoredBlockTriangleCount(block);
    size_t headlessFirst = 0;
    size_t headlessCount = 0;
    if (!authoredHeadlessBlockTail(headlessFirst, headlessCount) ||
        headlessCount != total ||
        headlessFirst + headlessCount > headless.triangles.size()) {
        failure = "tail-unavailable";
        npcFailure() = failure + " first=" + std::to_string(headlessFirst) +
            " count=" + std::to_string(headlessCount) +
            " total=" + std::to_string(total) +
            " headless=" + std::to_string(headless.triangles.size());
        return false;
    }

    std::unordered_map<std::string, NpcBlockRange> previous;
    for (const NpcBlockRange& range : current.ranges)
        previous.emplace(range.id, range);

    std::vector<PendingRange> pending;
    std::unordered_set<std::string> kept;
    size_t headlessCursor = headlessFirst;
    for (const AuthoredBlock& block : blocks) {
        const size_t count = authoredBlockTriangleCount(block);
        if (count == 0)
            continue;
        const uint64_t key = authoredBlockKey(block);
        auto found = previous.find(block.id);
        const bool unchanged = found != previous.end() &&
            found->second.key == key &&
            found->second.first + found->second.count <=
                mesh.triangles.size();
        PendingRange item;
        if (unchanged) {
            item.range = found->second;
            kept.insert(block.id);
        } else {
            item.range.id = block.id;
            item.range.key = key;
            item.range.count = count;
            item.headlessFirst = headlessCursor;
            item.isNew = true;
            if (headlessCursor + count > headless.triangles.size()) {
                failure = "headless-range";
                npcFailure() = failure;
                return false;
            }
        }
        pending.push_back(std::move(item));
        headlessCursor += count;
    }

    // Everything is validated. Mutate in place.
    ensureHiddenSized(npcWorld);
    bool changed = false;
    glm::vec3 dirtyMin(std::numeric_limits<float>::max());
    glm::vec3 dirtyMax(std::numeric_limits<float>::lowest());
    bool dirtyAny = false;
    size_t hiddenAdded = 0;
    for (const NpcBlockRange& range : current.ranges) {
        if (kept.count(range.id))
            continue;
        for (size_t i = range.first;
             i < range.first + range.count && i < mesh.triangles.size(); ++i) {
            const CollisionTriangle& triangle = mesh.triangles[i];
            const glm::vec3 mn = glm::min(glm::min(triangle.a, triangle.b),
                                          triangle.c);
            const glm::vec3 mx = glm::max(glm::max(triangle.a, triangle.b),
                                          triangle.c);
            dirtyMin = glm::min(dirtyMin, mn);
            dirtyMax = glm::max(dirtyMax, mx);
            dirtyAny = true;
        }
        hideNpcRange(npcWorld, range.first, range.count);
        hiddenAdded += range.count;
        changed = true;
    }

    std::vector<NpcBlockRange> nextRanges;
    nextRanges.reserve(pending.size());
    for (PendingRange& item : pending) {
        if (item.isNew) {
            appendHeadlessRange(npcWorld, headless, item.headlessFirst,
                                item.range.count, item.range,
                                dirtyMin, dirtyMax, dirtyAny);
            changed = true;
        }
        nextRanges.push_back(std::move(item.range));
    }

    current.hiddenTriangles += hiddenAdded;
    if (changed) {
        current.dirtyMin = dirtyMin;
        current.dirtyMax = dirtyMax;
        current.dirtyValid = dirtyAny;
        rebuildCollisionSolidBounds(mesh);
        rebuildNpcBaseTree(npcWorld);
        clearSmoothTopology(npcWorld);
        ++npcWorld.renderRevision;
    } else {
        current.dirtyValid = false;
    }
    current.ranges = std::move(nextRanges);
    current.valid = true;
    return true;
}

bool authoredNpcBlockTakeDirtyBounds(glm::vec3& outMin, glm::vec3& outMax)
{
    NpcBlockState& current = npcState();
    if (!current.dirtyValid)
        return false;
    outMin = current.dirtyMin;
    outMax = current.dirtyMax;
    current.dirtyValid = false;
    return true;
}

bool authoredNpcBlockNeedsCompaction(const World& npcWorld)
{
    const NpcBlockState& current = npcState();
    if (!current.valid || current.hiddenTriangles == 0)
        return false;
    if (current.hiddenTriangles >= 1024)
        return true;
    const size_t total = npcWorld.collisionMesh.triangles.size();
    return total > 0 && current.hiddenTriangles * 4 > total;
}
