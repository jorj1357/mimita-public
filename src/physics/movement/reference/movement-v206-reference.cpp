// 09 21 2026
/* purpose
* Frozen v2.0.6 movement reference implementation.
* Reconstructed from tag `v2.0.6`:
*   src/physics/physics-mini.cpp
*   src/physics/movement/physics-gravity.cpp
*   src/physics/movement/physics-freeze.cpp
*   src/physics/movement/physics-ground-return.cpp
*   src/physics/movement/physics-walk.cpp
*   src/physics/movement/physics-dash.cpp
*   src/physics/movement/physics-down-dash.cpp
*   src/physics/movement/physics-jump.cpp
*   src/physics/movement/physics-friction.cpp
*   src/physics/movement/physics-collision.cpp (swept GLB capsule pipeline)
* plus the effective v2.0.6 config/movement.json values.
*
* No Debug::, no DebugVis, no effects, no audio, no rendering, no Player&,
* no Perf, no PlayerSettings. Deterministic plain data.
*
* Collision scope: the v2.0.6 GLB triangle pipeline (gather -> swept capsule
* with step-up -> batched depenetration -> re-sweep -> ground snap -> emergency
* escape -> final velocity projection -> rotation-safety + final safety net).
* The legacy Block/AABB map path and limb/weapon body samples are NOT included;
* those are separate migration steps.
*/
#include "physics/movement/reference/movement-v206-reference.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace MimitaV206 {
namespace {

using glm::vec2;
using glm::vec3;

// ── Frozen v2.0.6 physics constants (src/physics/config.h) ──────────────────
constexpr float kAlmostZero = 0.00001f;
constexpr float kCollisionSkin = 0.02f;
constexpr float kSurfaceSlop = 0.01f;          // GLB_SURFACE_SLOP
constexpr float kMaxCorrection = 2.0f;         // GLB_MAX_CORRECTION
constexpr float kMaxMovePerIter = 0.6f;        // GLB_MAX_MOVE_PER_ITER
constexpr int   kSweepBaseIters = 5;           // GLB_SWEEP_BASE_ITERS
constexpr int   kSweepMaxIters = 12;           // GLB_SWEEP_MAX_ITERS
constexpr float kMaxStepHeight = 0.25f;
constexpr float kSeamTolerance = 0.035f;       // PlayerSettings default

struct Capsule {
    vec3 a{0.0f};
    vec3 b{0.0f};
    float r{0.0f};
};

struct SweepHit {
    bool hit = false;
    float time = 1.0f;
    vec3 point{0.0f};
    vec3 normal{0.0f, 0.0f, 1.0f};
    int triangleIndex = -1;
};

struct Contact {
    vec3 point{0.0f};
    vec3 normal{0.0f, 0.0f, 1.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
};

struct AABB {
    vec3 min{0.0f};
    vec3 max{0.0f};
};

struct RecoveryContact {
    vec3 normal{0.0f, 0.0f, 1.0f};
    vec3 point{0.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
};

Capsule capsuleFromState(const PlayerState& p, const Config& cfg)
{
    Capsule cap;
    cap.r = (cfg.playerRadius > 0.0f ? cfg.playerRadius : 0.7f) * p.sizeScale;
    float height = (cfg.playerHeight > 0.0f ? cfg.playerHeight : 3.6f) * p.sizeScale;
    float half = height * 0.5f;
    float seg = std::max(0.0f, half - cap.r);
    cap.a = p.pos - vec3(0.0f, 0.0f, seg);
    cap.b = p.pos + vec3(0.0f, 0.0f, seg);
    return cap;
}

// ── v2.0.6 physics-walk.cpp ─────────────────────────────────────────────────
void applyGroundFriction(vec2& velXY, const Config& cfg, float dt)
{
    float speed = glm::length(velXY);
    if (speed > 0.1f) {
        float drop = speed * cfg.groundFrictionAmount * dt;
        float newSpeed = std::max(0.0f, speed - drop);
        velXY *= newSpeed / speed;
    }
}

void applyGroundAccelerate(vec2& velXY, const vec2& wishDir, const Config& cfg, float dt)
{
    float currentSpeed = glm::dot(velXY, wishDir);
    float addSpeed = cfg.moveSpeed - currentSpeed;
    if (addSpeed <= 0.0f)
        return;
    float accelSpeed = cfg.groundAccelerate * cfg.moveSpeed * dt;
    addSpeed = std::min(addSpeed, accelSpeed);
    velXY += wishDir * addSpeed;
}

void applyAirAccelerate(vec2& velXY, const vec2& wishDir, const Config& cfg, float dt)
{
    float currentSpeed = glm::dot(velXY, wishDir);
    float addSpeed = cfg.moveSpeed - currentSpeed;
    if (addSpeed <= 0.0f)
        return;
    float accelSpeed = cfg.airAccelAmount * cfg.moveSpeed * dt;
    addSpeed = std::min(addSpeed, accelSpeed);
    velXY += wishDir * addSpeed;
}

void steerExternalImpulse(PlayerState& p, const vec2& wishDir, const Config& cfg, float dt)
{
    vec2 impulseXY(p.externalImpulse.x, p.externalImpulse.y);
    float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed <= 0.001f)
        return;

    vec2 impulseDir = impulseXY / impulseSpeed;
    float alignment = glm::dot(wishDir, impulseDir);
    float steerT = std::min(1.0f, cfg.externalImpulseSteerRate * dt);
    vec2 steeredDir = glm::mix(impulseDir, wishDir, steerT);
    if (glm::length(steeredDir) > 0.001f)
        impulseDir = glm::normalize(steeredDir);

    if (alignment < 0.0f) {
        float brake = std::exp(-cfg.externalImpulseBrakeRate * -alignment * dt);
        impulseSpeed *= brake;
    }

