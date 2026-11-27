// 2026-11-27
/* purpose
* Apply authored-block edits to an already loaded headless world without the
* whole-world collision LOD (decimate/union/face-removal) pass, so the
* authoritative server collision matches the editor on the next tick.
* Also carries the small "last applied block vs non-block geometry" bookkeeping
* used to decide whether a signature change is block-only.
* This file DOES NOT own map JSON, rendering, navmesh baking, or the editor.
* No new fields are added to World or HeadlessWorld.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gamemode/map-config.h"

struct World;

namespace MimitaNet {
struct HeadlessWorld;

// Rebuild the headless chunk broadphase without the 4^3 projectile sub-grid.
// Safe because gatherHeadlessTrianglesForAABB falls back to chunk-level when the
// sub-grid is absent, and an edited live world must not stall the fixed tick
// rebuilding per-chunk sub-cells that will be superseded on the next edit.
void buildHeadlessCollisionChunksWithoutSubGrids(HeadlessWorld& world);
}

// Content hash of every authored field that changes collision, spawn, GLB
// placement, or generated chunk geometry EXCEPT the authored block list.
uint64_t authoredNonBlockGeometrySignature(const MapObjectiveConfig& config);

// Rebuild the authored blocks (and GLB/chunk/spawn tail) on top of the pristine
// base captured by the last full map load. Skips decimate/union/face-removal
// and the navmesh bake; caller rebuilds the NPC mirror and refreshes broadphase
// via the normal NPC collision build.
bool rebuildAuthoredBlocksForAuthority(MimitaNet::HeadlessWorld& world,
                                       const std::string& mapId,
                                       bool allowStreamedFootprint);

// Deterministic collision triangle count for one authored block, using the
// same builders the headless apply uses. Used to locate the authored block tail
// that was appended after the pristine GLB base.
size_t authoredBlockTriangleCount(const AuthoredBlock& block);

// Locate the authored block tail recorded by the last non-finalized headless
// authored apply. Returns false when LOD rewrote indices or no tail exists.
bool authoredHeadlessBlockTail(size_t& first, size_t& count);

// Incremental NPC collision mirror. Ranges are captured once after a full
// non-finalized authored apply, then later edits hide removed block triangles
// and append added/changed block triangles copied from the headless tail.
// Returns false (without mutating) when the stored ranges cannot be trusted;
// the caller then does one full build and captures again.
bool authoredNpcBlockDeltaApply(
    World& npcWorld, const MimitaNet::HeadlessWorld& headless,
    const std::vector<AuthoredBlock>& blocks, std::string& failure);
void authoredNpcBlockStateCapture(
    const World& npcWorld, const MimitaNet::HeadlessWorld& headless,
    const std::vector<AuthoredBlock>& blocks);
void authoredNpcBlockStateInvalidate();
const std::string& authoredNpcBlockStateLastFailure();

// World-space bounds of the block triangles hidden/appended by the last patch.
// Consumed by the caller to mark only the touched Recast tiles.
bool authoredNpcBlockTakeDirtyBounds(glm::vec3& outMin, glm::vec3& outMax);

// True when hidden authored triangles have accumulated enough that one full
// NPC rebuild (which also restores smooth topology) is cheaper than carrying
// them indefinitely.
bool authoredNpcBlockNeedsCompaction(const World& npcWorld);

// Parity bookkeeping for the block-only fast path.
void authoredBlockAuthorityReset();
void authoredBlockAuthorityMarkApplied(uint64_t nonBlockSignature,
                                       uint64_t nowMs);
bool authoredBlockAuthorityCanApply(uint64_t nonBlockSignature);
bool authoredBlockAuthorityThrottleAllows(uint64_t nowMs);
// map-authored-parity: saved for batch build 2026-11-27T01:45Z
