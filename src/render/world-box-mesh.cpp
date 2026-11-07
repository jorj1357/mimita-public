// 2026-11-07
#include "render/world-box-mesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "hot-reload/module-host.h"

namespace {

const float kCubePositions[8][3] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

const int kFaceVerts[6][4] = {
    {1, 2, 6, 5}, {0, 4, 7, 3}, {3, 7, 6, 2},
    {0, 1, 5, 4}, {4, 5, 6, 7}, {1, 0, 3, 2},
};

const float kFaceNormals[6][3] = {
    {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
};

const int kFaceUvAxis[6][2] = {
    {2, 1}, {2, 1}, {0, 2}, {0, 2}, {0, 1}, {0, 1},
};

void transformPoint(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14];
}

bool fallbackBox(const mimita_world_rendering::BoxMeshV1& request,
                 mimita_world_rendering::BoxMeshV1& out)
{
    if (request.size[0] <= 0.0f || request.size[1] <= 0.0f ||
        request.size[2] <= 0.0f)
        return false;
    out = request;
    out.faceCount = mimita_world_rendering::FACE_COUNT;
    for (int face = 0; face < 6; ++face) {
        auto& meshFace = out.faces[face];
        meshFace.faceIndex = face;
        std::snprintf(meshFace.material, sizeof(meshFace.material), "%s",
                      request.materials[face]);
        std::memcpy(meshFace.normal, kFaceNormals[face],
                    sizeof(meshFace.normal));
        const int uAxis = kFaceUvAxis[face][0];
        const int vAxis = kFaceUvAxis[face][1];
        float uScale = request.faceUvScale[face][0] > 0.0f
            ? request.faceUvScale[face][0] : request.uvScale[0];
        float vScale = request.faceUvScale[face][1] > 0.0f
            ? request.faceUvScale[face][1] : request.uvScale[1];
        if (uScale <= 0.0f) uScale = 1.0f;
        if (vScale <= 0.0f) vScale = 1.0f;
        for (int corner = 0; corner < 4; ++corner) {
            const float* local = kCubePositions[kFaceVerts[face][corner]];
            transformPoint(request.transform, local, meshFace.positions[corner]);
            meshFace.uv[corner][0] =
                (local[uAxis] + 0.5f) * request.size[uAxis] * uScale;
            meshFace.uv[corner][1] =
                (local[vAxis] + 0.5f) * request.size[vAxis] * vScale;
        }
    }
    out.result = 1;
    return true;
}

} // namespace

bool buildBoxMesh(const mimita_world_rendering::BoxMeshV1& request,
                  mimita_world_rendering::BoxMeshV1& out)
{
    out = request;
    if (ModuleHost::instance().worldRenderingBuildBox(&out) && out.result)
        return true;
    return fallbackBox(request, out);
}
