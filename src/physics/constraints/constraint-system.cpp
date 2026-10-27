// 2026-10-27
/* purpose
* Solve generalized Point, Hinge, and Slider constraints on PhysicalEntity
* bodies at the fixed physics step. Reuses the entity rigid-body data and
* public impulse helpers so no second inertia or mass source exists.
* Does NOT integrate bodies, resolve world collision, or own gameplay policy.
*/

#include "physics/constraints/constraint-system.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {

constexpr float kEpsilon = 1.0e-6f;
constexpr int kIterations = 8;
constexpr float kPositionBeta = 0.2f;
constexpr float kMaxBiasSpeed = 4.0f;

bool isDynamic(const PhysicalEntity* e)
{
    return e && e->motion == PhysicalEntityMotion::Dynamic && e->mass > 0.0f;
}

float inverseMassOf(const PhysicalEntity* e)
{
    return isDynamic(e) ? 1.0f / e->mass : 0.0f;
}

glm::vec3 inverseInertiaOf(const PhysicalEntity* e, const glm::vec3& v)
{
    return isDynamic(e) ? physicalEntityInverseInertiaWorld(*e, v) : glm::vec3(0.0f);
}

glm::vec3 anchorWorld(const PhysicalEntity* e, const glm::vec3& localAnchor)
{
    return e ? glm::vec3(e->transform[3]) + e->orientation * localAnchor
             : localAnchor;
}

glm::vec3 centerWorld(const PhysicalEntity* e)
{
    return e ? physicalEntityCenterOfMassWorld(*e) : glm::vec3(0.0f);
}

glm::vec3 pointVelocity(const PhysicalEntity* e, const glm::vec3& worldPoint)
{
    return e ? physicalEntityPointVelocity(*e, worldPoint) : glm::vec3(0.0f);
}

glm::vec3 axisWorld(const PhysicalEntity* e, const glm::vec3& localAxis)
{
    glm::vec3 axis = e ? e->orientation * localAxis : localAxis;
    const float lengthSq = glm::dot(axis, axis);
    return lengthSq > kEpsilon ? axis / std::sqrt(lengthSq)
                               : glm::vec3(0.0f, 0.0f, 1.0f);
}

void applyLinearImpulse(PhysicalEntity* e, const glm::vec3& impulse,
                        const glm::vec3& worldPoint)
{
    if (!isDynamic(e))
        return;
    applyPhysicalEntityImpulse(*e, impulse, worldPoint);
}

void applyAngularImpulse(PhysicalEntity* e, const glm::vec3& angularImpulse)
{
    if (!isDynamic(e))
        return;
    applyPhysicalEntityAngularImpulse(*e, angularImpulse);
}

float effectiveMassAlong(const PhysicalEntity* e, const glm::vec3& r,
                         const glm::vec3& direction)
{
    if (!isDynamic(e))
        return 0.0f;
    const glm::vec3 rCrossN = glm::cross(r, direction);
    return inverseMassOf(e) +
           glm::dot(rCrossN, inverseInertiaOf(e, rCrossN));
}

float effectiveAngularMass(const PhysicalEntity* e, const glm::vec3& axis)
{
    if (!isDynamic(e))
        return 0.0f;
    return glm::dot(axis, inverseInertiaOf(e, axis));
}

void wake(PhysicalEntity* e)
{
    if (!isDynamic(e))
        return;
    e->sleeping = false;
    e->sleepTicks = 0;
    e->sleepAnchorValid = false;
}

void syncTransform(PhysicalEntity& e)
{
    e.transform = glm::translate(glm::mat4(1.0f), glm::vec3(e.transform[3])) *
                  glm::mat4_cast(glm::normalize(e.orientation));
}

