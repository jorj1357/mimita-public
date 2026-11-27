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

#include "gamemode/map-config.h"

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

// Parity bookkeeping for the block-only fast path.
void authoredBlockAuthorityReset();
void authoredBlockAuthorityMarkApplied(uint64_t nonBlockSignature,
                                       uint64_t nowMs);
bool authoredBlockAuthorityCanApply(uint64_t nonBlockSignature);
bool authoredBlockAuthorityThrottleAllows(uint64_t nowMs);
// map-authored-parity: saved for batch build 2026-11-27T01:45Z
