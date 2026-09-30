// 2026-09-30
/* purpose
* Implement the MiMITA boolean-mesh wrapper. This is the only translation unit
* in the game that includes the vendored Manifold headers. It converts a MiMITA
* BooleanMesh into Manifold's MeshGL, performs the subtraction, maps Manifold
* error values into MiMITA categories, and converts the result back into a
* MiMITA triangle soup with outward-CCW winding, per-triangle normals, and UVs.
* Does NOT own cut sizing, physics motion, rendering, or networking.
* Does NOT expose manifold:: types across its header.
*/

#include "impact/boolean-mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include <manifold/manifold.h>
#include <manifold/mesh.h>

namespace MimitaImpact {
namespace {

constexpr uint32_t kPropsPerVertex = 5; // x, y, z, u, v

// Manifold's automatic segment count (0) is far too coarse for small cutters,
// which produces a faceted cutter whose removed volume is wrong. Scale the
// segment count with radius so a hole keeps a roughly constant edge length.
int circularSegmentsForRadius(float radius)
{
    const int estimate = (int)std::ceil((2.0f * 3.14159265f * radius) / 0.05f);
    return std::clamp(estimate, 8, 48);
}

manifold::vec3 toVec3(const glm::vec3& v)
{
    return manifold::vec3((double)v.x, (double)v.y, (double)v.z);
}

BooleanError mapError(manifold::Manifold::Error error)
{
    switch (error)
    {
    case manifold::Manifold::Error::NoError:
        return BooleanError::None;
    case manifold::Manifold::Error::NotManifold:
        return BooleanError::NotManifold;
    case manifold::Manifold::Error::MissingPositionProperties:
        return BooleanError::MissingPositionProperties;
    case manifold::Manifold::Error::PropertiesWrongLength:
        return BooleanError::PropertiesWrongLength;
    case manifold::Manifold::Error::ResultTooLarge:
        return BooleanError::ResultTooLarge;
    default:
        return BooleanError::Internal;
    }
}

const char* errorText(manifold::Manifold::Error error)
{
    switch (error)
    {
    case manifold::Manifold::Error::NoError: return "NoError";
    case manifold::Manifold::Error::NonFiniteVertex: return "NonFiniteVertex";
    case manifold::Manifold::Error::NotManifold: return "NotManifold";
    case manifold::Manifold::Error::VertexOutOfBounds: return "VertexOutOfBounds";
    case manifold::Manifold::Error::PropertiesWrongLength: return "PropertiesWrongLength";
    case manifold::Manifold::Error::MissingPositionProperties: return "MissingPositionProperties";
    case manifold::Manifold::Error::MergeVectorsDifferentLengths: return "MergeVectorsDifferentLengths";
    case manifold::Manifold::Error::MergeIndexOutOfBounds: return "MergeIndexOutOfBounds";
    case manifold::Manifold::Error::TransformWrongLength: return "TransformWrongLength";
    case manifold::Manifold::Error::RunIndexWrongLength: return "RunIndexWrongLength";
    case manifold::Manifold::Error::FaceIDWrongLength: return "FaceIDWrongLength";
    case manifold::Manifold::Error::InvalidConstruction: return "InvalidConstruction";
    case manifold::Manifold::Error::ResultTooLarge: return "ResultTooLarge";
    case manifold::Manifold::Error::InvalidTangents: return "InvalidTangents";
    case manifold::Manifold::Error::Cancelled: return "Cancelled";
    default: return "Unknown";
    }
}

// Exact bit pattern of a position, used to weld coincident vertices for
// topology without collapsing distinct UVs (Manifold's merge vectors).
struct PositionKey
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    bool operator==(const PositionKey& o) const
    {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct PositionKeyHash
{
    size_t operator()(const PositionKey& k) const
    {
        size_t h = (size_t)k.x * 0x9E3779B97F4A7C15ull;
        h ^= (size_t)k.y * 0xC2B2AE3D27D4EB4Full;
        h ^= (size_t)k.z * 0x165667B19E3779F9ull;
        return h ^ (h >> 32);
    }
};

uint32_t floatBits(float f)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    // Collapse -0.0 onto +0.0 so mirrored authored vertices still weld.
    return bits == 0x80000000u ? 0u : bits;
}

// A triangle soup repeats each shared corner, so coincident positions must be
// merged for Manifold to reconstruct a closed manifold. Distinct UV/material
// corners keep their own vertex; only topology is merged.
void addPositionMerges(manifold::MeshGL& out)
{
    const size_t vertexCount =
        out.numProp > 0 ? out.vertProperties.size() / out.numProp : 0;
    if (vertexCount < 2)
        return;
    std::unordered_map<PositionKey, uint32_t, PositionKeyHash> first;
    first.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i)
    {
        const float* p = &out.vertProperties[i * out.numProp];
        const PositionKey key{floatBits(p[0]), floatBits(p[1]), floatBits(p[2])};
        const auto it = first.find(key);
        if (it == first.end())
        {
            first.emplace(key, (uint32_t)i);
            continue;
        }
        if (it->second == (uint32_t)i)
            continue;
        out.mergeFromVert.push_back((uint32_t)i);
        out.mergeToVert.push_back(it->second);
    }
}

manifold::MeshGL toMeshGL(const BooleanMesh& mesh)
{
    manifold::MeshGL out;
    out.numProp = kPropsPerVertex;
    out.vertProperties.resize(mesh.vertices.size() * kPropsPerVertex);
    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const BooleanMeshVertex& v = mesh.vertices[i];
        float* p = &out.vertProperties[i * kPropsPerVertex];
        p[0] = v.position.x;
        p[1] = v.position.y;
        p[2] = v.position.z;
        p[3] = v.uv.x;
        p[4] = v.uv.y;
    }
    out.triVerts.assign(mesh.indices.begin(), mesh.indices.end());
    addPositionMerges(out);
    return out;
}

