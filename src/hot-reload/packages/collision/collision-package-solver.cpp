// 09 17 2026
/* purpose
* The `collision.main` capability: one generic swept solve over the cached world
* broadphase. The caller supplies the collider list (capsule + body parts +
* future weapons/props); this file owns the union-AABB candidate gather, shared
* candidate list, per-shape narrowphase, depenetration, slide, bounce, grounding,
* contact merging, solver pass limits, and impact selection.
* Does NOT own movement policy, entity storage, networking, or rendering.
*/
#if defined(MIMITA_GAME_DLL)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/packages/collision/collision-abi.h"
#include "hot-reload/packages/collision/collision-world.h"

namespace HotCollisionPackage {

namespace {

// ── Solve tuning (edit live) ────────────────────────────────────────────────
constexpr float kGroundNormalZ = 0.35f;
constexpr float kWalkableSlopeDot = 0.70f;
constexpr float kSkin = 0.001f;
constexpr int   kResolvePasses = 3;
constexpr int   kMaxSubsteps = 8;
constexpr float kMinSubstepMove = 0.05f;
constexpr float kGroundSnapEpsilon = 0.05f;
constexpr float kMaxCapsulePush = 1000.0f;
constexpr float kMaxBodyPush = 0.5f;
constexpr int   kMaxRawContacts = 64;
// Extra padding around the swept union AABB so geometry brushed at the very
// edge of the sweep is still a candidate. Kept small: the sweep itself already
// covers the full move.
constexpr float kSweepMargin = 0.2f;
// Movement above this per-tick distance re-gathers in the middle of the sweep
// as a safety net against tunneling through sub-cell-thin geometry.
constexpr float kMaxSweepReGatherDistance = 6.0f;
constexpr float kImpactSphereSize = 1.0f;
constexpr std::uint32_t kImpactSphereLifetimeTicks = 1;
constexpr float kImpactMinIncomingSpeed = 0.0f;

constexpr std::uint32_t kBounceCooldownTicks =
    (std::uint32_t)(kBounceCooldown * 60.0f) + 1u;

std::unordered_map<std::uint64_t, std::uint64_t>& bounceTickMap()
{
    static std::unordered_map<std::uint64_t, std::uint64_t> m;
    return m;
}

// ── Throttled timing (edit live) ────────────────────────────────────────────
// Accumulates per-tick phase timings and logs one summary at most once per
// second. Never logs every solve.
constexpr float kTimingLogIntervalSeconds = 1.0f;
struct TimingStats {
    double broadphaseMs = 0.0;
    double narrowphaseMs = 0.0;
    double solverMs = 0.0;
    std::uint64_t solves = 0;
    std::uint64_t candidates = 0;
    std::uint64_t large = 0;
    std::uint64_t contacts = 0;
    float sinceLogSeconds = 0.0f;
};
TimingStats& timingStats()
{
    static TimingStats s;
    return s;
}
double nowMs()
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct ColliderRuntime {
    glm::vec3 localOffset;
    float radius;
    float halfHeight;
    std::uint32_t partId;
    std::uint32_t policyId;
    bool body;
};

struct ActorContact {
    std::uint32_t partId;
    std::uint32_t policyId;
    bool body;
    std::int32_t tri;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
    float incoming;
};

// Swept union AABB: the volume the whole collider list sweeps through for the
// entire tick. One gather over this region covers every substep, so a fast
// fall/dash can never move outside the candidate set and tunnel.
void sweptUnionAABB(const glm::vec3& pos, const ColliderRuntime* cols,
                    int colCount, const glm::vec3& move, glm::vec3& outMin,
                    glm::vec3& outMax)
{
    glm::vec3 mn(1e30f), mx(-1e30f);
    for (int i = 0; i < colCount; ++i) {
        const glm::vec3 c = pos + cols[i].localOffset;
        const float halfZ = cols[i].halfHeight > cols[i].radius
                                ? cols[i].halfHeight
                                : cols[i].radius;
        const glm::vec3 r(cols[i].radius, cols[i].radius, halfZ);
        mn = glm::min(mn, c - r);
        mx = glm::max(mx, c + r);
    }
    if (colCount == 0)
        mn = mx = pos;
    // Sweep the whole volume along the move and pad by the skin/step margin.
    const glm::vec3 endMin = mn + move;
    const glm::vec3 endMax = mx + move;
    mn = glm::min(mn, endMin) - glm::vec3(kSweepMargin);
    mx = glm::max(mx, endMax) + glm::vec3(kSweepMargin);
    outMin = mn;
    outMax = mx;
}

// Append all contacts for the collider list at the current root position.
int gatherActorContacts(const glm::vec3& root, const ColliderRuntime* cols,
                        int colCount, const glm::vec3& velocity,
                        const std::vector<std::uint32_t>& candidates,
                        ActorContact* out, int maxOut)
{
    int n = 0;
    for (int i = 0; i < colCount && n < maxOut; ++i) {
        const ColliderRuntime& col = cols[i];
        const glm::vec3 c = root + col.localOffset;
        const float halfSeg =
            col.halfHeight > col.radius ? col.halfHeight - col.radius : 0.0f;
        const glm::vec3 samples[3] = {
            c + glm::vec3(0.0f, 0.0f, halfSeg),
            c,
            c - glm::vec3(0.0f, 0.0f, halfSeg)};
        const int sampleCount = col.halfHeight > col.radius ? 3 : 1;
        for (int s = 0; s < sampleCount && n < maxOut; ++s) {
            SphereHit hits[8];
            const int hc = gatherSphereHits(samples[s], col.radius, candidates,
                                            hits, 8);
            for (int h = 0; h < hc && n < maxOut; ++h) {
                out[n].partId = col.partId;
                out[n].policyId = col.policyId;
                out[n].body = col.body;
                out[n].tri = hits[h].triangle;
                out[n].point = hits[h].point;
                out[n].normal = hits[h].normal;
                out[n].penetration = hits[h].penetration;
                out[n].incoming = -glm::dot(velocity, hits[h].normal);
                ++n;
            }
        }
    }
    return n;
}

int mergeContacts(const ActorContact* raw, int rawCount, ActorContact* merged)
{
    int mc = 0;
    for (int i = 0; i < rawCount; ++i) {
        int found = -1;
        for (int j = 0; j < mc; ++j) {
            if (merged[j].tri == raw[i].tri) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            merged[mc++] = raw[i];
            continue;
        }
        if (raw[i].penetration > merged[found].penetration) {
            const std::uint32_t part = merged[found].partId;
            const std::uint32_t policy = merged[found].policyId;
            const bool body = merged[found].body;
            merged[found] = raw[i];
            if (merged[found].partId == COLLISION_PART_CAPSULE && body) {
                merged[found].partId = part;
                merged[found].policyId = policy;
                merged[found].body = body;
            }
        }
        if (raw[i].body) {
            merged[found].body = true;
            merged[found].partId = raw[i].partId;
            merged[found].policyId = raw[i].policyId;
        }
    }
    return mc;
}

void applyVelocityResponse(glm::vec3& vel, const ActorContact& c,
                           std::uint64_t entity, std::uint64_t tick,
                           bool& bounced)
{
    const glm::vec3 n = c.normal;
    const float into = -glm::dot(vel, n);
    if (into <= 0.0f)
        return;
    const bool cooldownClear =
        (bounceTickMap()[entity] + kBounceCooldownTicks) <= tick;
    if (kBounceEnabled && kBounceStrength > 0.0f && into >= kBounceMinSpeed &&
        cooldownClear) {
        const float retention = 1.0f - kBounceFriction;
        const glm::vec3 tangent = vel - n * glm::dot(vel, n);
        vel = tangent * retention +
              n * (std::min(into, kBounceMaxSpeed) * kBounceStrength);
        bounceTickMap()[entity] = tick;
        bounced = true;
    } else {
        vel -= n * glm::dot(vel, n);   // slide
    }
}

void recordContacts(const ActorContact* merged, int mc, ActorContact* accum,
                    int& count)
{
    for (int i = 0; i < mc; ++i) {
        int found = -1;
        for (int j = 0; j < count; ++j) {
            if (accum[j].tri == merged[i].tri) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            if (count < (int)COLLISION_MAX_CONTACTS)
                accum[count++] = merged[i];
        } else if (merged[i].penetration > accum[found].penetration) {
            const std::uint32_t part = accum[found].partId;
            const std::uint32_t policy = accum[found].policyId;
            const bool body = accum[found].body;
            accum[found] = merged[i];
            if (accum[found].partId == COLLISION_PART_CAPSULE && body) {
                accum[found].partId = part;
                accum[found].policyId = policy;
                accum[found].body = body;
            }
        }
    }
}

void recordImpacts(const ActorContact* merged, int mc, ActorContact* accum,
                   int& count)
{
    for (int i = 0; i < mc; ++i) {
        if (merged[i].incoming <= kImpactMinIncomingSpeed)
            continue;
        int found = -1;
        for (int j = 0; j < count; ++j) {
            if (accum[j].partId == merged[i].partId) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            if (count < (int)COLLISION_MAX_IMPACTS)
                accum[count++] = merged[i];
        } else if (merged[i].incoming > accum[found].incoming) {
            accum[found] = merged[i];
        }
    }
}

int resolveOnce(glm::vec3& pos, glm::vec3& vel, std::uint64_t entity,
                std::uint64_t tick, const ColliderRuntime* cols, int colCount,
                const std::vector<std::uint32_t>& candidates, bool& grounded,
                bool& collided, bool& worldContact, bool& bodyContact,
                bool& bounced, ActorContact* contactAccum,
                int& contactAccumCount, ActorContact* impactAccum,
                int& impactAccumCount)
{
    ActorContact raw[kMaxRawContacts];
    const int rc = gatherActorContacts(pos, cols, colCount, vel, candidates,
                                       raw, kMaxRawContacts);
    ActorContact merged[kMaxRawContacts];
    const int mc = mergeContacts(raw, rc, merged);
    for (int i = 0; i < mc; ++i) {
        const float cap = merged[i].body ? kMaxBodyPush : kMaxCapsulePush;
        const float push = std::min(merged[i].penetration + kSkin, cap);
        pos += merged[i].normal * push;
        collided = true;
        worldContact = true;
        if (merged[i].body)
            bodyContact = true;
        if (merged[i].normal.z >= kGroundNormalZ)
            grounded = true;
        applyVelocityResponse(vel, merged[i], entity, tick, bounced);
    }
    recordContacts(merged, mc, contactAccum, contactAccumCount);
    recordImpacts(merged, mc, impactAccum, impactAccumCount);
    return mc;
}

void spawnImpactSphere(void* host, const CollisionImpactV1& ev)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<GameEffectPartFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_PART));
    if (!fn)
        return;
    GameEffectPartV1 p{};
    for (int k = 0; k < 3; ++k) {
        p.position[k] = ev.position[k];
        p.normal[k] = ev.normal[k];
    }
    p.color[0] = 1.0f;
    p.color[1] = 0.55f;
    p.color[2] = 0.15f;
    p.scale = ev.size;
    p.endScale = ev.size;
    p.alpha = 1.0f;
    p.maxLifetime = std::max(1.0f / 60.0f,
        (float)ev.lifetimeTicks * (1.0f / 60.0f));
    p.sticky = 1u;
    p.billboardText = 0u;
    const char* type = "impact_tick";
    for (int i = 0; type[i] != '\0' && i < (int)GAME_EFFECT_STRING - 1; ++i)
        p.replayType[i] = type[i];
    fn(ctx->host, &p);
}

