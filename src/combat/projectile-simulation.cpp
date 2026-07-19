// 07 19 2026, 09 29
/* purpose
* Shared deterministic projectile physics implementation.
* Advances projectile movement, rotation, swept collision, and bounce results.
* Provides caller-owned facts for server authority and future client prediction.
* Does NOT send packets, apply damage, spawn effects, or own weapon policy.
* Does NOT read weapon names, ammo, cooldowns, or networking state.
* Does NOT decide whether an impact becomes an explosion.
*/

#include "projectile-simulation.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// ── Closest point on triangle ────────────────────────────────────────
static glm::vec3 closestPointOnTriangle(
    const glm::vec3& p,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d5 >= 0.0f && d6 >= 0.0f) return c;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return a + (d1 / (d1 - d3)) * ab;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return a + (d2 / (d2 - d6)) * ac;
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);
    float denom = va + vb + vc;
    return denom == 0.0f ? a : (va * a + vb * b + vc * c) / denom;
}

// ── Closest point on line segment ────────────────────────────────────
static glm::vec3 closestPointOnSegment(
    const glm::vec3& p,
    const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 <= 1e-12f)
        return a;
    float t = glm::dot(p - a, ab) / len2;
    t = std::max(0.0f, std::min(1.0f, t));
    return a + ab * t;
}

// ── Helper: check if sphere at `center` overlaps triangle ────────────
static bool sphereOverlapsTriangle(
    const glm::vec3& center, float radius,
    const CollisionTriangle& tri,
    glm::vec3& outClosest, glm::vec3& outNormal)
{
    outClosest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
    glm::vec3 diff = center - outClosest;
    float dist = glm::length(diff);
    if (dist >= radius || dist <= 1e-8f)
        return false;
    outNormal = diff / dist;
    return true;
}

// Forward declaration (defined below, used by 7-feature triangle sweep)
static bool sweepSphereCapsule(
    const glm::vec3& from, const glm::vec3& to, float sphereRadius,
    const glm::vec3& capA, const glm::vec3& capB, float capRadius,
    float& outT, glm::vec3& outPoint, glm::vec3& outNormal);

