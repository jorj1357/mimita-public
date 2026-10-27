// 2026-10-27

#include "physics/constraints/constraint-commands.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "physics/constraints/constraint-system.h"
#include "physics/physical-entity.h"
#include "terminal/terminal-state.h"

namespace {

bool parseFloat(const std::string& text, float& out)
{
    try
    {
        size_t used = 0;
        out = std::stof(text, &used);
        return used == text.size() && std::isfinite(out);
    }
    catch (...)
    {
        return false;
    }
}

bool parseUint(const std::string& text, uint32_t& out)
{
    try
    {
        size_t used = 0;
        const unsigned long value = std::stoul(text, &used);
        if (used != text.size())
            return false;
        out = static_cast<uint32_t>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const char* typeName(ConstraintType type)
{
    switch (type)
    {
        case ConstraintType::Point: return "point";
        case ConstraintType::Hinge: return "hinge";
        case ConstraintType::Slider: return "slider";
    }
    return "unknown";
}

bool parseType(const std::string& text, ConstraintType& out)
{
    if (text == "point")
    {
        out = ConstraintType::Point;
        return true;
    }
    if (text == "hinge")
    {
        out = ConstraintType::Hinge;
        return true;
    }
    if (text == "slider" || text == "spring")
    {
        out = ConstraintType::Slider;
        return true;
    }
    return false;
}

uint32_t spawnDemoBox(const glm::vec3& center, const glm::vec3& half,
                      PhysicalEntityMotion motion, float density)
{
    std::vector<CollisionTriangle> triangles;
    buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), half);

    PhysicalEntitySystem& entities = PhysicalEntitySystem::instance();
    const uint32_t id = entities.add(
        triangles, glm::translate(glm::mat4(1.0f), center), motion, 0);
    if (PhysicalEntity* entity = entities.find(id))
    {
        entity->shape = PhysicalEntityShape::Box;
        entity->halfExtents = half;
        entity->density = density;
        entity->mass = std::max(
            density * (8.0f * half.x * half.y * half.z), 0.001f);
        entity->friction = 0.7f;
        entity->restitution = 0.0f;
        entity->linearDamping = 0.1f;
        entity->angularDamping = 1.2f;
        entity->sleeping = false;
        entity->sleepTicks = 0;
        entity->persistenceId = "constraint-demo-box-" + std::to_string(id);
        entity->modelPath = "generated:crate-box";
        entity->texturePath = "assets/textureshq/clouds11.png";
        refreshEntityMassProperties(*entity);
    }
    return id;
}

void logConstraintCreated(const Constraint& constraint)
{
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "[CONSTRAINT] created id=%u type=%s a=%u b=%u",
                  constraint.id, typeName(constraint.type),
                  constraint.entityA, constraint.entityB);
    Terminal::instance().addLog(buffer);

    nlohmann::json fields;
    fields["constraintId"] = constraint.id;
    fields["type"] = typeName(constraint.type);
    fields["entityA"] = constraint.entityA;
    fields["entityB"] = constraint.entityB;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "constraint.created", std::to_string(constraint.id),
        "runtime constraint created by terminal", 0, fields,
        __FILE__, __LINE__, __func__);
}

} // namespace

