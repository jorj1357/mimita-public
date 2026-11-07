// 2026-11-07
/* purpose
* shared world-rendering module generation
* owns the canonical six-face box geometry/UV policy and face/culling analysis
* the host owns GPU/device/world data and actual draw submission
* this file DOES NOT include GL, World, Camera, or engine headers
*/
#include "modules/api/world-rendering-api.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <module-identity.h>

namespace {

using mimita_module::HostServicesV1;
using mimita_module::ModuleEventV1;
using mimita_world_rendering::BoxMeshV1;
using mimita_world_rendering::FaceAuditV1;
using mimita_world_rendering::MeshFaceV1;

const HostServicesV1* gHost = nullptr;

// Canonical cube vertices in local [-0.5, 0.5] space.
const float kCubePositions[8][3] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

// Face order is +X, -X, +Y, -Y, +Z, -Z. Each quad is counter-clockwise as
// viewed from outside the box.
const int kFaceVerts[6][4] = {
    {1, 2, 6, 5}, // +X
    {0, 4, 7, 3}, // -X
    {3, 7, 6, 2}, // +Y
    {0, 1, 5, 4}, // -Y
    {4, 5, 6, 7}, // +Z
    {1, 0, 3, 2}, // -Z
};

const float kFaceNormals[6][3] = {
    {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
};

const int kFaceUvAxis[6][2] = {
    {2, 1}, // +X: z, y
    {2, 1}, // -X
    {0, 2}, // +Y: x, z
    {0, 2}, // -Y
    {0, 1}, // +Z: x, y
    {0, 1}, // -Z
};

void transformPoint(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14];
}

void transformNormal(const float m[16], const float in[3], float out[3])
{
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
    gHost = services;
    return 1;
}

void shutdown()
{
    gHost = nullptr;
}

float faceUvScale(const BoxMeshV1& request, int face, int axis)
{
    const float overrideValue = request.faceUvScale[face][axis];
    if (overrideValue > 0.0f)
        return overrideValue;
    return request.uvScale[axis] > 0.0f ? request.uvScale[axis] : 1.0f;
}

int buildBox(BoxMeshV1* request)
{
    if (!request || request->size[0] <= 0.0f || request->size[1] <= 0.0f ||
        request->size[2] <= 0.0f)
        return 0;

    request->faceCount = mimita_world_rendering::FACE_COUNT;
    for (int face = 0; face < 6; ++face) {
        MeshFaceV1& out = request->faces[face];
        out.faceIndex = face;
        const int uAxis = kFaceUvAxis[face][0];
        const int vAxis = kFaceUvAxis[face][1];
        const float uScale = faceUvScale(*request, face, 0);
        const float vScale = faceUvScale(*request, face, 1);
        std::snprintf(out.material, sizeof(out.material), "%s",
                      request->materials[face]);
        transformNormal(request->transform, kFaceNormals[face], out.normal);
        for (int corner = 0; corner < 4; ++corner) {
            const float* local = kCubePositions[kFaceVerts[face][corner]];
            transformPoint(request->transform, local, out.positions[corner]);
            out.uv[corner][0] =
                (local[uAxis] + 0.5f) * request->size[uAxis] * uScale;
            out.uv[corner][1] =
                (local[vAxis] + 0.5f) * request->size[vAxis] * vScale;
        }
    }
    request->result = 1;
    return 1;
}

int analyzeFaces(mimita_world_rendering::AnalyzeFacesV1* request)
{
    if (!request || request->capacity < mimita_world_rendering::FACE_COUNT)
        return 0;
    for (int face = 0; face < 6; ++face) {
        FaceAuditV1& out = request->faces[face];
        float localCenter[3] = {
            kFaceNormals[face][0] * 0.5f,
            kFaceNormals[face][1] * 0.5f,
            kFaceNormals[face][2] * 0.5f,
        };
        transformPoint(request->transform, localCenter, out.center);
        transformNormal(request->transform, kFaceNormals[face], out.normal);
        out.index = face;
        const float dx = request->cameraPosition[0] - out.center[0];
        const float dy = request->cameraPosition[1] - out.center[1];
        const float dz = request->cameraPosition[2] - out.center[2];
        out.cameraOutside =
            (out.normal[0] * dx + out.normal[1] * dy + out.normal[2] * dz) > 0.0f;
    }
    request->result = 1;
    return 1;
}

int onEvent(ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    switch (event->typeId) {
    case mimita_module::EVENT_RENDER_ANALYZE_FACES:
        return analyzeFaces(static_cast<mimita_world_rendering::AnalyzeFacesV1*>(
            event->payload));
    case mimita_module::EVENT_RENDER_BUILD_BOX:
        return buildBox(static_cast<BoxMeshV1*>(event->payload));
    default:
        return 0;
    }
}

std::uint32_t exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION,
     sizeof(mimita_module::ModuleV1),
     "world-rendering",
     MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize,
    shutdown,
    onEvent,
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
