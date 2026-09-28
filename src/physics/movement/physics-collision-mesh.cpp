// 09 27 2026
/* purpose
* Body-part collision from the part's REAL mesh triangles (triangle-vs-triangle
* against world collision triangles), replacing the one-sphere-per-part
* approximation for limbs. "What you see is what the hitbox is": the collider
* triangles loaded from the model are transformed by the part world transform
* (and the previous transform for sweep) and tested against world triangles.
* First version is deliberately brute-force inside each part's broadphase AABB;
* it is intended to be optimized later (decimation, tighter broadphase, caching).
* Does NOT own response, reset formulas, networking, rendering, audio, or weapon
* behavior. Does NOT change root capsule, sweep-slide, safety, or block phases.
*/
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/physics-types.h"
#include "physics/config.h"
#include "config/collision-config.h"
#include "entities/player.h"
#include "world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

namespace {

// Per-part / per-call budgets so a pathological model cannot freeze a frame.
constexpr int kMaxContactsPerPart = 64;
constexpr int kMaxTriangleTests = 200000;
// Movement-only rounded feature radius. This is geometry policy, not a JSON
// skin: triangle hitboxes remain exact, while the movement shell treats each
// vertex as a sphere and each edge as a capsule.

struct RoundedFeaturePair
{
    float distance = std::numeric_limits<float>::max();
    glm::vec3 actorPoint{0.0f};
    glm::vec3 worldPoint{0.0f};
};

bool pointInTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b,
                     const glm::vec3& c)
{
    const glm::vec3 v0 = b - a;
    const glm::vec3 v1 = c - a;
    const glm::vec3 v2 = p - a;
    const glm::vec3 planeNormal = glm::cross(v0, v1);
    const float planeLength = glm::length(planeNormal);
    if (planeLength < 1e-10f)
        return false;
    // Barycentric coordinates alone describe the triangle's projection. They
    // must be paired with a plane-distance test, otherwise a huge wall can
    // report points many meters in front of it as "inside" the triangle.
    if (std::fabs(glm::dot(v2, planeNormal / planeLength)) > 1e-4f)
        return false;
    const float d00 = glm::dot(v0, v0);
    const float d01 = glm::dot(v0, v1);
    const float d11 = glm::dot(v1, v1);
    const float d20 = glm::dot(v2, v0);
    const float d21 = glm::dot(v2, v1);
    const float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) < 1e-12f)
        return false;
    const float v = (d11 * d20 - d01 * d21) / denom;
    const float w = (d00 * d21 - d01 * d20) / denom;
    const float u = 1.0f - v - w;
    constexpr float kEps = 1e-3f;
    return u >= -kEps && v >= -kEps && w >= -kEps;
}

// Segment p->q against triangle (a,b,c). Returns true when they intersect.
bool segmentTriangleIntersect(const glm::vec3& p, const glm::vec3& q,
                              const glm::vec3& a, const glm::vec3& b,
                              const glm::vec3& c)
{
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float nLen = glm::length(n);
    if (nLen < 1e-10f)
        return false;
    const glm::vec3 nn = n / nLen;

    const float dp = glm::dot(p - a, nn);
    const float dq = glm::dot(q - a, nn);
    if (std::fabs(dp) < 1e-7f && std::fabs(dq) < 1e-7f) {
        // Coplanar: intersect when an endpoint lies inside the triangle.
        return pointInTriangle(p, a, b, c) || pointInTriangle(q, a, b, c);
    }
    if (dp * dq > 0.0f)
        return false;
    const float denom = dp - dq;
    if (std::fabs(denom) < 1e-12f)
        return false;
    const float t = dp / denom;
    if (t < -1e-4f || t > 1.0f + 1e-4f)
        return false;
    const glm::vec3 hit = p + (q - p) * t;
    return pointInTriangle(hit, a, b, c);
}