// Deterministic triplanar fallback for surfaces Manifold created (the cavity
// and tunnel walls), which do not carry meaningful source UVs.
glm::vec2 fallbackUv(const glm::vec3& p, const glm::vec3& normal,
                     const glm::vec3& half)
{
    const glm::vec3 a = glm::abs(normal);
    const glm::vec3 h = glm::max(half, glm::vec3(1e-4f));
    if (a.z >= a.x && a.z >= a.y)
        return glm::vec2(p.x / (2.0f * h.x) + 0.5f, p.y / (2.0f * h.y) + 0.5f);
    if (a.y >= a.x)
        return glm::vec2(p.x / (2.0f * h.x) + 0.5f, p.z / (2.0f * h.z) + 0.5f);
    return glm::vec2(p.y / (2.0f * h.y) + 0.5f, p.z / (2.0f * h.z) + 0.5f);
}

BooleanMesh fromMeshGL(const manifold::MeshGL& gl, uint32_t baseRunId,
                       uint32_t materialId, const glm::vec3& half)
{
    BooleanMesh out;
    const uint32_t triCount = (uint32_t)gl.NumTri();
    out.vertices.reserve((size_t)triCount * 3u);
    out.indices.reserve((size_t)triCount * 3u);

    size_t run = 0;
    for (uint32_t t = 0; t < triCount; ++t)
    {
        const uint32_t base = 3u * t;
        while (run + 1u < gl.runIndex.size() && gl.runIndex[run + 1u] <= base)
            ++run;
        const bool isBaseRun =
            run < gl.runOriginalID.size() && gl.runOriginalID[run] == baseRunId;

        auto pos = [&](uint32_t corner) {
            const uint32_t v = gl.triVerts[base + corner];
            const float* p = &gl.vertProperties[(size_t)v * gl.numProp];
            return glm::vec3(p[0], p[1], p[2]);
        };
        auto uv = [&](uint32_t corner) {
            const uint32_t v = gl.triVerts[base + corner];
            if (!isBaseRun || gl.numProp < 5u)
                return glm::vec2(0.0f);
            const float* p = &gl.vertProperties[(size_t)v * gl.numProp];
            return glm::vec2(p[3], p[4]);
        };

        const glm::vec3 p0 = pos(0);
        const glm::vec3 p1 = pos(1);
        const glm::vec3 p2 = pos(2);
        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(n);
        if (len <= 1e-12f)
            continue; // Manifold should not emit degenerate triangles.
        n /= len;

        for (uint32_t c = 0; c < 3; ++c)
        {
            const glm::vec3 p = pos(c);
            BooleanMeshVertex v;
            v.position = p;
            v.normal = n;
            v.uv = isBaseRun ? uv(c) : fallbackUv(p, n, half);
            v.materialId = materialId;
            out.indices.push_back((uint32_t)out.vertices.size());
            out.vertices.push_back(v);
        }
    }
    return out;
}

