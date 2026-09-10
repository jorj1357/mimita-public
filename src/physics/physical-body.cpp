// 2026-09-10
/* purpose
* Implements the reusable rigid-body, joint, and collision primitives declared
* in physical-body.h.
* Uses the existing exact capsule/triangle sweep and contact helpers so ragdoll
* physics shares the same world collision meaning as the rest of the engine.
* Does NOT render, send packets, read input, or apply gameplay damage.
*/

#include "physics/physical-body.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtx/quaternion.hpp>

#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "world/world.h"

namespace {

void clampSpeed(glm::vec3& v, float maxSpeed)
{
    if (maxSpeed <= 0.0f) return;
    float speed = glm::length(v);
    if (speed > maxSpeed) v *= maxSpeed / speed;
}

glm::vec3 rotateVector(const glm::quat& q, const glm::vec3& v)
{
    return q * v;
}

// Closest points between segments [p1,q1] and [p2,q2].
void closestSegmentSegment(const glm::vec3& p1, const glm::vec3& q1,
                           const glm::vec3& p2, const glm::vec3& q2,
                           glm::vec3& c1, glm::vec3& c2)
{
    glm::vec3 d1 = q1 - p1;
    glm::vec3 d2 = q2 - p2;
    glm::vec3 r = p1 - p2;
    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1e-12f && e <= 1e-12f) {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a <= 1e-12f) {
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(d1, r);
        if (e <= 1e-12f) {
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;
            if (denom != 0.0f)
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

} // namespace

Capsule capsuleOf(const RigidBody& body)
{
    glm::vec3 axis = rotateVector(body.orientation, body.localAxis) * body.capsuleHalfHeight;
    Capsule cap;
    cap.a = body.position - axis;
    cap.b = body.position + axis;
    cap.r = body.capsuleRadius;
    return cap;
}

void setBodyMass(RigidBody& body, float mass)
{
    if (mass < 0.0001f) mass = 0.0001f;
    body.mass = mass;
    body.invMass = body.staticBody ? 0.0f : 1.0f / mass;

    // Isotropic capsule inertia about the center, dominated by the long axis so
    // limbs tip and swing instead of spinning like a point.
    float h = body.capsuleHalfHeight;
    float r = body.capsuleRadius;
    body.inertia = mass * (1.33f * h * h + 0.4f * r * r);
    if (body.inertia < 1e-6f) body.inertia = 1e-6f;
    body.invInertia = body.staticBody ? 0.0f : 1.0f / body.inertia;
}

glm::vec3 pointVelocity(const RigidBody& body, const glm::vec3& worldPoint)
{
    glm::vec3 r = worldPoint - body.position;
    return body.linearVelocity + glm::cross(body.angularVelocity, r);
}

float inverseMassAlong(const RigidBody& body, const glm::vec3& r, const glm::vec3& n)
{
    if (body.invMass <= 0.0f && body.invInertia <= 0.0f) return 0.0f;
    glm::vec3 rx = glm::cross(r, n);
    return body.invMass + body.invInertia * glm::dot(rx, rx);
}

void integrate(RigidBody& body, const glm::vec3& gravity, float dt)
{
    if (body.staticBody) return;

    body.linearVelocity += gravity * dt;
    body.linearVelocity *= std::max(0.0f, 1.0f - body.linearDamping * dt);
    body.angularVelocity *= std::max(0.0f, 1.0f - body.angularDamping * dt);

    clampSpeed(body.linearVelocity, body.maxLinearSpeed);
    clampSpeed(body.angularVelocity, body.maxAngularSpeed);

    body.position += body.linearVelocity * dt;

    float w = glm::length(body.angularVelocity);
    if (w > 1e-5f) {
        glm::quat delta = glm::angleAxis(w * dt, body.angularVelocity / w);
        body.orientation = glm::normalize(delta * body.orientation);
    }
}

void applyImpulseAtPoint(RigidBody& body, const glm::vec3& impulse, const glm::vec3& worldPoint)
{
    if (body.staticBody) return;
    glm::vec3 r = worldPoint - body.position;
    body.linearVelocity += impulse * body.invMass;
    body.angularVelocity += glm::cross(r, impulse) * body.invInertia;
}

void rotateBody(RigidBody& body, const glm::vec3& deltaTheta)
{
    if (body.staticBody) return;
    float w = glm::length(deltaTheta);
    if (w > 1e-8f) {
        glm::quat delta = glm::angleAxis(w, deltaTheta / w);
        body.orientation = glm::normalize(delta * body.orientation);
    }
}

void solvePointJointVelocity(RigidBody& a, const glm::vec3& anchorA,
                             RigidBody& b, const glm::vec3& anchorB)
{
    glm::vec3 err = anchorB - anchorA;
    float len = glm::length(err);
    if (len < 1e-6f) return;
    glm::vec3 n = err / len;

    glm::vec3 rA = anchorA - a.position;
    glm::vec3 rB = anchorB - b.position;
    glm::vec3 vrel = pointVelocity(b, anchorB) - pointVelocity(a, anchorA);

    float k = inverseMassAlong(a, rA, n) + inverseMassAlong(b, rB, n);
    if (k < 1e-8f) return;

    float j = -glm::dot(vrel, n) / k;
    glm::vec3 P = n * j;
    applyImpulseAtPoint(a, -P, anchorA);
    applyImpulseAtPoint(b, P, anchorB);
}

void solvePointJointPosition(RigidBody& a, const glm::vec3& anchorA,
                             RigidBody& b, const glm::vec3& anchorB,
                             float beta)
{
    glm::vec3 err = anchorB - anchorA;
    float len = glm::length(err);
    if (len < 1e-6f) return;
    glm::vec3 n = err / len;

    glm::vec3 rA = anchorA - a.position;
    glm::vec3 rB = anchorB - b.position;

    float k = inverseMassAlong(a, rA, n) + inverseMassAlong(b, rB, n);
    if (k < 1e-8f) return;

    glm::vec3 P = n * (len * beta / k);
    a.position += P * a.invMass;
    b.position -= P * b.invMass;
    rotateBody(a, glm::cross(rA, P) * a.invInertia);
    rotateBody(b, -glm::cross(rB, P) * b.invInertia);
}

void solvePointToWorld(RigidBody& body, const glm::vec3& bodyAnchor,
                       const glm::vec3& worldPoint, float beta)
{
    glm::vec3 err = worldPoint - bodyAnchor;
    float len = glm::length(err);
    if (len < 1e-6f) return;
    glm::vec3 n = err / len;

    glm::vec3 r = bodyAnchor - body.position;
    glm::vec3 rx = glm::cross(r, n);
    float k = body.invMass + body.invInertia * glm::dot(rx, rx);
    if (k < 1e-8f) return;

    glm::vec3 P = n * (len * beta / k);
    body.position += P * body.invMass;
    rotateBody(body, glm::cross(r, P) * body.invInertia);
}

bool collideWithWorld(RigidBody& body, const World& world, float dt)
{
    if (body.staticBody) return false;
    if (world.collisionMesh.triangles.empty()) return false;

    bool contacted = false;

    float speed = glm::length(body.linearVelocity);
    float maxStep = std::max(body.capsuleRadius * 0.5f, 0.08f);
    int steps = std::max(1, std::min(8, (int)std::ceil(speed * dt / maxStep)));
    glm::vec3 stepMove = body.linearVelocity * (dt / (float)steps);

    static thread_local std::vector<int> candidates;
    candidates.clear();

    for (int s = 0; s < steps; ++s) {
        Capsule cap = capsuleOf(body);

        AABB bounds;
        bounds.min = glm::min(glm::min(cap.a, cap.b),
                              glm::min(cap.a + stepMove, cap.b + stepMove));
        bounds.max = glm::max(glm::max(cap.a, cap.b),
                              glm::max(cap.a + stepMove, cap.b + stepMove));
        bounds.min -= glm::vec3(cap.r + 0.25f);
        bounds.max += glm::vec3(cap.r + 0.25f);

        candidates.clear();
        appendChunkTrianglesForAABB(world, bounds, 0.1f, candidates,
                                    "ragdollModePhysicsCollision");

        float bestT = 1.0f;
        int bestIdx = -1;
        glm::vec3 bestNormal(0.0f);
        glm::vec3 bestPoint(0.0f);

        for (int ti : candidates) {
            if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            SweepHit hit;
            if (capsuleTriangleSweep(cap, stepMove, tri, ti, hit) && hit.hit) {
                if (hit.time < bestT) {
                    bestT = hit.time;
                    bestIdx = ti;
                    bestNormal = hit.normal;
                    bestPoint = hit.point;
                }
            }
        }

        if (bestIdx >= 0 && bestT <= 1.0f) {
            body.position += stepMove * bestT;
            cap = capsuleOf(body);

            const CollisionTriangle& tri = world.collisionMesh.triangles[bestIdx];
            Contact contact;
            if (capsuleTriangleContact(cap, tri, bestIdx, contact) && contact.penetration > 0.0f) {
                body.position += contact.normal * contact.penetration;
                bestNormal = contact.normal;
                bestPoint = contact.point;
            }

            glm::vec3 r = bestPoint - body.position;
            glm::vec3 v = pointVelocity(body, bestPoint);
            float vn = glm::dot(v, bestNormal);
            if (vn < 0.0f) {
                float k = inverseMassAlong(body, r, bestNormal);
                if (k > 1e-8f) {
                    float jn = -(1.0f + body.restitution) * vn / k;
                    glm::vec3 P = bestNormal * jn;
                    applyImpulseAtPoint(body, P, bestPoint);

                    glm::vec3 vt = v - bestNormal * vn;
                    float vtLen = glm::length(vt);
                    if (vtLen > 1e-5f) {
                        glm::vec3 t = vt / vtLen;
                        float kt = inverseMassAlong(body, r, t);
                        if (kt > 1e-8f) {
                            float jt = glm::clamp(-vtLen / kt, -body.friction * jn, body.friction * jn);
                            applyImpulseAtPoint(body, t * jt, bestPoint);
                        }
                    }
                }
            }
            contacted = true;
        } else {
            body.position += stepMove;
        }
    }

    return contacted;
}

bool collideBodies(RigidBody& a, RigidBody& b)
{
    Capsule ca = capsuleOf(a);
    Capsule cb = capsuleOf(b);

    glm::vec3 pa, pb;
    closestSegmentSegment(ca.a, ca.b, cb.a, cb.b, pa, pb);

    glm::vec3 diff = pb - pa;
    float dist = glm::length(diff);
    float radiusSum = ca.r + cb.r;
    if (dist >= radiusSum) return false;

    glm::vec3 n;
    if (dist > 1e-6f) {
        n = diff / dist;
    } else {
        n = b.position - a.position;
        float nl = glm::length(n);
        n = (nl > 1e-6f) ? n / nl : glm::vec3(0.0f, 0.0f, 1.0f);
    }

    float penetration = radiusSum - dist;
    float totalInvMass = a.invMass + b.invMass;

    // Positional separation, mass weighted.
    if (totalInvMass > 1e-8f) {
        float correction = penetration * 0.8f;
        a.position -= n * (correction * a.invMass / totalInvMass);
        b.position += n * (correction * b.invMass / totalInvMass);
    }

    glm::vec3 ra = pa - a.position;
    glm::vec3 rb = pb - b.position;
    glm::vec3 vrel = pointVelocity(b, pb) - pointVelocity(a, pa);
    float vn = glm::dot(vrel, n);
    if (vn < 0.0f) {
        float k = inverseMassAlong(a, ra, n) + inverseMassAlong(b, rb, n);
        if (k > 1e-8f) {
            float j = -(1.0f + std::min(a.restitution, b.restitution)) * vn / k;
            glm::vec3 P = n * j;
            applyImpulseAtPoint(a, -P, pa);
            applyImpulseAtPoint(b, P, pb);
        }
    }

    return true;
}
