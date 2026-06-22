// Body part + weapon world collision.
//
// Unified collision system: every body part capsule (head, torso, arms, legs)
// and the weapon capsule contribute to a single collision manifold.
// Contacts are gathered via sphere-sweep tests that catch movement-induced
// penetration before limbs tunnel. The batched solver resolves all contacts
// together, pushing the root position out. Velocity is dampened against
// contact normals to prevent re-penetration.
//
// Weapon capsule is recomputed from the right arm transform each collision
// frame (using stored local-space data from weapon-viewmodel.cpp), so the
// weapon capsule is always in the current-frame position — no latency.

#include <algorithm>
#include <cmath>

#include "physics/config.h"
#include "entities/player.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "physics/movement/physics-collision.h"

#include "physics/movement/physics-body-collision.h"

#define BODY_LOG(...) Debug::logThrottled(Debug::Category::Physics, "body-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

// =====================================================
// CONSTANTS
// =====================================================

constexpr float EPSILON                 = 0.001f;
constexpr int   SPHERES_PER_CAPSULE     = 5;
constexpr int   MAX_BODY_ITERATIONS     = 8;
constexpr float DEPEN_SLOP              = 0.005f;
constexpr float SLIDE_SLOP              = 0.002f;
constexpr float SWEEP_EPSILON           = 0.00001f;

// =====================================================
// GEOMETRY HELPERS
// =====================================================

static glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

static bool pointInTriangle(glm::vec3 p, const CollisionTriangle& tri)
{
    glm::vec3 closest = closestPointOnTriangle(p, tri.a, tri.b, tri.c);
    glm::vec3 delta = closest - p;
    return glm::dot(delta, delta) < 0.0001f;
}

// =====================================================
// BODY PART CAPSULE COMPUTATION
// =====================================================

static bool computeBodyPartCenter(
    const glm::mat4& xform,
    const Collider& collider,
    glm::vec3& outCenter,
    float& outRadius
) {
    glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
    glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;

    outCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
    outRadius = std::max({localExtents.x, localExtents.y, localExtents.z, BODY_SAMPLE_RADIUS});
    outRadius = std::min(outRadius, 0.35f);
    return true;
}

// =====================================================
// SWEEP SPHERE vs TRIANGLE (self-contained math)
// =====================================================

// Sweep sphere vs point
static bool sweepSpherePoint(
    glm::vec3 start, glm::vec3 move, float radius, glm::vec3 point,
    float& hitTime, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    float a = glm::dot(move, move);
    if (a < SWEEP_EPSILON) return false;

    glm::vec3 rel = start - point;
    float b = 2.0f * glm::dot(rel, move);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f) return false;

    glm::vec3 center = start + move * t;
    glm::vec3 n = center - point;
    if (glm::dot(n, n) < 0.000001f)
        n = -glm::normalize(move);
    else
        n = glm::normalize(n);

    hitTime = t;
    hitNormal = n;
    hitPoint = point;
    return true;
}

// Sweep sphere vs line segment (edge)
static bool sweepSphereEdge(
    glm::vec3 start, glm::vec3 move, float radius, glm::vec3 edgeA, glm::vec3 edgeB,
    float& hitTime, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    glm::vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f) return false;
    edgeDir /= edgeLen;

    glm::vec3 rel = start - edgeA;
    float proj = glm::dot(rel, edgeDir);
    glm::vec3 relPerp = rel - edgeDir * proj;
    glm::vec3 movePerp = move - edgeDir * glm::dot(move, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    if (a < SWEEP_EPSILON) return false;

    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f) return false;

    glm::vec3 centerAtT = start + move * t;
    glm::vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen) return false;

    glm::vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    glm::vec3 normal = centerAtT - closestOnEdge;
    float dist = glm::length(normal);
    if (dist < 0.000001f) return false;
    normal /= dist;

    hitTime = t;
    hitNormal = normal;
    hitPoint = closestOnEdge;
    return true;
}