// Same test as above, but reports only a crossing after the sweep has
// actually started. A hit at t=0 belongs to the old pose and must not glue an
// actor to a surface while it is leaving that surface.
bool segmentTriangleIntersectAfterStart(const glm::vec3& p,
                                        const glm::vec3& q,
                                        const glm::vec3& a,
                                        const glm::vec3& b,
                                        const glm::vec3& c,
                                        float& outT)
{
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float nLen = glm::length(n);
    if (nLen < 1e-10f)
        return false;
    const glm::vec3 nn = n / nLen;
    const float dp = glm::dot(p - a, nn);
    const float dq = glm::dot(q - a, nn);
    if (dp * dq > 0.0f)
        return false;
    const float denom = dp - dq;
    if (std::fabs(denom) < 1e-12f)
        return false;
    const float t = dp / denom;
    if (t <= 1e-4f || t > 1.0f + 1e-4f)
        return false;
    const glm::vec3 hit = p + (q - p) * t;
    if (!pointInTriangle(hit, a, b, c))
        return false;
    outT = t;
    return true;
}

// Triangle vs triangle: edges of each against the other, then containment.
bool triangleTriangleIntersect(const glm::vec3& v0, const glm::vec3& v1,
                               const glm::vec3& v2, const glm::vec3& u0,
                               const glm::vec3& u1, const glm::vec3& u2)
{
    if (segmentTriangleIntersect(v0, v1, u0, u1, u2)) return true;
    if (segmentTriangleIntersect(v1, v2, u0, u1, u2)) return true;
    if (segmentTriangleIntersect(v2, v0, u0, u1, u2)) return true;
    if (segmentTriangleIntersect(u0, u1, v0, v1, v2)) return true;
    if (segmentTriangleIntersect(u1, u2, v0, v1, v2)) return true;
    if (segmentTriangleIntersect(u2, u0, v0, v1, v2)) return true;
    if (pointInTriangle(v0, u0, u1, u2)) return true;
    if (pointInTriangle(u0, v0, v1, v2)) return true;
    return false;
}

AABB transformedColliderAABB(const glm::vec3& localMin,
                             const glm::vec3& localMax,
                             const glm::mat4& xform)
{
    AABB box;
    box.min = glm::vec3(1e30f);
    box.max = glm::vec3(-1e30f);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner(
            (i & 1) ? localMax.x : localMin.x,
            (i & 2) ? localMax.y : localMin.y,
            (i & 4) ? localMax.z : localMin.z);
        const glm::vec3 w = glm::vec3(xform * glm::vec4(corner, 1.0f));
        box.min = glm::min(box.min, w);
        box.max = glm::max(box.max, w);
    }
    return box;
}

AABB mergedAABB(const AABB& a, const AABB& b)
{
    AABB result;
    result.min = glm::min(a.min, b.min);
    result.max = glm::max(a.max, b.max);
    return result;
}

glm::vec3 closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a,
                                 const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a;

    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
        return b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
        return c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    const float denom = va + vb + vc;
    if (std::fabs(denom) < 1e-12f)
        return a;
    const float inv = 1.0f / denom;
    const float v = vb * inv;
    const float w = vc * inv;
    return a + ab * v + ac * w;
}

void considerRoundedFeature(RoundedFeaturePair& best,
                            const glm::vec3& actorPoint,
                            const glm::vec3& worldPoint)
{
    const float distance = glm::length(actorPoint - worldPoint);
    if (distance < best.distance)
    {
        best.distance = distance;
        best.actorPoint = actorPoint;
        best.worldPoint = worldPoint;
    }
}

// Closest points between two finite segments. Used only by the rounded
// movement shell; exact triangle contacts remain the authoritative fallback.
void considerSegmentPair(RoundedFeaturePair& best,
                         const glm::vec3& a0, const glm::vec3& a1,
                         const glm::vec3& b0, const glm::vec3& b1)
{
    const glm::vec3 d1 = a1 - a0;
    const glm::vec3 d2 = b1 - b0;
    const glm::vec3 r = a0 - b0;
    const float aa = glm::dot(d1, d1);
    const float ee = glm::dot(d2, d2);
    const float f = glm::dot(d2, r);
    float s = 0.0f;
    float t = 0.0f;
    constexpr float kEps = 1e-10f;
    if (aa <= kEps && ee <= kEps)
    {
        considerRoundedFeature(best, a0, b0);
        return;
    }
    if (aa <= kEps)
    {
        t = ee > kEps ? glm::clamp(f / ee, 0.0f, 1.0f) : 0.0f;
    }
    else
    {
        const float c = glm::dot(d1, r);
        if (ee <= kEps)
        {
            s = glm::clamp(-c / aa, 0.0f, 1.0f);
        }
        else
        {
            const float b = glm::dot(d1, d2);
            const float denom = aa * ee - b * b;
            if (denom != 0.0f)
                s = glm::clamp((b * f - c * ee) / denom, 0.0f, 1.0f);
            t = (b * s + f) / ee;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = glm::clamp(-c / aa, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = glm::clamp((b - c) / aa, 0.0f, 1.0f);
            }
        }
    }
    considerRoundedFeature(best, a0 + d1 * s, b0 + d2 * t);
}

