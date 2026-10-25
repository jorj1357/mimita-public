#include "map/map-static-union.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "physics/physical-entity.h"

namespace {

constexpr float kNormalQuantum = 2048.0f;
constexpr float kOffsetQuantum = 64.0f;
constexpr float kPlaneTolerance = 0.004f;
constexpr float kOppositeNormalDot = -0.995f;
constexpr float kFullCoverageFraction = 0.995f;
constexpr int kBarycentricSamples = 8;
constexpr float kAreaEpsilon = 1.0e-8f;
constexpr float kPointEpsilon = 1.0e-6f;

struct PlaneKey
{
    int32_t nx = 0;
    int32_t ny = 0;
    int32_t nz = 0;
    int32_t d = 0;

    bool operator==(const PlaneKey& other) const
    {
        return nx == other.nx && ny == other.ny && nz == other.nz && d == other.d;
    }
};

struct PlaneKeyHash
{
    size_t operator()(const PlaneKey& key) const
    {
        size_t h = static_cast<size_t>(static_cast<uint32_t>(key.nx)) * 0x9E3779B97F4A7C15ull;
        h ^= static_cast<size_t>(static_cast<uint32_t>(key.ny)) * 0xC2B2AE3D27D4EB4Full;
        h ^= static_cast<size_t>(static_cast<uint32_t>(key.nz)) * 0x165667B19E3779F9ull;
        h ^= static_cast<size_t>(static_cast<uint32_t>(key.d)) * 0x27D4EB2F165667C5ull;
        return h ^ (h >> 32);
    }
};

struct IVec3KeyHash
{
    size_t operator()(const glm::ivec3& value) const
    {
        size_t h = static_cast<size_t>(static_cast<uint32_t>(value.x)) * 0x9E3779B97F4A7C15ull;
        h ^= static_cast<size_t>(static_cast<uint32_t>(value.y)) * 0xC2B2AE3D27D4EB4Full;
        h ^= static_cast<size_t>(static_cast<uint32_t>(value.z)) * 0x165667B19E3779F9ull;
        return h ^ (h >> 32);
    }
};

struct PlaneGroup
{
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 u{1.0f, 0.0f, 0.0f};
    glm::vec3 v{0.0f, 1.0f, 0.0f};
    glm::vec3 origin{0.0f};
    float d = 0.0f;
    std::vector<uint32_t> tris;
};

struct CandidatePolygon
{
    uint32_t index = 0;
    std::array<glm::vec2, 3> points{};
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
};

int32_t quantize(float value, float scale)
{
    if (std::fabs(value) < 1.0e-7f)
        value = 0.0f;
    return static_cast<int32_t>(std::lround(static_cast<double>(value) * scale));
}

PlaneKey makePlaneKey(const glm::vec3& normal, float offset)
{
    PlaneKey key;
    key.nx = quantize(normal.x, kNormalQuantum);
    key.ny = quantize(normal.y, kNormalQuantum);
    key.nz = quantize(normal.z, kNormalQuantum);
    key.d = quantize(offset, kOffsetQuantum);
    return key;
}

PlaneKey makeOppositePlaneKey(const glm::vec3& normal, float offset)
{
    return makePlaneKey(-normal, -offset);
}

void makePlaneBasis(const glm::vec3& normal, glm::vec3& u, glm::vec3& v)
{
    const glm::vec3 helper =
        std::fabs(normal.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                   : glm::vec3(0.0f, 1.0f, 0.0f);
    u = glm::normalize(glm::cross(helper, normal));
    v = glm::cross(normal, u);
}

glm::vec2 projectPoint(const PlaneGroup& group, const glm::vec3& point)
{
    const glm::vec3 delta = point - group.origin;
    return glm::vec2(glm::dot(delta, group.u), glm::dot(delta, group.v));
}

float cross2(const glm::vec2& a, const glm::vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

float polygonArea(const std::vector<glm::vec2>& polygon)
{
    if (polygon.size() < 3)
        return 0.0f;
    float twice = 0.0f;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        const glm::vec2& a = polygon[i];
        const glm::vec2& b = polygon[(i + 1) % polygon.size()];
        twice += cross2(a, b);
    }
    return std::fabs(twice) * 0.5f;
}

float signedTriangleArea2(const std::array<glm::vec2, 3>& triangle)
{
    return cross2(triangle[1] - triangle[0], triangle[2] - triangle[0]);
}

glm::vec2 intersectSegments(const glm::vec2& p,
                            const glm::vec2& q,
                            const glm::vec2& a,
                            const glm::vec2& b)
{
    const glm::vec2 r = q - p;
    const glm::vec2 s = b - a;
    const float denom = cross2(r, s);
    if (std::fabs(denom) < 1.0e-12f)
        return q;
    const float t = cross2(a - p, s) / denom;
    return p + r * t;
}

bool clipPolygonToTriangle(const std::vector<glm::vec2>& subject,
                           std::array<glm::vec2, 3> clipper,
                           std::vector<glm::vec2>& output)
{
    if (signedTriangleArea2(clipper) < 0.0f)
        std::swap(clipper[1], clipper[2]);

    output = subject;
    for (int edge = 0; edge < 3 && !output.empty(); ++edge)
    {
        const glm::vec2 a = clipper[edge];
        const glm::vec2 b = clipper[(edge + 1) % 3];
        const std::vector<glm::vec2> input = output;
        output.clear();
        glm::vec2 previous = input.back();
        bool previousInside = cross2(b - a, previous - a) >= -kPointEpsilon;
        for (const glm::vec2& current : input)
        {
            const bool currentInside = cross2(b - a, current - a) >= -kPointEpsilon;
            if (currentInside)
            {
                if (!previousInside)
                    output.push_back(intersectSegments(previous, current, a, b));
                output.push_back(current);
            }
            else if (previousInside)
            {
                output.push_back(intersectSegments(previous, current, a, b));
            }
            previous = current;
            previousInside = currentInside;
        }
    }
    return output.size() >= 3;
}

bool pointInTriangle2D(const glm::vec2& point,
                       const std::array<glm::vec2, 3>& triangle)
{
    const float d1 = cross2(triangle[1] - triangle[0], point - triangle[0]);
    const float d2 = cross2(triangle[2] - triangle[1], point - triangle[1]);
    const float d3 = cross2(triangle[0] - triangle[2], point - triangle[2]);
    const bool hasNegative =
        d1 < -kPointEpsilon || d2 < -kPointEpsilon || d3 < -kPointEpsilon;
    const bool hasPositive =
        d1 > kPointEpsilon || d2 > kPointEpsilon || d3 > kPointEpsilon;
    return !(hasNegative && hasPositive);
}

std::array<glm::vec2, 3> projectTriangle(const PlaneGroup& group,
                                         const CollisionTriangle& triangle)
{
    return {projectPoint(group, triangle.a),
            projectPoint(group, triangle.b),
            projectPoint(group, triangle.c)};
}

std::array<glm::vec2, 2> polygonBounds(const std::array<glm::vec2, 3>& points)
{
    glm::vec2 min = points[0];
    glm::vec2 max = points[0];
    for (int i = 1; i < 3; ++i)
    {
        min = glm::min(min, points[i]);
        max = glm::max(max, points[i]);
    }
    return {min, max};
}

bool boundsOverlap(const std::array<glm::vec2, 2>& a,
                   const std::array<glm::vec2, 2>& b)
{
    return a[0].x <= b[1].x + kPointEpsilon &&
           b[0].x <= a[1].x + kPointEpsilon &&
           a[0].y <= b[1].y + kPointEpsilon &&
           b[0].y <= a[1].y + kPointEpsilon;
}

glm::vec3 normalizedNormal(const CollisionTriangle& triangle)
{
    const float length = glm::length(triangle.normal);
    if (length < 1.0e-6f)
        return glm::vec3(0.0f);
    return triangle.normal / length;
}

void logStats(const StaticUnionStats& stats)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map.collision.static-face-removal", "static-collision-removal",
        stats.fallback ? "fallback" : "completed", 0,
        nlohmann::json{
            {"input_triangles", stats.inputTriangles},
            {"output_triangles", stats.outputTriangles},
            {"plane_groups", stats.planeGroups},
            {"solid_groups", stats.solidGroups},
            {"candidate_pairs", stats.candidatePairs},
            {"removed_triangles", stats.removedTriangles},
            {"elapsed_ms", stats.elapsedMs},
            {"fallback", stats.fallback},
            {"fallback_reason", stats.fallbackReason}},
        __FILE__, __LINE__, __FUNCTION__);
}

