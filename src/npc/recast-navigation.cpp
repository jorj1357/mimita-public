// 2026-10-06
// Initial Recast/Detour integration proof. This is deliberately query-only:
// MiMITA still owns goals, movement intent, physics, collision, and traversal.
//
// Coordinate note: MiMITA is Z-up (gravity acts on .z, floors have normal
// (0,0,1)). Recast assumes Y-up and marks a triangle walkable when norm[1]
// exceeds cos(slope). Feeding raw Z-up geometry makes every floor look like a
// vertical wall, leaving no walkable surface and producing path_not_found. The
// adapter therefore converts Z-up -> Y-up on input with the handedness-
// preserving rotation (x,y,z) -> (x, z, -y) and converts query results back
// with (x,y,z) -> (x, -z, y). Recorded in RecastNavigationResult.
#include "npc/recast-navigation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>

#include "Recast.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "perf/perf.h"

namespace {

constexpr const char* kCoordinateSystem = "mimita_z_up_to_recast_y_up";

glm::vec3 toRecast(const glm::vec3& v)
{
    return glm::vec3(v.x, v.z, -v.y);
}

glm::vec3 fromRecast(const glm::vec3& v)
{
    return glm::vec3(v.x, -v.z, v.y);
}

class SilentBuildContext final : public rcContext
{
public:
    SilentBuildContext() : rcContext(false) {}
};

struct BuildData
{
    rcHeightfield* heightfield = nullptr;
    rcCompactHeightfield* compact = nullptr;
    rcContourSet* contours = nullptr;
    rcPolyMesh* polyMesh = nullptr;
    rcPolyMeshDetail* detail = nullptr;
    int walkableTriangleCount = 0;

