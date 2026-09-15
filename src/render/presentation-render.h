// 09 14 2026
/* purpose
* Cold generic presentation renderer. Owns the low-level GPU mechanism for the
* generic `render.mesh` capability: it resolves logical mesh/texture resource
* ids through the generation-aware PresentationResourceProvider and draws with
* the shared basic shader. Hot presentation systems decide what to submit; this
* file only executes generic GPU work. No Player/NPC/Projectile branch.
* Does NOT own asset policy, scene traversal, or gameplay state.
*/
#pragma once

#include <cstdint>
#include <string>

#include "hot-reload/game-api.h"

namespace PresentationRender {

// Registers the built-in generic resources (a procedural cube mesh and one
// texture) with the generation-aware provider. Safe to call once after the
// renderer exists.
void init();

// Re-applies any file-backed generic resources whose content hash changed.
// Failed loads preserve the last-good generation.
void poll();

// Generic mesh submission. Resolves mesh/texture handles from the provider and
// draws them; a missing renderer or handle is a safe no-op. Always counts the
// submission so headless tests can verify the chain.
void submitMesh(const GameRenderMeshCommandV1& command);

// Number of generic mesh submissions observed (verified headlessly).
std::uint64_t submittedMeshCount();

// Register an arbitrary logical presentation resource (GLB mesh or image
// texture) from hot code. The provider owns parsing, validation, generation
// swap, and last-good preservation; a file-backed resource is re-polled so its
// generation can change without recreating the entities that reference it.
// Generic across tools/props/particles; no per-feature resource slot.
bool registerLogicalResource(std::uint64_t logicalId, std::uint32_t kind,
                             const char* path, bool applyNow,
                             std::uint32_t* outGeneration);

// Resolve the bind transform of a named part on the entity's current generic
// mesh, relative to the entity model. Resolves the live resource generation each
// call (never a raw handle), so a generation swap cannot leave a stale pointer.
bool meshPartBind(std::uint64_t entity, std::uint64_t part, float outMat16[16]);

// Generic skeleton consumption: a mesh whose parts carry bone hashes is drawn
// per part using the entity's SkeletonInstances pose (looked up by EntityId).
// Entities without an instance fall back to a single static draw.
std::uint64_t skinnedSubmissionCount();
std::uint64_t staticFallbackCount();

// Number of generic mesh submissions drawn in camera-relative VIEW space (the
// generic first-person/viewmodel presentation context).
std::uint64_t viewSpaceSubmissionCount();

// Headless test hook: install a part mesh (bone hashes, optional bind matrices)
// without GPU buffers so the EntityId -> SkeletonInstances consumption and the
// static fallback can be verified without a GL context.
bool debugInstallPartMesh(std::uint64_t logicalId, const std::uint64_t* boneHashes,
                          std::uint32_t boneCount, const float* bindMatrices16);

// Validate a GLB container (magic/version/length) without parsing or uploading.
// Used headlessly to prove malformed files are rejected while last-good stays.
bool validateGlbFile(const char* path, std::string& error);

// Parse a GLB and report its articulated part count / bone hashes without GPU
// upload, so the real actor mesh's part structure can be verified headlessly.
std::uint32_t inspectGlbParts(const char* path, std::uint64_t* outPartHashes,
                              std::uint32_t maxOut);

} // namespace PresentationRender
