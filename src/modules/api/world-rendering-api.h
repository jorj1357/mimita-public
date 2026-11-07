// 2026-11-07
/* purpose
* define the versioned plain-data boundary for the shared world-rendering module
* host owns GPU/device/world data; the module owns renderer policy calculations
* this file DOES NOT expose GL, World, Camera, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_world_rendering {

constexpr std::uint32_t ABI_VERSION = 2;
constexpr std::uint32_t FACE_COUNT = 6;
constexpr std::uint32_t FACE_MATERIAL_CAPACITY = 64;

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    mimita_module::LogFn log;
};

struct FaceAuditV1
{
    std::int32_t index;
    float normal[3];
    float center[3];
    std::int32_t cameraOutside;
};

struct AnalyzeFacesV1
{
    float transform[16];
    float cameraPosition[3];
    FaceAuditV1 faces[FACE_COUNT];
    std::uint32_t capacity;
    int result;
};

// One canonical quad for a box face. Face order is +X, -X, +Y, -Y, +Z, -Z.
struct MeshFaceV1
{
    std::int32_t faceIndex;
    float normal[3];
    float positions[4][3];
    float uv[4][2];
    char material[FACE_MATERIAL_CAPACITY];
};

struct BoxMeshV1
{
    float transform[16];
    float size[3];
    char materials[FACE_COUNT][FACE_MATERIAL_CAPACITY];
    // Default UV repeat multiplier. 1/1 means one texture repeat per world unit.
    float uvScale[2];
    // Per-face overrides; <= 0 falls back to uvScale on that axis.
    float faceUvScale[FACE_COUNT][2];
    MeshFaceV1 faces[FACE_COUNT];
    std::uint32_t faceCount;
    int result;
};

} // namespace mimita_world_rendering

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