void appendTestBox(std::vector<CollisionTriangle>& triangles,
                   std::vector<uint32_t>& solidIds,
                   const glm::vec3& center,
                   const glm::vec3& half,
                   uint32_t solidId)
{
    const size_t before = triangles.size();
    buildBoxCollisionTriangles(triangles, center, half);
    solidIds.resize(triangles.size(), solidId);
    for (size_t i = before; i < triangles.size(); ++i)
        solidIds[i] = solidId;
}

} // namespace

bool isClosedSolidTriangleRange(const std::vector<CollisionTriangle>& triangles,
                                size_t first,
                                size_t last)
{
    first = std::min(first, triangles.size());
    last = std::min(last, triangles.size());
    if (last <= first)
        return false;

    std::unordered_map<glm::ivec3, uint32_t, IVec3KeyHash> vertexIds;
    vertexIds.reserve((last - first) / 2);
    std::unordered_map<uint64_t, uint32_t> undirectedEdges;
    std::unordered_map<uint64_t, uint32_t> directedEdges;
    double volume = 0.0;
    uint32_t triangleCount = 0;

    const auto vertexId = [&](const glm::vec3& p) {
        const glm::ivec3 key(
            (int)std::lround(p.x * 4096.0f),
            (int)std::lround(p.y * 4096.0f),
            (int)std::lround(p.z * 4096.0f));
        auto found = vertexIds.find(key);
        if (found != vertexIds.end())
            return found->second;
        const uint32_t id = static_cast<uint32_t>(vertexIds.size());
        vertexIds.emplace(key, id);
        return id;
    };

    for (size_t i = first; i < last; ++i)
    {
        const glm::vec3& a = triangles[i].a;
        const glm::vec3& b = triangles[i].b;
        const glm::vec3& c = triangles[i].c;
        const uint32_t ia = vertexId(a);
        const uint32_t ib = vertexId(b);
        const uint32_t ic = vertexId(c);
        if (ia == ib || ib == ic || ia == ic)
            return false;

        const uint32_t ids[3] = {ia, ib, ic};
        for (int edge = 0; edge < 3; ++edge)
        {
            const uint32_t from = ids[edge];
            const uint32_t to = ids[(edge + 1) % 3];
            const uint64_t directed = (static_cast<uint64_t>(from) << 32) | to;
            ++directedEdges[directed];
            const uint32_t lo = std::min(from, to);
            const uint32_t hi = std::max(from, to);
            ++undirectedEdges[(static_cast<uint64_t>(lo) << 32) | hi];
        }

        volume += static_cast<double>(
            glm::dot(a, glm::cross(b, c))) / 6.0;
        ++triangleCount;
    }

    if (triangleCount < 4 || std::fabs(volume) <= 1.0e-6)
        return false;

    for (const auto& entry : undirectedEdges)
    {
        if (entry.second != 2)
            return false;
        const uint32_t lo = static_cast<uint32_t>(entry.first >> 32);
        const uint32_t hi = static_cast<uint32_t>(entry.first & 0xFFFFFFFFu);
        const uint64_t forward = (static_cast<uint64_t>(lo) << 32) | hi;
        const uint64_t reverse = (static_cast<uint64_t>(hi) << 32) | lo;
        auto f = directedEdges.find(forward);
        auto r = directedEdges.find(reverse);
        if (f == directedEdges.end() || r == directedEdges.end() ||
            f->second != 1 || r->second != 1)
            return false;
    }

    return true;
}

