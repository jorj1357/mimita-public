#pragma once
#include <cstdint>
#include "modules/api/module-abi.h"

namespace mimita_actor_rendering {
constexpr std::uint32_t ABI_VERSION = 2;
struct PoseV1 {
    float current[16];
    float previous[16];
    float result[16];
    float alpha;
    float stepMeters;
    std::uint32_t interpolate;
    std::uint32_t hasPrevious;
    std::uint32_t usedPrevious;
};
struct BoundsV1 {
    // Positions are borrowed for this synchronous call and never retained.
    const std::uint8_t* vertices;
    std::uint32_t first;
    std::uint32_t count;
    std::uint32_t stride;
    float minimum[3];
    float maximum[3];
    std::uint32_t valid;
};
struct OutlineV1 {
    float maxOffset;
    float maxExtent;
    float cameraDistance;
    float sizeScale;
    float thickness;
    float worldThickness;
    std::uint32_t boundsValid;
    std::uint32_t draw;
    std::uint32_t rejected;
    std::uint32_t cameraInside;
    std::uint32_t isLocal;
};
struct PartV1 {
    std::uint32_t nodeValid;
    std::uint32_t vertexCount;
    std::uint32_t isHead;
    std::uint32_t bodyLayer; // ActorBodyLayer numeric value; unknown maps to Skin.
};
struct DrawPlanV1 {
    const PartV1* parts;
    std::uint8_t* partDrawMask;
    std::uint32_t partCount;
    std::uint32_t partCapacity;
    std::uint8_t requestedLayers[4]; // trail, capsule, outline, wireframe = 0..3.
    std::uint8_t orderedLayers[4];
    std::uint32_t hideHead;
    std::uint32_t outlinePass;
    std::uint32_t whiteOverride;
    std::uint32_t debugCull;
    std::uint32_t combineAllowed;
};
}
MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
