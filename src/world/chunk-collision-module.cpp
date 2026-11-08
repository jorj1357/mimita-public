// 2026-11-07
/* purpose
* live generated-chunk collision and bounded contact correction policy
* selects collidable parts and produces world-space box triangles after chunk
* and part transforms; the host owns static union and physical registration
* this file DOES NOT include PhysicalEntitySystem, World, or STL types
*/
#include "modules/api/actor-contact-policy-api.h"
#include "modules/api/chunk-collision-api.h"

#include <cmath>
#include <cstring>

#include <module-identity.h>

namespace mimita_collision_policy_module {
void fillContactPolicy(mimita_collision_policy::ContactPolicyV1* policy);
void fillSurfacePolicy(mimita_collision_policy::SurfacePolicyV1* policy);
int solveContacts(mimita_collision_policy::ContactSolveV1* request);
}

namespace {

using mimita_chunk_collision::BuildChunkV1;
using mimita_chunk_collision::CollisionPartV1;
using mimita_chunk_collision::TriangleV1;
using mimita_module::HostServicesV1;
using mimita_module::ModuleEventV1;

const HostServicesV1* gHost = nullptr;

const float kCube[8][3] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

const int kFaceVerts[6][4] = {
    {1, 2, 6, 5}, {0, 4, 7, 3}, {3, 7, 6, 2},
    {0, 1, 5, 4}, {4, 5, 6, 7}, {1, 0, 3, 2},
};

void multiply(const float a[16], const float b[16], float out[16])
{
    float result[16] = {};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += a[k * 4 + row] * b[column * 4 + k];
            result[column * 4 + row] = value;
        }
    }
    std::memcpy(out, result, sizeof(result));
}

void partMatrix(const CollisionPartV1& part, float out[16])
{
    const float cx = std::cos(part.rotation[0] * 3.14159265358979323846f / 180.0f);
    const float sx = std::sin(part.rotation[0] * 3.14159265358979323846f / 180.0f);
    const float cy = std::cos(part.rotation[1] * 3.14159265358979323846f / 180.0f);
    const float sy = std::sin(part.rotation[1] * 3.14159265358979323846f / 180.0f);
    const float cz = std::cos(part.rotation[2] * 3.14159265358979323846f / 180.0f);
    const float sz = std::sin(part.rotation[2] * 3.14159265358979323846f / 180.0f);

    // Rz * Ry * Rx.
    const float r00 = cz * cy;
    const float r01 = cz * sy * sx - sz * cx;
    const float r02 = cz * sy * cx + sz * sx;
    const float r10 = sz * cy;
    const float r11 = sz * sy * sx + cz * cx;
    const float r12 = sz * sy * cx - cz * sx;
    const float r20 = -sy;
    const float r21 = cy * sx;
    const float r22 = cy * cx;

    std::memset(out, 0, sizeof(float) * 16);
    out[0] = r00 * part.size[0];
    out[1] = r10 * part.size[0];
    out[2] = r20 * part.size[0];
    out[4] = r01 * part.size[1];
    out[5] = r11 * part.size[1];
    out[6] = r21 * part.size[1];
    out[8] = r02 * part.size[2];
    out[9] = r12 * part.size[2];
    out[10] = r22 * part.size[2];
    out[12] = part.position[0];
    out[13] = part.position[1];
    out[14] = part.position[2];
    out[15] = 1.0f;
}

void transformPoint(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14];
}

void subtract3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

void cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void normalize3(float v[3])
{
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length <= 0.000001f) {
        v[0] = v[1] = v[2] = 0.0f;
        return;
    }
    v[0] /= length;
    v[1] /= length;
    v[2] /= length;
}

int buildChunk(BuildChunkV1* request)
{
    if (!request || !request->parts || !request->outTriangles)
        return 0;
    std::uint32_t count = 0;
    for (std::uint32_t i = 0; i < request->partCount; ++i) {
        const CollisionPartV1& part = request->parts[i];
        if (!part.collision || part.size[0] <= 0.0001f ||
            part.size[1] <= 0.0001f || part.size[2] <= 0.0001f)
            continue;
        float local[16];
        partMatrix(part, local);
        float world[16];
        multiply(request->chunkTransform, local, world);

        float corners[8][3];
        for (int v = 0; v < 8; ++v)
            transformPoint(world, kCube[v], corners[v]);

        for (int face = 0; face < 6; ++face) {
            const float* p0 = corners[kFaceVerts[face][0]];
            const float* p1 = corners[kFaceVerts[face][1]];
            const float* p2 = corners[kFaceVerts[face][2]];
            const float* p3 = corners[kFaceVerts[face][3]];
            const int order[2][3] = {{0, 1, 2}, {0, 2, 3}};
            const float* quad[4] = {p0, p1, p2, p3};
            for (int triangle = 0; triangle < 2; ++triangle) {
                if (count >= request->capacity)
                    return 0;
                if (request->outSolidIds)
                    request->outSolidIds[count] = i + 1;
                TriangleV1& out = request->outTriangles[count++];
                std::memcpy(out.a, quad[order[triangle][0]], sizeof(out.a));
                std::memcpy(out.b, quad[order[triangle][1]], sizeof(out.b));
                std::memcpy(out.c, quad[order[triangle][2]], sizeof(out.c));
                float edge0[3], edge1[3], normal[3];
                subtract3(out.b, out.a, edge0);
                subtract3(out.c, out.a, edge1);
                cross3(edge0, edge1, normal);
                normalize3(normal);
                std::memcpy(out.normal, normal, sizeof(out.normal));
            }
        }
    }
    request->outCount = count;
    request->result = 1;
    return 1;
}

int initialize(const HostServicesV1* services)
{
    if (!services || services->version != mimita_chunk_collision::ABI_VERSION ||
        services->structSize != sizeof(HostServicesV1))
        return 0;
    gHost = services;
    return 1;
}

void shutdown()
{
    gHost = nullptr;
}

int onEvent(ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId == mimita_module::EVENT_COLLISION_BUILD_CHUNK)
        return buildChunk(static_cast<BuildChunkV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_COLLISION_CONTACT_POLICY) {
        mimita_collision_policy_module::fillContactPolicy(
            static_cast<mimita_collision_policy::ContactPolicyV1*>(
                event->payload));
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_COLLISION_SURFACE_POLICY) {
        mimita_collision_policy_module::fillSurfacePolicy(
            static_cast<mimita_collision_policy::SurfacePolicyV1*>(
                event->payload));
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_COLLISION_CONTACT_SOLVE &&
        event->payloadSize == sizeof(mimita_collision_policy::ContactSolveV1))
        return mimita_collision_policy_module::solveContacts(
            static_cast<mimita_collision_policy::ContactSolveV1*>(event->payload));
    return 0;
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
     "chunk-collision",
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