// ── Swept sphere vs triangle (7-feature decomposition) ──────────────
// Tests the face (offset plane), three edges (capsules), and three
// vertices (spheres). Returns the earliest TOI in [0,1].
// Deterministic feature ordering at equal TOI: face < edge < vertex.
static bool sweepSphereTriangle(
    const glm::vec3& from, const glm::vec3& to, float radius,
    const CollisionTriangle& tri,
    float& outT, glm::vec3& outPoint, glm::vec3& outNormal)
{
    const glm::vec3& a = tri.a;
    const glm::vec3& b = tri.b;
    const glm::vec3& c = tri.c;
    glm::vec3 vel = to - from;
    float velLen = glm::length(vel);

    // Static overlap at start — catches t=0 case
    {
        glm::vec3 tmpP, tmpN;
        if (sphereOverlapsTriangle(from, radius, tri, tmpP, tmpN))
        {
            outT = 0.0f;
            outPoint = tmpP;
            outNormal = tmpN;
            return true;
        }
    }

    if (velLen < 1e-10f)
        return false;

    struct FeatureHit {
        float t = 2.0f;
        int featureTie = 99; // 0=face, 1=AB, 2=BC, 3=CA, 4=A, 5=B, 6=C
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f, 0.0f, 1.0f};
        bool valid = false;
    } best;

    const glm::vec3 n = glm::length(tri.normal) > 0.5f
        ? glm::normalize(tri.normal)
        : glm::normalize(glm::cross(b - a, c - a));

    auto updateBest = [&](float t, int tie, const glm::vec3& p, const glm::vec3& norm) {
        if (t < 0.0f || t > 1.0f) return;
        if (t < best.t - 1e-8f || (std::fabs(t - best.t) < 1e-8f && tie < best.featureTie))
        {
            best.t = t; best.featureTie = tie;
            best.point = p; best.normal = norm;
            best.valid = true;
        }
        else if (std::fabs(t - best.t) < 1e-8f && tie == best.featureTie)
        {
            // Preserve first match for exact same feature (deterministic)
        }
    };

    // ── 1. Face: plane intersection with inside-triangle check ─────
    {
        float planeD = glm::dot(n, tri.a);
        float distToPlane = glm::dot(n, from) - planeD;
        float approachSpeed = glm::dot(n, vel / velLen);
        if (approachSpeed < 0.0f)
        {
            float travelNeeded = distToPlane - radius;
            float tHit = travelNeeded / (-approachSpeed * velLen);
            if (tHit >= 0.0f && tHit <= 1.0f)
            {
                glm::vec3 hitCenter = from + vel * tHit;
                // Project onto plane and check inside triangle
                glm::vec3 proj = hitCenter - n * glm::dot(hitCenter - a, n);
                // Barycentric inside test
                glm::vec3 v0 = b - a, v1 = c - a, v2 = proj - a;
                float d00 = glm::dot(v0, v0), d01 = glm::dot(v0, v1);
                float d11 = glm::dot(v1, v1), d20 = glm::dot(v2, v0);
                float d21 = glm::dot(v2, v1);
                float denom = d00 * d11 - d01 * d01;
                if (std::fabs(denom) > 1e-12f)
                {
                    float u = (d11 * d20 - d01 * d21) / denom;
                    float v = (d00 * d21 - d01 * d20) / denom;
                    if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.0f + 1e-6f)
                    {
                        glm::vec3 faceNormal = (approachSpeed < 0.0f && distToPlane >= 0.0f) ? n : -n;
                        updateBest(tHit, 0, proj, faceNormal);
                    }
                }
            }
        }
    }

    // ── 2-4. Edges: swept sphere vs line segment (capsule with capRadius=0) ──
    {
        float et, ep[3]; glm::vec3 hp, hn;
        if (sweepSphereCapsule(from, to, radius, a, b, 0.0f, et, hp, hn))
            updateBest(et, 1, hp, hn);
        if (sweepSphereCapsule(from, to, radius, b, c, 0.0f, et, hp, hn))
            updateBest(et, 2, hp, hn);
        if (sweepSphereCapsule(from, to, radius, c, a, 0.0f, et, hp, hn))
            updateBest(et, 3, hp, hn);
    }

    // ── 5-7. Vertices: swept sphere vs point ───────────────────────
    {
        float et; glm::vec3 hp, hn;
        // Vertex A: use capsule with zero-length segment
        if (sweepSphereCapsule(from, to, radius, a, a, 0.0f, et, hp, hn))
            updateBest(et, 4, hp, hn);
        // Vertex B
        if (sweepSphereCapsule(from, to, radius, b, b, 0.0f, et, hp, hn))
            updateBest(et, 5, hp, hn);
        // Vertex C
        if (sweepSphereCapsule(from, to, radius, c, c, 0.0f, et, hp, hn))
            updateBest(et, 6, hp, hn);
    }

    if (best.valid)
    {
        outT = best.t;
        outPoint = best.point;
        outNormal = best.normal;
        return true;
    }

    return false;
}