    ~BuildData()
    {
        if (detail) rcFreePolyMeshDetail(detail);
        if (polyMesh) rcFreePolyMesh(polyMesh);
        if (contours) rcFreeContourSet(contours);
        if (compact) rcFreeCompactHeightfield(compact);
        if (heightfield) rcFreeHeightField(heightfield);
    }
};

float horizontalLength(const glm::vec3& a, const glm::vec3& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
                     (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

// findNearestPoly with a bounded ascending search box. Tactical destinations
// are often a few units above the floor or slightly past the navmesh edge, and
// the default agent-sized box is too tight for them. The escalation is finite
// and the final projection distance is reported so a bad goal is visible
// rather than silently accepted.
dtPolyRef nearestPolyEscalated(dtNavMeshQuery* query, const dtQueryFilter* filter,
                               const float* point,
                               const NavigationAgentProfile& profile,
                               float* nearestOut, float* projectionDistance)
{
    const float baseH = std::max(profile.radius * 4.0f, 1.5f);
    const float baseV = std::max(profile.height, 1.5f);
    const float attempts[3][3] = {
        {baseH, baseV, baseH},
        {baseH * 3.0f, baseV * 3.0f, baseH * 3.0f},
        {40.0f, 40.0f, 40.0f},
    };
    for (const auto& extents : attempts) {
        float nearest[3]{};
        dtPolyRef ref = 0;
        const dtStatus status =
            query->findNearestPoly(point, extents, filter, &ref, nearest);
        if (dtStatusSucceed(status) && ref != 0) {
            if (nearestOut) {
                nearestOut[0] = nearest[0];
                nearestOut[1] = nearest[1];
                nearestOut[2] = nearest[2];
            }
            if (projectionDistance) {
                const float dx = nearest[0] - point[0];
                const float dy = nearest[1] - point[1];
                const float dz = nearest[2] - point[2];
                *projectionDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            return ref;
        }
    }
    return 0;
}

bool buildNavMesh(const World& world, const NavigationAgentProfile& profile,
                  dtNavMesh*& outMesh, RecastNavigationResult& diag,
                  std::string& error)
{
    if (world.collisionMesh.triangles.empty()) {
        error = "collision_mesh_empty";
        return false;
    }

    const float cellSize = 0.25f;
    const float cellHeight = 0.20f;
    const float agentHeight = std::max(0.2f, profile.height);
    const float agentRadius = std::max(0.05f, profile.radius);
    const float stepHeight = std::max(0.01f, profile.stepHeight);

    glm::vec3 bmin = toRecast(world.collisionMesh.boundsMin);
    glm::vec3 bmax = toRecast(world.collisionMesh.boundsMax);
    // Recompute from converted vertices when the cached bounds are absent, so
    // the conversion is always applied exactly once to authoritative data.
    glm::vec3 vmin(1.0e30f), vmax(-1.0e30f);
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        for (const glm::vec3& raw : {tri.a, tri.b, tri.c}) {
            const glm::vec3 v = toRecast(raw);
            vmin = glm::min(vmin, v);
            vmax = glm::max(vmax, v);
        }
    }
    if (!(bmin.x < bmax.x && bmin.y < bmax.y && bmin.z < bmax.z))
        bmin = vmin, bmax = vmax;
    if (!(bmin.x < bmax.x && bmin.y < bmax.y && bmin.z < bmax.z)) {
        error = "collision_bounds_invalid";
        return false;
    }

    std::vector<float> verts;
    std::vector<int> tris;
    const auto& source = world.collisionMesh.triangles;
    verts.reserve(source.size() * 9);
    tris.reserve(source.size() * 3);
    for (const CollisionTriangle& tri : source) {
        const int base = static_cast<int>(verts.size() / 3);
        for (const glm::vec3& raw : {tri.a, tri.b, tri.c}) {
            const glm::vec3 v = toRecast(raw);
            verts.push_back(v.x);
            verts.push_back(v.y);
            verts.push_back(v.z);
        }
        tris.push_back(base);
        tris.push_back(base + 1);
        tris.push_back(base + 2);
    }

    rcConfig cfg{};
    cfg.cs = cellSize;
    cfg.ch = cellHeight;
    cfg.walkableSlopeAngle = std::clamp(profile.maxSlopeDegrees, 1.0f, 89.0f);
    cfg.walkableHeight = static_cast<int>(std::ceil(agentHeight / cellHeight));
    cfg.walkableClimb = static_cast<int>(std::floor(stepHeight / cellHeight));
    cfg.walkableRadius = static_cast<int>(std::ceil(agentRadius / cellSize));
    cfg.maxEdgeLen = static_cast<int>(12.0f / cellSize);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = 8;
    cfg.mergeRegionArea = 20;
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = cellSize * 6.0f;
    cfg.detailSampleMaxError = cellHeight * 1.0f;
    rcVcopy(cfg.bmin, &bmin.x);
    rcVcopy(cfg.bmax, &bmax.x);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    if (cfg.width <= 0 || cfg.height <= 0 || cfg.width > 8192 || cfg.height > 8192) {
        error = "recast_grid_out_of_bounds";
        return false;
    }

    SilentBuildContext context;
    BuildData data;
    data.heightfield = rcAllocHeightfield();
    if (!data.heightfield || !rcCreateHeightfield(&context, *data.heightfield,
                                                   cfg.width, cfg.height,
                                                   cfg.bmin, cfg.bmax,
                                                   cfg.cs, cfg.ch)) {
        error = "heightfield_create_failed";
        return false;
    }

    std::vector<unsigned char> areas(source.size(), 0);
    rcMarkWalkableTriangles(&context, cfg.walkableSlopeAngle,
                            verts.data(), static_cast<int>(verts.size() / 3),
                            tris.data(), static_cast<int>(source.size()),
                            areas.data());
    for (unsigned char a : areas)
        if (a != 0) ++data.walkableTriangleCount;
    if (!rcRasterizeTriangles(&context, verts.data(),
                              static_cast<int>(verts.size() / 3), tris.data(),
                              areas.data(), static_cast<int>(source.size()),
                              *data.heightfield, cfg.walkableClimb)) {
        error = "rasterization_failed";
        return false;
    }
    rcFilterLowHangingWalkableObstacles(&context, cfg.walkableClimb,
                                        *data.heightfield);
    rcFilterLedgeSpans(&context, cfg.walkableHeight, cfg.walkableClimb,
                       *data.heightfield);
    rcFilterWalkableLowHeightSpans(&context, cfg.walkableHeight,
                                   *data.heightfield);

    data.compact = rcAllocCompactHeightfield();
    if (!data.compact || !rcBuildCompactHeightfield(&context,
                                                     cfg.walkableHeight,
                                                     cfg.walkableClimb,
                                                     *data.heightfield,
                                                     *data.compact) ||
        !rcErodeWalkableArea(&context, cfg.walkableRadius, *data.compact) ||
        !rcBuildDistanceField(&context, *data.compact) ||
        !rcBuildRegions(&context, *data.compact, 0, cfg.minRegionArea,
                        cfg.mergeRegionArea)) {
        error = "compact_regions_failed";
        return false;
    }

    data.contours = rcAllocContourSet();
    if (!data.contours || !rcBuildContours(&context, *data.compact,
                                           cfg.maxSimplificationError,
                                           cfg.maxEdgeLen, *data.contours)) {
        error = "contour_build_failed";
        return false;
    }
    data.polyMesh = rcAllocPolyMesh();
    if (!data.polyMesh || !rcBuildPolyMesh(&context, *data.contours,
                                           cfg.maxVertsPerPoly,
                                           *data.polyMesh)) {
        error = "polygon_mesh_failed";
        return false;
    }
    data.detail = rcAllocPolyMeshDetail();
    if (!data.detail || !rcBuildPolyMeshDetail(&context, *data.polyMesh,
                                               *data.compact,
                                               cfg.detailSampleDist,
                                               cfg.detailSampleMaxError,
                                               *data.detail)) {
        error = "detail_mesh_failed";
        return false;
    }

    for (int i = 0; i < data.polyMesh->npolys; ++i) {
        if (data.polyMesh->areas[i] == RC_WALKABLE_AREA) {
            data.polyMesh->areas[i] = 1;
            data.polyMesh->flags[i] = 1;
        }
    }

    dtNavMeshCreateParams params{};
    params.verts = data.polyMesh->verts;
    params.vertCount = data.polyMesh->nverts;
    params.polys = data.polyMesh->polys;
    params.polyAreas = data.polyMesh->areas;
    params.polyFlags = data.polyMesh->flags;
    params.polyCount = data.polyMesh->npolys;
    params.nvp = data.polyMesh->nvp;
    params.detailMeshes = data.detail->meshes;
    params.detailVerts = data.detail->verts;
    params.detailVertsCount = data.detail->nverts;
    params.detailTris = data.detail->tris;
    params.detailTriCount = data.detail->ntris;
    params.walkableHeight = agentHeight;
    params.walkableRadius = agentRadius;
    params.walkableClimb = stepHeight;
    rcVcopy(params.bmin, data.polyMesh->bmin);
    rcVcopy(params.bmax, data.polyMesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        error = "detour_data_failed";
        return false;
    }
    dtNavMesh* mesh = dtAllocNavMesh();
    if (!mesh) {
        dtFree(navData);
        error = "detour_mesh_alloc_failed";
        return false;
    }
    if (dtStatusFailed(mesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        dtFreeNavMesh(mesh);
        error = "detour_mesh_init_failed";
        return false;
    }
    outMesh = mesh;

    diag.sourceTriangleCount = static_cast<int>(source.size());
    diag.walkableTriangleCount = data.walkableTriangleCount;
    diag.navMeshPolyCount = data.polyMesh->npolys;
    diag.navMeshVertCount = data.polyMesh->nverts;
    diag.recastBoundsMin = bmin;
    diag.recastBoundsMax = bmax;
    return true;
}

} // namespace

struct RecastNavigationBackend::State
{
    dtNavMesh* mesh = nullptr;
    dtNavMeshQuery* query = nullptr;
    const World* world = nullptr;
    std::uint64_t geometryRevision = 0;
    std::size_t triangleCount = 0;
    std::uint64_t version = 0;
    int sourceTriangleCount = 0;
    int walkableTriangleCount = 0;
    int navMeshPolyCount = 0;
    int navMeshVertCount = 0;
    glm::vec3 recastBoundsMin{0.0f};
    glm::vec3 recastBoundsMax{0.0f};

    ~State()
    {
        if (query) dtFreeNavMeshQuery(query);
        if (mesh) dtFreeNavMesh(mesh);
    }
};

RecastNavigationBackend& RecastNavigationBackend::instance()
{
    static RecastNavigationBackend backend;
    if (!backend.mState) backend.mState = new State();
    return backend;
}

std::uint64_t RecastNavigationBackend::navmeshVersion() const
{
    return mState ? mState->version : 0;
}

void RecastNavigationBackend::invalidate()
{
    if (!mState) mState = new State();
    if (mState->query) {
        dtFreeNavMeshQuery(mState->query);
        mState->query = nullptr;
    }
    if (mState->mesh) {
        dtFreeNavMesh(mState->mesh);
        mState->mesh = nullptr;
    }
    mState->world = nullptr;
    mState->geometryRevision = 0;
    mState->triangleCount = 0;
}

RecastNavigationResult RecastNavigationBackend::prepare(
    const World& world, const NavigationAgentProfile& profile)
{
    Perf::ScopedTimer recastPrepareTimer("Npc::RecastPrepare");
    RecastNavigationResult result;
    const auto begin = std::chrono::steady_clock::now();
    if (!mState) mState = new State();

    const bool needsBuild = !mState->mesh || mState->world != &world ||
        mState->geometryRevision != world.renderRevision ||
        mState->triangleCount != world.collisionMesh.triangles.size();
    if (needsBuild) {
        invalidate();
        std::string error;
        if (!buildNavMesh(world, profile, mState->mesh, result, error)) {
            result.failure = error;
            result.buildMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            result.queryMilliseconds = result.buildMilliseconds;
            return result;
        }
        mState->world = &world;
        mState->geometryRevision = world.renderRevision;
        mState->triangleCount = world.collisionMesh.triangles.size();
        mState->sourceTriangleCount = result.sourceTriangleCount;
        mState->walkableTriangleCount = result.walkableTriangleCount;
        mState->navMeshPolyCount = result.navMeshPolyCount;
        mState->navMeshVertCount = result.navMeshVertCount;
        mState->recastBoundsMin = result.recastBoundsMin;
        mState->recastBoundsMax = result.recastBoundsMax;
        ++mState->version;
        mState->query = dtAllocNavMeshQuery();
        if (!mState->query || dtStatusFailed(mState->query->init(mState->mesh, 4096))) {
            result.failure = "detour_query_init_failed";
            result.buildMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            result.queryMilliseconds = result.buildMilliseconds;
            return result;
        }
    }

    // Publish build diagnostics even on a cache hit. A successful bake is a
    // build claim, not a route claim.
    result.available = true;
    result.coordinateSystem = kCoordinateSystem;
    result.navmeshVersion = mState->version;
    result.sourceTriangleCount = mState->sourceTriangleCount;
    result.walkableTriangleCount = mState->walkableTriangleCount;
    result.navMeshPolyCount = mState->navMeshPolyCount;
    result.navMeshVertCount = mState->navMeshVertCount;
    result.recastBoundsMin = mState->recastBoundsMin;
    result.recastBoundsMax = mState->recastBoundsMax;
    result.buildMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    result.queryMilliseconds = result.buildMilliseconds;
    return result;
}

RecastNavigationResult RecastNavigationBackend::query(
    const World& world, const glm::vec3& start, const glm::vec3& destination,
    const NavigationAgentProfile& profile)
{
    Perf::ScopedTimer recastQueryTimer("Npc::RecastQuery");
    RecastNavigationResult result;
    if (!mState) mState = new State();

    const auto begin = std::chrono::steady_clock::now();
    const bool needsBuild = !mState->mesh || mState->world != &world ||
        mState->geometryRevision != world.renderRevision ||
        mState->triangleCount != world.collisionMesh.triangles.size();
    if (needsBuild) {
        invalidate();
        std::string error;
        const auto buildBegin = std::chrono::steady_clock::now();
        if (!buildNavMesh(world, profile, mState->mesh, result, error)) {
            result.failure = error;
            result.buildMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - buildBegin).count();
            result.queryMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            return result;
        }
        result.buildMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - buildBegin).count();
        mState->world = &world;
        mState->geometryRevision = world.renderRevision;
        mState->triangleCount = world.collisionMesh.triangles.size();
        mState->sourceTriangleCount = result.sourceTriangleCount;
        mState->walkableTriangleCount = result.walkableTriangleCount;
        mState->navMeshPolyCount = result.navMeshPolyCount;
        mState->navMeshVertCount = result.navMeshVertCount;
        mState->recastBoundsMin = result.recastBoundsMin;
        mState->recastBoundsMax = result.recastBoundsMax;
        ++mState->version;
        mState->query = dtAllocNavMeshQuery();
        if (!mState->query || dtStatusFailed(mState->query->init(mState->mesh, 4096))) {
            result.failure = "detour_query_init_failed";
            result.queryMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            return result;
        }
    }

    result.available = true;
    result.coordinateSystem = kCoordinateSystem;
    result.navmeshVersion = mState->version;
    result.sourceTriangleCount = mState->sourceTriangleCount;
    result.walkableTriangleCount = mState->walkableTriangleCount;
    result.navMeshPolyCount = mState->navMeshPolyCount;
    result.navMeshVertCount = mState->navMeshVertCount;
    result.recastBoundsMin = mState->recastBoundsMin;
    result.recastBoundsMax = mState->recastBoundsMax;
    if (!mState->query) {
        result.failure = "detour_query_unavailable";
    } else {
        dtQueryFilter filter;
        filter.setIncludeFlags(1);
        const glm::vec3 sRecast = toRecast(start);
        const glm::vec3 dRecast = toRecast(destination);
        const float s[3] = {sRecast.x, sRecast.y, sRecast.z};
        const float d[3] = {dRecast.x, dRecast.y, dRecast.z};
        float nearestStart[3]{}, nearestDest[3]{};
        dtPolyRef startRef = nearestPolyEscalated(
            mState->query, &filter, s, profile, nearestStart,
            &result.startProjectionDistance);
        dtPolyRef destRef = nearestPolyEscalated(
            mState->query, &filter, d, profile, nearestDest,
            &result.destProjectionDistance);

        result.startPolyFound = startRef != 0;
        result.destPolyFound = destRef != 0;
        result.startPolyRef = startRef;
        result.destPolyRef = destRef;
        if (result.startPolyFound) {
            result.nearestStart = fromRecast(
                glm::vec3(nearestStart[0], nearestStart[1], nearestStart[2]));
        }
        if (result.destPolyFound) {
            result.nearestDest = fromRecast(
                glm::vec3(nearestDest[0], nearestDest[1], nearestDest[2]));
        }

        if (!result.startPolyFound && !result.destPolyFound) {
            result.failure = "nearest_polygon_failed_both";
        } else if (!result.startPolyFound) {
            result.failure = "nearest_polygon_failed_start";
        } else if (!result.destPolyFound) {
            result.failure = "nearest_polygon_failed_dest";
        } else {
            dtPolyRef polys[256]{};
            int polyCount = 0;
            const dtStatus pathStatus = mState->query->findPath(
                startRef, destRef, nearestStart, nearestDest, &filter, polys,
                &polyCount, 256);
            if (dtStatusFailed(pathStatus)) {
                result.failure = "find_path_query_failed";
            } else if (polyCount <= 0) {
                result.failure = "path_not_found";
            } else {
                result.polygonCount = polyCount;
                float straight[256 * 3]{};
                unsigned char flags[256]{};
                dtPolyRef refs[256]{};
                int pointCount = 0;
                const dtStatus straightStatus = mState->query->findStraightPath(
                    nearestStart, nearestDest, polys, polyCount, straight,
                    flags, refs, &pointCount, 256);
                if (dtStatusFailed(straightStatus) || pointCount <= 0) {
                    result.failure = "straight_path_failed";
                } else {
                    result.success = true;
                    result.points.reserve(pointCount);
                    for (int i = 0; i < pointCount; ++i) {
                        result.points.push_back(fromRecast(glm::vec3(
                            straight[i * 3], straight[i * 3 + 1],
                            straight[i * 3 + 2])));
                    }
                    for (std::size_t i = 1; i < result.points.size(); ++i) {
                        result.pathLength += horizontalLength(result.points[i - 1],
                                                              result.points[i]);
                    }
                }
            }
        }
    }
    result.queryMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    return result;
}
