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

// Validate a GLB container (magic/version/length) without parsing or uploading.
// Used headlessly to prove malformed files are rejected while last-good stays.
bool validateGlbFile(const char* path, std::string& error);

} // namespace PresentationRender