// ── Swept sphere vs capsule (line segment with radius) ───────────────
static bool sweepSphereCapsule(
    const glm::vec3& from, const glm::vec3& to, float sphereRadius,
    const glm::vec3& capA, const glm::vec3& capB, float capRadius,
    float& outT, glm::vec3& outPoint, glm::vec3& outNormal)
{
    glm::vec3 vel = to - from;
    float velLen = glm::length(vel);
    float totalRadius = sphereRadius + capRadius;

    // ── Static overlap at start ────────────────────────────────────
    {
        glm::vec3 closest = closestPointOnSegment(from, capA, capB);
        glm::vec3 diff = from - closest;
        float dist = glm::length(diff);
        if (dist < totalRadius)
        {
            outT = 0.0f;
            outPoint = closest;
            outNormal = (dist > 1e-6f) ? glm::normalize(diff) : glm::vec3(0.0f, 0.0f, 1.0f);
            return true;
        }
    }

    if (velLen < 1e-10f)
        return false;

    // Find the parameter t (along sphere movement) and s (along capsule axis)
    // that minimizes distance between the two line segments.
    // Sphere center moves on: from + vel*t, t in [0,1]
    // Capsule center line: capA + segDir*s, s in [0,1]
    // Minimize |(from + vel*t) - (capA + segDir*s)|^2

    glm::vec3 segDir = capB - capA;
    float a = glm::dot(vel, vel);
    float b = -glm::dot(vel, segDir);
    float c = glm::dot(segDir, segDir);
    glm::vec3 w0 = from - capA;
    float d = glm::dot(vel, w0);
    float e = -glm::dot(segDir, w0);
    float det = a * c - b * b;

    float t, s;
    if (std::fabs(det) < 1e-12f)
    {
        // Degenerate: parallel or zero-length
        t = 0.0f;
        s = (c > 1e-12f) ? glm::clamp(-e / c, 0.0f, 1.0f) : 0.0f;
        // Clamp s to segment, re-solve for t
        glm::vec3 Qs = capA + segDir * s;
        glm::vec3 w = from - Qs;
        float a2 = glm::dot(vel, vel);
        float d2 = glm::dot(vel, w);
        t = (a2 > 1e-12f) ? glm::clamp(-d2 / a2, 0.0f, 1.0f) : 0.0f;
    }
    else
    {
        t = (d * c - b * e) / det;
        s = (b * d - a * e) / det;
        s = glm::clamp(s, 0.0f, 1.0f);
        // Re-solve for t with clamped s
        glm::vec3 Qs = capA + segDir * s;
        glm::vec3 w = from - Qs;
        float a2 = glm::dot(vel, vel);
        float d2 = glm::dot(vel, w);
        t = (a2 > 1e-12f) ? glm::clamp(-d2 / a2, 0.0f, 1.0f) : 0.0f;
    }

    // Check distance at closest approach
    glm::vec3 Pt = from + vel * t;
    glm::vec3 Qs = capA + segDir * s;
    glm::vec3 diff = Pt - Qs;
    float dist = glm::length(diff);

    if (dist >= totalRadius)
        return false;

    // Binary refine to find exact entry time
    float lo = 0.0f, hi = (t >= 0.0f && t <= 1.0f) ? t : 1.0f;
    for (int iter = 0; iter < 10; ++iter)
    {
        float mid = (lo + hi) * 0.5f;
        glm::vec3 midCenter = from + vel * mid;
        glm::vec3 midClosest = closestPointOnSegment(midCenter, capA, capB);
        float midDist = glm::length(midCenter - midClosest);
        if (midDist < totalRadius)
            hi = mid;
        else
            lo = mid;
    }

    outT = hi;
    Pt = from + vel * outT;
    Qs = closestPointOnSegment(Pt, capA, capB);
    outPoint = Qs;
    diff = Pt - Qs;
    outNormal = (glm::length(diff) > 1e-6f) ? glm::normalize(diff) : glm::vec3(0.0f, 0.0f, 1.0f);
    return true;
}

