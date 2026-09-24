// 09 23 2026
/* purpose
* Hot geometry primitive module. Registers the generic `net.geometry` capability
* and a lookup table of named primitives served from
* `hot-reload/hot-geometry.h`. Editing that header (or this file) changes the
* primitive math live, and adding a new primitive is a hot source edit: register
* it here with a new id, no EXE call site. The cold callers resolve primitives
* through the generic doorway and fall back to the shared implementation when no
* provider is loaded.
* Does NOT own entities, world storage, or gameplay policy.
*/
#if defined(MIMITA_GAME_DLL)

#include <cstddef>

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-geometry.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL geomRayAabb(void* /*host*/, GameGeometryQueryV1* request)
{
    HotGeometryImpl::rayAabb(*request);
}
void MIMITA_GAME_CALL geomRayTriangle(void* /*host*/, GameGeometryQueryV1* request)
{
    HotGeometryImpl::rayTriangle(*request);
}
void MIMITA_GAME_CALL geomSweptPointSphere(void* /*host*/, GameGeometryQueryV1* request)
{
    HotGeometryImpl::sweptPointSphere(*request);
}
void MIMITA_GAME_CALL geomPointInAabb(void* /*host*/, GameGeometryQueryV1* request)
{
    HotGeometryImpl::pointInAabb(*request);
}
void MIMITA_GAME_CALL geomClosestSegment(void* /*host*/, GameGeometryQueryV1* request)
{
    HotGeometryImpl::closestOnSegment(*request);
}

// The primitive table. Adding a new primitive is one more entry here.
const GameGeometryPrimitiveV1 kPrimitives[] = {
    {GAME_GEOM_RAY_AABB, 1, 0, &geomRayAabb, "geom.ray.aabb"},
    {GAME_GEOM_RAY_TRIANGLE, 1, 0, &geomRayTriangle, "geom.ray.triangle"},
    {GAME_GEOM_SWEPT_POINT_SPHERE, 1, 0, &geomSweptPointSphere, "geom.swept.point-sphere"},
    {GAME_GEOM_POINT_IN_AABB, 1, 0, &geomPointInAabb, "geom.point.aabb"},
    {GAME_GEOM_CLOSEST_POINT_SEGMENT, 1, 0, &geomClosestSegment, "geom.closest.segment-point"},
};

const GameGeometryPrimitiveV1* MIMITA_GAME_CALL lookupGeometry(
    void* /*host*/, std::uint64_t primitiveId)
{
    for (const GameGeometryPrimitiveV1& p : kPrimitives)
        if (p.primitiveId == primitiveId)
            return &p;
    return nullptr;
}

const GameCapabilityDescriptorV1 kGeometryProvider{
    GAME_CAP_GEOMETRY, GAME_SIG_GEOMETRY, 0,
    reinterpret_cast<void*>(&lookupGeometry), "net.geometry"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_geometryProviderRegistrar{
    kGeometryProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_geometryRequirement{
    GAME_CAP_GEOMETRY, GAME_SIG_GEOMETRY, 0};

#endif // MIMITA_GAME_DLL