void makeTangents(const glm::vec3& axis, glm::vec3& t1, glm::vec3& t2)
{
    const glm::vec3 reference =
        std::fabs(axis.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                 : glm::vec3(1.0f, 0.0f, 0.0f);
    t1 = glm::normalize(glm::cross(axis, reference));
    t2 = glm::normalize(glm::cross(axis, t1));
}

void correctPoint(
    Constraint& constraint, PhysicalEntity* a, PhysicalEntity* b, float dt)
{
    const glm::vec3 anchorA =
        anchorWorld(a, constraint.localAnchorA);
    const glm::vec3 anchorB =
        anchorWorld(b, constraint.localAnchorB);
    const glm::vec3 error = anchorB - anchorA;
    const float errorLength = glm::length(error);
    if (errorLength <= kEpsilon)
        return;

    const glm::vec3 normal = error / errorLength;
    const glm::vec3 rA = anchorA - centerWorld(a);
    const glm::vec3 rB = anchorB - centerWorld(b);
    const glm::vec3 relativeVelocity =
        pointVelocity(a, anchorA) - pointVelocity(b, anchorB);
    const float effectiveMass =
        effectiveMassAlong(a, rA, normal) +
        effectiveMassAlong(b, rB, normal);
    if (effectiveMass <= kEpsilon)
        return;

    const float bias = std::clamp(
        kPositionBeta * errorLength / dt, -kMaxBiasSpeed, kMaxBiasSpeed);
    const float normalSpeed = glm::dot(relativeVelocity, normal);
    const float lambda = (bias - normalSpeed) / effectiveMass;
    applyLinearImpulse(a, normal * lambda, anchorA);
    applyLinearImpulse(b, -normal * lambda, anchorB);
}

void solvePoint(Constraint& constraint, PhysicalEntity* a, PhysicalEntity* b,
                float dt)
{
    for (int i = 0; i < kIterations; ++i)
        correctPoint(constraint, a, b, dt);
}

void alignAxes(Constraint& constraint, PhysicalEntity* a, PhysicalEntity* b,
               float dt)
{
    const glm::vec3 axisA = axisWorld(a, constraint.localAxisA);
    const glm::vec3 axisB = axisWorld(b, constraint.localAxisB);
    if (glm::dot(axisA, axisA) <= kEpsilon ||
        glm::dot(axisB, axisB) <= kEpsilon)
        return;

    glm::vec3 t1, t2;
    makeTangents(axisA, t1, t2);
    const glm::vec3 crossAB = glm::cross(axisA, axisB);

    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        for (const glm::vec3& tangent : {t1, t2})
        {
            const float effectiveMass =
                effectiveAngularMass(a, tangent) +
                effectiveAngularMass(b, tangent);
            if (effectiveMass <= kEpsilon)
                continue;

            glm::vec3 angularA(0.0f);
            glm::vec3 angularB(0.0f);
            if (isDynamic(a))
                angularA = a->angularVelocity;
            if (isDynamic(b))
                angularB = b->angularVelocity;
            const float relativeSpeed =
                glm::dot(angularA - angularB, tangent);
            const float angleError = glm::dot(crossAB, tangent);
            const float bias = std::clamp(
                kPositionBeta * angleError / dt, -kMaxBiasSpeed,
                kMaxBiasSpeed);
            const float lambda = (bias - relativeSpeed) / effectiveMass;
            applyAngularImpulse(a, tangent * lambda);
            applyAngularImpulse(b, -tangent * lambda);
        }
    }
}

void solveHingeMotor(Constraint& constraint, PhysicalEntity* a,
                     PhysicalEntity* b, float dt)
{
    if (!constraint.motorEnabled)
        return;

    const glm::vec3 axis = axisWorld(a, constraint.localAxisA);
    const float effectiveMass =
        effectiveAngularMass(a, axis) + effectiveAngularMass(b, axis);
    if (effectiveMass <= kEpsilon)
        return;

    glm::vec3 angularA(0.0f);
    glm::vec3 angularB(0.0f);
    if (isDynamic(a))
        angularA = a->angularVelocity;
    if (isDynamic(b))
        angularB = b->angularVelocity;
    const float relativeSpeed = glm::dot(angularA - angularB, axis);
    float lambda = (constraint.motorSpeed - relativeSpeed) / effectiveMass;
    if (constraint.motorMaxForce > 0.0f)
    {
        const float maxImpulse = constraint.motorMaxForce * dt;
        lambda = std::clamp(lambda, -maxImpulse, maxImpulse);
    }
    applyAngularImpulse(a, axis * lambda);
    applyAngularImpulse(b, -axis * lambda);
}