RoundedFeaturePair closestRoundedTriangleFeatures(
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
    const glm::vec3& w0, const glm::vec3& w1, const glm::vec3& w2)
{
    RoundedFeaturePair best;
    for (const glm::vec3& v : {a, b, c})
        considerRoundedFeature(best, v, closestPointOnTriangle(v, w0, w1, w2));
    for (const glm::vec3& v : {w0, w1, w2})
        considerRoundedFeature(best, closestPointOnTriangle(v, a, b, c), v);
    considerSegmentPair(best, a, b, w0, w1);
    considerSegmentPair(best, a, b, w1, w2);
    considerSegmentPair(best, a, b, w2, w0);
    considerSegmentPair(best, b, c, w0, w1);
    considerSegmentPair(best, b, c, w1, w2);
    considerSegmentPair(best, b, c, w2, w0);
    considerSegmentPair(best, c, a, w0, w1);
    considerSegmentPair(best, c, a, w1, w2);
    considerSegmentPair(best, c, a, w2, w0);
    return best;
}

} // namespace

// Local-space bounds of a mesh's triangles. Body parts carry collider localMin/
// localMax, but the generic actor mesh only guarantees triangles, so derive a
// box the same way. Cheap: triangle counts are small (12 for a part).
static void localBoundsOfMesh(const ActorCollisionMesh& mesh,
                              glm::vec3& outMin, glm::vec3& outMax)
{
    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(-std::numeric_limits<float>::max());
    if (!mesh.localTriangles)
        return;
    for (const CollisionTriangle& t : *mesh.localTriangles)
        for (const glm::vec3& v : {t.a, t.b, t.c})
        {
            outMin = glm::min(outMin, v);
            outMax = glm::max(outMax, v);
        }
}

AABB makeSweptActorMeshAABB(const std::vector<ActorCollisionMesh>& meshes,
                            const glm::vec3& move)
{
    AABB out;
    out.min = glm::vec3(std::numeric_limits<float>::max());
    out.max = glm::vec3(-std::numeric_limits<float>::max());
    for (const ActorCollisionMesh& mesh : meshes)
    {
        glm::vec3 lo, hi;
        localBoundsOfMesh(mesh, lo, hi);
        if (lo.x > hi.x)
            continue;
        const AABB desired = transformedColliderAABB(lo, hi, mesh.desiredTransform);
        const AABB previous = transformedColliderAABB(lo, hi, mesh.previousTransform);
        AABB box = mergedAABB(desired, previous);
        box.min += glm::min(glm::vec3(0.0f), move);
        box.max += glm::max(glm::vec3(0.0f), move);
        out.min = glm::min(out.min, box.min);
        out.max = glm::max(out.max, box.max);
    }
    return out;
}

