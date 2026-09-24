// 09 23 2026
/* purpose
* Cold dispatch for the generic geometry primitive library. Resolves the hot
* `net.geometry` provider by capability id and calls the named primitive; falls
* back to the shared implementation when no provider is loaded. Never cached
* across a generation swap.
* Does NOT own geometry math (the header does) or any caller.
*/
#include "hot-reload/hot-geometry.h"

#include "hot-reload/generic-runtime.h"

namespace MimitaNet {

void runGeometryPrimitive(GameGeometryQueryV1& query)
{
    query.structSize = sizeof(GameGeometryQueryV1);
    auto lookup = reinterpret_cast<GameGeometryLookupFn>(
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_GEOMETRY));
    if (lookup) {
        const GameGeometryPrimitiveV1* primitive =
            lookup(nullptr, query.primitiveId);
        if (primitive && primitive->invoke) {
            primitive->invoke(nullptr, &query);
            return;
        }
    }
    // Shared fallback: the same code the hot provider serves.
    switch (query.primitiveId) {
    case GAME_GEOM_RAY_AABB: HotGeometryImpl::rayAabb(query); return;
    case GAME_GEOM_RAY_TRIANGLE: HotGeometryImpl::rayTriangle(query); return;
    case GAME_GEOM_SWEPT_POINT_SPHERE: HotGeometryImpl::sweptPointSphere(query); return;
    case GAME_GEOM_POINT_IN_AABB: HotGeometryImpl::pointInAabb(query); return;
    case GAME_GEOM_CLOSEST_POINT_SEGMENT: HotGeometryImpl::closestOnSegment(query); return;
    default: query.result = 1u; query.hit = 0u; return;
    }
}

} // namespace MimitaNet
