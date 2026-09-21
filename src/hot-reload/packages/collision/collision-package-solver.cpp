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
#include "hot-reload/packages/collision/collision-log.h"
#include "hot-reload/packages/collision/collision-world.h"

namespace HotCollisionPackage {

namespace {

// ── Solve tuning (edit live) ────────────────────────────────────────────────
// These are the hot owner's values; edit and save to retune while the EXE runs.
constexpr float kWalkableSlopeDot = 0.80f;   // old cold MAX_WALKABLE_SLOPE_DOT
constexpr float kSkin = 0.001f;
constexpr int   kResolvePasses = 3;
constexpr int   kMaxSubsteps = 8;
constexpr float kMinSubstepMove = 0.05f;
constexpr float kGroundSnapEpsilon = 0.05f;
constexpr float kMaxCapsulePush = 1000.0f;
// Limbs and weapons are authoritative over the actor position, exactly like the
// capsule: a limb sunk into a wall pushes the body out instead of only nudging
// it (so an arm on a ledge holds the player). Same cap as the capsule.
constexpr float kMaxBodyPush = 1000.0f;
constexpr int   kMaxRawContacts = 64;
// Contact tolerance: a sphere within this distance of a triangle is treated as
// touching even when it is not penetrating. Without it a resting capsule sits at
// exactly `radius`, the contact is discarded, grounding flickers off, gravity is
// applied, and the actor can never jump. Live-editable: change and save.
constexpr float kContactTolerance = 0.02f;
// Ground must be a contact near the feet, matching the old cold rule, so a
// ceiling/overhead contact with a walkable normal cannot ground the actor.
constexpr float kGroundMaxHeightAboveFeet = 0.15f;
// Old cold-path grounding stability values (preserved behavior).
constexpr float kContactHysteresisSeconds = 0.033f;   // world contact memory
constexpr float kStableGroundGraceSeconds = 0.08f;    // ground loss grace
// Old cold doGroundSnap/doFloorRecovery distance: how far below the capsule a
// walkable surface is still pulled up to resting contact. This is what stops
// the actor hovering a fraction above the floor after a fast landing.
constexpr float kGroundSettleDistance = 0.25f;
constexpr float kGroundSettleEpsilon = 0.005f;
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

// Per-entity contact/ground memory, keyed by stable EntityId (never a pointer,
// so it survives generation changes). Mirrors the old cold Player::GroundState
// hysteresis: a real contact keeps `hasWorldContact` true for a short window,
// and ground loss is only accepted after a grace window. Without this a slope
// seam or a contact at exactly the tolerance boundary flickers grounded off.
struct GroundMemory {
    float worldContactLostTimer = 0.0f;
    float groundLostTimer = 0.0f;
    bool groundSticky = false;   // was grounded within the grace window
    std::uint64_t lastSolveTick = 0;
};
std::unordered_map<std::uint64_t, GroundMemory>& groundMemoryMap()
{
    static std::unordered_map<std::uint64_t, GroundMemory> m;
    return m;
}

// Per-contact logging switch. On by default so "am I touching anything" is
// answerable from the stream alone; set false to reduce volume in a hot loop.
// The kernel still aggregates identical repeats.
constexpr bool kLogEveryContact = true;
inline bool contactLogEnabled() { return kLogEveryContact; }

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
    std::uint64_t declines = 0;
    std::uint64_t noContact = 0;
    float sinceLogSeconds = 0.0f;
};

// Unconditional per-second heartbeat for the "no collision" failure mode. The
// kernel aggregates repeats, but a decline must always be visible, so the
// package throttles it itself. Live-editable.
constexpr float kDeclineLogIntervalSeconds = 1.0f;
struct DeclineLog {
    float sinceLogSeconds = 0.0f;
    std::uint64_t suppressed = 0;
};
DeclineLog& declineLog()
{
    static DeclineLog d;
    return d;
}
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
    // Oriented capsule axis and half segment length. Z-aligned capsules use
    // axis (0,0,1) and halfHeight - radius; oriented capsules use the direction
    // from `position` to `endPosition`.
    glm::vec3 axis{0.0f, 0.0f, 1.0f};
    float halfSeg = 0.0f;
    std::uint32_t partId;
    std::uint32_t policyId;
    bool body;
};