// Sweep sphere vs triangle (face + 3 edges + 3 vertices)
static bool sweepSphereTriangle(
    glm::vec3 start, glm::vec3 move, float radius, const CollisionTriangle& tri,
    float& hitTime, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    float bestT = 1.0f;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);
    bool hit = false;

    // Face sweep
    glm::vec3 n = tri.normal;
    float dist = glm::dot(start - tri.a, n);
    if (dist < 0.0f) { n = -n; dist = -dist; }

    float denom = glm::dot(move, n);
    if (denom < -SWEEP_EPSILON)
    {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t <= 1.0f)
        {
            glm::vec3 centerAtHit = start + move * t;
            glm::vec3 planePoint = centerAtHit - n * radius;
            if (pointInTriangle(planePoint, tri))
            {
                bestT = t; bestN = n; bestP = planePoint; hit = true;
            }
        }
    }

    // Edge sweeps
    glm::vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
    for (auto& ep : edgePairs)
    {
        float t = 1.0f; glm::vec3 en(0.0f); glm::vec3 epPt(0.0f);
        if (sweepSphereEdge(start, move, radius, ep[0], ep[1], t, en, epPt) && t < bestT)
        {
            bestT = t; bestN = en; bestP = epPt; hit = true;
        }
    }

    // Vertex sweeps
    glm::vec3 pts[3] = {tri.a, tri.b, tri.c};
    for (glm::vec3 pt : pts)
    {
        float t = 1.0f; glm::vec3 pn(0.0f); glm::vec3 pp(0.0f);
        if (sweepSpherePoint(start, move, radius, pt, t, pn, pp) && t < bestT)
        {
            bestT = t; bestN = pn; bestP = pp; hit = true;
        }
    }

    if (!hit) return false;
    hitTime = bestT; hitNormal = bestN; hitPoint = bestP;
    return true;
}

// =====================================================
// STATIC SPHERE vs TRIANGLE
// =====================================================

static bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    glm::vec3& outNormal,
    float& outDepth
) {
    glm::vec3 closest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
    glm::vec3 delta = center - closest;
    float dist2 = glm::dot(delta, delta);

    if (dist2 > radius * radius)
        return false;

    float dist = sqrtf(std::max(dist2, 0.0f));
    if (dist > 0.0001f) {
        outNormal = delta / dist;
        float edgeSum = 0.0f;
        glm::vec3 edgeDirs[3] = {tri.b - tri.a, tri.c - tri.b, tri.a - tri.c};
        for (int ei = 0; ei < 3; ++ei) {
            float edgeLen = glm::length(edgeDirs[ei]);
            if (edgeLen < 0.0001f) continue;
            glm::vec3 edgeDir = edgeDirs[ei] / edgeLen;
            glm::vec3 toClosest = closest - (ei == 0 ? tri.a : (ei == 1 ? tri.b : tri.c));
            float alongEdge = glm::dot(toClosest, edgeDir);
            if (alongEdge >= 0.0f && alongEdge <= edgeLen)
                edgeSum += 1.0f;
        }
        if (edgeSum < 2.5f)
            outNormal = glm::normalize(outNormal + tri.normal * 0.3f);
    } else {
        float side = glm::dot(center - tri.a, tri.normal);
        outNormal = (side >= 0.0f) ? tri.normal : -tri.normal;
    }

    outDepth = radius - dist;
    return true;
}

// =====================================================
// LOCAL CONTACT STRUCT
// =====================================================

struct BodyContact {
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
    const char* label = "";
};

// =====================================================
// WORLD QUERY
// =====================================================

// =====================================================
// WEAPON CAPSULE RECOMPUTATION
// =====================================================
//
// Called at the start of doBodyCollision so the weapon capsule
// is in the current frame's world position (no one-frame latency).

static void recomputeWeaponCapsule(Player& p)
{
    if (!p.hasWeaponCollisionCapsule)
        return;

    for (const PhysicalBodyPart& part : p.physicalBody.parts)
    {
        if (part.name != "rightArm")
            continue;

        glm::mat4 weaponXform = part.worldTransform * p.weaponLocalToArm;
        p.weaponCollisionCapsule.a = glm::vec3(weaponXform * glm::vec4(p.weaponGripLocal, 1.0f));
        p.weaponCollisionCapsule.b = glm::vec3(weaponXform * glm::vec4(p.weaponMuzzleLocal, 1.0f));
        p.weaponCollisionCapsule.r = p.weaponRadiusLocal;
        return;
    }

    // Right arm not found — disable weapon collision
    p.hasWeaponCollisionCapsule = false;
}

// =====================================================
// LOCAL BATCHED SOLVER (Gauss-Seidel)
// =====================================================

