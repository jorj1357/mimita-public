#include "modules/api/actor-rendering-api.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <module-identity.h>

namespace {
bool finite3(const float* p) { return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]); }
int pose(mimita_actor_rendering::PoseV1& p) {
    std::memcpy(p.result, p.current, sizeof(p.result));
    p.usedPrevious = 0;
    glm::mat4 current(1.0f), previous(1.0f);
    std::memcpy(&current[0][0], p.current, sizeof(p.current));
    std::memcpy(&previous[0][0], p.previous, sizeof(p.previous));
    p.stepMeters = glm::length(glm::vec3(current[3]) - glm::vec3(previous[3]));
    if (!p.interpolate || !p.hasPrevious || !std::isfinite(p.alpha) ||
        p.alpha < 0.0f || p.alpha >= 0.999f ||
        !std::isfinite(p.stepMeters) || p.stepMeters > 8.0f) return 1;
    glm::vec3 a(0), b(0), sa(1), sb(1), skewA(0), skewB(0);
    glm::quat qa(1,0,0,0), qb(1,0,0,0);
    glm::vec4 pa(0), pb(0);
    if (!glm::decompose(previous, sa, qa, a, skewA, pa) ||
        !glm::decompose(current, sb, qb, b, skewB, pb)) return 1;
    glm::mat4 result = glm::translate(glm::mat4(1), glm::mix(a,b,p.alpha)) *
        glm::mat4_cast(glm::slerp(qa,qb,p.alpha)) *
        glm::scale(glm::mat4(1), glm::mix(sa,sb,p.alpha));
    for (int col=0; col<4; ++col)
        for (int row=0; row<4; ++row)
            if (!std::isfinite(result[col][row])) return 1;
    std::memcpy(p.result, &result[0][0], sizeof(p.result));
    p.usedPrevious = 1;
    return 1;
}
int bounds(mimita_actor_rendering::BoundsV1& b) {
    b.valid = 0;
    if (!b.vertices || !b.count || b.stride < sizeof(float)*3 || b.stride > 4096) return 1;
    float lo[3] = {INFINITY,INFINITY,INFINITY};
    float hi[3] = {-INFINITY,-INFINITY,-INFINITY};
    for (std::uint32_t i=0; i<b.count; ++i) {
        float p[3];
        std::memcpy(p, b.vertices + (std::size_t(b.first)+i)*b.stride, sizeof(p));
        if (!finite3(p)) return 1;
        for (int axis=0;axis<3;++axis) { lo[axis]=std::min(lo[axis],p[axis]); hi[axis]=std::max(hi[axis],p[axis]); }
    }
    std::memcpy(b.minimum,lo,sizeof(lo));
    std::memcpy(b.maximum,hi,sizeof(hi));
    b.valid=1;
    return 1;
}
int outline(mimita_actor_rendering::OutlineV1& o) {
    const float radius=std::max(64.0f,std::abs(o.sizeScale)*64.0f);
    o.worldThickness=std::clamp(o.thickness*0.015f,0.01f,0.15f);
    o.cameraInside = !o.isLocal && std::isfinite(o.cameraDistance) &&
        o.cameraDistance <= o.maxExtent + o.worldThickness;
    o.rejected= !o.boundsValid || !std::isfinite(o.maxOffset) ||
        !std::isfinite(o.maxExtent) ||
        (!o.isLocal && !std::isfinite(o.cameraDistance)) ||
        o.maxOffset>radius ||
        o.maxExtent>radius*2.0f || o.cameraInside;
    o.draw = o.draw && !o.rejected;
    return 1;
}
int drawPlan(mimita_actor_rendering::DrawPlanV1& p) {
    if (p.partCount > p.partCapacity || (p.partCount && (!p.parts || !p.partDrawMask))) return 0;
    bool allSkin = true, allValid = true;
    for (std::uint32_t i=0; i<p.partCount; ++i) {
        const auto& part=p.parts[i];
        const bool valid=part.nodeValid && part.vertexCount;
        p.partDrawMask[i] = valid && !(p.hideHead && part.isHead) && part.bodyLayer!=3;
        allSkin = allSkin && part.bodyLayer==0;
        allValid = allValid && part.nodeValid;
    }
    p.combineAllowed=allSkin && allValid && !p.hideHead && !p.outlinePass &&
        !p.whiteOverride && !p.debugCull;
    std::uint32_t out=0;
    bool used[4]{};
    for (int i=0;i<8;++i) {
        const unsigned value = i<4 ? p.requestedLayers[i] : unsigned(i-4);
        if (value<4 && !used[value]) { used[value]=true; p.orderedLayers[out++]=std::uint8_t(value); }
    }
    return 1;
}
int onEvent(mimita_module::ModuleEventV1* e) {
    if (!e || !e->payload) return 0;
    if (e->typeId==mimita_module::EVENT_ACTOR_RENDER_POSE && e->payloadSize==sizeof(mimita_actor_rendering::PoseV1))
        return pose(*static_cast<mimita_actor_rendering::PoseV1*>(e->payload));
    if (e->typeId==mimita_module::EVENT_ACTOR_RENDER_BOUNDS && e->payloadSize==sizeof(mimita_actor_rendering::BoundsV1))
        return bounds(*static_cast<mimita_actor_rendering::BoundsV1*>(e->payload));
    if (e->typeId==mimita_module::EVENT_ACTOR_RENDER_OUTLINE && e->payloadSize==sizeof(mimita_actor_rendering::OutlineV1))
        return outline(*static_cast<mimita_actor_rendering::OutlineV1*>(e->payload));
    if (e->typeId==mimita_module::EVENT_ACTOR_RENDER_DRAW_PLAN && e->payloadSize==sizeof(mimita_actor_rendering::DrawPlanV1))
        return drawPlan(*static_cast<mimita_actor_rendering::DrawPlanV1*>(e->payload));
    return 0;
}
int initialize(const mimita_module::HostServicesV1* h) { return h && h->version==mimita_module::ABI_VERSION && h->structSize==sizeof(*h); }
void shutdown() {}
std::uint32_t exportState(std::uint8_t*,std::uint32_t) { return 0; }
int importState(const std::uint8_t*,std::uint32_t size) { return size==0; }
const mimita_module::ModuleV1 module = {
    {mimita_module::ABI_VERSION,sizeof(mimita_module::ModuleV1),"actor-rendering",
     MIMITA_MODULE_GENERATION,MIMITA_MODULE_SOURCE_HASH,MIMITA_MODULE_STATE_SCHEMA},
    initialize,shutdown,onEvent,exportState,importState};
}
MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL GetMimitaModuleAPI(std::uint32_t version) {
    return version==mimita_module::ABI_VERSION ? &module.base : nullptr;
}
