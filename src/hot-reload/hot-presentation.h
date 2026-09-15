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
};

// Logical resource ids registered by the cold presentation renderer through the
// generation-aware PresentationResourceProvider.
static constexpr std::uint64_t HOT_MESH_CUBE = gameHash("mesh.cube");
static constexpr std::uint64_t HOT_MESH_ACTOR = gameHash("mesh.actor");
static constexpr std::uint64_t HOT_MESH_ROCKET = gameHash("mesh.rocket");
static constexpr std::uint64_t HOT_MESH_GRENADE = gameHash("mesh.grenade");
static constexpr std::uint64_t HOT_TEX_DEFAULT = gameHash("texture.default");
static constexpr std::uint64_t HOT_TEX_ROCKET = gameHash("texture.rocket");
static constexpr std::uint64_t HOT_TEX_GRENADE = gameHash("texture.grenade");