static glm::vec3 solveContactsLocal(
    const std::vector<BodyContact>& contacts,
    float slop,
    float* outMaxPenetration
) {
    constexpr int NUM_PASSES = 6;
    constexpr float RELAXATION = 0.8f;

    // Sort by penetration depth (deepest first)
    std::vector<const BodyContact*> sorted;
    sorted.reserve(contacts.size());
    for (const auto& c : contacts) sorted.push_back(&c);
    std::sort(sorted.begin(), sorted.end(),
        [](const BodyContact* a, const BodyContact* b) {
            return a->penetration > b->penetration;
        });

    // Merge similar normals (dot > 0.95)
    std::vector<glm::vec3> normals;
    std::vector<float> depths;
    for (const BodyContact* c : sorted) {
        bool merged = false;
        for (size_t i = 0; i < normals.size(); ++i) {
            if (glm::dot(normals[i], c->normal) > 0.95f) {
                depths[i] = std::max(depths[i], c->penetration);
                merged = true;
                break;
            }
        }
        if (!merged) {
            normals.push_back(c->normal);
            depths.push_back(c->penetration);
        }
    }

    glm::vec3 correction(0.0f);
    float maxPen = 0.0f;

    for (int pass = 0; pass < NUM_PASSES; ++pass) {
        for (size_t i = 0; i < normals.size(); ++i) {
            float pen = depths[i];
            if (pen <= slop) continue;
            maxPen = std::max(maxPen, pen);

            // Current separation along this normal
            float currSep = glm::dot(correction, normals[i]);
            float needed = pen + slop - currSep;
            if (needed <= 0.0f) continue;

            glm::vec3 push = normals[i] * needed * RELAXATION;
            correction += push;
        }
    }

    if (outMaxPenetration) *outMaxPenetration = maxPen;
    return correction;
}

// =====================================================
// UNIFIED BODY + WEAPON COLLISION — SWEEP-BASED
// =====================================================