glm::vec3 meshHalfExtent(const BooleanMesh& mesh)
{
    glm::vec3 half(0.0f);
    for (const BooleanMeshVertex& v : mesh.vertices)
        half = glm::max(half, glm::abs(v.position));
    return glm::max(half, glm::vec3(0.5f));
}

// Signed-tetrahedron centroid of a closed mesh, used to seed a fractured
// piece's velocity about the parent. Falls back to the first vertex if the
// mesh is degenerate.
glm::vec3 computeCentroid(const BooleanMesh& mesh)
{
    double volume = 0.0;
    glm::dvec3 moment(0.0);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const glm::dvec3 a(mesh.vertices[mesh.indices[i]].position);
        const glm::dvec3 b(mesh.vertices[mesh.indices[i + 1]].position);
        const glm::dvec3 c(mesh.vertices[mesh.indices[i + 2]].position);
        const double v = glm::dot(a, glm::cross(b, c)) / 6.0;
        volume += v;
        moment += v * (a + b + c) / 4.0;
    }
    if (std::fabs(volume) < 1e-12)
        return mesh.vertices.empty() ? glm::vec3(0.0f)
                                     : mesh.vertices[0].position;
    return glm::vec3(moment / volume);
}

manifold::Manifold buildCapsule(const BooleanCutter& cutter)
{
    const double r = (double)std::max(cutter.radius, 0.0f);
    const double len = (double)std::max(cutter.length, 0.0f);
    const int segments = circularSegmentsForRadius(cutter.radius);

    manifold::Manifold body = manifold::Manifold::Cylinder(len, r, r, segments, true);
    body = body + manifold::Manifold::Sphere(r, segments).Translate(manifold::vec3(0.0, 0.0, len * 0.5));
    body = body + manifold::Manifold::Sphere(r, segments).Translate(manifold::vec3(0.0, 0.0, -len * 0.5));

    glm::vec3 z(0.0f, 0.0f, 1.0f);
    const float dirLen = glm::length(cutter.localDirection);
    if (dirLen > 1e-6f)
        z = cutter.localDirection / dirLen;
    const glm::vec3 up = std::fabs(z.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 x = glm::normalize(glm::cross(up, z));
    const glm::vec3 y = glm::cross(z, x);

    const manifold::mat3x4 transform{
        {x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z},
        {cutter.localCenter.x, cutter.localCenter.y, cutter.localCenter.z}};
    return body.Transform(transform);
}

// Converts one accumulated Manifold difference into the MiMITA-owned result.
void fillResult(const manifold::Manifold& difference, uint32_t baseRunId,
                const BooleanMesh& base, BooleanCutResult& result)
{
    result.remainingVolume = (float)difference.Volume();
    result.triangleCount = (uint32_t)difference.NumTri();

    const std::vector<manifold::Manifold> shells = difference.Decompose();
    result.shellCount = (uint32_t)shells.size();
    for (const manifold::Manifold& shell : shells)
        if (shell.Volume() > 1e-6)
            ++result.componentCount;

    const manifold::MeshGL out = difference.GetMeshGL();
    result.mesh = fromMeshGL(out, baseRunId, base.vertices[0].materialId,
                             meshHalfExtent(base));
    result.success = !result.mesh.empty();
    if (!result.success)
    {
        result.error = BooleanError::Internal;
        result.message = "result conversion produced no triangles";
    }
}

// One cached running boolean result, keyed by a caller-owned session id.
struct BooleanSession
{
    manifold::Manifold running;
    size_t appliedCuts = 0;
    uint32_t baseRunId = 0;
    uint64_t lastUse = 0;
};

std::mutex gSessionMutex;
std::unordered_map<uint64_t, BooleanSession> gSessions;
uint64_t gSessionClock = 0;
constexpr size_t kMaxSessions = 512;

} // namespace