bool removeFullyCoveredStaticTriangles(
    std::vector<CollisionTriangle>& triangles,
    std::vector<uint32_t>& solidIds,
    StaticUnionStats* outStats)
{
    const auto started = std::chrono::steady_clock::now();
    StaticUnionStats stats;
    stats.inputTriangles = static_cast<uint32_t>(triangles.size());

    if (triangles.empty())
    {
        stats.outputTriangles = 0;
        stats.elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (outStats)
            *outStats = stats;
        return true;
    }

    if (solidIds.size() != triangles.size())
    {
        stats.fallback = true;
        stats.fallbackReason = "solid_ids_size_mismatch";
        stats.outputTriangles = static_cast<uint32_t>(triangles.size());
        stats.elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (outStats)
            *outStats = stats;
        logStats(stats);
        return false;
    }

    std::vector<PlaneGroup> groups;
    groups.reserve(triangles.size() / 4);
    std::unordered_map<PlaneKey, uint32_t, PlaneKeyHash> groupByPlane;
    groupByPlane.reserve(triangles.size());
    std::unordered_set<uint32_t> solidGroupSet;

    for (uint32_t i = 0; i < triangles.size(); ++i)
    {
        const glm::vec3 normal = normalizedNormal(triangles[i]);
        if (glm::length(normal) < 0.5f)
            continue;

        const float offset = glm::dot(normal, triangles[i].a);
        const PlaneKey key = makePlaneKey(normal, offset);
        auto found = groupByPlane.find(key);
        uint32_t groupIndex = 0;
        if (found == groupByPlane.end())
        {
            groupIndex = static_cast<uint32_t>(groups.size());
            PlaneGroup group;
            group.normal = normal;
            group.origin = triangles[i].a;
            group.d = offset;
            makePlaneBasis(group.normal, group.u, group.v);
            groups.push_back(std::move(group));
            groupByPlane.emplace(key, groupIndex);
        }
        else
        {
            groupIndex = found->second;
        }
        groups[groupIndex].tris.push_back(i);

        if (solidIds[i] != 0)
            solidGroupSet.insert(solidIds[i]);
    }

    stats.planeGroups = static_cast<uint32_t>(groups.size());
    stats.solidGroups = static_cast<uint32_t>(solidGroupSet.size());

    std::vector<bool> removed(triangles.size(), false);
    std::vector<glm::vec2> intersection;
    std::vector<CandidatePolygon> candidates;

    for (const PlaneGroup& group : groups)
    {
        const PlaneKey oppositeKey = makeOppositePlaneKey(group.normal, group.d);
        auto oppositeFound = groupByPlane.find(oppositeKey);
        if (oppositeFound == groupByPlane.end())
            continue;
        const PlaneGroup& opposite = groups[oppositeFound->second];
        if (opposite.tris.empty())
            continue;

        for (uint32_t i : group.tris)
        {
            if (removed[i])
                continue;
            const uint32_t solidI = solidIds[i];
            if (solidI == 0)
                continue;

            const CollisionTriangle& triangleI = triangles[i];
            if (std::fabs(glm::dot(group.normal, triangleI.a - group.origin)) >
                kPlaneTolerance)
                continue;

            const std::array<glm::vec2, 3> polygonI = projectTriangle(group, triangleI);
            const float areaI = std::fabs(signedTriangleArea2(polygonI)) * 0.5f;
            if (areaI <= kAreaEpsilon)
                continue;
            const std::array<glm::vec2, 2> boundsI = polygonBounds(polygonI);

            candidates.clear();
            float overlapArea = 0.0f;

            for (uint32_t j : opposite.tris)
            {
                if (j == i)
                    continue;
                const uint32_t solidJ = solidIds[j];
                if (solidJ == 0 || solidJ == solidI)
                    continue;

                const CollisionTriangle& triangleJ = triangles[j];
                const glm::vec3 normalJ = normalizedNormal(triangleJ);
                if (glm::length(normalJ) < 0.5f ||
                    glm::dot(group.normal, normalJ) > kOppositeNormalDot)
                    continue;
                if (std::fabs(glm::dot(group.normal, triangleJ.a - group.origin)) >
                    kPlaneTolerance)
                    continue;

                const std::array<glm::vec2, 3> polygonJ = projectTriangle(group, triangleJ);
                const std::array<glm::vec2, 2> boundsJ = polygonBounds(polygonJ);
                ++stats.candidatePairs;
                if (!boundsOverlap(boundsI, boundsJ))
                    continue;

                if (!clipPolygonToTriangle({polygonI[0], polygonI[1], polygonI[2]},
                                           polygonJ, intersection))
                    continue;
                const float overlap = polygonArea(intersection);
                if (overlap <= kAreaEpsilon)
                    continue;

                overlapArea += overlap;
                CandidatePolygon candidate;
                candidate.index = j;
                candidate.points = polygonJ;
                candidate.min = boundsJ[0];
                candidate.max = boundsJ[1];
                candidates.push_back(candidate);
            }

            if (std::getenv("MIMITA_STATIC_UNION_DEBUG"))
                std::fprintf(stderr,
                    "[SFR] i=%u solid=%u normal=(%.2f %.2f %.2f) d=%.2f area=%.4f overlap=%.4f candidates=%zu\n",
                    i, solidI, group.normal.x, group.normal.y, group.normal.z,
                    group.d, areaI, overlapArea, candidates.size());
            if (candidates.empty() || overlapArea < areaI * kFullCoverageFraction)
                continue;

            bool fullyCovered = true;
            for (int a = 0; a <= kBarycentricSamples && fullyCovered; ++a)
            {
                for (int b = 0; b <= kBarycentricSamples - a; ++b)
                {
                    const float wa =
                        static_cast<float>(a) / static_cast<float>(kBarycentricSamples);
                    const float wb =
                        static_cast<float>(b) / static_cast<float>(kBarycentricSamples);
                    const float wc = 1.0f - wa - wb;
                    const glm::vec2 sample =
                        polygonI[0] * wa + polygonI[1] * wb + polygonI[2] * wc;
                    bool inside = false;
                    for (const CandidatePolygon& candidate : candidates)
                    {
                        if (sample.x < candidate.min.x - kPointEpsilon ||
                            sample.x > candidate.max.x + kPointEpsilon ||
                            sample.y < candidate.min.y - kPointEpsilon ||
                            sample.y > candidate.max.y + kPointEpsilon)
                            continue;
                        if (pointInTriangle2D(sample, candidate.points))
                        {
                            inside = true;
                            break;
                        }
                    }
                    if (!inside)
                    {
                        fullyCovered = false;
                        break;
                    }
                }
            }

            if (fullyCovered)
                removed[i] = true;
        }
    }

    size_t kept = 0;
    for (size_t i = 0; i < triangles.size(); ++i)
    {
        if (removed[i])
        {
            ++stats.removedTriangles;
            continue;
        }
        if (kept != i)
        {
            triangles[kept] = triangles[i];
            solidIds[kept] = solidIds[i];
        }
        ++kept;
    }
    triangles.resize(kept);
    solidIds.resize(kept);

    stats.outputTriangles = static_cast<uint32_t>(kept);
    stats.elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    if (outStats)
        *outStats = stats;
    logStats(stats);
    return true;
}