void solveSlider(Constraint& constraint, PhysicalEntity* a, PhysicalEntity* b,
                 float dt)
{
    const glm::vec3 axis = axisWorld(a, constraint.localAxisA);
    const glm::vec3 anchorA =
        anchorWorld(a, constraint.localAnchorA);
    const glm::vec3 anchorB =
        anchorWorld(b, constraint.localAnchorB);
    const glm::vec3 delta = anchorB - anchorA;
    const glm::vec3 rA = anchorA - centerWorld(a);
    const glm::vec3 rB = anchorB - centerWorld(b);

    glm::vec3 t1, t2;
    makeTangents(axis, t1, t2);

    // Lock the two axes perpendicular to the slider axis.
    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        for (const glm::vec3& tangent : {t1, t2})
        {
            const float error = glm::dot(delta, tangent);
            const float effectiveMass =
                effectiveMassAlong(a, rA, tangent) +
                effectiveMassAlong(b, rB, tangent);
            if (effectiveMass <= kEpsilon)
                continue;
            const glm::vec3 relativeVelocity =
                pointVelocity(a, anchorA) - pointVelocity(b, anchorB);
            const float bias = std::clamp(
                kPositionBeta * error / dt, -kMaxBiasSpeed, kMaxBiasSpeed);
            const float lambda =
                (bias - glm::dot(relativeVelocity, tangent)) / effectiveMass;
            applyLinearImpulse(a, tangent * lambda, anchorA);
            applyLinearImpulse(b, -tangent * lambda, anchorB);
        }
    }

    // Spring/damper along the free axis.
    const float length = glm::dot(delta, axis);
    const glm::vec3 relativeVelocity =
        pointVelocity(a, anchorA) - pointVelocity(b, anchorB);
    const float axisSpeed = glm::dot(relativeVelocity, axis);
    float impulse = 0.0f;
    if (constraint.springStiffness > 0.0f ||
        constraint.springDamping > 0.0f)
    {
        const float force = constraint.springStiffness *
                                (length - constraint.restLength) -
                            constraint.springDamping * axisSpeed;
        impulse = force * dt;
    }

    if (constraint.limitsEnabled)
    {
        if (length < constraint.minLimit)
        {
            const float error = constraint.minLimit - length;
            impulse += std::min(error / dt, kMaxBiasSpeed) *
                       (inverseMassOf(a) + inverseMassOf(b) > 0.0f
                            ? 1.0f / (inverseMassOf(a) + inverseMassOf(b))
                            : 0.0f);
        }
        else if (length > constraint.maxLimit)
        {
            const float error = constraint.maxLimit - length;
            impulse += std::max(error / dt, -kMaxBiasSpeed) *
                       (inverseMassOf(a) + inverseMassOf(b) > 0.0f
                            ? 1.0f / (inverseMassOf(a) + inverseMassOf(b))
                            : 0.0f);
        }
    }

    if (std::fabs(impulse) > kEpsilon)
    {
        applyLinearImpulse(a, axis * impulse, anchorA);
        applyLinearImpulse(b, -axis * impulse, anchorB);
    }

    alignAxes(constraint, a, b, dt);
}

} // namespace

ConstraintSystem& ConstraintSystem::instance()
{
    static ConstraintSystem system;
    return system;
}

uint32_t ConstraintSystem::add(const Constraint& constraint)
{
    Constraint stored = constraint;
    stored.id = mNextId++;
    mConstraints.push_back(stored);
    return stored.id;
}

bool ConstraintSystem::remove(uint32_t id)
{
    for (auto it = mConstraints.begin(); it != mConstraints.end(); ++it)
    {
        if (it->id == id)
        {
            mConstraints.erase(it);
            return true;
        }
    }
    return false;
}

void ConstraintSystem::clear()
{
    mConstraints.clear();
    mNextId = 1;
}

void ConstraintSystem::solve(float dt)
{
    if (dt <= 0.0f)
        return;

    PhysicalEntitySystem& entities = PhysicalEntitySystem::instance();
    for (Constraint& constraint : mConstraints)
    {
        if (!constraint.enabled)
            continue;

        PhysicalEntity* a = constraint.entityA != 0
                                ? entities.find(constraint.entityA)
                                : nullptr;
        PhysicalEntity* b = constraint.entityB != 0
                                ? entities.find(constraint.entityB)
                                : nullptr;
        if (constraint.entityA != 0 && !a)
            continue;
        if (constraint.entityB != 0 && !b)
            continue;
        if (!isDynamic(a) && !isDynamic(b))
            continue;

        wake(a);
        wake(b);

        switch (constraint.type)
        {
            case ConstraintType::Point:
                solvePoint(constraint, a, b, dt);
                break;
            case ConstraintType::Hinge:
                solvePoint(constraint, a, b, dt);
                alignAxes(constraint, a, b, dt);
                solveHingeMotor(constraint, a, b, dt);
                break;
            case ConstraintType::Slider:
                solveSlider(constraint, a, b, dt);
                break;
        }

        if (a && a->motion == PhysicalEntityMotion::Dynamic)
            syncTransform(*a);
        if (b && b->motion == PhysicalEntityMotion::Dynamic)
            syncTransform(*b);
    }
}
