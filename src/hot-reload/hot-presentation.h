// 09 14 2026
/* purpose
* Shared hot presentation-entity state. One canonical `PresentationState`
* dynamic component describes how any entity draws (logical mesh/texture
* resource ids, flags, scale, color). Used by tool/projectile behaviors that
* create an entity and by the canonical `hot.presentation-mesh` render system.
* Logical resource ids only - never a raw GPU handle. Hot-only header: not a
* GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

// Generic presentation component. Any entity may carry it; the conceptual type
// is never inspected by the renderer.
static constexpr std::uint64_t HOT_PRESENTATION_COMPONENT =
    gameHash("PresentationState");

struct HotPresentationStateV1 {
    std::uint64_t meshResourceId;
    std::uint64_t textureResourceId;
    std::uint32_t flags;
    float scale;
    float color[4];
    // Append-only: per-axis scale multiplier applied on top of `scale`. Zero
    // components mean 1. Lets a generic cylinder mesh become an elongated beam/
    // tracer (thickness != length) without a new renderer branch.
    float scaleXYZ[3];
    std::uint32_t reserved2;
};

// Generic attachment state. A presentation entity may carry it to follow a
// named attachment point (socket/bone hash) on another entity. Hot policy owns
// the local offset/rotation/scale and the presentation context; the kernel owns
// the socket transform mechanism. It is a PRESENTATION override: the child's
// authoritative Transform is never overwritten. The resolved world transform is
// written back here and consumed by the render system.
static constexpr std::uint64_t HOT_ATTACHMENT_COMPONENT = gameHash("AttachmentState");
static constexpr std::uint32_t HOT_ATTACHMENT_CONTEXT_WORLD = 0;
static constexpr std::uint32_t HOT_ATTACHMENT_CONTEXT_VIEW = 1;
static constexpr std::uint32_t HOT_ATTACHMENT_FLAG_VISIBLE = 1u;

struct HotAttachmentStateV1 {
    // in
    std::uint64_t parentEntity;
    std::uint64_t socket;          // gameHash("rightArm") / gameHash("muzzle")
    float localPosition[3];
    float localRotation[4];        // quaternion xyzw
    float localScale[3];
    std::uint32_t context;         // HOT_ATTACHMENT_CONTEXT_*
    std::uint32_t flags;           // HOT_ATTACHMENT_FLAG_*
    // out (filled each frame by hot.attachment; presentation-only)
    float worldPosition[3];
    float worldRotation[4];
    float worldScale[3];
    std::uint32_t resolved;
    std::uint32_t reserved;
    // Muzzle: local offset (in, from the recipe) and the resolved world point +
    // barrel direction (out). The cold fire path reads the world muzzle so the
    // shot/tracer/flash starts at the visible gun. Hot-owned.
    float localMuzzle[3];
    float muzzleWorldPosition[3];
    float forward[3];
};

// Written by hot actor-overlay policy on an actor entity to declare that the
// generic overlay path owns that actor's name/health rendering. The cold
// nameplate/healthbar path yields per-actor (no duplicate owner, partial
// coverage stays safe).
static constexpr std::uint64_t HOT_OVERLAY_CLAIM_COMPONENT =
    gameHash("ActorOverlayClaim");
struct HotOverlayClaimV1 {
    std::uint32_t owned;      // 1 = hot overlay owns this actor
    std::uint32_t reserved;
};

// Written by hot tool-presentation policy on a possessed actor to declare that
// the equipped tool's presentation is hot-owned. Cold weapon presentation reads
// it and yields for that toolKey (one owner). Generic: a tool key hash, never a
// weapon enum.
static constexpr std::uint64_t HOT_TOOL_CLAIM_COMPONENT =
    gameHash("ToolPresentationClaim");
struct HotToolClaimV1 {
    std::uint64_t toolKey;    // equipped tool identity (network id/hash)
    std::uint32_t context;    // HOT_ATTACHMENT_CONTEXT_*
    std::uint32_t migrated;   // 1 = hot presentation owns this tool
    // Append-only: the claimed tool EntityId and its logical mesh. The cold
    // renderer verifies the mesh actually resolves before yielding, so a claim
    // alone can never suppress the normal viewmodel.
    std::uint64_t toolEntity;
    std::uint64_t meshResourceId;
};

// Logical resource ids registered by the cold presentation renderer through the
// generation-aware PresentationResourceProvider.
static constexpr std::uint64_t HOT_MESH_CUBE = gameHash("mesh.cube");
static constexpr std::uint64_t HOT_MESH_ACTOR = gameHash("mesh.actor");
static constexpr std::uint64_t HOT_MESH_ROCKET = gameHash("mesh.rocket");
static constexpr std::uint64_t HOT_MESH_GRENADE = gameHash("mesh.grenade");
// Generic primitive aliases registered by the cold renderer (no feature slot).
static constexpr std::uint64_t HOT_MESH_SPHERE = gameHash("mesh.sphere");
static constexpr std::uint64_t HOT_MESH_BEAM = gameHash("mesh.beam");
static constexpr std::uint64_t HOT_MESH_HEXAGON = gameHash("mesh.hexagon");
// Generic dynamic-light emitter kind for the existing effect.spawn descriptor.
static constexpr std::uint64_t HOT_EFFECT_LIGHT = gameHash("light.dynamic");
static constexpr std::uint64_t HOT_TEX_DEFAULT = gameHash("texture.default");
static constexpr std::uint64_t HOT_TEX_ROCKET = gameHash("texture.rocket");
static constexpr std::uint64_t HOT_TEX_GRENADE = gameHash("texture.grenade");
