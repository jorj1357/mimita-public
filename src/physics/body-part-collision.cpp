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

struct AABB {
    glm::vec3 min, max;
};

static AABB makeTriAABB(const CollisionTriangle& tri) {
    AABB b;
    b.min = glm::min(tri.a, glm::min(tri.b, tri.c));
    b.max = glm::max(tri.a, glm::max(tri.b, tri.c));
    return b;
}

static bool overlaps(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

// Gather candidate triangles by testing all world mesh triangles against capsule AABB.
// This is a simplified version of appendChunkTrianglesForAABB that doesn't require
// internal functions from physics-collision.cpp.
static void gatherCandidates(const World& world, const AABB& capsuleBounds,
                             float capsuleRadius, std::vector<int>& out) {
    const auto& tris = world.collisionMesh.triangles;
    AABB queryBounds = capsuleBounds;
    queryBounds.min -= glm::vec3(capsuleRadius);
    queryBounds.max += glm::vec3(capsuleRadius);

    for (int i = 0; i < (int)tris.size(); ++i) {
        AABB tb = makeTriAABB(tris[i]);
        if (overlaps(queryBounds, tb))
            out.push_back(i);
    }
}

// Build a world-space capsule for a body part from its collider bounds.
bool bodyPartWorldCapsule(const Player& p, const Collider& collider, Capsule& out) {
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

// Sphere-triangle contact test
static bool sphereTriContact(glm::vec3 center, float radius,
                             const CollisionTriangle& tri, Contact& contact) {
    // Closest point on triangle
    glm::vec3 ab = tri.b - tri.a, ac = tri.c - tri.a, ap = center - tri.a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) { /* region A */ }
    glm::vec3 bp = center - tri.b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) { /* region B */ }
    glm::vec3 cp = center - tri.c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) { /* region C */ }

    float vc = d1 * d4 - d3 * d2;
    float vb = d5 * d2 - d1 * d6;
    float va = d3 * d6 - d5 * d4;

    glm::vec3 closest;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        closest = tri.a + v * ab;
    } else if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float v = d2 / (d2 - d6);
        closest = tri.a + v * ac;
    } else if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        closest = tri.b + v * (tri.c - tri.b);
    } else {
        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom, w = vc * denom;
        closest = tri.a + v * ab + w * ac;
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

} // namespace

// ============================================================
// PUBLIC
// ============================================================

void resolveBodyPartCollisions(Player& p, const World& world, float dt) {
    if (world.collisionMesh.triangles.empty()) return;
    if (p.bodyColliders.empty()) return;

    // Recompute body part transforms at final position after capsule collision
    p.updateModelWorldTransforms();

    static float logTimer = 0.0f;
    logTimer += dt;

    for (size_t ci = 0; ci < p.bodyColliders.size(); ++ci) {
        const Collider& collider = p.bodyColliders[ci];
        Capsule bodyCap;
        if (!bodyPartWorldCapsule(p, collider, bodyCap)) continue;

        // Find the body part index for adjustment
        int partIndex = -1;
        for (size_t pi = 0; pi < p.physicalBody.parts.size(); ++pi) {
            if (p.physicalBody.parts[pi].name == collider.name) {
                partIndex = (int)pi;
                break;
            }
        }
        if (partIndex < 0) continue;

        // Gather candidate triangles
        glm::vec3 capHalf = (bodyCap.b - bodyCap.a) * 0.5f;
        glm::vec3 capCenter = (bodyCap.a + bodyCap.b) * 0.5f;
        AABB capBounds = { capCenter - glm::abs(capHalf), capCenter + glm::abs(capHalf) };
        std::vector<int> candidates;
        gatherCandidates(world, capBounds, bodyCap.r, candidates);

        // Check each candidate triangle for sphere contact along capsule axis
        float maxPen = 0.0f;
        glm::vec3 avgNormal(0.0f);
        int penCount = 0;

        for (int ti : candidates) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            // Sample 3 spheres along the capsule
            for (int s = 0; s < 3; ++s) {
                float st = (float)s / 2.0f;
                glm::vec3 sp = bodyCap.a + (bodyCap.b - bodyCap.a) * st;
                Contact ct;
                if (sphereTriContact(sp, bodyCap.r, tri, ct)) {
                    if (ct.penetration > maxPen) {
                        maxPen = ct.penetration;
                        avgNormal = ct.normal;
                    }
                    penCount++;
                    std::string label = collider.name + "_contact";
                    DebugVis::recordContact(ct.point, ct.normal, ct.penetration, ti, label.c_str());
                }
            }
        }

        // Apply push-back if penetrating
        if (maxPen > 0.001f && penCount > 0) {
            glm::vec3 correction = avgNormal * maxPen * 1.0f;

            PhysicalBodyPart& part = p.physicalBody.parts[partIndex];
            part.worldTransform[3][0] += correction.x;
            part.worldTransform[3][1] += correction.y;
            part.worldTransform[3][2] += correction.z;

            // Sync to legacy node transform
            if (part.nodeIndex >= 0 && part.nodeIndex < (int)p.nodes.size()) {
                p.nodes[part.nodeIndex].worldTransform[3][0] += correction.x;
                p.nodes[part.nodeIndex].worldTransform[3][1] += correction.y;
                p.nodes[part.nodeIndex].worldTransform[3][2] += correction.z;
            }

            if (logTimer >= 1.0f) {
                Debug::log(Debug::Category::Collision,
                    "[BODY PART] %s corrected: pen=%.4f norm=(%.2f %.2f %.2f) hits=%d\n",
                    collider.name.c_str(), maxPen, avgNormal.x, avgNormal.y, avgNormal.z, penCount);
            }
            std::string depLabel = collider.name + "_push";
            DebugVis::recordDepenetration(part.worldTransform[3], correction, depLabel.c_str());
        }
    }

    if (logTimer >= 1.0f) logTimer = 0.0f;

    // Sync corrected body part meshes to renderer
    p.bodyPartMeshes = p.physicalBody.partMeshes;
}
