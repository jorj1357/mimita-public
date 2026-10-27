// 2026-10-27

#include "physics/constraints/constraint-debug.h"

#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "gamemode/map-config.h"
#include "physics/constraints/constraint-system.h"
#include "physics/physical-entity.h"

namespace {

glm::vec3 anchorWorld(const Constraint& constraint, bool endpointB)
{
    const uint32_t entityId =
        endpointB ? constraint.entityB : constraint.entityA;
    const glm::vec3 local =
        endpointB ? constraint.localAnchorB : constraint.localAnchorA;
    PhysicalEntity* entity = PhysicalEntitySystem::instance().find(entityId);
    return entity ? glm::vec3(entity->transform * glm::vec4(local, 1.0f))
                  : local;
}

glm::vec3 axisWorld(const Constraint& constraint, bool endpointB)
{
    const uint32_t entityId =
        endpointB ? constraint.entityB : constraint.entityA;
    const glm::vec3 local =
        endpointB ? constraint.localAxisB : constraint.localAxisA;
    PhysicalEntity* entity = PhysicalEntitySystem::instance().find(entityId);
    glm::vec3 axis = entity ? entity->orientation * local : local;
    const float lengthSq = glm::dot(axis, axis);
    return lengthSq > 1e-8f ? axis / std::sqrt(lengthSq)
                            : glm::vec3(0.0f, 0.0f, 1.0f);
}

const char* typeName(ConstraintType type)
{
    switch (type)
    {
        case ConstraintType::Point: return "point";
        case ConstraintType::Hinge: return "hinge";
        case ConstraintType::Slider: return "slider";
    }
    return "constraint";
}

} // namespace

void drawConstraintDebug(const Camera& camera)
{
    if (!MapConfigRegistry::instance().entityVisibility() ||
        !DebugVis::masterEnabled())
        return;

    const auto& constraints = ConstraintSystem::instance().constraints();
    for (const Constraint& constraint : constraints)
    {
        if (!constraint.enabled)
            continue;

        const glm::vec3 a = anchorWorld(constraint, false);
        const glm::vec3 b = anchorWorld(constraint, true);
        const glm::vec3 axis = axisWorld(constraint, false);

        const glm::vec4 anchorColor(1.0f, 0.85f, 0.2f, 1.0f);
        const glm::vec4 lineColor(0.4f, 0.85f, 1.0f, 1.0f);
        const glm::vec4 axisColor(1.0f, 0.35f, 0.35f, 1.0f);

        DebugVis::drawWireSphere(camera, a, 0.09f, anchorColor);
        DebugVis::drawLine(camera, a, b, lineColor);

        if (constraint.entityB != 0)
            DebugVis::drawWireSphere(camera, b, 0.09f, anchorColor);

        if (constraint.type == ConstraintType::Hinge)
        {
            DebugVis::drawLine(camera, a - axis * 0.8f, a + axis * 0.8f,
                               axisColor);
            DebugVis::drawLine(camera, a, a + axisWorld(constraint, true) * 0.8f,
                               {0.9f, 0.9f, 0.9f, 1.0f});
        }
        else if (constraint.type == ConstraintType::Slider)
        {
            const glm::vec3 travelA = a + axis * constraint.minLimit;
            const glm::vec3 travelB = a + axis * constraint.maxLimit;
            DebugVis::drawLine(camera, travelA, travelB, axisColor);
            DebugVis::drawLine(camera, travelA - axis * 0.15f,
                               travelA + axis * 0.15f, axisColor);
            DebugVis::drawLine(camera, travelB - axis * 0.15f,
                               travelB + axis * 0.15f, axisColor);
            const glm::vec3 rest = a + axis * constraint.restLength;
            DebugVis::drawPointCross(camera, rest, 0.12f,
                                     {0.5f, 1.0f, 0.5f, 1.0f});
        }

        char label[96];
        std::snprintf(label, sizeof(label), "%s #%u%s",
                      typeName(constraint.type), constraint.id,
                      constraint.motorEnabled ? " motor" : "");
        DebugVis::drawWorldLabel(a + glm::vec3(0.0f, 0.0f, 0.25f), label,
                                 anchorColor);
    }
}
