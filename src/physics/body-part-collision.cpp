#include "body-part-collision.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"

namespace {

struct BodyAABB { glm::vec3 min, max; };

static BodyAABB makeTriAABB(const CollisionTriangle& tri) {
    BodyAABB b;
    b.min = glm::min(tri.a, glm::min(tri.b, tri.c));
    b.max = glm::max(tri.a, glm::max(tri.b, tri.c));
    return b;
}

static bool overlaps(const BodyAABB& a, const BodyAABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

static void gatherCandidates(const World& world, const BodyAABB& capBounds,
                             float capRadius, std::vector<int>& out) {
    const auto& tris = world.collisionMesh.triangles;
    BodyAABB query = capBounds;
    query.min -= glm::vec3(capRadius);
    query.max += glm::vec3(capRadius);
    for (int i = 0; i < (int)tris.size(); ++i) {
        BodyAABB tb = makeTriAABB(tris[i]);
        if (overlaps(query, tb)) out.push_back(i);
    }
}

static bool bodyPartWorldCapsule(const Player& p, const Collider& collider, Capsule& out) {
    auto it = std::find_if(p.nodes.begin(), p.nodes.end(), [&](const TransformNode& node) {
        return node.name == collider.name;
    });
    if (it == p.nodes.end()) return false;

    const glm::mat4& xform = it->worldTransform;
    glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
    glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;
    float axisLen = glm::length(localExtents);
    if (axisLen < 0.001f) return false;

    glm::vec3 axisDir = localExtents / axisLen;
    float radius = std::min(localExtents.x, localExtents.y) * 1.5f;
    radius = std::max(radius, 0.08f);
    radius = std::min(radius, 0.35f);

    glm::vec3 worldCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
    glm::vec3 worldA = worldCenter - glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
    glm::vec3 worldB = worldCenter + glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));

    out.a = worldA;
    out.b = worldB;
    out.r = radius;
    return true;
}

static bool sphereTriContact(glm::vec3 center, float radius,
                             const CollisionTriangle& tri, Contact& contact) {
    glm::vec3 ab = tri.b - tri.a, ac = tri.c - tri.a, ap = center - tri.a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    glm::vec3 bp = center - tri.b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    glm::vec3 cp = center - tri.c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    float vc = d1 * d4 - d3 * d2;
    float vb = d5 * d2 - d1 * d6;
    float va = d3 * d6 - d5 * d4;

    glm::vec3 closest;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        closest = tri.a + (d1 / (d1 - d3)) * ab;
    else if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        closest = tri.a + (d2 / (d2 - d6)) * ac;
    else if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        closest = tri.b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (tri.c - tri.b);
    else {
        float denom = 1.0f / (va + vb + vc);
        closest = tri.a + (vb * denom) * ab + (vc * denom) * ac;
    }

    glm::vec3 diff = center - closest;
    float dist2 = glm::dot(diff, diff);
    if (dist2 >= radius * radius) return false;
    float dist = std::sqrt(dist2);
    if (dist < 0.000001f) {
        contact.point = closest;
        contact.normal = tri.normal;
        contact.penetration = radius;
    } else {
        contact.point = closest;
        contact.normal = diff / dist;
        contact.penetration = radius - dist;
    }
    return true;
}

static void projectRootVelocity(Player& p, const glm::vec3& normal) {
    float intoVel = glm::dot(p.vel, normal);
    if (intoVel < 0.0f)
        p.vel -= normal * intoVel;

    float intoImp = glm::dot(p.externalImpulse, normal);
    if (intoImp < 0.0f)
        p.externalImpulse -= normal * intoImp;
}

} // namespace

void resolveBodyPartCollisions(Player& p, const World& world, float dt) {
    (void)dt;
    if (world.collisionMesh.triangles.empty()) return;
    if (p.bodyColliders.empty()) return;

    p.updateModelWorldTransforms();

    bool bodyGrounded = false;

    for (size_t ci = 0; ci < p.bodyColliders.size(); ++ci) {
        const Collider& collider = p.bodyColliders[ci];
        Capsule bodyCap;
        if (!bodyPartWorldCapsule(p, collider, bodyCap)) continue;

        int partIndex = -1;
        for (size_t pi = 0; pi < p.physicalBody.parts.size(); ++pi) {
            if (p.physicalBody.parts[pi].name == collider.name) {
                partIndex = (int)pi;
                break;
            }
        }
        if (partIndex < 0) continue;

        glm::vec3 capHalf = (bodyCap.b - bodyCap.a) * 0.5f;
        glm::vec3 capCenter = (bodyCap.a + bodyCap.b) * 0.5f;
        BodyAABB capBounds = { capCenter - glm::abs(capHalf), capCenter + glm::abs(capHalf) };
        std::vector<int> candidates;
        gatherCandidates(world, capBounds, bodyCap.r, candidates);

        float maxPen = 0.0f;
        glm::vec3 avgNormal(0.0f);
        int penCount = 0;

        for (int ti : candidates) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            for (int s = 0; s < 3; ++s) {
                float st = (float)s / 2.0f;
                glm::vec3 sp = bodyCap.a + (bodyCap.b - bodyCap.a) * st;
                Contact ct;
                if (sphereTriContact(sp, bodyCap.r, tri, ct)) {
                    if (ct.penetration > maxPen) { maxPen = ct.penetration; avgNormal = ct.normal; }
                    penCount++;
                    std::string label = collider.name + "_contact";
                    DebugVis::recordContact(ct.point, ct.normal, ct.penetration, ti, label.c_str());
                }
            }
        }

        if (maxPen > 0.005f && penCount > 0) {
            glm::vec3 correction = avgNormal * maxPen;
            glm::vec3 rootBefore = p.pos;

            p.pos += correction;
            projectRootVelocity(p, avgNormal);

            if (avgNormal.z > 0.70f) {
                bodyGrounded = true;
                if (p.vel.z < 0.0f)
                    p.vel.z = 0.0f;
                if (p.externalImpulse.z < 0.0f)
                    p.externalImpulse.z = 0.0f;
            }

            p.updateModelWorldTransforms();

            Debug::logThrottled(Debug::Category::Collision, "body-root-response", 0.25f,
                "[BODY COLLISION] part=%s penetration=%.4f correction=(%.4f %.4f %.4f) hits=%d\n"
                "[ROOT RESPONSE] rootCorrection=(%.4f %.4f %.4f) pos=(%.4f %.4f %.4f)\n",
                collider.name.c_str(), maxPen,
                correction.x, correction.y, correction.z, penCount,
                correction.x, correction.y, correction.z,
                p.pos.x, p.pos.y, p.pos.z);

            std::string depLabel = collider.name + "_push";
            DebugVis::recordDepenetration(rootBefore, correction, depLabel.c_str());
        }
    }

    if (bodyGrounded) {
        p.onGround = true;
        p.stableOnGround = true;
        p.groundLostTimer = 0.0f;
        p.airborneTimer = 0.0f;
    }

    // Sync render meshes after root-authoritative collision response.
    p.updateModelWorldTransforms();
    p.bodyPartMeshes = p.physicalBody.partMeshes;
}