std::vector<RecoveryContact> collectActorMeshContacts(
    const World& world,
    const std::vector<ActorCollisionMesh>& meshes,
    const std::vector<int>& candidates,
    const glm::vec3& actorPos,
    bool filterCandidatesByMeshAabb,
    float contactSkin)
{
    std::vector<RecoveryContact> contacts;
    if (world.collisionMesh.triangles.empty() || candidates.empty())
        return contacts;

    // Use the shared collision skin for actor-triangle query bounds. It is a
    // search/recovery margin; actual solver penetration is computed below.
    const float kSkin = std::max(0.0f, contactSkin >= 0.0f
                                           ? contactSkin
                                           : CollisionConfig::instance().collisionSkin());
    // A non-negative explicit skin is used by the moving-entity adapter. Keep
    // that legacy entity contact contract exact until entity support normals
    // are migrated to the rounded feature manifold as well.
    const bool roundedFeatures = contactSkin < 0.0f;
    const float queryMargin = roundedFeatures
        ? std::max(kSkin, MOVEMENT_FEATURE_SMOOTHNESS)
        : kSkin;
    int triangleTests = 0;

    // Candidate gathering is done once for the swept actor, but each body part
    // still needs its own narrowphase AABB filter. Without this, a hand or
    // weapon tests against every triangle near the whole actor, including
    // triangles that belong only to another limb or nearby geometry.
    for (const ActorCollisionMesh& mesh : meshes)
    {
        if (!mesh.localTriangles || mesh.localTriangles->empty())
            continue;

        AABB meshBounds;
        if (filterCandidatesByMeshAabb)
        {
            glm::vec3 localMin, localMax;
            localBoundsOfMesh(mesh, localMin, localMax);
            if (localMin.x > localMax.x)
                continue;
            meshBounds = mergedAABB(
                transformedColliderAABB(localMin, localMax, mesh.previousTransform),
                transformedColliderAABB(localMin, localMax, mesh.desiredTransform));
            meshBounds.min -= glm::vec3(queryMargin);
            meshBounds.max += glm::vec3(queryMargin);
        }

        const int triCount = (int)mesh.localTriangles->size();
        int partContacts = 0;

        for (int ti = 0; ti < triCount && partContacts < kMaxContactsPerPart; ++ti)
        {
            const CollisionTriangle& lt = (*mesh.localTriangles)[ti];
            const glm::vec3 a = glm::vec3(mesh.desiredTransform * glm::vec4(lt.a, 1.0f));
            const glm::vec3 b = glm::vec3(mesh.desiredTransform * glm::vec4(lt.b, 1.0f));
            const glm::vec3 c = glm::vec3(mesh.desiredTransform * glm::vec4(lt.c, 1.0f));
            const glm::vec3 pa = glm::vec3(mesh.previousTransform * glm::vec4(lt.a, 1.0f));
            const glm::vec3 pb = glm::vec3(mesh.previousTransform * glm::vec4(lt.b, 1.0f));
            const glm::vec3 pc = glm::vec3(mesh.previousTransform * glm::vec4(lt.c, 1.0f));

            const glm::vec3 centroid = (a + b + c) / 3.0f;
            const glm::vec3 prevCentroid = (pa + pb + pc) / 3.0f;
            const glm::vec3 sweep = centroid - prevCentroid;

            const float maxVertexTravel = std::max({
                glm::length(a - pa), glm::length(b - pb), glm::length(c - pc)});
            const int sweepSteps = std::clamp(
                (int)std::ceil(maxVertexTravel / 0.08f), 1, 64);
            AABB actorTriangleBounds;
            actorTriangleBounds.min = glm::min(glm::min(a, glm::min(b, c)),
                                               glm::min(pa, glm::min(pb, pc)));
            actorTriangleBounds.max = glm::max(glm::max(a, glm::max(b, c)),
                                               glm::max(pa, glm::max(pb, pc)));
            if (roundedFeatures)
            {
                actorTriangleBounds.min -= glm::vec3(MOVEMENT_FEATURE_SMOOTHNESS);
                actorTriangleBounds.max += glm::vec3(MOVEMENT_FEATURE_SMOOTHNESS);
            }

            for (size_t ci = 0; ci < candidates.size(); ++ci)
            {
                if (triangleTests >= kMaxTriangleTests)
                    return contacts;
                const int wi = candidates[ci];
                if (wi < 0 || wi >= (int)world.collisionMesh.triangles.size())
                    continue;
                const AABB candidateBounds = makeTriangleAABB(
                    world.collisionMesh.triangles[wi]);
                if (filterCandidatesByMeshAabb &&
                    !overlaps(meshBounds, candidateBounds))
                    continue;
                // The rounded narrowphase is more expensive than the exact
                // triangle test. AABB distance is a conservative rejection:
                // if these expanded boxes do not touch, no sphere/capsule
                // feature pair can be within the movement radius.
                if (!overlaps(actorTriangleBounds, candidateBounds))
                    continue;
                const CollisionTriangle& wt = world.collisionMesh.triangles[wi];
                glm::vec3 hitCentroid(0.0f);
                bool hit = false;
                bool roundedFeatureHit = false;
                RoundedFeaturePair roundedFeature;
                float timeOfImpact = 0.0f;
                bool currentOverlap = triangleTriangleIntersect(
                    a, b, c, wt.a, wt.b, wt.c);
                ++triangleTests;
                if (currentOverlap) {
                    hitCentroid = centroid;
                    hit = true;
                } else if (glm::dot(sweep, sweep) > 1e-8f) {
                    float crossingT = 0.0f;
                    if (segmentTriangleIntersectAfterStart(
                            pa, a, wt.a, wt.b, wt.c, crossingT) ||
                        segmentTriangleIntersectAfterStart(
                            pb, b, wt.a, wt.b, wt.c, crossingT) ||
                        segmentTriangleIntersectAfterStart(
                            pc, c, wt.a, wt.b, wt.c, crossingT)) {
                        hitCentroid = prevCentroid + sweep * crossingT;
                        timeOfImpact = crossingT;
                        hit = true;
                    }

                    for (int si = 1; !hit && si < sweepSteps; ++si) {
                        const float alpha = (float)si / (float)sweepSteps;
                        const glm::vec3 sa = pa + (a - pa) * alpha;
                        const glm::vec3 sb = pb + (b - pb) * alpha;
                        const glm::vec3 sc = pc + (c - pc) * alpha;
                        ++triangleTests;
                        if (triangleTriangleIntersect(sa, sb, sc,
                                                       wt.a, wt.b, wt.c)) {
                            hitCentroid = (sa + sb + sc) / 3.0f;
                            timeOfImpact = alpha;
                            hit = true;
                            break;
                        }
                        if (triangleTests >= kMaxTriangleTests)
                            return contacts;
                    }
                }
                if (!hit && roundedFeatures)
                {
                    // Rounded movement shell: the actor face is solid, its
                    // edges are capsules, and its vertices are spheres. This
                    // adds a real geometric gap, rather than inflating the
                    // later correction or relying on a JSON skin value.
                    roundedFeature = closestRoundedTriangleFeatures(
                        a, b, c, wt.a, wt.b, wt.c);
                    if (roundedFeature.distance > MOVEMENT_FEATURE_SMOOTHNESS)
                        continue;
                    hit = true;
                    roundedFeatureHit = true;
                    hitCentroid = roundedFeature.actorPoint;
                    timeOfImpact = 0.0f;
                }
                if (!hit)
                    continue;

                glm::vec3 n = wt.normal;
                float penetration = 0.0f;
                glm::vec3 roundedNormal = n;
                if (roundedFeatureHit)
                {
                    const glm::vec3 separation =
                        roundedFeature.actorPoint - roundedFeature.worldPoint;
                    const float separationLength = glm::length(separation);
                    if (separationLength > 1e-5f)
                        roundedNormal = separation / separationLength;
                    else if (glm::dot(sweep, n) > 0.0f)
                        roundedNormal = -n;
                    n = roundedNormal;
                    penetration = MOVEMENT_FEATURE_SMOOTHNESS - roundedFeature.distance;
                }
                if (currentOverlap) {
                    // Only flip when the actor is clearly on the other side.
                    // An embedded actor whose center lies on the plane must not
                    // flip the world normal by a rounding-sized epsilon.
                    if (glm::dot(actorPos - wt.a, n) < -1e-4f)
                        n = -n;
                } else {
                    if (glm::dot(sweep, n) > 0.0f)
                        n = -n;
                }

                const float s0 = glm::dot(a - wt.a, n);
                const float s1 = glm::dot(b - wt.a, n);
                const float s2 = glm::dot(c - wt.a, n);
                const float rawPenetration =
                    std::max(0.0f, -std::min({s0, s1, s2}));
                // A triangle intersection at exactly a block edge can be a
                // touching seam with no physical penetration. Do not inflate
                // that into a full skin-sized blocking contact: that is what
                // makes limbs snag on otherwise continuous block surfaces.
                if (currentOverlap &&
                    rawPenetration <= CollisionConfig::instance().edgeTouchTolerance())
                    continue;

                // The skin is a query/recovery margin, not physical
                // penetration. Inflating every shallow triangle contact to
                // the full skin makes a 0.1 skin behave like a spring and is
                // the source of repeated edge bounces. Preserve real overlap;
                // swept crossings get only the small solver slop needed to
                // enter the correction manifold.
                if (!roundedFeatureHit)
                    penetration = rawPenetration;
                if (!currentOverlap && !roundedFeatureHit)
                    penetration = std::max(
                        penetration,
                        CollisionConfig::instance().edgeTouchTolerance());

                const glm::vec3 point = closestPointOnTriangle(
                    hitCentroid, wt.a, wt.b, wt.c);
                RecoveryContact rc{n, point, sweep, penetration, wi, nullptr,
                                   mesh.label};
                rc.timeOfImpact = timeOfImpact;
                // Keep the exact triangle normal for depenetration, but use a
                // rounded feature normal for response. At a face interior the
                // closest-point vector is the face normal; at an edge or
                // vertex it points away from that feature, just like the
                // normal of a capsule/sphere swept around the triangle. This
                // prevents a limb corner from repeatedly choosing one hard
                // triangle normal at a block seam while the exact triangle
                // still prevents penetration.
                glm::vec3 featureVector = hitCentroid - point;
                if (roundedFeatures)
                {
                    const RoundedFeaturePair responseFeature =
                        closestRoundedTriangleFeatures(a, b, c, wt.a, wt.b, wt.c);
                    const glm::vec3 featureSeparation =
                        responseFeature.actorPoint - responseFeature.worldPoint;
                    if (glm::length(featureSeparation) > 1e-5f)
                        featureVector = featureSeparation;
                }
                const float featureLength = glm::length(featureVector);
                if (featureLength > 1e-5f)
                {
                    const glm::vec3 roundedNormal = featureVector / featureLength;
                    if (glm::dot(roundedNormal, n) > 0.05f)
                        rc.responseNormal = roundedNormal;
                    else
                        rc.responseNormal = n;
                }
                else
                {
                    rc.responseNormal = n;
                }
                contacts.push_back(rc);
                ++partContacts;
                if (partContacts >= kMaxContactsPerPart)
                    break;
            }
        }
    }

    std::vector<RecoveryContact> merged;
    merged.reserve(contacts.size());
    for (const RecoveryContact& c : contacts)
    {
        int found = -1;
        for (size_t j = 0; j < merged.size(); ++j)
        {
            if (merged[j].triangleIndex == c.triangleIndex &&
                merged[j].label && c.label &&
                std::strcmp(merged[j].label, c.label) == 0)
            {
                found = (int)j;
                break;
            }
        }
        if (found < 0)
            merged.push_back(c);
        else if (c.penetration > merged[found].penetration)
            merged[found] = c;
    }
    return merged;
}

std::vector<RecoveryContact> collectBodyMeshContacts(Player& p, const World& world)
{
    if (world.collisionMesh.triangles.empty())
        return {};

    constexpr float kSkin = 0.01f;
    std::vector<ActorCollisionMesh> meshes = collectActorBodyCollisionMeshes(p);
    std::vector<int> candidates;
    for (const ActorCollisionMesh& mesh : meshes)
    {
        glm::vec3 lo, hi;
        localBoundsOfMesh(mesh, lo, hi);
        if (lo.x > hi.x)
            continue;
        const AABB currentBox = transformedColliderAABB(
            lo, hi, mesh.desiredTransform);
        const AABB previousBox = transformedColliderAABB(
            lo, hi, mesh.previousTransform);
        const AABB partBox = mergedAABB(currentBox, previousBox);
        appendChunkTrianglesForAABB(world, partBox, kSkin, candidates,
                                    "bodyMeshGather");
    }
    return collectActorMeshContacts(world, meshes, candidates, p.pos);
}