// ── Bounce reflection ─────────────────────────────────────────────────
static void applyBounce(
    glm::vec3& velocity, glm::vec3& angularVelocity,
    const glm::vec3& normal, float restitution, float friction,
    int& bounceCount)
{
    glm::vec3 n = glm::length(normal) > 0.5f ? glm::normalize(normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    float intoSpeed = glm::dot(-velocity, n);
    if (intoSpeed <= 0.0f) return;
    glm::vec3 tangent = velocity - n * glm::dot(velocity, n);
    float tangentRetention = std::clamp(1.0f - friction, 0.0f, 1.0f);
    velocity = tangent * tangentRetention + n * intoSpeed * restitution;
    angularVelocity *= 0.35f;
    ++bounceCount;
}

// ── Simulation entry point ────────────────────────────────────────────
ProjectileStepResult simulateProjectileTick(
    ProjectilePhysicsState& state,
    const ProjectilePhysicsConfig& config,
    const CollisionWorldView& world,
    float fixedDt)
{
    ProjectileStepResult result;

    // ── Input validation ────────────────────────────────────────────
    if (!std::isfinite(state.position.x) || !std::isfinite(state.position.y) || !std::isfinite(state.position.z) ||
        !std::isfinite(state.velocity.x) || !std::isfinite(state.velocity.y) || !std::isfinite(state.velocity.z) ||
        fixedDt <= 0.0f || !std::isfinite(fixedDt))
    {
        return result;
    }

    // ── Sleeping: only tick age for fuse/lifetime ──────────────────
    if (state.exploded)
        return result;

    if (state.sleeping)
    {
        state.age += fixedDt;
        if (config.lifetime > 0.0f && state.age >= config.lifetime)
        {
            result.type = ProjectileCollisionType::LifetimeExpired;
        }
        return result;
    }

    state.age += fixedDt;

    // ── Lifetime expiry ────────────────────────────────────────────
    if (config.lifetime > 0.0f && state.age >= config.lifetime)
    {
        result.type = ProjectileCollisionType::LifetimeExpired;
        return result;
    }

    // ── Adaptive substeps ──────────────────────────────────────────
    float speed = glm::length(state.velocity);
    float maxStep = std::max(config.radius * 0.5f, 0.1f);
    int subSteps = std::min((int)std::ceil(speed * fixedDt / maxStep) + 1, 8);
    float subDt = fixedDt / (float)subSteps;

    static constexpr int MAX_BOUNCE_ITER = 4;

    for (int s = 0; s < subSteps && !state.exploded && !state.sleeping; ++s)
    {
        float remainingFraction = 1.0f;

        for (int bounceIter = 0; bounceIter < MAX_BOUNCE_ITER && remainingFraction > 1e-6f && !state.exploded; ++bounceIter)
        {
            // ── Physics integration ──────────────────────────────────
            // Gravity (Z up)
            state.velocity.z -= config.gravity * subDt * remainingFraction;

            // Linear drag
            state.velocity *= std::max(0.0f, 1.0f - config.drag * subDt * remainingFraction);

            // Angular
            float angSpd = glm::length(state.angularVelocity);
            if (config.angularDrag > 0.0f && angSpd > 0.0f)
                state.angularVelocity *= std::max(0.0f, 1.0f - config.angularDrag * subDt * remainingFraction);
            if (angSpd > 0.0001f)
            {
                float newAngSpd = glm::length(state.angularVelocity);
                if (newAngSpd > 0.0001f)
                {
                    glm::quat delta = glm::angleAxis(
                        newAngSpd * subDt * remainingFraction, glm::normalize(state.angularVelocity));
                    state.rotation = glm::normalize(delta * state.rotation);
                }
            }

            // Proposed new position
            glm::vec3 step = state.velocity * subDt * remainingFraction;
            glm::vec3 prevPos = state.position;
            glm::vec3 proposedPos = state.position + step;

            // ── Swept collision detection ────────────────────────────
            // Gather all candidates
            struct CollisionCandidate {
                float t;
                int type; // 0=world, 1=player
                int triIdx;       // valid when type==0
                int capsuleIdx;   // valid when type==1
                uint32_t playerId;
                uint32_t spawnGeneration;
                glm::vec3 hitPos;
                glm::vec3 hitNormal;
                float impactSpeed;
                // Deterministic ordering:
                // 1. smaller t wins
                // 2. equal t: world (0) beats player (1)
                // 3. equal t, same type: lower triIdx / lower playerId wins
                // 4. equal t, same player: lower spawnGeneration wins
                bool operator<(const CollisionCandidate& o) const {
                    if (std::fabs(t - o.t) > 1e-8f) return t < o.t;
                    if (type != o.type) return type < o.type;
                    if (type == 0) return triIdx < o.triIdx;
                    if (playerId != o.playerId) return playerId < o.playerId;
                    return spawnGeneration < o.spawnGeneration;
                }
            };
            CollisionCandidate best{};
            best.t = 2.0f;

            std::vector<int> triCandidates;
            world.queryTrianglesSwept(state.position, proposedPos, config.radius, triCandidates);
            for (int triIdx : triCandidates)
            {
                const CollisionTriangle& tri = world.triangleAt(triIdx);
                float t; glm::vec3 hp, hn;
                if (sweepSphereTriangle(prevPos, proposedPos, config.radius, tri, t, hp, hn))
                {
                    CollisionCandidate c{};
                    c.t = t; c.type = 0; c.triIdx = triIdx;
                    c.hitPos = hp; c.hitNormal = hn;
                    c.impactSpeed = glm::dot(-state.velocity, hn);
                    if (c < best) best = c;
                }
            }

            std::vector<SweptPlayerCapsule> capsuleCandidates;
            world.queryPlayerCapsulesSwept(state.position, proposedPos, config.radius, capsuleCandidates);
            for (int ci = 0; ci < (int)capsuleCandidates.size(); ++ci)
            {
                const SweptPlayerCapsule& cap = capsuleCandidates[ci];
                float t; glm::vec3 hp, hn;
                if (sweepSphereCapsule(prevPos, proposedPos, config.radius,
                                       cap.a, cap.b, cap.radius, t, hp, hn))
                {
                    CollisionCandidate c{};
                    c.t = t; c.type = 1; c.capsuleIdx = ci;
                    c.playerId = cap.playerId;
                    c.spawnGeneration = cap.spawnGeneration;
                    c.hitPos = hp; c.hitNormal = hn;
                    c.impactSpeed = glm::dot(-state.velocity, hn);
                    if (c < best) best = c;
                }
            }

            float earliestT = best.t;
            int earliestCapsuleIdx = (best.type == 1 && best.t <= 1.0f) ? best.capsuleIdx : -1;
            glm::vec3 earliestHitPos = best.hitPos;
            glm::vec3 earliestNormal = best.hitNormal;
            float earliestImpactSpeed = best.impactSpeed;

            // ── Process earliest collision ────────────────────────────
            if (earliestT <= 1.0f)
            {
                // Move to collision point
                state.position = prevPos + step * earliestT;
                result.travelDistance += glm::length(step) * earliestT;

                // When earliestT == 0 (static overlap), the entire fraction is consumed
                // because the collision prevents any forward progress this substep.
                // Without this, remainingFraction stays unchanged and the bounce loop
                // oscillates the projectile in place until maxBounceCount.
                if (earliestT <= 0.0f)
                    remainingFraction = 0.0f;
                else
                    remainingFraction *= (1.0f - earliestT);
                if (earliestCapsuleIdx >= 0)
                {
                    const SweptPlayerCapsule& cap = capsuleCandidates[earliestCapsuleIdx];
                    result.type = ProjectileCollisionType::PlayerImpact;
                    result.hitPosition = earliestHitPos;
                    result.hitNormal = earliestNormal;
                    result.impactSpeed = earliestImpactSpeed;
                    result.hitPlayerId = cap.playerId;
                    result.hitPlayerSpawnGeneration = cap.spawnGeneration;
                    return result;
                }
                else
                {
                    // World collision — depenetrate
                    glm::vec3 diff = state.position - earliestHitPos;
                    float pen = config.radius - glm::length(diff);
                    if (pen > 0.0f)
                        state.position += earliestNormal * (pen + 0.001f);

                    float intoSpeed = glm::dot(-state.velocity, earliestNormal);
                    bool shouldBounce = config.bounceEnabled && intoSpeed > 0.0f &&
                        state.bounceCount < config.maxBounceCount;

                    if (shouldBounce)
                    {
                        applyBounce(state.velocity, state.angularVelocity,
                                    earliestNormal, config.restitution, config.friction,
                                    state.bounceCount);
                        result.type = ProjectileCollisionType::WorldBounce;
                        result.hitPosition = earliestHitPos;
                        result.hitNormal = earliestNormal;
                        result.impactSpeed = intoSpeed;
                    }
                    else
                    {
                        state.velocity = glm::vec3(0.0f);
                        state.angularVelocity = glm::vec3(0.0f);
                        state.sleeping = true;
                        result.type = ProjectileCollisionType::WorldImpact;
                        result.hitPosition = earliestHitPos;
                        result.hitNormal = earliestNormal;
                        result.impactSpeed = intoSpeed;
                        remainingFraction = 0.0f;
                    }
                }
            }
            else
            {
                // No collision
                state.position = proposedPos;
                result.travelDistance += glm::length(step);
                remainingFraction = 0.0f;
            }
        }
    }

    // ── Post-step: max-bounce settle ─────────────────────────────────
    if (state.bounceCount >= config.maxBounceCount && config.maxBounceCount > 0 && !state.sleeping && !state.exploded)
    {
        float spd = glm::length(state.velocity);
        if (spd > config.minBounceSpeed)
        {
            glm::vec3 lateral(state.velocity.x, state.velocity.y, 0.0f);
            state.velocity -= lateral * 0.95f;
            if (state.velocity.z < -20.0f)
                state.velocity.z = -20.0f;
        }
        else
        {
            state.velocity = glm::vec3(0.0f);
            state.angularVelocity = glm::vec3(0.0f);
            state.sleeping = true;
        }
    }

    return result;
}