void registerConstraintCommands()
{
    Terminal::instance().registerCommand({
        "constraint_create",
        "Create a point/hinge/slider constraint at a world anchor",
        "constraint_create <point|hinge|slider> <entityA> <entityB|0> <x y z> [ax ay az]",
        [](const std::vector<std::string>& args) {
            ConstraintType type = ConstraintType::Point;
            uint32_t a = 0;
            uint32_t b = 0;
            if (args.size() < 6 || !parseType(args[0], type) ||
                !parseUint(args[1], a) || !parseUint(args[2], b))
            {
                Terminal::instance().addLog(
                    "[CONSTRAINT] usage: constraint_create "
                    "<point|hinge|slider> <entityA> <entityB|0> <x y z> [ax ay az]");
                return;
            }

            glm::vec3 anchor(0.0f);
            for (int i = 0; i < 3; ++i)
            {
                if (!parseFloat(args[3 + i], anchor[i]))
                {
                    Terminal::instance().addLog(
                        "[CONSTRAINT] anchor must be three finite numbers");
                    return;
                }
            }

            glm::vec3 axis(0.0f, 0.0f, 1.0f);
            if (args.size() >= 9)
            {
                for (int i = 0; i < 3; ++i)
                {
                    if (!parseFloat(args[6 + i], axis[i]))
                    {
                        Terminal::instance().addLog(
                            "[CONSTRAINT] axis must be three finite numbers");
                        return;
                    }
                }
            }

            PhysicalEntitySystem& entities = PhysicalEntitySystem::instance();
            PhysicalEntity* entityA = a != 0 ? entities.find(a) : nullptr;
            PhysicalEntity* entityB = b != 0 ? entities.find(b) : nullptr;
            if ((a != 0 && !entityA) || (b != 0 && !entityB))
            {
                Terminal::instance().addLog(
                    "[CONSTRAINT] entity not found; use physical_entity_list");
                return;
            }

            const glm::vec3 safeAxis =
                glm::dot(axis, axis) > 1e-6f ? glm::normalize(axis)
                                             : glm::vec3(0.0f, 0.0f, 1.0f);

            Constraint constraint;
            constraint.type = type;
            constraint.entityA = a;
            constraint.entityB = b;
            constraint.localAnchorA =
                entityA ? glm::vec3(glm::inverse(entityA->transform) *
                                    glm::vec4(anchor, 1.0f))
                        : anchor;
            constraint.localAnchorB =
                entityB ? glm::vec3(glm::inverse(entityB->transform) *
                                    glm::vec4(anchor, 1.0f))
                        : anchor;
            constraint.localAxisA =
                entityA ? glm::normalize(glm::inverse(entityA->orientation) *
                                         safeAxis)
                        : safeAxis;
            constraint.localAxisB =
                entityB ? glm::normalize(glm::inverse(entityB->orientation) *
                                         safeAxis)
                        : safeAxis;
            constraint.springStiffness = type == ConstraintType::Slider
                                             ? 2000.0f
                                             : 0.0f;
            constraint.springDamping = type == ConstraintType::Slider
                                           ? 80.0f
                                           : 0.0f;
            constraint.restLength = glm::length(anchor);
            constraint.limitsEnabled = false;

            const uint32_t id = ConstraintSystem::instance().add(constraint);
            constraint.id = id;
            logConstraintCreated(constraint);
        }
    });

    Terminal::instance().registerCommand({
        "constraint_spring",
        "Tune a slider/spring constraint",
        "constraint_spring <id> <stiffness> <damping> <restLength>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            float stiffness = 0.0f;
            float damping = 0.0f;
            float restLength = 0.0f;
            if (args.size() < 4 || !parseUint(args[0], id) ||
                !parseFloat(args[1], stiffness) ||
                !parseFloat(args[2], damping) ||
                !parseFloat(args[3], restLength))
            {
                Terminal::instance().addLog(
                    "[CONSTRAINT] usage: constraint_spring <id> <stiffness> "
                    "<damping> <restLength>");
                return;
            }
            for (Constraint& constraint : ConstraintSystem::instance().constraints())
            {
                if (constraint.id != id)
                    continue;
                constraint.springStiffness = std::max(stiffness, 0.0f);
                constraint.springDamping = std::max(damping, 0.0f);
                constraint.restLength = restLength;
                Terminal::instance().addLog("[CONSTRAINT] spring tuned for id=" +
                                            std::to_string(id));
                return;
            }
            Terminal::instance().addLog("[CONSTRAINT] id not found");
        }
    });

    Terminal::instance().registerCommand({
        "constraint_motor",
        "Tune a hinge motor",
        "constraint_motor <id> <targetSpeed> <maxForce>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            float speed = 0.0f;
            float maxForce = 0.0f;
            if (args.size() < 3 || !parseUint(args[0], id) ||
                !parseFloat(args[1], speed) ||
                !parseFloat(args[2], maxForce))
            {
                Terminal::instance().addLog(
                    "[CONSTRAINT] usage: constraint_motor <id> <targetSpeed> "
                    "<maxForce>");
                return;
            }
            for (Constraint& constraint : ConstraintSystem::instance().constraints())
            {
                if (constraint.id != id)
                    continue;
                constraint.motorEnabled = true;
                constraint.motorSpeed = speed;
                constraint.motorMaxForce = std::max(maxForce, 0.0f);
                Terminal::instance().addLog("[CONSTRAINT] motor tuned for id=" +
                                            std::to_string(id));
                return;
            }
            Terminal::instance().addLog("[CONSTRAINT] id not found");
        }
    });

    Terminal::instance().registerCommand({
        "constraint_list",
        "List active constraints",
        "constraint_list",
        [](const std::vector<std::string>&) {
            const auto& constraints = ConstraintSystem::instance().constraints();
            Terminal::instance().addLog("[CONSTRAINT] count=" +
                                        std::to_string(constraints.size()));
            for (const Constraint& constraint : constraints)
            {
                char buffer[320];
                std::snprintf(buffer, sizeof(buffer),
                              "[CONSTRAINT] id=%u type=%s a=%u b=%u "
                              "anchorA=(%.2f %.2f %.2f) anchorB=(%.2f %.2f %.2f) "
                              "spring=(%.1f %.1f rest=%.2f) motor=%d enabled=%d",
                              constraint.id, typeName(constraint.type),
                              constraint.entityA, constraint.entityB,
                              constraint.localAnchorA.x,
                              constraint.localAnchorA.y,
                              constraint.localAnchorA.z,
                              constraint.localAnchorB.x,
                              constraint.localAnchorB.y,
                              constraint.localAnchorB.z,
                              constraint.springStiffness,
                              constraint.springDamping,
                              constraint.restLength,
                              constraint.motorEnabled ? 1 : 0,
                              constraint.enabled ? 1 : 0);
                Terminal::instance().addLog(buffer);
            }
        }
    });

    Terminal::instance().registerCommand({
        "constraint_delete",
        "Delete one constraint by id",
        "constraint_delete <id>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            if (args.empty() || !parseUint(args[0], id))
            {
                Terminal::instance().addLog(
                    "[CONSTRAINT] usage: constraint_delete <id>");
                return;
            }
            const bool removed = ConstraintSystem::instance().remove(id);
            Terminal::instance().addLog(
                removed ? "[CONSTRAINT] deleted id=" + std::to_string(id)
                        : "[CONSTRAINT] id not found");
        }
    });

    Terminal::instance().registerCommand({
        "constraint_clear",
        "Delete every constraint",
        "constraint_clear",
        [](const std::vector<std::string>&) {
            ConstraintSystem::instance().clear();
            Terminal::instance().addLog("[CONSTRAINT] cleared");
        }
    });

    Terminal::instance().registerCommand({
        "constraint_demo",
        "Spawn the two-block hinge or spring demo in front of the player",
        "constraint_demo [hinge|spring]",
        [](const std::vector<std::string>& args) {
            const bool spring = !args.empty() && args[0] == "spring";
            Player& player = THE_PLAYER;
            Camera& camera = THE_CAMERA;

            glm::vec3 forward = camera.front;
            if (glm::dot(forward, forward) < 1e-6f)
                forward = glm::vec3(1.0f, 0.0f, 0.0f);
            forward = glm::normalize(forward);

            glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));
            if (glm::dot(right, right) < 1e-6f)
                right = glm::vec3(1.0f, 0.0f, 0.0f);
            right = glm::normalize(right);

            const glm::vec3 base =
                player.pos + forward * 6.0f +
                right * 1.0f + glm::vec3(0.0f, 0.0f, 3.0f);
            const glm::vec3 half(1.0f);
            const glm::vec3 axis = right;

            const uint32_t anchorId = spawnDemoBox(
                base - axis * 1.0f, half, PhysicalEntityMotion::Static, 600.0f);
            const uint32_t movingId = spawnDemoBox(
                base + axis * 1.0f, half, PhysicalEntityMotion::Dynamic, 120.0f);

            Constraint constraint;
            constraint.type = spring ? ConstraintType::Slider
                                     : ConstraintType::Hinge;
            constraint.entityA = anchorId;
            constraint.entityB = movingId;
            constraint.localAnchorA = axis * 1.0f;
            constraint.localAnchorB = -axis * 1.0f;
            constraint.localAxisA = axis;
            constraint.localAxisB = axis;
            if (spring)
            {
                constraint.springStiffness = 2500.0f;
                constraint.springDamping = 90.0f;
                constraint.restLength = 2.0f;
                constraint.limitsEnabled = true;
                constraint.minLimit = 1.0f;
                constraint.maxLimit = 3.0f;
            }

            const uint32_t id = ConstraintSystem::instance().add(constraint);
            constraint.id = id;
            logConstraintCreated(constraint);
            Terminal::instance().addLog(
                spring ? "[CONSTRAINT] spring demo spawned"
                       : "[CONSTRAINT] hinge demo spawned; the box should swing");
        }
    });
}
