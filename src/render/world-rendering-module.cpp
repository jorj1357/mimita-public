// 2026-11-07
/* purpose
* first live world-rendering module generation
* owns shared authored-box face/culling analysis while the host owns the GPU,
* renderer, world data, and actual draw submission
* this file DOES NOT include GL, World, Camera, or engine headers
*/
#include "modules/api/world-rendering-api.h"

#include <cmath>
#include <cstring>

#include <module-identity.h>

namespace {

using mimita_world_rendering::FaceAuditV1;
using mimita_world_rendering::HostServicesV1;

const HostServicesV1* gServices = nullptr;

const float kLocalNormals[6][3] = {
    {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
    {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
};

void transformPoint(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14];
}

void transformNormal(const float m[16], const float in[3], float out[3])
{
    // Inverse-transpose upper 3x3 for non-uniform scale. Computed explicitly
    // for the small fixed-size matrix.
    const float a = m[0], b = m[4], c = m[8];
    const float d = m[1], e = m[5], f = m[9];
    const float g = m[2], h = m[6], i = m[10];
    const float cof0 = e * i - f * h;
    const float cof1 = f * g - d * i;
    const float cof2 = d * h - e * g;
    const float det = a * cof0 + b * cof1 + c * cof2;
    if (det == 0.0f) {
        std::memcpy(out, in, sizeof(float) * 3);
        return;
    }
    const float invDet = 1.0f / det;
    const float nx = (cof0 * in[0] + (c * h - b * i) * in[1] +
                      (b * f - c * e) * in[2]) * invDet;
    const float ny = (cof1 * in[0] + (a * i - c * g) * in[1] +
                      (c * d - a * f) * in[2]) * invDet;
    const float nz = (cof2 * in[0] + (b * g - a * h) * in[1] +
                      (a * e - b * d) * in[2]) * invDet;
    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length <= 0.000001f) {
        std::memcpy(out, in, sizeof(float) * 3);
        return;
    }
    out[0] = nx / length;
    out[1] = ny / length;
    out[2] = nz / length;
}

int initialize(const HostServicesV1* services)
{
    if (!services || services->version != mimita_world_rendering::ABI_VERSION ||
        services->structSize != sizeof(HostServicesV1))
        return 0;
    gServices = services;
    return 1;
}

void shutdown()
{
    gServices = nullptr;
}

int analyzeFaces(const float transform[16], const float cameraPosition[3],
                 FaceAuditV1* outFaces, std::uint32_t faceCapacity)
{
    if (!transform || !cameraPosition || !outFaces ||
        faceCapacity < mimita_world_rendering::FACE_COUNT)
        return 0;

    for (int face = 0; face < 6; ++face) {
        float localCenter[3] = {
            kLocalNormals[face][0] * 0.5f,
            kLocalNormals[face][1] * 0.5f,
            kLocalNormals[face][2] * 0.5f,
        };
        transformPoint(transform, localCenter, outFaces[face].center);
        transformNormal(transform, kLocalNormals[face], outFaces[face].normal);
        outFaces[face].index = face;
        const float dx = cameraPosition[0] - outFaces[face].center[0];
        const float dy = cameraPosition[1] - outFaces[face].center[1];
        const float dz = cameraPosition[2] - outFaces[face].center[2];
        outFaces[face].cameraOutside =
            (outFaces[face].normal[0] * dx + outFaces[face].normal[1] * dy +
             outFaces[face].normal[2] * dz) > 0.0f;
    }
    return 1;
}

std::uint32_t exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_world_rendering::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION,
     sizeof(mimita_world_rendering::ModuleV1),
     "world-rendering",
     MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize,
    shutdown,
    analyzeFaces,
    exportState,
    importState,
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    if (abiVersion != mimita_module::ABI_VERSION)
        return nullptr;
    return &gModule.base;
}
// hot-reload proof: world-rendering generation 2