void doBodyCollision(
    Player& p,
    const World& world,
    float dt
) {
    if (world.collisionMesh.empty()) {
        p.debugBodyCollisionPush = glm::vec3(0.0f);
        p.debugWeaponCollisionPush = glm::vec3(0.0f);
        return;
    }

    // Recompute weapon capsule from current right-arm transform
    // so both body AND weapon use this frame's positions.
    recomputeWeaponCapsule(p);

    glm::vec3 moveVec = p.vel * dt;
    float moveLen = glm::length(moveVec);

    glm::vec3 totalCorrection(0.0f);

    for (int iter = 0; iter < MAX_BODY_ITERATIONS; ++iter)
    {
        std::vector<BodyContact> allContacts;

        AABB queryBounds;
        bool queryBoundsSet = false;

        struct BodySphere {
            glm::vec3 center;
            float radius;
            const char* label;
            glm::vec3 sweepDelta;
        };
        std::vector<BodySphere> spheres;
        spheres.reserve(p.physicalBody.parts.size() * SPHERES_PER_CAPSULE + SPHERES_PER_CAPSULE);

        // --------------------------------------------------
        // 1. Body part spheres
        // --------------------------------------------------
        for (const PhysicalBodyPart& part : p.physicalBody.parts)
        {
            glm::vec3 center;
            float radius;
            if (!computeBodyPartCenter(part.worldTransform, part.collider, center, radius))
                continue;

            glm::vec3 prevCenter;
            computeBodyPartCenter(part.previousWorldTransform, part.collider, prevCenter, radius);
            glm::vec3 sweepDelta = moveVec + (center - prevCenter);

            for (int si = 0; si < SPHERES_PER_CAPSULE; si++)
            {
                spheres.push_back({center, radius, part.name.c_str(), sweepDelta});

                glm::vec3 queryExpand(radius + std::abs(sweepDelta.z) + moveLen + 0.5f);
                if (!queryBoundsSet) {
                    queryBounds.min = center - queryExpand;
                    queryBounds.max = center + queryExpand;
                    queryBoundsSet = true;
                } else {
                    queryBounds.min = glm::min(queryBounds.min, center - queryExpand);
                    queryBounds.max = glm::max(queryBounds.max, center + queryExpand);
                }
            }
        }

        // --------------------------------------------------
        // 2. Weapon capsule spheres
        // --------------------------------------------------
        if (p.hasWeaponCollisionCapsule)
        {
            const Capsule& wc = p.weaponCollisionCapsule;

            for (int si = 0; si < SPHERES_PER_CAPSULE; si++)
            {
                float t = (SPHERES_PER_CAPSULE > 1)
                    ? (float)si / (float)(SPHERES_PER_CAPSULE - 1) : 0.5f;
                glm::vec3 spherePos = wc.a + (wc.b - wc.a) * t;
                spheres.push_back({spherePos, wc.r, "weapon", moveVec});

                glm::vec3 queryExpand(wc.r + moveLen + 0.5f);
                if (!queryBoundsSet) {
                    queryBounds.min = spherePos - queryExpand;
                    queryBounds.max = spherePos + queryExpand;
                    queryBoundsSet = true;
                } else {
                    queryBounds.min = glm::min(queryBounds.min, spherePos - queryExpand);
                    queryBounds.max = glm::max(queryBounds.max, spherePos + queryExpand);
                }
            }
        }

        if (!queryBoundsSet)
            break;

        std::vector<int> worldTris;
        appendChunkTrianglesForAABB(world, queryBounds, 0.5f, worldTris);

        // --------------------------------------------------
        // 3. Sweep + static contacts
        // --------------------------------------------------
        for (const BodySphere& bs : spheres)
        {
            for (int triIdx : worldTris)
            {
                if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size())
                    continue;

                const CollisionTriangle& tri = world.collisionMesh.triangles[triIdx];

                float hitTime = 1.0f;
                glm::vec3 hitNormal, hitPoint;
                if (sweepSphereTriangle(bs.center, bs.sweepDelta, bs.radius, tri,
                                        hitTime, hitNormal, hitPoint) && hitTime < 1.0f)
                {
                    float depth = bs.radius - glm::dot(bs.center - hitPoint, hitNormal);
                    if (depth > SLIDE_SLOP) {
                        allContacts.push_back({hitNormal, hitPoint, depth, triIdx, bs.label});
                        continue;
                    }
                }

                glm::vec3 normal;
                float depth;
                if (sphereTriangleContact(bs.center, bs.radius, tri, normal, depth)
                    && depth > SLIDE_SLOP)
                {
                    allContacts.push_back({normal, bs.center, depth, triIdx, bs.label});
                }
            }
        }

        if (allContacts.empty())
            break;

        // --------------------------------------------------
        // 4. Batched solver
        // --------------------------------------------------
        float maxPen = 0.0f;
        glm::vec3 correction = solveContactsLocal(allContacts, DEPEN_SLOP, &maxPen);

        float corrLen = glm::length(correction);
        if (corrLen < EPSILON)
            break;

        // --------------------------------------------------
        // 5. Velocity dampening
        // --------------------------------------------------
        {
            glm::vec3 velXY(p.vel.x, p.vel.y, 0.0f);
            float velLen = glm::length(velXY);
            if (velLen > 0.001f) {
                glm::vec3 moveDir = velXY / velLen;
                for (const BodyContact& c : allContacts) {
                    float dot = glm::dot(moveDir, c.normal);
                    if (dot < -0.01f) {
                        glm::vec3 pushComp = c.normal * dot * velLen * 0.5f;
                        p.vel -= pushComp;
                        p.externalImpulse -= pushComp;
                    }
                }
            }
        }

        // --------------------------------------------------
        // 6. Apply correction to root
        // --------------------------------------------------
        p.pos += correction;
        totalCorrection += correction;
        p.syncLegacyStateToLayers();
        p.updateModelWorldTransforms();

        DebugVis::recordDepenetration(p.pos - correction, correction, "body-weapon-depen");

        if (DebugConfig::DEBUG_COLLISION_LIMB)
        {
            BODY_LOG("[BODY ITER %d] contacts=%zu maxPen=%.4f corr=(%.4f %.4f %.4f) |corr|=%.4f\n",
                     iter, allContacts.size(), maxPen,
                     correction.x, correction.y, correction.z, corrLen);
            for (const BodyContact& c : allContacts)
                DebugVis::recordContact(c.point, c.normal, c.penetration, c.triangleIndex, c.label);
        }
    }

    // Store debug values (body + weapon combined)
    p.debugBodyCollisionPush = totalCorrection;
    p.debugWeaponCollisionPush = totalCorrection; // combined now

    if (DebugConfig::DEBUG_COLLISION_LIMB && glm::length(totalCorrection) > 0.001f)
        BODY_LOG("[BODY] total corr=(%.4f %.4f %.4f) |corr|=%.4f\n",
                 totalCorrection.x, totalCorrection.y, totalCorrection.z,
                 glm::length(totalCorrection));
}