    impulseXY = impulseDir * impulseSpeed;
    p.externalImpulse.x = impulseXY.x;
    p.externalImpulse.y = impulseXY.y;
}

void doWalk(PlayerState& p, const vec2& wishMoveXY, bool jumpHeld, const Config& cfg, float dt)
{
    float wishLen = glm::length(wishMoveXY);
    vec2 velXY(p.vel.x, p.vel.y);

    if (p.stableOnGround && !jumpHeld) {
        if (wishLen > 0.001f) {
            vec2 wishDir = wishMoveXY / wishLen;
            steerExternalImpulse(p, wishDir, cfg, dt);
            applyGroundAccelerate(velXY, wishDir, cfg, dt);
        } else {
            applyGroundFriction(velXY, cfg, dt);
        }
        p.vel.x = velXY.x;
        p.vel.y = velXY.y;
        return;
    }

    if (wishLen > 0.001f) {
        vec2 wishDir = wishMoveXY / wishLen;
        steerExternalImpulse(p, wishDir, cfg, dt);
        applyAirAccelerate(velXY, wishDir, cfg, dt);
        p.vel.x = velXY.x;
        p.vel.y = velXY.y;
    } else {
        if (glm::length(vec2(p.externalImpulse.x, p.externalImpulse.y)) > 0.001f) {
            vec2 impulseDir = glm::normalize(vec2(p.externalImpulse.x, p.externalImpulse.y));
            steerExternalImpulse(p, impulseDir, cfg, dt);
        }
    }
}

// ── v2.0.6 physics-gravity.cpp ──────────────────────────────────────────────
void doGravity(PlayerState& p, const Config& cfg, float dt)
{
    float safeDt = std::min(dt, 0.033f);
    p.vel.z += cfg.gravity * safeDt;
    if (p.vel.z < -cfg.maxFallSpeed)
        p.vel.z = -cfg.maxFallSpeed;
}

// ── v2.0.6 physics-freeze.cpp ───────────────────────────────────────────────
float freezeVelocityMultiplier(float t)
{
    if (t < 2.5f) {
        float x = t / 2.5f;
        return x * x * 0.2f;
    }
    float x = (t - 2.5f) / 2.5f;
    return 0.2f + x * x * 0.8f;
}

void doFreeze(PlayerState& p, bool freezeHeld, const Config& cfg, float dt)
{
    bool freezeJustPressed = freezeHeld && !p.freezeHeldPrev;
    bool freezeJustReleased = !freezeHeld && p.freezeHeldPrev;
    p.freezeHeldPrev = freezeHeld;

    if (freezeJustPressed) {
        if (!p.freezeAvailable)
            return;
        p.freezeActive = true;
        p.freezeTimer = 0.0f;
        p.freezeAvailable = false;
    }

    if (freezeJustReleased && p.freezeActive)
        p.freezeActive = false;

    if (!p.freezeActive)
        return;

    p.freezeTimer += dt;
    if (p.freezeTimer > cfg.freezeMaxTime)
        p.freezeTimer = cfg.freezeMaxTime;

    float mult = freezeVelocityMultiplier(p.freezeTimer);
    p.vel.x *= mult;
    p.vel.y *= mult;
    p.vel.z *= mult;
}

// ── v2.0.6 physics-ground-return.cpp ────────────────────────────────────────
void doGroundReturn(PlayerState& p, bool groundReturnPressed)
{
    if (!groundReturnPressed)
        return;
    if (!p.groundReturnAvailable)
        return;
    if (p.onGround)
        return;
    p.vel.z += -150.0f; // GROUND_RETURN_SPEED
    p.groundReturnAvailable = false;
}

// ── v2.0.6 physics-jump.cpp ─────────────────────────────────────────────────
void doJump(PlayerState& p, bool jumpHeld, const Config& cfg, float dt)
{
    p.jumpIntentTimer = std::max(0.0f, p.jumpIntentTimer - dt);
    p.coyoteTimer = std::max(0.0f, p.coyoteTimer - dt);

    if (p.onGround)
        p.coyoteTimer = cfg.coyoteJumpTime;

    if (jumpHeld)
        p.jumpIntentTimer = cfg.jumpBufferTime;

    if (!jumpHeld && p.jumpHeldPrev) {
        p.airJumpArmed = true;
        p.airJumpLocked = false;
    }
    p.jumpHeldPrev = jumpHeld;

    bool onActualGround = p.onGround || p.coyoteTimer > 0.0f;
    bool wantsJump = p.jumpIntentTimer > 0.0f;
    if (!wantsJump)
        return;

    if (onActualGround) {
        p.dashAvailable = true;
        p.vel.z = cfg.jumpStrength;
        p.onGround = false;
        p.coyoteTimer = 0.0f;
        p.jumpIntentTimer = 0.0f;
        p.airJumpsLeft = cfg.airJumpsMax;
        p.airJumpLocked = true;
        p.airJumpArmed = false;
        p.didGroundJump = true;
        return;
    }

    if (p.hasWorldContact) {
        p.vel.z = cfg.jumpStrength;
        p.jumpIntentTimer = 0.0f;
        p.airJumpsLeft = cfg.airJumpsMax;
        p.didGroundJump = true;
        return;
    }

    if (!p.didGroundJump && p.airJumpsLeft > 0 && !p.airJumpLocked &&
        p.airJumpArmed && jumpHeld) {
        p.vel.z = cfg.jumpStrength;
        p.airJumpsLeft--;
        p.airJumpLocked = false;
        p.jumpIntentTimer = 0.0f;
        p.didAirJump = true;
    }
}

// ── v2.0.6 physics-dash.cpp ─────────────────────────────────────────────────
float dashQualityMultiplier(int ticks)
{
    if (ticks <= 1) return 1.00f;
    if (ticks == 2) return 0.85f;
    if (ticks == 3) return 0.70f;
    if (ticks == 4) return 0.55f;
    return 0.40f;
}

void doAirDash(PlayerState& p, const vec2& wishMoveXY, bool triggerPressed,
               bool airborne, const vec3& camForward, const Config& cfg)
{
    if (!triggerPressed) return;
    if (!airborne) return;
    if (!p.dashAvailable) return;
    if (p.freezeActive) return;

    vec2 dashDir = wishMoveXY;
    if (glm::length(dashDir) < 0.001f) {
        dashDir = vec2(camForward.x, camForward.y);
        if (glm::length(dashDir) < 0.001f)
            return;
    }
    dashDir = glm::normalize(dashDir);

    int quality = (p.dashMovementTicks <= 1) ? 0 : (p.dashMovementTicks == 2) ? 1
                : (p.dashMovementTicks == 3) ? 2 : (p.dashMovementTicks == 4) ? 3 : 4;
    float impulse = cfg.airDashImpulse * dashQualityMultiplier(quality);

    p.vel.x += dashDir.x * impulse;
    p.vel.y += dashDir.y * impulse;
    p.dashAvailable = false;
    p.didDash = true;
    p.lastDashQuality = quality;
    p.airJumpsLeft = 0;
}

void doDash(PlayerState& p, const vec2& wishMoveXY, bool dashPressed,
            const vec3& camForward, const Config& cfg)
{
    if (!dashPressed) return;
    if (!p.dashAvailable) return;
    if (p.freezeActive) return;

    vec2 dashDir = wishMoveXY;
    if (glm::length(dashDir) < 0.001f) {
        dashDir = vec2(camForward.x, camForward.y);
        if (glm::length(dashDir) < 0.001f)
            return;
    }
    dashDir = glm::normalize(dashDir);

    vec2 impulse = dashDir * cfg.dashImpulse;
    p.vel.x += impulse.x;
    p.vel.y += impulse.y;

    vec2 velXY(p.vel.x, p.vel.y);
    float speed = glm::length(velXY);
    if (speed > cfg.maxPlayerMoveSpeed) {
        vec2 clamped = (velXY / speed) * cfg.maxPlayerMoveSpeed;
        p.vel.x = clamped.x;
        p.vel.y = clamped.y;
    }

    p.dashAvailable = false;
    p.didDash = true;
}

// ── v2.0.6 physics-down-dash.cpp ────────────────────────────────────────────
void doDownDash(PlayerState& p, bool downDashPressed, const Config& cfg)
{
    if (!downDashPressed) return;
    if (p.freezeActive) return;
    if (!p.downDashAvailable) return;

    p.vel.z += cfg.downDashSpeed;
    p.downDashAvailable = false;
}

// ── v2.0.6 physics-friction.cpp ─────────────────────────────────────────────
void doFriction(PlayerState& p, const Config& cfg, float dt)
{
    dt = std::min(dt, 0.033f);
    float impulseDecay = std::exp(-cfg.externalImpulseDecay * dt);
    p.externalImpulse *= impulseDecay;
    if (glm::length(p.externalImpulse) < cfg.almostZero)
        p.externalImpulse = vec3(0.0f);

    vec2 impulseXY(p.externalImpulse.x, p.externalImpulse.y);
    float impulseSpeed = glm::length(impulseXY);
    if (impulseSpeed > cfg.maxExternalImpulseSpeed) {
        impulseXY *= cfg.maxExternalImpulseSpeed / impulseSpeed;
        p.externalImpulse.x = impulseXY.x;
        p.externalImpulse.y = impulseXY.y;
    }
}

// ── v2.0.6 physics-collision.cpp contact response ───────────────────────────
void applyTouchResets(PlayerState& p, const Config& cfg)
{
    p.airJumpsLeft = cfg.airJumpsMax;
    p.dashAvailable = true;
    p.groundReturnAvailable = true;
    p.downDashAvailable = true;
    p.freezeAvailable = true;
}

void projectVelocityAgainstNormal(PlayerState& p, const vec3& normal)
{
    float into = glm::dot(p.vel, normal);
    if (into < 0.0f)
        p.vel -= normal * into;
}

void applyCollisionContact(PlayerState& p, bool& groundedThisFrame,
                           const vec3& normal, const Config& cfg)
{
    p.hasWorldContact = true;

    if (normal.z > cfg.maxWalkableSlopeDot) {
        groundedThisFrame = true;
        applyTouchResets(p, cfg);
        p.vel.z = 0.0f;
        if (p.externalImpulse.z > 0.0f)
            p.externalImpulse.z = 0.0f;
    } else if (normal.z > 0.0f) {
        applyTouchResets(p, cfg);
        projectVelocityAgainstNormal(p, normal);
    } else if (normal.z < -cfg.maxWalkableSlopeDot) {
        applyTouchResets(p, cfg);
        if (p.vel.z > 0.0f)
            p.vel.z = 0.0f;
    } else {
        projectVelocityAgainstNormal(p, normal);
        applyTouchResets(p, cfg);
    }
}

// ── v2.0.6 narrowphase math (verbatim behavior) ─────────────────────────────
vec3 closestPointOnTriangle(const vec3& p, const Triangle& tri)
{
    const vec3 a = tri.a, b = tri.b, c = tri.c;
    const vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    const vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    const vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom, w = vc * denom;
    return a + ab * v + ac * w;
}

bool pointInTriangle(const vec3& p, const Triangle& tri)
{
    vec3 closest = closestPointOnTriangle(p, tri);
    vec3 delta = closest - p;
    return glm::dot(delta, delta) < 0.0001f;
}

bool sweepSpherePoint(const vec3& start, const vec3& move, float radius,
                      const vec3& point, float& hitTime, vec3& hitNormal, vec3& hitPoint)
{
    float a = glm::dot(move, move);
    if (a < kAlmostZero)
        return false;

    vec3 rel = start - point;
    float b = 2.0f * glm::dot(rel, move);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - std::sqrt(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    vec3 center = start + move * t;
    vec3 n = center - point;
    if (glm::dot(n, n) < 0.000001f)
        n = -glm::normalize(move);
    else
        n = glm::normalize(n);

    hitTime = t;
    hitNormal = n;
    hitPoint = point;
    return true;
}

bool sweepSphereEdge(const vec3& start, const vec3& move, float radius,
                     const vec3& edgeA, const vec3& edgeB,
                     float& hitTime, vec3& hitNormal, vec3& hitPoint)
{
    vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f)
        return false;
    edgeDir /= edgeLen;

    vec3 rel = start - edgeA;
    float proj = glm::dot(rel, edgeDir);
    vec3 relPerp = rel - edgeDir * proj;
    vec3 movePerp = move - edgeDir * glm::dot(move, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - std::sqrt(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    vec3 centerAtT = start + move * t;
    vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen)
        return false;

    vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    vec3 normal = centerAtT - closestOnEdge;
    float dist = glm::length(normal);
    if (dist < 0.000001f)
        return false;
    normal /= dist;

    hitTime = t;
    hitNormal = normal;
    hitPoint = closestOnEdge;
    return true;
}

bool sweepSphereTriangle(const vec3& start, const vec3& move, float radius,
                         const Triangle& tri, float& hitTime, vec3& hitNormal, vec3& hitPoint)
{
    float bestT = 1.0f;
    vec3 bestN(0.0f), bestP(0.0f);
    bool hit = false;

    vec3 n = tri.normal;
    float dist = glm::dot(start - tri.a, n);
    if (dist < 0.0f) {
        n = -n;
        dist = -dist;
    }

    float denom = glm::dot(move, n);
    if (denom < -kAlmostZero) {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t <= 1.0f) {
            vec3 centerAtHit = start + move * t;
            vec3 planePoint = centerAtHit - n * radius;
            if (pointInTriangle(planePoint, tri)) {
                bestT = t;
                bestN = n;
                bestP = planePoint;
                hit = true;
            }
        }
    }

    const vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
    for (auto& ep : edgePairs) {
        float t = 1.0f;
        vec3 en(0.0f), epPt(0.0f);
        if (sweepSphereEdge(start, move, radius, ep[0], ep[1], t, en, epPt) && t < bestT) {
            bestT = t;
            bestN = en;
            bestP = epPt;
            hit = true;
        }
    }

    const vec3 pts[3] = {tri.a, tri.b, tri.c};
    for (const vec3& pt : pts) {
        float t = 1.0f;
        vec3 pn(0.0f), pp(0.0f);
        if (sweepSpherePoint(start, move, radius, pt, t, pn, pp) && t < bestT) {
            bestT = t;
            bestN = pn;
            bestP = pp;
            hit = true;
        }
    }

    if (!hit)
        return false;

    hitTime = bestT;
    hitNormal = bestN;
    hitPoint = bestP;
    return true;
}

bool sphereTriangleContact(const vec3& center, float radius,
                           const Triangle& tri, Contact& contact)
{
    vec3 closest = closestPointOnTriangle(center, tri);
    vec3 delta = center - closest;
    float dist2 = glm::dot(delta, delta);
    if (dist2 > radius * radius)
        return false;

    float dist = std::sqrt(std::max(dist2, 0.0f));
    vec3 n;
    if (dist > 0.00001f) {
        n = delta / dist;
        float barycentricSum = 0.0f;
        vec3 edgeDirs[3] = {tri.b - tri.a, tri.c - tri.b, tri.a - tri.c};
        for (int ei = 0; ei < 3; ++ei) {
            float edgeLen = glm::length(edgeDirs[ei]);
            if (edgeLen < 0.0001f) continue;
            vec3 edgeDir = edgeDirs[ei] / edgeLen;
            vec3 toClosest = closest - (ei == 0 ? tri.a : (ei == 1 ? tri.b : tri.c));
            float alongEdge = glm::dot(toClosest, edgeDir);
            if (alongEdge >= 0.0f && alongEdge <= edgeLen)
                barycentricSum += 1.0f;
        }
        if (barycentricSum < 2.5f)
            n = glm::normalize(n + tri.normal * 0.3f);
    } else {
        float side = glm::dot(center - tri.a, tri.normal);
        n = (side >= 0.0f) ? tri.normal : -tri.normal;
    }

    contact.point = closest;
    contact.normal = n;
    contact.penetration = radius - dist;
    return true;
}

void capsuleSamples(const Capsule& cap, vec3* samples, int count)
{
    for (int i = 0; i < count; ++i) {
        float t = (float)i / (float)(count - 1);
        samples[i] = cap.a + (cap.b - cap.a) * t;
    }
}

bool capsuleTriangleSweep(const Capsule& cap, const vec3& move,
                          const Triangle& tri, int triIndex, SweepHit& out)
{
    constexpr int NUM_SAMPLES = 11;
    vec3 samples[NUM_SAMPLES];
    capsuleSamples(cap, samples, NUM_SAMPLES);
    bool hit = false;
    float bestT = 1.0f;
    vec3 bestN(0.0f), bestP(0.0f);

    for (const vec3& sample : samples) {
        float t = 1.0f;
        vec3 n(0.0f), p(0.0f);
        if (sweepSphereTriangle(sample, move, cap.r, tri, t, n, p) && t < bestT) {
            bestT = t;
            bestN = n;
            bestP = p;
            hit = true;
        }
    }

    if (!hit)
        return false;

    out.hit = true;
    out.time = bestT;
    out.normal = bestN;
    out.point = bestP;
    out.triangleIndex = triIndex;
    return true;
}

bool capsuleTriangleContact(const Capsule& cap, const Triangle& tri,
                            int triIndex, Contact& out)
{
    constexpr int NUM_SAMPLES = 11;
    vec3 samples[NUM_SAMPLES];
    capsuleSamples(cap, samples, NUM_SAMPLES);
    bool hit = false;
    Contact best;

    for (const vec3& sample : samples) {
        Contact c;
        float skinRadius = cap.r + kCollisionSkin;
        if (sphereTriangleContact(sample, skinRadius, tri, c)) {
            c.penetration = std::max(0.0f, c.penetration - kCollisionSkin);
            if (!hit || c.penetration > best.penetration) {
                best = c;
                hit = true;
            }
        }
    }

    if (!hit)
        return false;

    best.triangleIndex = triIndex;
    out = best;
    return true;
}

bool rejectBelowTopFaceContact(const Capsule& cap, const Triangle& tri,
                               const vec3& normal, const vec3& point,
                               const Config& cfg)
{
    if (normal.z <= cfg.maxWalkableSlopeDot)
        return false;
    float feetZ = cap.a.z - cap.r;
    float tolerance = std::max(kSeamTolerance, kCollisionSkin + 0.005f);
    if (feetZ + tolerance >= point.z)
        return false;
    return true;
}

AABB makeSweptCapsuleAABB(const Capsule& cap, const vec3& move)
{
    vec3 mn = glm::min(glm::min(cap.a, cap.b), glm::min(cap.a + move, cap.b + move));
    vec3 mx = glm::max(glm::max(cap.a, cap.b), glm::max(cap.a + move, cap.b + move));
    return {mn - vec3(cap.r), mx + vec3(cap.r)};
}

AABB makeTriangleAABB(const Triangle& tri)
{
    return {glm::min(glm::min(tri.a, tri.b), tri.c),
            glm::max(glm::max(tri.a, tri.b), tri.c)};
}

bool overlaps(const AABB& a, const AABB& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

void gatherCandidates(const World& world, const Capsule& cap, const vec3& move,
                      std::vector<int>& out)
{
    out.clear();
    AABB q = makeSweptCapsuleAABB(cap, move);
    for (int i = 0; i < (int)world.triangles.size(); ++i) {
        AABB tb = makeTriangleAABB(world.triangles[i]);
        tb.min -= vec3(cap.r);
        tb.max += vec3(cap.r);
        if (overlaps(q, tb))
            out.push_back(i);
    }
}

std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world, const Capsule& cap, const std::vector<int>& candidates,
    const Config& cfg)
{
    std::vector<RecoveryContact> contacts;
    for (int triIndex : candidates) {
        if (triIndex < 0 || triIndex >= (int)world.triangles.size())
            continue;
        const Triangle& tri = world.triangles[triIndex];
        Contact contact;
        if (capsuleTriangleContact(cap, tri, triIndex, contact)) {
            if (rejectBelowTopFaceContact(cap, tri, contact.normal, contact.point, cfg))
                continue;
            contacts.push_back({contact.normal, contact.point, contact.penetration,
                                contact.triangleIndex});
        }
    }
    return contacts;
}

vec3 solveBatchedCorrection(const std::vector<RecoveryContact>& contacts, float slop)
{
    std::vector<RecoveryContact> manifold;
    for (const RecoveryContact& contact : contacts) {
        bool found = false;
        for (RecoveryContact& existing : manifold) {
            float alignment = glm::dot(existing.normal, contact.normal);
            if (alignment >= 0.85f) {
                existing.normal = glm::normalize(existing.normal + contact.normal);
                existing.point = (existing.point + contact.point) * 0.5f;
                existing.penetration = std::max(existing.penetration, contact.penetration);
                found = true;
                break;
            }
        }
        if (!found)
            manifold.push_back(contact);
    }

    std::sort(manifold.begin(), manifold.end(),
              [](const RecoveryContact& a, const RecoveryContact& b) {
                  return a.penetration > b.penetration;
              });

    vec3 correction(0.0f);
    constexpr int SOLVER_PASSES = 32;
    constexpr float RELAXATION = 0.85f;
    for (int pass = 0; pass < SOLVER_PASSES; ++pass) {
        for (const RecoveryContact& c : manifold) {
            float required = c.penetration + slop;
            float satisfied = glm::dot(correction, c.normal);
            if (satisfied < required)
                correction += c.normal * (required - satisfied) * RELAXATION;
        }
    }

    float corrMag = glm::length(correction);
    if (corrMag > kMaxCorrection)
        correction *= kMaxCorrection / corrMag;
    return correction;
}

// ── v2.0.6 doGLBTriangleCollisions (movement-relevant subset) ───────────────
void doGLBTriangleCollisions(PlayerState& p, const World& world,
                             bool& groundedThisFrame, const Config& cfg, float dt)
{
    (void)dt;
    glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
    Capsule cap = capsuleFromState(p, cfg);

    std::vector<int> candidates;
    gatherCandidates(world, cap, totalMove, candidates);
    std::vector<int> allCandidates = candidates;

    // Phase 1: sweep + slide.
    int sweepIters = kSweepBaseIters + (int)(glm::length(totalMove) / kMaxMovePerIter);
    sweepIters = std::clamp(sweepIters, 1, std::max(1, kSweepMaxIters));
    glm::vec3 remainingMove = totalMove;

    for (int iter = 0; iter < sweepIters; ++iter) {
        SweepHit earliest;
        earliest.time = 1.0f;
        std::vector<SweepHit> toiHits;
        constexpr float TOI_EPSILON = 0.001f;

        cap = capsuleFromState(p, cfg);
        gatherCandidates(world, cap, remainingMove, candidates);
        for (int triIndex : allCandidates)
            if (std::find(candidates.begin(), candidates.end(), triIndex) == candidates.end())
                candidates.push_back(triIndex);

        for (int triIndex : candidates) {
            const Triangle& tri = world.triangles[triIndex];
            SweepHit hit;
            if (!capsuleTriangleSweep(cap, remainingMove, tri, triIndex, hit))
                continue;
            if (rejectBelowTopFaceContact(cap, tri, hit.normal, hit.point, cfg))
                continue;
            if (!earliest.hit || hit.time + TOI_EPSILON < earliest.time) {
                earliest = hit;
                toiHits.clear();
                toiHits.push_back(hit);
            } else if (hit.time <= earliest.time + TOI_EPSILON) {
                toiHits.push_back(hit);
            }
        }

        glm::vec3 stepMove = remainingMove * earliest.time;
        p.pos += stepMove;
        cap = capsuleFromState(p, cfg);

        if (!earliest.hit) {
            remainingMove = vec3(0.0f);
            break;
        }

        // GLB step-up.
        if (std::fabs(earliest.normal.z) < 0.2f) {
            float feetZ = cap.a.z - cap.r;
            float stepHeight = earliest.point.z - feetZ;

            if (stepHeight > 0.0f && stepHeight <= kMaxStepHeight) {
                glm::vec3 originalPos = p.pos;

                int consistentSamples = 0;
                for (int s = 0; s < 5; s++) {
                    float t = (float)s / 4.0f;
                    vec3 samplePos = cap.a + (cap.b - cap.a) * t;
                    float sampleFeetZ = samplePos.z - cap.r;
                    for (int triIndex : candidates) {
                        const Triangle& tri = world.triangles[triIndex];
                        float triZ = std::max({tri.a.z, tri.b.z, tri.c.z});
                        if (sampleFeetZ < triZ && triZ - sampleFeetZ <= kMaxStepHeight) {
                            consistentSamples++;
                            break;
                        }
                    }
                }

                if (consistentSamples < 3) {
                    p.pos = originalPos;
                    continue;
                }

                p.pos.z += stepHeight + 0.01f;
                Capsule stepCap = capsuleFromState(p, cfg);

                bool blocked = false;
                for (int triIndex : candidates) {
                    Contact c;
                    if (capsuleTriangleContact(stepCap, world.triangles[triIndex], triIndex, c)) {
                        if (c.normal.z < 0.5f) {
                            blocked = true;
                            break;
                        }
                    }
                }

                bool hasFloor = false;
                if (!blocked) {
                    Capsule checkCap = stepCap;
                    checkCap.a.z -= 0.3f;
                    checkCap.b.z -= 0.3f;
                    for (int triIndex : candidates) {
                        Contact fc;
                        if (capsuleTriangleContact(checkCap, world.triangles[triIndex], triIndex, fc)) {
                            if (fc.normal.z >= cfg.maxWalkableSlopeDot) {
                                hasFloor = true;
                                break;
                            }
                        }
                    }
                }

                if (!blocked && hasFloor) {
                    groundedThisFrame = true;
                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;
                    remainingMove -= stepMove;
                    continue;
                }

                p.pos = originalPos;
            }
        }

        vec3 depen = earliest.normal;
        if (depen.z > 0.0f && depen.z < 0.7f)
            depen.z = 0.0f;
        if (glm::length(depen) > 0.0001f)
            depen = glm::normalize(depen);

        p.pos += depen * kSurfaceSlop;
        remainingMove -= stepMove;

        for (const SweepHit& hit : toiHits) {
            float into = glm::dot(remainingMove, hit.normal);
            if (into < 0.0f)
                remainingMove -= hit.normal * into;
        }

        // Multi-contact slide (Gauss-Seidel).
        {
            Capsule slideCap = capsuleFromState(p, cfg);
            std::vector<int> slideCandidates;
            gatherCandidates(world, slideCap, vec3(0.0f), slideCandidates);
            std::vector<RecoveryContact> slideContacts =
                collectCapsuleRecoveryContacts(world, slideCap, slideCandidates, cfg);
            constexpr int SLIDE_SOLVER_PASSES = 12;
            for (int slidePass = 0; slidePass < SLIDE_SOLVER_PASSES; ++slidePass) {
                for (const auto& sc : slideContacts) {
                    float vn = glm::dot(remainingMove, sc.normal);
                    if (vn < 0.0f)
                        remainingMove -= sc.normal * vn;
                }
            }
        }

        for (const SweepHit& hit : toiHits)
            applyCollisionContact(p, groundedThisFrame, hit.normal, cfg);

        if (glm::dot(remainingMove, remainingMove) < 0.000001f)
            break;
    }

    // Phase 2: batched depenetration.
    for (int depenIter = 0; depenIter < 4; ++depenIter) {
        cap = capsuleFromState(p, cfg);
        gatherCandidates(world, cap, vec3(0.0f), candidates);
        std::vector<RecoveryContact> contacts =
            collectCapsuleRecoveryContacts(world, cap, candidates, cfg);
        if (contacts.empty())
            break;

        glm::vec3 correction = solveBatchedCorrection(contacts, kSurfaceSlop);
        float corrLen = glm::length(correction);
        if (corrLen > kMaxCorrection)
            correction *= kMaxCorrection / corrLen;
        p.pos += correction;

        for (const RecoveryContact& c : contacts)
            applyCollisionContact(p, groundedThisFrame, c.normal, cfg);

        if (glm::dot(correction, correction) < 0.0000001f)
            break;
    }

    // Phase 2.5: re-sweep from depenetrated position.
    {
        vec3 curMove = remainingMove;
        if (glm::length(curMove) > 0.001f) {
            Capsule resweepCap = capsuleFromState(p, cfg);
            std::vector<int> resweepCandidates;
            gatherCandidates(world, resweepCap, curMove, resweepCandidates);
            SweepHit resweepHit;
            resweepHit.time = 1.0f;
            for (int triIndex : resweepCandidates) {
                SweepHit hit;
                if (capsuleTriangleSweep(resweepCap, curMove, world.triangles[triIndex],
                                         triIndex, hit) &&
                    hit.time < resweepHit.time)
                    resweepHit = hit;
            }
            if (resweepHit.hit && resweepHit.time < 1.0f) {
                vec3 resweepStep = curMove * resweepHit.time;
                p.pos += resweepStep;
                p.pos += resweepHit.normal * kSurfaceSlop;
                vec3 afterStep = curMove - resweepStep;
                float into = glm::dot(afterStep, resweepHit.normal);
                if (into < 0.0f)
                    afterStep -= resweepHit.normal * into;
                p.pos += afterStep;
                applyCollisionContact(p, groundedThisFrame, resweepHit.normal, cfg);
            } else {
                p.pos += curMove;
            }
            remainingMove = vec3(0.0f);
        }
    }

    // Ground snap.
    {
        constexpr float GROUND_SNAP_DISTANCE = 0.01f;
        constexpr float MAX_UPWARD_VEL_FOR_SNAP = 0.5f;
        if (p.vel.z <= MAX_UPWARD_VEL_FOR_SNAP) {
            Capsule checkCap = capsuleFromState(p, cfg);
            float feetZ = checkCap.a.z - checkCap.r;
            std::vector<int> groundCandidates;
            gatherCandidates(world, checkCap, vec3(0.0f, 0.0f, -GROUND_SNAP_DISTANCE),
                             groundCandidates);
            float bestGroundZ = -FLT_MAX;

            for (int triIndex : groundCandidates) {
                const Triangle& tri = world.triangles[triIndex];
                if (tri.normal.z < cfg.maxWalkableSlopeDot)
                    continue;
                vec3 capCenter(checkCap.a.x, checkCap.a.y, 0.0f);
                float planeDist = glm::dot(capCenter - tri.a, tri.normal);
                vec3 proj = capCenter - tri.normal * planeDist;
                if (pointInTriangle(proj, tri)) {
                    float triCenterZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
                    if (triCenterZ < feetZ && triCenterZ > bestGroundZ)
                        bestGroundZ = triCenterZ;
                } else {
                    vec3 nearest = closestPointOnTriangle(capCenter, tri);
                    float dist2 = glm::dot(nearest - capCenter, nearest - capCenter);
                    float r2 = cfg.playerRadius * 0.5f * (cfg.playerRadius * 0.5f);
                    if (dist2 < r2) {
                        if (nearest.z < feetZ && nearest.z > bestGroundZ)
                            bestGroundZ = nearest.z;
                    }
                }
            }

            if (bestGroundZ > -FLT_MAX) {
                float distToGround = feetZ - bestGroundZ;
                if (distToGround > 0.0f && distToGround < GROUND_SNAP_DISTANCE) {
                    p.pos.z -= distToGround;
                    groundedThisFrame = true;
                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;

                    Capsule postSnapCap = capsuleFromState(p, cfg);
                    std::vector<int> postSnapCandidates;
                    gatherCandidates(world, postSnapCap, vec3(0.0f), postSnapCandidates);
                    std::vector<RecoveryContact> postSnapContacts =
                        collectCapsuleRecoveryContacts(world, postSnapCap,
                                                       postSnapCandidates, cfg);
                    if (!postSnapContacts.empty()) {
                        glm::vec3 snapCorrection =
                            solveBatchedCorrection(postSnapContacts, kSurfaceSlop);
                        float snapCorrLen = glm::length(snapCorrection);
                        if (snapCorrLen > kMaxCorrection)
                            snapCorrection *= kMaxCorrection / snapCorrLen;
                        p.pos += snapCorrection;
                        for (const RecoveryContact& c : postSnapContacts)
                            applyCollisionContact(p, groundedThisFrame, c.normal, cfg);
                    }
                }
            }
        }
    }

    // Emergency stuck prevention.
    {
        constexpr float STUCK_THRESHOLD = 0.05f;
        const float EMERGENCY_SEARCH_RADIUS =
            cfg.playerHeight + cfg.playerRadius * 4.0f;
        Capsule stuckCheckCap = capsuleFromState(p, cfg);
        std::vector<int> stuckCandidates;
        gatherCandidates(world, stuckCheckCap, vec3(0.0f), stuckCandidates);
        std::vector<RecoveryContact> stuckContacts =
            collectCapsuleRecoveryContacts(world, stuckCheckCap, stuckCandidates, cfg);

        float worstPen = 0.0f;
        for (const auto& c : stuckContacts)
            worstPen = std::max(worstPen, c.penetration);

        if (worstPen > STUCK_THRESHOLD) {
            p.collisionStuckFrames++;
            if (p.collisionStuckFrames >= 3) {
                const vec3 searchDirs[] = {
                    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
                    {0, 0, 1}, {1, 1, 0}, {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0},
                    {1, 0, 1}, {-1, 0, 1}, {0, 1, 1}, {0, -1, 1}};
                vec3 bestPos = p.pos;
                float bestPen = worstPen;
                bool foundFree = false;

                for (vec3 dir : searchDirs) {
                    if (glm::length(dir) > 0.001f)
                        dir = glm::normalize(dir);
                    for (float dist = 0.05f; dist <= EMERGENCY_SEARCH_RADIUS; dist += 0.05f) {
                        PlayerState testP = p;
                        testP.pos = p.pos + dir * dist;
                        Capsule testCap = capsuleFromState(testP, cfg);
                        std::vector<int> testCandidates;
                        gatherCandidates(world, testCap, vec3(0.0f), testCandidates);
                        std::vector<RecoveryContact> testContacts =
                            collectCapsuleRecoveryContacts(world, testCap, testCandidates, cfg);
                        float testPen = 0.0f;
                        for (const auto& tc : testContacts)
                            testPen = std::max(testPen, tc.penetration);
                        if (testPen < 0.01f) {
                            bestPos = testP.pos;
                            bestPen = testPen;
                            foundFree = true;
                            break;
                        }
                        if (testPen < bestPen) {
                            bestPos = testP.pos;
                            bestPen = testPen;
                        }
                    }
                    if (foundFree) break;
                }

                if (foundFree || bestPen < worstPen) {
                    p.pos = bestPos;
                    p.vel = vec3(0.0f);
                    p.collisionStuckFrames = 0;
                }
            }
        } else {
            p.collisionStuckFrames = 0;
        }
    }

    // Final velocity projection against all contacts (skip walkable ground).
    cap = capsuleFromState(p, cfg);
    gatherCandidates(world, cap, vec3(0.0f), candidates);
    std::vector<RecoveryContact> finalContacts =
        collectCapsuleRecoveryContacts(world, cap, candidates, cfg);
    for (const RecoveryContact& c : finalContacts) {
        if (c.normal.z > cfg.maxWalkableSlopeDot)
            continue;
        projectVelocityAgainstNormal(p, c.normal);
    }

    // Rotation-safety pass.
    {
        Capsule safetyCap = capsuleFromState(p, cfg);
        std::vector<int> safetyCandidates;
        gatherCandidates(world, safetyCap, vec3(0.0f), safetyCandidates);
        std::vector<RecoveryContact> safetyContacts =
            collectCapsuleRecoveryContacts(world, safetyCap, safetyCandidates, cfg);
        if (!safetyContacts.empty()) {
            glm::vec3 safetyCorrection = solveBatchedCorrection(safetyContacts, kSurfaceSlop);
            float corrLen = glm::length(safetyCorrection);
            if (corrLen > kMaxCorrection)
                safetyCorrection *= kMaxCorrection / corrLen;
            p.pos += safetyCorrection;
            for (const RecoveryContact& c : safetyContacts) {
                if (c.normal.z <= cfg.maxWalkableSlopeDot)
                    projectVelocityAgainstNormal(p, c.normal);
                applyCollisionContact(p, groundedThisFrame, c.normal, cfg);
            }
        }
    }

    // Final safety net.
    {
        Capsule finalCap = capsuleFromState(p, cfg);
        std::vector<int> finalCandidates;
        gatherCandidates(world, finalCap, vec3(0.0f), finalCandidates);
        std::vector<RecoveryContact> contacts =
            collectCapsuleRecoveryContacts(world, finalCap, finalCandidates, cfg);
        float finalMaxPen = 0.0f;
        for (const auto& fc : contacts)
            finalMaxPen = std::max(finalMaxPen, fc.penetration);
        if (finalMaxPen > kCollisionSkin * 0.5f) {
            glm::vec3 finalCorrection = solveBatchedCorrection(contacts, kSurfaceSlop);
            float finalCorrLen = glm::length(finalCorrection);
            if (finalCorrLen > kMaxCorrection)
                finalCorrection *= kMaxCorrection / finalCorrLen;
            p.pos += finalCorrection;
            for (const RecoveryContact& fc : contacts) {
                if (fc.normal.z <= cfg.maxWalkableSlopeDot)
                    projectVelocityAgainstNormal(p, fc.normal);
            }
        }
    }
}

void doCollisions(PlayerState& p, const World& world, bool& groundedThisFrame,
                  const Config& cfg, float dt)
{
    // Matches v2.0.6 physics-mini.cpp: six substeps sharing one grounded flag
    // and one hasWorldContact flag across the whole frame.
    groundedThisFrame = false;
    p.hasWorldContact = false;
    const int steps = 6;
    float subdt = dt / (float)steps;
    for (int i = 0; i < steps; ++i)
        doGLBTriangleCollisions(p, world, groundedThisFrame, cfg, subdt);
}

} // namespace

void step(PlayerState& p, const Input& in, const World& world, const Config& cfg)
{
    float dt = std::min(in.dt, 0.033f);
    const bool wasOnGround = p.wasOnGround;

    p.didGroundJump = false;
    p.didAirJump = false;
    p.didDash = false;
    p.didLand = false;

    doGravity(p, cfg, dt);
    doFreeze(p, in.freezeHeld, cfg, dt);
    doGroundReturn(p, in.groundReturnPressed);
    doWalk(p, in.wishMoveXY, in.jumpHeld, cfg, dt);

    bool groundedThisFrame = false;
    doCollisions(p, world, groundedThisFrame, cfg, dt);

    if (!groundedThisFrame && in.movementPressed) {
        if (p.dashMovementTicks < 99)
            p.dashMovementTicks++;
    } else {
        p.dashMovementTicks = 0;
    }

    if (groundedThisFrame)
        doDash(p, in.wishMoveXY, in.dashPressed, in.camForward, cfg);
    else
        doAirDash(p, in.wishMoveXY, in.dashPressed, true, in.camForward, cfg);

    doDownDash(p, in.downDashPressed, cfg);
    doJump(p, in.jumpHeld, cfg, dt);

    if (groundedThisFrame)
        p.dashAvailable = true;

    p.onGround = groundedThisFrame;
    if (groundedThisFrame)
        p.groundLostTimer = 0.0f;
    else
        p.groundLostTimer += dt;

    p.stableOnGround = groundedThisFrame || (p.groundLostTimer < cfg.stableGroundWindow);

    doFriction(p, cfg, dt);

    float previousAirborneTime = p.airborneTimer;
    if (p.stableOnGround)
        p.airborneTimer = 0.0f;
    else
        p.airborneTimer += dt;

    if (!wasOnGround && p.stableOnGround && previousAirborneTime > 0.08f)
        p.didLand = true;

    p.wasOnGround = p.stableOnGround;
}

Trace traceOf(const PlayerState& p)
{
    Trace t;
    t.pos = p.pos;
    t.vel = p.vel;
    t.externalImpulse = p.externalImpulse;
    t.onGround = p.onGround;
    t.stableOnGround = p.stableOnGround;
    t.hasWorldContact = p.hasWorldContact;
    t.didGroundJump = p.didGroundJump;
    t.didAirJump = p.didAirJump;
    t.didDash = p.didDash;
    t.didLand = p.didLand;
    return t;
}

bool runMovementV206ReferenceSelfTest(std::string& report)
{
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += std::string(cond ? "[ok] " : "[FAIL] ") + name + "\n";
        ok = ok && cond;
    };

