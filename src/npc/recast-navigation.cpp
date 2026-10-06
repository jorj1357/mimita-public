// 2026-10-06
// Initial Recast/Detour integration proof. This is deliberately query-only:
// MiMITA still owns goals, movement intent, physics, collision, and traversal.
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

namespace {

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

bool buildNavMesh(const World& world, const NavigationAgentProfile& profile,
                  dtNavMesh*& outMesh, std::string& error)
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

    glm::vec3 bmin = world.collisionMesh.boundsMin;
    glm::vec3 bmax = world.collisionMesh.boundsMax;
    if (bmin == bmax) {
        bmin = glm::vec3(1.0e30f);
        bmax = glm::vec3(-1.0e30f);
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            bmin = glm::min(bmin, glm::min(tri.a, glm::min(tri.b, tri.c)));
            bmax = glm::max(bmax, glm::max(tri.a, glm::max(tri.b, tri.c)));
        }
    }
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
        for (const glm::vec3& v : {tri.a, tri.b, tri.c}) {
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
    glm::vec3 center = (world.collisionMesh.boundsMin +
                        world.collisionMesh.boundsMax) * 0.5f;
    return query(world, center, center, profile);
}

RecastNavigationResult RecastNavigationBackend::query(
    const World& world, const glm::vec3& start, const glm::vec3& destination,
    const NavigationAgentProfile& profile)
{
    RecastNavigationResult result;
    const auto begin = std::chrono::steady_clock::now();
    if (!mState) mState = new State();
    const bool needsBuild = !mState->mesh || mState->world != &world ||
        mState->geometryRevision != world.renderRevision ||
        mState->triangleCount != world.collisionMesh.triangles.size();
    if (needsBuild) {
        invalidate();
        std::string error;
        if (!buildNavMesh(world, profile, mState->mesh, error)) {
            result.failure = error;
            result.queryMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
            return result;
        }
        mState->world = &world;
        mState->geometryRevision = world.renderRevision;
        mState->triangleCount = world.collisionMesh.triangles.size();
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
    result.navmeshVersion = mState->version;
    if (!mState->query) {
        result.failure = "detour_query_unavailable";
    } else {
        dtQueryFilter filter;
        filter.setIncludeFlags(1);
        const float extents[3] = {profile.radius * 4.0f, profile.height, profile.radius * 4.0f};
        const float s[3] = {start.x, start.y, start.z};
        const float d[3] = {destination.x, destination.y, destination.z};
        float nearestStart[3]{}, nearestDest[3]{};
        dtPolyRef startRef = 0, destRef = 0;
        if (dtStatusFailed(mState->query->findNearestPoly(s, extents, &filter,
                                                          &startRef, nearestStart)) ||
            dtStatusFailed(mState->query->findNearestPoly(d, extents, &filter,
                                                           &destRef, nearestDest))) {
            result.failure = "nearest_polygon_failed";
        } else {
            dtPolyRef polys[256]{};
            int polyCount = 0;
            if (dtStatusFailed(mState->query->findPath(startRef, destRef,
                                                       nearestStart, nearestDest,
                                                       &filter, polys, &polyCount,
                                                       256)) || polyCount <= 0) {
                result.failure = "path_not_found";
            } else {
                float straight[256 * 3]{};
                unsigned char flags[256]{};
                dtPolyRef refs[256]{};
                int pointCount = 0;
                const dtStatus status = mState->query->findStraightPath(
                    nearestStart, nearestDest, polys, polyCount, straight,
                    flags, refs, &pointCount, 256);
                if (dtStatusFailed(status) || pointCount <= 0) {
                    result.failure = "straight_path_failed";
                } else {
                    result.success = true;
                    result.polygonCount = polyCount;
                    result.points.reserve(pointCount);
                    for (int i = 0; i < pointCount; ++i)
                        result.points.emplace_back(straight[i * 3], straight[i * 3 + 1],
                                                   straight[i * 3 + 2]);
                    for (std::size_t i = 1; i < result.points.size(); ++i)
                        result.pathLength += horizontalLength(result.points[i - 1],
                                                              result.points[i]);
                }
            }
        }
    }
    result.queryMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    return result;
}