bool staticFaceRemovalSelfTest(std::string& report)
{
    bool ok = true;
    auto check = [&](bool condition, const char* label) {
        if (!condition)
        {
            ok = false;
            report += "FAIL: ";
            report += label;
            report += "\n";
        }
    };

    auto run = [&](const char* label,
                   std::vector<CollisionTriangle> triangles,
                   std::vector<uint32_t> solidIds,
                   uint32_t expected) {
        StaticUnionStats stats;
        const bool applied = removeFullyCoveredStaticTriangles(triangles, solidIds, &stats);
        check(applied, "removal pass applied");
        if (triangles.size() != expected)
        {
            report += "FAIL: ";
            report += label;
            report += " expected=";
            report += std::to_string(expected);
            report += " actual=";
            report += std::to_string(triangles.size());
            report += "\n";
            ok = false;
        }
        check(solidIds.size() == triangles.size(), "solid ids stayed aligned");
        check(!stats.fallback, "removal did not fall back");
        report += label;
        report += " input=" + std::to_string(stats.inputTriangles);
        report += " output=" + std::to_string(stats.outputTriangles);
        report += " planes=" + std::to_string(stats.planeGroups);
        report += " solids=" + std::to_string(stats.solidGroups);
        report += " pairs=" + std::to_string(stats.candidatePairs);
        report += " removed=" + std::to_string(stats.removedTriangles);
        report += "\n";
    };

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f), 1);
        appendTestBox(triangles, ids, glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.5f), 2);
        run("touch", std::move(triangles), std::move(ids), 20);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(0.0f), glm::vec3(0.5f), 1);
        appendTestBox(triangles, ids, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.5f), 2);
        run("stack", std::move(triangles), std::move(ids), 20);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(-0.6f), glm::vec3(0.5f), 1);
        appendTestBox(triangles, ids, glm::vec3(0.6f), glm::vec3(0.5f), 2);
        run("gap", std::move(triangles), std::move(ids), 24);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(0.0f), glm::vec3(1.0f), 1);
        appendTestBox(triangles, ids, glm::vec3(1.5f, 0.0f, 0.0f),
                      glm::vec3(0.5f), 2);
        run("partial", std::move(triangles), std::move(ids), 22);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(0.0f), glm::vec3(1.0f), 1);
        appendTestBox(triangles, ids, glm::vec3(1.0f), glm::vec3(1.0f), 2);
        run("overlap", std::move(triangles), std::move(ids), 24);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(0.0f), glm::vec3(0.5f), 1);
        run("single", std::move(triangles), std::move(ids), 12);
    }

    {
        std::vector<CollisionTriangle> triangles;
        std::vector<uint32_t> ids;
        appendTestBox(triangles, ids, glm::vec3(-0.5f), glm::vec3(0.5f), 0);
        appendTestBox(triangles, ids, glm::vec3(0.5f), glm::vec3(0.5f), 0);
        run("unknown", std::move(triangles), std::move(ids), 24);
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