    Config cfg;

    // Floor at z = 0 (two triangles of a large quad, normal +Z).
    World floorWorld;
    {
        Triangle t0;
        t0.a = vec3(-100.0f, -100.0f, 0.0f);
        t0.b = vec3(100.0f, -100.0f, 0.0f);
        t0.c = vec3(100.0f, 100.0f, 0.0f);
        t0.normal = vec3(0.0f, 0.0f, 1.0f);
        floorWorld.triangles.push_back(t0);
    }

    // Gravity only: free fall accelerates downward.
    {
        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 5.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        step(p, in, floorWorld, cfg);
        check(p.vel.z < -0.9f && p.vel.z > -1.1f, "gravity adds ~0.966 velocityZ in one tick");
    }

    // Walk: grounded wish reaches moveSpeed on XY.
    {
        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        in.wishMoveXY = vec2(1.0f, 0.0f);
        in.movementPressed = true;
        for (int i = 0; i < 120; ++i)
            step(p, in, floorWorld, cfg);
        float speed = glm::length(vec2(p.vel.x, p.vel.y));
        check(std::fabs(speed - cfg.moveSpeed) < 1.0f, "ground walk approaches moveSpeed");
        check(p.stableOnGround, "ground walk stays stableOnGround");
    }

    // Ground dash adds dashImpulse to horizontal velocity.
    {
        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        in.wishMoveXY = vec2(1.0f, 0.0f);
        in.movementPressed = true;
        for (int i = 0; i < 10; ++i)
            step(p, in, floorWorld, cfg);
        float before = p.vel.x;
        in.dashPressed = true;
        step(p, in, floorWorld, cfg);
        check(p.vel.x > before + 50.0f, "ground dash adds a large horizontal impulse");
        check(p.didDash, "ground dash reports didDash");
    }