BooleanMesh buildBooleanBoxMeshAt(const glm::vec3& center, const glm::vec3& halfExtents,
                                  uint32_t materialId)
{
    const glm::vec3 h = glm::max(halfExtents, glm::vec3(1e-3f));
    const float p[8][3] = {
        {center.x - h.x, center.y - h.y, center.z - h.z},
        {center.x + h.x, center.y - h.y, center.z - h.z},
        {center.x + h.x, center.y + h.y, center.z - h.z},
        {center.x - h.x, center.y + h.y, center.z - h.z},
        {center.x - h.x, center.y - h.y, center.z + h.z},
        {center.x + h.x, center.y - h.y, center.z + h.z},
        {center.x + h.x, center.y + h.y, center.z + h.z},
        {center.x - h.x, center.y + h.y, center.z + h.z},
    };
    const uint32_t tri[12][3] = {
        {4, 5, 6}, {4, 6, 7}, {1, 0, 3}, {1, 3, 2}, {0, 4, 7}, {0, 7, 3},
        {5, 1, 2}, {5, 2, 6}, {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
    };

    BooleanMesh mesh;
    mesh.vertices.resize(8);
    for (int v = 0; v < 8; ++v)
    {
        const glm::vec3 position(p[v][0], p[v][1], p[v][2]);
        BooleanMeshVertex& out = mesh.vertices[v];
        out.position = position;
        out.normal = glm::normalize(position - center);
        out.uv = glm::vec2((position.x - center.x) / (2.0f * h.x) + 0.5f,
                           (position.y - center.y) / (2.0f * h.y) + 0.5f);
        out.materialId = materialId;
    }
    mesh.indices.reserve(36);
    for (int t = 0; t < 12; ++t)
        for (int c = 0; c < 3; ++c)
            mesh.indices.push_back(tri[t][c]);
    return mesh;
}

BooleanMesh buildBooleanBoxMesh(const glm::vec3& halfExtents, uint32_t materialId)
{
    const glm::vec3 h = glm::max(halfExtents, glm::vec3(1e-3f));
    const float p[8][3] = {
        {-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z}, {-h.x, h.y, -h.z},
        {-h.x, -h.y,  h.z}, {h.x, -h.y,  h.z}, {h.x, h.y,  h.z}, {-h.x, h.y,  h.z},
    };
    const uint32_t tri[12][3] = {
        {4, 5, 6}, {4, 6, 7}, {1, 0, 3}, {1, 3, 2}, {0, 4, 7}, {0, 7, 3},
        {5, 1, 2}, {5, 2, 6}, {0, 1, 5}, {0, 5, 4}, {3, 7, 6}, {3, 6, 2},
    };

    BooleanMesh mesh;
    mesh.vertices.resize(8);
    for (int v = 0; v < 8; ++v)
    {
        const glm::vec3 position(p[v][0], p[v][1], p[v][2]);
        BooleanMeshVertex& out = mesh.vertices[v];
        out.position = position;
        out.normal = glm::normalize(position);
        out.uv = glm::vec2(position.x / (2.0f * h.x) + 0.5f,
                           position.y / (2.0f * h.y) + 0.5f);
        out.materialId = materialId;
    }
    mesh.indices.reserve(36);
    for (int t = 0; t < 12; ++t)
        for (int c = 0; c < 3; ++c)
            mesh.indices.push_back(tri[t][c]);
    return mesh;
}

BooleanCutResult booleanSubtractAll(const BooleanMesh& base,
                                    const std::vector<BooleanCutter>& cutters)
{
    BooleanCutResult result;

    if (base.indices.size() < 3u || base.vertices.empty())
    {
        result.error = BooleanError::InvalidTarget;
        result.message = "base mesh is empty";
        return result;
    }

    manifold::MeshGL baseGL = toMeshGL(base);
    const uint32_t baseRunId = manifold::Manifold::ReserveIDs(1);
    baseGL.runOriginalID = {baseRunId};
    baseGL.runIndex = {0u};

    manifold::Manifold difference(baseGL);
    if (difference.Status() != manifold::Manifold::Error::NoError)
    {
        result.error = mapError(difference.Status());
        result.message = std::string("target import failed: ") + errorText(difference.Status());
        return result;
    }
    if (difference.IsEmpty())
    {
        result.error = BooleanError::InvalidTarget;
        result.message = "target mesh is empty";
        return result;
    }

    // Replay the authoritative cut history against the canonical base. Chaining
    // in-memory Manifold objects avoids re-importing a seamed output mesh, which
    // would require merge vectors and could drift.
    for (const BooleanCutter& cutter : cutters)
    {
        if (!(cutter.radius > 0.0f))
        {
            result.error = BooleanError::InvalidCutter;
            result.message = "cutter radius must be positive";
            return result;
        }

        manifold::Manifold cutterManifold = (cutter.type == BooleanCutterType::Capsule)
            ? buildCapsule(cutter)
            : manifold::Manifold::Sphere((double)cutter.radius,
                                         circularSegmentsForRadius(cutter.radius))
                  .Translate(toVec3(cutter.localCenter));

        if (cutterManifold.Status() != manifold::Manifold::Error::NoError ||
            cutterManifold.IsEmpty())
        {
            result.error = BooleanError::InvalidCutter;
            result.message = std::string("cutter construction failed: ") +
                             errorText(cutterManifold.Status());
            return result;
        }

        difference = difference - cutterManifold;
        if (difference.Status() != manifold::Manifold::Error::NoError)
        {
            result.error = mapError(difference.Status());
            result.message = std::string("boolean failed: ") + errorText(difference.Status());
            return result;
        }
        if (difference.IsEmpty())
        {
            result.error = BooleanError::EmptyResult;
            result.message = "boolean produced an empty solid";
            return result;
        }
    }

    fillResult(difference, baseRunId, base, result);
    return result;
}

BooleanCutResult booleanSubtractIncremental(uint64_t sessionId,
                                            const BooleanMesh& base,
                                            const std::vector<BooleanCutter>& cutters)
{
    BooleanCutResult result;
    if (sessionId == 0)
        return booleanSubtractAll(base, cutters);
    if (base.indices.size() < 3u || base.vertices.empty())
    {
        result.error = BooleanError::InvalidTarget;
        result.message = "base mesh is empty";
        return result;
    }

    std::lock_guard<std::mutex> lock(gSessionMutex);
    BooleanSession& session = gSessions[sessionId];
    session.lastUse = ++gSessionClock;
    if (session.baseRunId == 0)
        session.baseRunId = manifold::Manifold::ReserveIDs(1);

    // A shrunk history means a cut rolled back or the record was rebuilt from
    // bulk changes: drop the running result and replay from the canonical base.
    if (session.appliedCuts > cutters.size())
    {
        session.running = manifold::Manifold();
        session.appliedCuts = 0;
    }

    if (session.appliedCuts == 0)
    {
        manifold::MeshGL baseGL = toMeshGL(base);
        baseGL.runOriginalID = {session.baseRunId};
        baseGL.runIndex = {0u};
        manifold::Manifold imported(baseGL);
        if (imported.Status() != manifold::Manifold::Error::NoError)
        {
            result.error = mapError(imported.Status());
            result.message = std::string("target import failed: ") +
                             errorText(imported.Status());
            return result;
        }
        if (imported.IsEmpty())
        {
            result.error = BooleanError::InvalidTarget;
            result.message = "target mesh is empty";
            return result;
        }
        session.running = std::move(imported);
    }

    for (size_t i = session.appliedCuts; i < cutters.size(); ++i)
    {
        const BooleanCutter& cutter = cutters[i];
        if (!(cutter.radius > 0.0f))
        {
            result.error = BooleanError::InvalidCutter;
            result.message = "cutter radius must be positive";
            return result;
        }

        manifold::Manifold cutterManifold = (cutter.type == BooleanCutterType::Capsule)
            ? buildCapsule(cutter)
            : manifold::Manifold::Sphere((double)cutter.radius,
                                         circularSegmentsForRadius(cutter.radius))
                  .Translate(toVec3(cutter.localCenter));

        if (cutterManifold.Status() != manifold::Manifold::Error::NoError ||
            cutterManifold.IsEmpty())
        {
            result.error = BooleanError::InvalidCutter;
            result.message = std::string("cutter construction failed: ") +
                             errorText(cutterManifold.Status());
            return result;
        }

        // Subtract into a temporary and only commit on success, so a failed cut
        // leaves the cached running result at the last good state.
        manifold::Manifold next = session.running - cutterManifold;
        if (next.Status() != manifold::Manifold::Error::NoError)
        {
            result.error = mapError(next.Status());
            result.message = std::string("boolean failed: ") + errorText(next.Status());
            return result;
        }
        if (next.IsEmpty())
        {
            result.error = BooleanError::EmptyResult;
            result.message = "boolean produced an empty solid";
            return result;
        }
        session.running = std::move(next);
        session.appliedCuts = i + 1;
    }

    fillResult(session.running, session.baseRunId, base, result);

    if (gSessions.size() > kMaxSessions)
    {
        auto oldest = gSessions.begin();
        for (auto it = gSessions.begin(); it != gSessions.end(); ++it)
            if (it->second.lastUse < oldest->second.lastUse)
                oldest = it;
        gSessions.erase(oldest);
    }
    return result;
}

void booleanSessionRelease(uint64_t sessionId)
{
    if (sessionId == 0)
        return;
    std::lock_guard<std::mutex> lock(gSessionMutex);
    gSessions.erase(sessionId);
}

BooleanCutResult booleanSubtract(const BooleanMesh& base, const BooleanCutter& cutter)
{
    return booleanSubtractAll(base, std::vector<BooleanCutter>{cutter});
}

BooleanError booleanValidate(const BooleanMesh& mesh, std::string* reason)
{
    if (mesh.indices.size() < 3u || mesh.vertices.empty())
    {
        if (reason) *reason = "mesh is empty";
        return BooleanError::InvalidTarget;
    }
    manifold::MeshGL gl = toMeshGL(mesh);
    gl.runOriginalID = {manifold::Manifold::ReserveIDs(1)};
    gl.runIndex = {0u};
    manifold::Manifold m(gl);
    if (m.Status() != manifold::Manifold::Error::NoError)
    {
        if (reason) *reason = errorText(m.Status());
        return mapError(m.Status());
    }
    return BooleanError::None;
}

BooleanMesh booleanUnion(const std::vector<BooleanMesh>& meshes, uint32_t materialId)
{
    BooleanMesh result;
    if (meshes.empty())
        return result;

    manifold::Manifold accumulated;
    bool haveAny = false;
    for (const BooleanMesh& mesh : meshes)
    {
        if (mesh.indices.size() < 3u || mesh.vertices.empty())
            continue;
        manifold::MeshGL gl = toMeshGL(mesh);
        gl.runOriginalID = {manifold::Manifold::ReserveIDs(1)};
        gl.runIndex = {0u};
        manifold::Manifold imported(gl);
        if (imported.Status() != manifold::Manifold::Error::NoError ||
            imported.IsEmpty())
            return result;
        accumulated = haveAny ? (accumulated + imported) : imported;
        if (accumulated.Status() != manifold::Manifold::Error::NoError)
            return result;
        haveAny = true;
    }
    if (!haveAny || accumulated.IsEmpty())
        return result;

    const glm::vec3 half = meshHalfExtent(meshes[0]);
    result = fromMeshGL(accumulated.GetMeshGL(), 0u, materialId, half);
    if (result.indices.empty())
        result.vertices.clear();
    return result;
}

std::vector<BooleanPiece> booleanDecomposePieces(const BooleanMesh& base,
                                                 const std::vector<BooleanCutter>& cutters)
{
    std::vector<BooleanPiece> pieces;
    if (base.indices.size() < 3u || base.vertices.empty())
        return pieces;

    // Reconstruct the same accumulated solid the rebuild uses.
    manifold::MeshGL baseGL = toMeshGL(base);
    const uint32_t baseRunId = manifold::Manifold::ReserveIDs(1);
    baseGL.runOriginalID = {baseRunId};
    baseGL.runIndex = {0u};
    manifold::Manifold difference(baseGL);
    if (difference.Status() != manifold::Manifold::Error::NoError ||
        difference.IsEmpty())
        return pieces;

    for (const BooleanCutter& cutter : cutters)
    {
        if (!(cutter.radius > 0.0f))
            return {};
        manifold::Manifold cutterManifold = (cutter.type == BooleanCutterType::Capsule)
            ? buildCapsule(cutter)
            : manifold::Manifold::Sphere((double)cutter.radius,
                                         circularSegmentsForRadius(cutter.radius))
                  .Translate(toVec3(cutter.localCenter));
        if (cutterManifold.Status() != manifold::Manifold::Error::NoError ||
            cutterManifold.IsEmpty())
            return {};
        difference = difference - cutterManifold;
        if (difference.Status() != manifold::Manifold::Error::NoError ||
            difference.IsEmpty())
            return {};
    }

    const std::vector<manifold::Manifold> shells = difference.Decompose();
    const glm::vec3 half = meshHalfExtent(base);
    for (const manifold::Manifold& shell : shells)
    {
        const double volume = shell.Volume();
        if (!(volume > 1e-6))
            continue; // interior cavity / negative or empty shell: not matter.
        BooleanPiece piece;
        piece.mesh = fromMeshGL(shell.GetMeshGL(), baseRunId,
                                base.vertices[0].materialId, half);
        piece.volume = (float)volume;
        if (!piece.mesh.empty())
            piece.centroid = computeCentroid(piece.mesh);
        pieces.push_back(std::move(piece));
    }

    std::sort(pieces.begin(), pieces.end(),
              [](const BooleanPiece& a, const BooleanPiece& b) {
                  return a.volume > b.volume;
              });
    return pieces;
}

} // namespace MimitaImpact