struct ActorContact {
    std::uint32_t partId;
    std::uint32_t policyId;
    bool body;
    bool touching;      // within tolerance but not penetrating: classify, no push
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
        const glm::vec3 ext = glm::vec3(cols[i].radius) +
                              glm::abs(cols[i].axis) * cols[i].halfSeg;
        mn = glm::min(mn, c - ext);
        mx = glm::max(mx, c + ext);
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
        const float halfSeg = col.halfSeg;
        const glm::vec3 samples[3] = {c + col.axis * halfSeg, c,
                                      c - col.axis * halfSeg};
        const int sampleCount = halfSeg > 0.0f ? 3 : 1;
        for (int s = 0; s < sampleCount && n < maxOut; ++s) {
            SphereHit hits[8];
            const int hc = gatherSphereHits(samples[s], col.radius,
                                            kContactTolerance, candidates, hits, 8);
            for (int h = 0; h < hc && n < maxOut; ++h) {
                out[n].partId = col.partId;
                out[n].policyId = col.policyId;
                out[n].body = col.body;
                out[n].touching = hits[h].touching != 0u;
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
        // Merge key includes the touching flag so a penetrating contact and a
        // touching-only contact on the same triangle stay distinguishable.
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
        // Only touching if every contributor on this triangle is touching.
        merged[found].touching = merged[found].touching && raw[i].touching;
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

// Old cold doGroundSnap: pull the actor down to a walkable surface within the
// settle distance so a fast landing rests exactly on the floor instead of
// hovering a fraction above it. Only snaps down (never up) and only to a
// near-feet walkable surface. Returns true when it grounded the actor.
bool settleToGround(glm::vec3& pos, glm::vec3& vel,
                    const ColliderRuntime* cols, int colCount,
                    const std::vector<std::uint32_t>& candidates)
{
    if (cols[0].partId != COLLISION_PART_CAPSULE && colCount > 0)
        return false;
    for (int i = 0; i < colCount; ++i) {
        if (cols[i].partId != COLLISION_PART_CAPSULE)
            continue;
        const glm::vec3 c = pos + cols[i].localOffset;
        const float halfSeg = cols[i].halfHeight > cols[i].radius
                                  ? cols[i].halfHeight - cols[i].radius
                                  : 0.0f;
        // Bottom sphere centre; its surface is the actor's feet.
        const glm::vec3 bottomCenter = c - glm::vec3(0.0f, 0.0f, halfSeg);
        const float feetZ = c.z - (cols[i].halfHeight > cols[i].radius
                                       ? cols[i].halfHeight
                                       : cols[i].radius);
        // Probe a sphere just below the feet for a walkable surface.
        SphereHit hits[8];
        const int hc = gatherSphereHits(bottomCenter,
                                        cols[i].radius + kGroundSettleDistance,
                                        kContactTolerance, candidates, hits, 8);
        float bestZ = -1e30f;
        for (int h = 0; h < hc; ++h) {
            if (hits[h].normal.z <= kWalkableSlopeDot)
                continue;
            const float surfaceZ = hits[h].point.z;
            if (surfaceZ > bestZ && surfaceZ < c.z + 1.0f)
                bestZ = surfaceZ;
        }
        if (bestZ <= -1e29f)
            continue;
        const float distance = feetZ - bestZ;
        if (distance > 0.0f && distance < kGroundSettleDistance) {
            pos.z -= distance - kGroundSettleEpsilon;
            if (vel.z < 0.0f)
                vel.z = 0.0f;
            return true;
        }
    }
    return false;
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

    bool hasAuthoritativeBody = false;
    for (int i = 0; i < colCount; ++i)
        hasAuthoritativeBody = hasAuthoritativeBody || cols[i].body;

    // Actor feet plane, in the same frame as `pos`, for the old cold rule that
    // only a contact near the feet counts as ground. Include leg proxies so the
    // visible body can establish support instead of leaving the root hovering.
    float feetZ = pos.z;
    for (int i = 0; i < colCount; ++i) {
        if (cols[i].partId != COLLISION_PART_CAPSULE &&
            cols[i].partId != COLLISION_PART_LEFT_LEG &&
            cols[i].partId != COLLISION_PART_RIGHT_LEG)
            continue;
        const glm::vec3 c = pos + cols[i].localOffset;
        const float bottom = c.z - (cols[i].halfHeight > cols[i].radius
                                        ? cols[i].halfHeight
                                        : cols[i].radius);
        feetZ = std::min(feetZ, bottom);
    }

    for (int i = 0; i < mc; ++i) {
        ActorContact& c = merged[i];
        // Classify ground with the old cold rule: walkable normal and the
        // contact point near the feet. A touching-only contact grounds the actor
        // without pushing, so a resting capsule stays stable.
        const bool walkable = c.normal.z > kWalkableSlopeDot;
        const bool nearFeet = c.point.z <= feetZ + kGroundMaxHeightAboveFeet;
        const bool isGround = walkable && nearFeet;
        if (isGround)
            grounded = true;

        worldContact = true;
        if (c.body)
            bodyContact = true;
        collided = true;

        // The capsule is a support/step helper only. Once body colliders are
        // available, capsule wall/ceiling penetration must not veto the
        // authoritative body pose. Feet still establish support below.
        if (hasAuthoritativeBody && c.partId == COLLISION_PART_CAPSULE && !isGround)
            continue;

        if (c.touching) {
            // Touching-only: no depenetration and no velocity response. Still
            // cancels into-surface velocity so a resting actor does not creep.
            const float into = glm::dot(vel, c.normal);
            if (into < 0.0f)
                vel -= c.normal * into;
            continue;
        }

        const float cap = c.body ? kMaxBodyPush : kMaxCapsulePush;
        const float push = std::min(c.penetration + kSkin, cap);
        pos += c.normal * push;

        if (isGround) {
            // Ground settles, it does not bounce: cancel the into-ground
            // component and slide. This is the old cold ground response; the
            // bounce policy is for walls, ceilings, and body parts.
            const float into = glm::dot(vel, c.normal);
            if (into < 0.0f)
                vel -= c.normal * into;
        } else {
            applyVelocityResponse(vel, c, entity, tick, bounced);
        }
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

// Throttled COLLISION/decline record. Reports why a solve did nothing so the
// "fall through the world" failure is visible in events.jsonl live.
void logDecline(void* host, const CollisionSolveV1* q, const char* why)
{
    DeclineLog& d = declineLog();
    d.suppressed++;
    d.sinceLogSeconds += q->dt;
    if (d.sinceLogSeconds < kDeclineLogIntervalSeconds)
        return;

    const WorldCache& c = worldCache();
    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "collision declined why=%s cachedTris=%u ready=%d pos=(%.2f %.2f %.2f) vz=%.2f suppressed=%llu",
                  why, c.total, (int)c.ready, q->position[0], q->position[1],
                  q->position[2], q->velocity[2],
                  (unsigned long long)(d.suppressed - 1));
    collisionLogFull(host, 4u, "COLLISION", "collision.declined", msg, why,
                     q->entityId, q->entityId, q->actorKind, q->frame,
                     q->serverTick, q->clientTick, q->tick);
    d.sinceLogSeconds = 0.0f;
    d.suppressed = 0;
}

// Emitted on the first solve of a new (entity, tick) run and whenever the
// contact/no-contact verdict changes, so "the solver was never reached" is
// distinguishable from "reached but found nothing".
std::unordered_map<std::uint64_t, std::int32_t>& lastContactVerdict()
{
    static std::unordered_map<std::uint64_t, std::int32_t> m;
    return m;
}
std::unordered_map<std::uint64_t, bool>& contactVerdictSeen()
{
    static std::unordered_map<std::uint64_t, bool> m;
    return m;
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
        logDecline(host, q, "non_finite_input");
        return;
    }

    // The package is the single collision owner. `ensureWorld` must succeed for
    // a solve to be meaningful; if the world cache cannot be built the caller is
    // told the solve did not happen (handled = 0) instead of being handed a
    // silently-uncollided position that would let the actor fall through.
    if (!ensureWorld(host)) {
        // This is the "no collision at all" failure mode: surface it live.
        logDecline(host, q, "world_unavailable");
        return;
    }

    const int colCount = std::min((int)q->colliderCount, (int)COLLISION_MAX_COLLIDERS);
    ColliderRuntime cols[COLLISION_MAX_COLLIDERS];
    for (int i = 0; i < colCount; ++i) {
        const CollisionColliderV1& c = q->colliders[i];
        const glm::vec3 startW(c.position[0], c.position[1], c.position[2]);
        const glm::vec3 endW(c.endPosition[0], c.endPosition[1],
                             c.endPosition[2]);
        const glm::vec3 seg = endW - startW;
        const float segLen = glm::length(seg);
        cols[i].radius = c.radius > 0.0f ? c.radius : 0.1f;
        if ((c.flags & COLLISION_COLLIDER_ORIENTED_CAPSULE) != 0u &&
            segLen > 1e-4f) {
            // Oriented capsule: segment midpoint is the collider centre.
            cols[i].axis = seg / segLen;
            cols[i].halfSeg = segLen * 0.5f;
            cols[i].localOffset = (startW + endW) * 0.5f - pos;
            cols[i].halfHeight = cols[i].halfSeg + cols[i].radius;
        } else {
            cols[i].axis = glm::vec3(0.0f, 0.0f, 1.0f);
            const float hh = c.halfHeight;
            cols[i].halfSeg = hh > cols[i].radius ? hh - cols[i].radius : 0.0f;
            cols[i].localOffset = startW - pos;
            cols[i].halfHeight = hh;
        }
        cols[i].partId = c.partId;
        cols[i].policyId = c.policyId;
        // Honor the explicit collider roles. A body-authoritative collider is
        // never treated as a helper; a helper is never authoritative. Callers
        // that set neither flag fall back to the legacy sphere-shape inference.
        const bool flagHelper = (c.flags & COLLISION_COLLIDER_HELPER) != 0u;
        const bool flagBodyAuthoritative =
            (c.flags & COLLISION_COLLIDER_BODY_AUTHORITATIVE) != 0u;
        cols[i].body = flagBodyAuthoritative
                           ? true
                           : (flagHelper ? false
                                         : c.shape == COLLISION_SHAPE_SPHERE);
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

    // Contact/ground hysteresis, matching the old cold Player::GroundState:
    //  - a real contact refreshes `worldContactLostTimer`, so worldContact stays
    //    sticky for a short window across slope seams and sub-cell edges;
    //  - `groundLostTimer` counts time since the last real ground contact, and
    //    the actor stays grounded through a short grace window.
    // This is what keeps the actor jump-eligible and the walk animation stable
    // instead of flickering grounded off for one tick.
    {
        GroundMemory& gm = groundMemoryMap()[q->entityId];
        const bool realContact = worldContact || bodyContact;
        if (realContact)
            gm.worldContactLostTimer = kContactHysteresisSeconds;
        else
            gm.worldContactLostTimer =
                std::max(0.0f, gm.worldContactLostTimer - q->dt);

        const bool realGround = grounded;
        if (realGround) {
            gm.groundLostTimer = 0.0f;
            gm.groundSticky = true;
        } else {
            gm.groundLostTimer += q->dt;
            if (gm.groundLostTimer >= kStableGroundGraceSeconds)
                gm.groundSticky = false;
        }

        const bool hasWorldContact = gm.worldContactLostTimer > 0.0f;
        // Persist ground only through the grace window after real ground; never
        // let a wall contact alone keep the actor grounded.
        grounded = realGround || gm.groundSticky;
        worldContact = hasWorldContact;
    }

    // Ground settle, matching the old cold doGroundSnap: if a walkable surface
    // is within the settle distance below the feet, rest exactly on it. This
    // keeps walking/standing stable and jump-eligible after a fast landing.
    if (!bounced && vel.z <= 0.0f && settleToGround(pos, vel, cols, colCount,
                                                    candidates))
        grounded = true;

    if (grounded && !bounced && vel.z > -kGroundSnapEpsilon &&
        vel.z < kGroundSnapEpsilon)
        vel.z = 0.0f;

    // Throttled phase timing: one summary per interval, never per solve. The
    // summary is emitted into events.jsonl so collision behavior is observable
    // live (candidates/contacts prove the broadphase saw geometry).
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
        if (!collided)
            t.noContact++;
        t.sinceLogSeconds += q->dt;
        if (t.sinceLogSeconds >= kTimingLogIntervalSeconds && t.solves > 0) {
            const double inv = 1.0 / (double)t.solves;
            const WorldCache& wc = worldCache();
            char msg[256];
            std::snprintf(
                msg, sizeof(msg),
                "solves=%llu worldTris=%u broadMs=%.3f narrowMs=%.3f totalMs=%.3f "
                "avgCand=%.1f large=%llu avgContacts=%.1f noContact=%llu grounded=%d",
                (unsigned long long)t.solves, wc.total, t.broadphaseMs * inv,
                t.narrowphaseMs * inv, t.solverMs * inv,
                (double)t.candidates * inv, (unsigned long long)t.large,
                (double)t.contacts * inv, (unsigned long long)t.noContact,
                (int)grounded);
            collisionLogFull(host, 2u, "COLLISION", "collision.solve.summary",
                             msg, collided ? "contact" : "no_contact",
                             q->entityId, q->entityId, q->actorKind, q->frame,
                             q->serverTick, q->clientTick, q->tick);
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

    // Per-contact record: names the actor, the collider/part, the world
    // triangle index on the touched surface, the contact point, the normal, and
    // the penetration. This is the "did the code think I touched anything"
    // evidence, with the triangle that was actually touched.
    if (q->contactCount > 0 && contactLogEnabled()) {
        for (std::uint32_t i = 0; i < q->contactCount; ++i) {
            const CollisionContactV1& c = q->contacts[i];
            char msg[256];
            std::snprintf(
                msg, sizeof(msg),
                "part=%u tri=%d point=(%.3f %.3f %.3f) n=(%.2f %.2f %.2f) "
                "pen=%.4f incoming=%.2f colCount=%u",
                c.sourcePart, c.triangle, c.point[0], c.point[1], c.point[2],
                c.normal[0], c.normal[1], c.normal[2], c.penetration,
                c.incomingSpeed, (unsigned)colCount);
            collisionLogFull(host, 2u, "COLLISION", "collision.contact", msg,
                             q->grounded ? "grounded" : "contact",
                             q->entityId, q->entityId, q->actorKind, q->frame,
                             q->serverTick, q->clientTick, q->tick);
        }
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

    // One record per accepted impact (the kernel aggregates repeats and
    // rate-limits to one per object/part per tick at the source).
    if (q->impactCount > 0) {
        const CollisionImpactV1& ev = q->impacts[0];
        char msg[192];
        std::snprintf(msg, sizeof(msg),
                      "impact part=%u pos=(%.2f %.2f %.2f) n=(%.2f %.2f %.2f) "
                      "contacts=%u grounded=%d bounced=%d",
                      ev.partId, ev.position[0], ev.position[1], ev.position[2],
                      ev.normal[0], ev.normal[1], ev.normal[2],
                      contactAccumCount, (int)grounded, (int)bounced);
        collisionLogFull(host, 2u, "COLLISION", "collision.impact", msg,
                         "impact", q->entityId, q->entityId, q->actorKind,
                         q->frame, q->serverTick, q->clientTick, q->tick);
    }

    // Verdict change: emitted immediately whenever an entity flips between
    // touching and not touching, so the first-ever tick and every transition is
    // captured without waiting for the 1s summary.
    {
        const std::int32_t verdict = collided ? 1 : 0;
        std::int32_t& last = lastContactVerdict()[q->entityId];
        bool& s = contactVerdictSeen()[q->entityId];
        if (!s || last != verdict) {
            s = true;
            last = verdict;
            char msg[224];
            std::snprintf(
                msg, sizeof(msg),
                "touch=%d worldContact=%d grounded=%d candidates=%u contacts=%u "
                "colliders=%u pos=(%.2f %.2f %.2f) vz=%.2f",
                (int)collided, (int)worldContact, (int)grounded,
                (unsigned)candidates.size(), (unsigned)contactAccumCount,
                (unsigned)colCount, pos.x, pos.y, pos.z, vel.z);
            collisionLogFull(host, 2u, "COLLISION", "collision.touch", msg,
                             collided ? "touch" : "none", q->entityId,
                             q->entityId, q->actorKind, q->frame, q->serverTick,
                             q->clientTick, q->tick);
        }
    }

    q->handled = 1u;
}

void MIMITA_GAME_CALL onCollisionSolve(void* host, CollisionSolveV1* q)
{
    solve(host, q);
}

// Generic capsule move: the same `solve` as collision.main, exposed as a plain
// POD capability so cold callers (server headless movement, spawn validation,
// the parity harness) do not need the DLL-only collision ABI.
void MIMITA_GAME_CALL onCapsuleMove(void* host, GameCapsuleMoveV1* m)
{
    if (!m || m->structSize != sizeof(GameCapsuleMoveV1))
        return;
    m->handled = 0u;
    CollisionSolveV1 q{};
    q.entityId = m->entityId;
    q.dt = m->dt;
    q.yaw = m->yaw;
    q.sizeScale = m->sizeScale > 0.0f ? m->sizeScale : 1.0f;
    q.mask = COLLISION_MASK_WORLD;
    q.flags = 0u;
    for (int i = 0; i < 3; ++i) {
        q.position[i] = m->position[i];
        q.velocity[i] = m->velocity[i];
    }
    CollisionColliderV1& c = q.colliders[q.colliderCount++];
    c.partId = COLLISION_PART_CAPSULE;
    c.shape = COLLISION_SHAPE_CAPSULE;
    c.policyId = COLLISION_POLICY_CAPSULE;
    c.flags = COLLISION_COLLIDER_HELPER;
    c.radius = m->radius > 0.0f ? m->radius : 0.4f;
    c.halfHeight = m->halfHeight > 0.0f ? m->halfHeight : 0.9f;
    for (int i = 0; i < 3; ++i)
        c.position[i] = m->position[i];
    solve(host, &q);
    if (q.handled == 0u)
        return;
    for (int i = 0; i < 3; ++i) {
        m->outPosition[i] = q.outPosition[i];
        m->outVelocity[i] = q.outVelocity[i];
    }
    m->grounded = q.grounded;
    m->collided = (q.worldContact || q.bodyContact) ? 1u : 0u;
    m->handled = 1u;
}

} // namespace

void collisionSolve(void* host, CollisionSolveV1* q)
{
    solve(host, q);
}

void collisionResetRuntimeState()
{
    bounceTickMap().clear();
    groundMemoryMap().clear();
    lastContactVerdict().clear();
    contactVerdictSeen().clear();
}

const MimitaHotPackage::CapabilityRegistrar s_collisionProvider{
    {GAME_CAP_COLLISION, GAME_SIG_COLLISION, 0,
     reinterpret_cast<void*>(&onCollisionSolve), "collision.main"}};

const MimitaHotPackage::CapabilityRegistrar s_capsuleMoveProvider{
    {GAME_CAP_CAPSULE_MOVE, gameHash("sig.collision.capsuleMove.v1"), 0,
     reinterpret_cast<void*>(&onCapsuleMove), "collision.capsuleMove"}};

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