    // Jump sets vertical velocity to jumpStrength from a grounded state.
    {
        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        for (int i = 0; i < 10; ++i)
            step(p, in, floorWorld, cfg);
        in.jumpHeld = true;
        step(p, in, floorWorld, cfg);
        check(p.vel.z > cfg.jumpStrength - 0.5f, "held jump launches with jumpStrength");
        check(p.didGroundJump, "ground jump reports didGroundJump");
    }

    // Resting capsule does not sink through the floor.
    {
        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        for (int i = 0; i < 300; ++i)
            step(p, in, floorWorld, cfg);
        check(p.pos.z > 0.5f, "resting player does not sink through the floor");
        check(p.stableOnGround, "resting player remains grounded");
    }

    // Wall: walking into a wall stops forward progress and does not tunnel.
    {
        World world = floorWorld;
        Triangle wall;
        wall.a = vec3(3.0f, -100.0f, 0.0f);
        wall.b = vec3(3.0f, 100.0f, 0.0f);
        wall.c = vec3(3.0f, -100.0f, 20.0f);
        wall.normal = vec3(-1.0f, 0.0f, 0.0f);
        world.triangles.push_back(wall);

        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        in.wishMoveXY = vec2(1.0f, 0.0f);
        in.movementPressed = true;
        for (int i = 0; i < 240; ++i)
            step(p, in, world, cfg);
        check(p.pos.x < 3.0f, "player does not tunnel through the wall");
        check(std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && std::isfinite(p.pos.z),
              "wall collision result is finite");
    }