void solve(void* host, CollisionSolveV1* q)
{
    if (!q)
        return;
    q->handled = 0u;
    q->contactCount = 0u;
    q->impactCount = 0u;
    for (int i = 0; i < 3; ++i) {
        q->outPosition[i] = q->position[i];
        q->outVelocity[i] = q->velocity[i];
    }
    q->grounded = q->worldContact = q->bodyContact = 0u;
    if (q->dt <= 0.0f)
        return;

    glm::vec3 pos(q->position[0], q->position[1], q->position[2]);
    glm::vec3 vel(q->velocity[0], q->velocity[1], q->velocity[2]);
    // Invalid input must never reach the broadphase; keep the actor in place
    // rather than propagating NaN into the spatial index.
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) ||
        !std::isfinite(pos.z) || !std::isfinite(vel.x) ||
        !std::isfinite(vel.y) || !std::isfinite(vel.z)) {
        q->outVelocity[0] = q->outVelocity[1] = q->outVelocity[2] = 0.0f;
        q->handled = 1u;   // keep ownership; never hand NaN to the fallback
        return;
    }

    // The package is the single collision owner. `ensureWorld` must succeed for
    // a solve to be meaningful; if the world cache cannot be built the caller is
    // told the solve did not happen (handled = 0) instead of being handed a
    // silently-uncollided position that would let the actor fall through.
    if (!ensureWorld(host))
        return;

    const int colCount = std::min((int)q->colliderCount, (int)COLLISION_MAX_COLLIDERS);
    ColliderRuntime cols[COLLISION_MAX_COLLIDERS];
    for (int i = 0; i < colCount; ++i) {
        const CollisionColliderV1& c = q->colliders[i];
        cols[i].localOffset = glm::vec3(c.position[0], c.position[1], c.position[2]) - pos;
        cols[i].radius = c.radius > 0.0f ? c.radius : 0.1f;
        cols[i].halfHeight = c.halfHeight;
        cols[i].partId = c.partId;
        cols[i].policyId = c.policyId;
        cols[i].body = c.shape == COLLISION_SHAPE_SPHERE;
    }

    bool grounded = false, collided = false, worldContact = false,
         bodyContact = false, bounced = false;
    ActorContact contactAccum[COLLISION_MAX_CONTACTS];
    int contactAccumCount = 0;
    ActorContact impactAccum[COLLISION_MAX_IMPACTS];
    int impactAccumCount = 0;

    // One swept-union-AABB gather for the whole tick. Every substep tests only
    // this shared candidate list, so the broadphase cost does not scale with
    // total map triangles and never re-queries per sphere.
    const double bpStart = nowMs();
    glm::vec3 sweepMin, sweepMax;
    sweptUnionAABB(pos, cols, colCount, vel * q->dt, sweepMin, sweepMax);
    std::vector<std::uint32_t> candidates;
    gatherCandidates(sweepMin, sweepMax, candidates);
    const double bpMs = nowMs() - bpStart;
    const double narrowStart = nowMs();

    // Establish resting contacts before integrating.
    resolveOnce(pos, vel, q->entityId, q->tick, cols, colCount, candidates,
                grounded, collided, worldContact, bodyContact, bounced,
                contactAccum, contactAccumCount, impactAccum, impactAccumCount);

    const float speed = glm::length(vel);
    int steps = (int)std::ceil(speed * q->dt / kMinSubstepMove);
    steps = std::max(1, std::min(steps, kMaxSubsteps));
    const float stepDt = q->dt / (float)steps;
    const glm::vec3 stepMove = vel * stepDt;
    // Re-gather mid-sweep only for very long moves; the swept AABB already
    // covers the full path, this is a safety net for pathological speeds.
    const float moveDistance = speed * q->dt;
    const bool reGather = moveDistance > kMaxSweepReGatherDistance;
    for (int s = 0; s < steps; ++s) {
        pos += stepMove;
        if (reGather && s == steps / 2) {
            glm::vec3 midMin, midMax;
            sweptUnionAABB(pos, cols, colCount, vel * stepDt * (float)(steps - s),
                           midMin, midMax);
            gatherCandidates(midMin, midMax, candidates);
        }
        for (int pass = 0; pass < kResolvePasses; ++pass) {
            resolveOnce(pos, vel, q->entityId, q->tick, cols, colCount,
                        candidates, grounded, collided, worldContact,
                        bodyContact, bounced, contactAccum, contactAccumCount,
                        impactAccum, impactAccumCount);
        }
    }

    if (grounded && !bounced && vel.z > -kGroundSnapEpsilon &&
        vel.z < kGroundSnapEpsilon)
        vel.z = 0.0f;

    // Throttled phase timing: one summary per interval, never per solve.
    {
        TimingStats& t = timingStats();
        const double narrowMs = nowMs() - narrowStart;
        t.broadphaseMs += bpMs;
        t.narrowphaseMs += narrowMs;
        t.solverMs += bpMs + narrowMs;
        t.solves++;
        t.candidates += candidates.size();
        t.large += (std::uint64_t)worldCache().always.size();
        t.contacts += (std::uint64_t)contactAccumCount;
        t.sinceLogSeconds += q->dt;
        if (t.sinceLogSeconds >= kTimingLogIntervalSeconds && t.solves > 0) {
            const double inv = 1.0 / (double)t.solves;
            std::printf(
                "[COLLISION PACKAGE] solves=%llu broadMs=%.3f narrowMs=%.3f "
                "totalMs=%.3f avgCandidates=%.1f large=%llu avgContacts=%.1f\n",
                (unsigned long long)t.solves, t.broadphaseMs * inv,
                t.narrowphaseMs * inv, t.solverMs * inv,
                (double)t.candidates * inv, (unsigned long long)t.large,
                (double)t.contacts * inv);
            t = TimingStats{};
        }
    }

    q->outPosition[0] = pos.x;
    q->outPosition[1] = pos.y;
    q->outPosition[2] = pos.z;
    q->outVelocity[0] = vel.x;
    q->outVelocity[1] = vel.y;
    q->outVelocity[2] = vel.z;
    q->grounded = grounded ? 1u : 0u;
    q->worldContact = worldContact ? 1u : 0u;
    q->bodyContact = bodyContact ? 1u : 0u;

    for (int i = 0; i < contactAccumCount &&
                    q->contactCount < COLLISION_MAX_CONTACTS; ++i) {
        CollisionContactV1& c = q->contacts[q->contactCount++];
        c.sourceEntity = q->entityId;
        c.sourcePart = contactAccum[i].partId;
        c.targetKind = 0;
        c.targetEntity = 0;
        c.targetPart = 0;
        c.triangle = contactAccum[i].tri;
        c.point[0] = contactAccum[i].point.x;
        c.point[1] = contactAccum[i].point.y;
        c.point[2] = contactAccum[i].point.z;
        c.normal[0] = contactAccum[i].normal.x;
        c.normal[1] = contactAccum[i].normal.y;
        c.normal[2] = contactAccum[i].normal.z;
        c.penetration = contactAccum[i].penetration;
        c.incomingSpeed = contactAccum[i].incoming;
    }

    for (int i = 0; i < impactAccumCount &&
                    q->impactCount < COLLISION_MAX_IMPACTS; ++i) {
        CollisionImpactV1& ev = q->impacts[q->impactCount++];
        ev.entityId = q->entityId;
        ev.partId = impactAccum[i].partId;
        ev.position[0] = impactAccum[i].point.x;
        ev.position[1] = impactAccum[i].point.y;
        ev.position[2] = impactAccum[i].point.z;
        ev.normal[0] = impactAccum[i].normal.x;
        ev.normal[1] = impactAccum[i].normal.y;
        ev.normal[2] = impactAccum[i].normal.z;
        ev.size = kImpactSphereSize;
        ev.lifetimeTicks = kImpactSphereLifetimeTicks;
    }

    if ((q->flags & COLLISION_SOLVE_SPAWN_IMPACTS) != 0u) {
        for (std::uint32_t i = 0; i < q->impactCount; ++i)
            spawnImpactSphere(host, q->impacts[i]);
    }
    q->handled = 1u;
}

void MIMITA_GAME_CALL onCollisionSolve(void* host, CollisionSolveV1* q)
{
    solve(host, q);
}

} // namespace

void collisionSolve(void* host, CollisionSolveV1* q)
{
    solve(host, q);
}

const MimitaHotPackage::CapabilityRegistrar s_collisionProvider{
    {GAME_CAP_COLLISION, GAME_SIG_COLLISION, 0,
     reinterpret_cast<void*>(&onCollisionSolve), "collision.main"}};

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
