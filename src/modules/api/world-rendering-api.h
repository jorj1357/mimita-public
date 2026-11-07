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

constexpr std::uint32_t ABI_VERSION = 1;
constexpr std::uint32_t FACE_COUNT = 6;

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

struct ModuleV1
{
    mimita_module::BaseV1 base;
    int (MIMITA_MODULE_CALL* initialize)(const HostServicesV1* services);
    void (MIMITA_MODULE_CALL* shutdown)();
    // Transform is a column-major 4x4 model matrix. Unit-cube local centers
    // and outward normals are transformed by the module. Returns 1 on success.
    int (MIMITA_MODULE_CALL* analyzeFaces)(
        const float transform[16], const float cameraPosition[3],
        FaceAuditV1* outFaces, std::uint32_t faceCapacity);
    std::uint32_t (MIMITA_MODULE_CALL* exportState)(
        std::uint8_t* buffer, std::uint32_t capacity);
    int (MIMITA_MODULE_CALL* importState)(
        const std::uint8_t* buffer, std::uint32_t size);
};

} // namespace mimita_world_rendering

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