    // Step-up: a 0.2 high step under the walk path is climbed.
    {
        World world = floorWorld;
        Triangle stepTri;
        stepTri.a = vec3(2.0f, -100.0f, 0.0f);
        stepTri.b = vec3(2.0f, 100.0f, 0.0f);
        stepTri.c = vec3(2.0f, -100.0f, 0.2f);
        stepTri.normal = vec3(-1.0f, 0.0f, 0.0f);
        world.triangles.push_back(stepTri);
        Triangle stepTop;
        stepTop.a = vec3(2.0f, -100.0f, 0.2f);
        stepTop.b = vec3(4.0f, -100.0f, 0.2f);
        stepTop.c = vec3(4.0f, 100.0f, 0.2f);
        stepTop.normal = vec3(0.0f, 0.0f, 1.0f);
        world.triangles.push_back(stepTop);

        PlayerState p;
        p.pos = vec3(0.0f, 0.0f, 2.0f);
        Input in;
        in.dt = 1.0f / 60.0f;
        in.wishMoveXY = vec2(1.0f, 0.0f);
        in.movementPressed = true;
        for (int i = 0; i < 120; ++i)
            step(p, in, world, cfg);
        check(p.pos.x > 4.0f, "player passes a 0.2 high step instead of being blocked");
    }

    return ok;
}

} // namespace MimitaV206
