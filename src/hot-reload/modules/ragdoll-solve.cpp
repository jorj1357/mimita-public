// 09 16 2026
/* purpose
* Hot ragdoll solver. This module owns the ragdoll solve algorithm; the cold
* host only snapshots limb/joint/grab state and applies the results. It is fully
* live-editable: edit and save and the running client's ragdoll behaviour changes
* on the next generation switch, with no EXE rebuild.
* First cut: position-based dynamics with joint and rotation-limit constraints,
* grabs, gravity integration, limb self-collision and world collision. Limbs are
* approximated as spheres (radius + halfHeight) for collision so the solver is
* self-contained; the tuning below is intentionally explicit and easy to change.
* World collision geometry comes from the `world.collision` kernel capability.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

constexpr float kGravity = 9.81f;
constexpr float kCellSize = 2.0f;

using WorldCollisionFn = void (MIMITA_GAME_CALL *)(void*,
                                                   GameWorldCollisionPageV1*);

// ── Live tuning (edit these; no restart) ──────────────────────────────
struct Tuning {
    float stiffness = 1.0f;        // multiplier on base stiffness
    float damping = 1.0f;          // >1 relaxes velocity
    float gravity = 1.0f;          // multiplier on base gravity
    float jointBeta = 0.35f;       // position correction per joint iteration
    float limitBeta = 1.0f;        // rotation-limit correction
    float grabBeta = 0.5f;         // grab correction per iteration
    float selfBeta = 0.6f;         // limb-limb separation
    float worldBeta = 0.8f;        // world push-out
    float worldSkin = 0.005f;      // world penetration slop
    float selfSkin = 0.01f;        // limb penetration slop
    float stopLinearSpeed = 0.0f;  // 0 = disabled
    float stopAngularSpeed = 0.0f; // 0 = disabled
};
constexpr Tuning kTune{};

struct Tri { glm::vec3 a, b, c; };
struct Cache {
    std::vector<Tri> tris;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid;
    std::uint32_t total = 0;
    bool valid = false;
};
Cache g_cache;

std::uint64_t cellKey(int x, int y, int z)
{
    return ((std::uint64_t)(x + 4096) & 0x1fffff) |
           (((std::uint64_t)(y + 4096) & 0x1fffff) << 21) |
           (((std::uint64_t)(z + 4096) & 0x1fffff) << 42);
}
glm::ivec3 cellOf(const glm::vec3& p)
{
    return glm::ivec3((int)std::floor(p.x / kCellSize),
                      (int)std::floor(p.y / kCellSize),
                      (int)std::floor(p.z / kCellSize));
}

void ensureWorld(void* host)
{
    if (!host)
        return;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<WorldCollisionFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_WORLD_COLLISION));
    if (!fn)
        return;

    std::vector<GameCollisionTriangleV1> buf(4096);
    GameWorldCollisionPageV1 page{};
    page.offset = 0;
    page.maxTriangles = (std::uint32_t)buf.size();
    page.out = buf.data();
    fn(ctx->host, &page);  // total + first page
    if (page.total == 0)
        return;
    if (g_cache.valid && g_cache.total == page.total)
        return;

    g_cache.tris.clear();
    g_cache.grid.clear();
    g_cache.total = page.total;
    for (std::uint32_t off = 0; off < page.total;) {
        GameWorldCollisionPageV1 q{};
        q.offset = off;
        q.maxTriangles = (std::uint32_t)buf.size();
        q.out = buf.data();
        fn(ctx->host, &q);
        if (q.count == 0)
            break;
        for (std::uint32_t i = 0; i < q.count; ++i) {
            Tri t{{buf[i].a[0], buf[i].a[1], buf[i].a[2]},
                  {buf[i].b[0], buf[i].b[1], buf[i].b[2]},
                  {buf[i].c[0], buf[i].c[1], buf[i].c[2]}};
            const std::uint32_t idx = (std::uint32_t)g_cache.tris.size();
            g_cache.tris.push_back(t);
            const glm::vec3 c = (t.a + t.b + t.c) / 3.0f;
            g_cache.grid[cellKey((int)std::floor(c.x / kCellSize),
                                 (int)std::floor(c.y / kCellSize),
                                 (int)std::floor(c.z / kCellSize))]
                .push_back(idx);
        }
        off += q.count;
    }
    g_cache.valid = true;
}

glm::vec3 closestPointOnTri(const glm::vec3& p, const Tri& t)
{
    const glm::vec3 ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return t.a;
    const glm::vec3 bp = p - t.b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return t.b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return t.a + ab * (d1 / (d1 - d3));
    const glm::vec3 cp = p - t.c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return t.c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return t.a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float denom = 1.0f / (va + vb + vc);
    return t.a + ab * (vb * denom) + ac * (vc * denom);
}

float limbRadius(const GameRagdollLimbStaticV1& st)
{
    return st.radius + st.halfHeight;
}
glm::vec3 limbPos(const GameRagdollLimbStateV1& l)
{
    return glm::vec3(l.position[0], l.position[1], l.position[2]);
}
void setLimbPos(GameRagdollLimbStateV1& l, const glm::vec3& p)
{
    l.position[0] = p.x; l.position[1] = p.y; l.position[2] = p.z;
}
glm::quat limbQuat(const GameRagdollLimbStateV1& l)
{
    return glm::quat(l.orientation[0], l.orientation[1], l.orientation[2], l.orientation[3]);
}

glm::vec3 quatToRotationVector(const glm::quat& q)
{
    glm::quat n = glm::normalize(q);
    const float w = glm::clamp(n.w, -1.0f, 1.0f);
    float angle = 2.0f * std::acos(w);
    const float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
    if (s < 1e-5f) return glm::vec3(0.0f);
    glm::vec3 axis(n.x, n.y, n.z);
    axis /= s;
    if (angle > 3.14159265f) angle -= 2.0f * 3.14159265f;
    return axis * angle;
}
void rotateBody(glm::quat& o, const glm::vec3& rot)
{
    if (glm::length(rot) < 1e-8f) return;
    o = glm::normalize(glm::angleAxis(glm::length(rot), glm::normalize(rot)) * o);
}

// ── Solver passes ─────────────────────────────────────────────────────
void integrate(GameRagdollSolveV1* s, const glm::vec3& gravity, float dt)
{
    for (std::uint32_t i = 0; i < s->limbCount; ++i) {
        GameRagdollLimbStateV1& l = s->limbs[i];
        l.linearVelocity[0] += gravity.x * dt;
        l.linearVelocity[1] += gravity.y * dt;
        l.linearVelocity[2] += gravity.z * dt;
        setLimbPos(l, limbPos(l) + glm::vec3(l.linearVelocity[0], l.linearVelocity[1],
                                             l.linearVelocity[2]) * dt);
    }
}

void solveJoints(GameRagdollSolveV1* s, int iterations, bool velocityPass, float beta)
{
    for (int it = 0; it < iterations; ++it) {
        for (std::uint32_t i = 0; i < s->limbCount; ++i) {
            const GameRagdollLimbStaticV1& st = s->statics[i];
            if (st.parentIndex >= GAME_MAX_RAGDOLL_LIMBS || st.parentIndex >= s->limbCount)
                continue;
            GameRagdollLimbStateV1& child = s->limbs[i];
            GameRagdollLimbStateV1& parent = s->limbs[st.parentIndex];
            const glm::vec3 parentAnchor = limbPos(parent) +
                limbQuat(parent) * glm::vec3(st.parentLocalAnchor[0], st.parentLocalAnchor[1], st.parentLocalAnchor[2]);
            const glm::vec3 childAnchor = limbPos(child) +
                limbQuat(child) * glm::vec3(st.childLocalAnchor[0], st.childLocalAnchor[1], st.childLocalAnchor[2]);
            const glm::vec3 delta = childAnchor - parentAnchor;
            const float dist = glm::length(delta);
            if (dist < 1e-6f) continue;
            const glm::vec3 n = delta / dist;

            if (velocityPass) {
                const glm::vec3 cv(child.linearVelocity[0], child.linearVelocity[1], child.linearVelocity[2]);
                const glm::vec3 pv(parent.linearVelocity[0], parent.linearVelocity[1], parent.linearVelocity[2]);
                const float vn = glm::dot(cv - pv, n);
                const float invA = st.inverseMass;
                const float invB = s->statics[st.parentIndex].inverseMass;
                const float tot = invA + invB;
                if (tot < 1e-8f) continue;
                const glm::vec3 corr = n * (vn / tot);
                glm::vec3 nc = cv - corr * invA;
                glm::vec3 np = pv + corr * invB;
                child.linearVelocity[0] = nc.x; child.linearVelocity[1] = nc.y; child.linearVelocity[2] = nc.z;
                parent.linearVelocity[0] = np.x; parent.linearVelocity[1] = np.y; parent.linearVelocity[2] = np.z;
                continue;
            }

            const float target = (st.maxStretch > 0.0f && dist > st.maxStretch) ? st.maxStretch : 0.0f;
            if (dist <= target + 1e-6f) continue;
            const glm::vec3 correction = n * (dist - target) * beta;
            const float invA = st.inverseMass;
            const float invB = s->statics[st.parentIndex].inverseMass;
            const float tot = invA + invB;
            if (tot < 1e-8f) continue;
            setLimbPos(child, limbPos(child) - correction * (invA / tot));
            setLimbPos(parent, limbPos(parent) + correction * (invB / tot));
        }
    }
}

void solveRotationLimits(GameRagdollSolveV1* s, float beta)
{
    for (std::uint32_t i = 0; i < s->limbCount; ++i) {
        const GameRagdollLimbStaticV1& st = s->statics[i];
        if (!st.hasRotationLimits) continue;
        if (st.parentIndex >= s->limbCount) continue;
        GameRagdollLimbStateV1& child = s->limbs[i];
        GameRagdollLimbStateV1& parent = s->limbs[st.parentIndex];
        const glm::quat bind(st.bindRotation[0], st.bindRotation[1], st.bindRotation[2], st.bindRotation[3]);
        const glm::quat rel = glm::normalize(glm::inverse(limbQuat(parent)) * limbQuat(child));
        const glm::vec3 rv = quatToRotationVector(glm::normalize(rel * glm::inverse(bind)));
        const glm::vec3 clamped(std::max(st.rotMinDeg[0], std::min(rv.x, st.rotMaxDeg[0])),
                                std::max(st.rotMinDeg[1], std::min(rv.y, st.rotMaxDeg[1])),
                                std::max(st.rotMinDeg[2], std::min(rv.z, st.rotMaxDeg[2])));
        const glm::vec3 rejected = rv - clamped;
        if (glm::length(rejected) < 1e-6f) continue;
        const glm::vec3 worldCorrection = limbQuat(parent) * (-rejected) * beta;
        glm::quat co = limbQuat(child);
        glm::quat po = limbQuat(parent);
        rotateBody(co, worldCorrection * 0.5f);
        rotateBody(po, -worldCorrection * 0.5f);
        child.orientation[0]=co.w; child.orientation[1]=co.x; child.orientation[2]=co.y; child.orientation[3]=co.z;
        parent.orientation[0]=po.w; parent.orientation[1]=po.x; parent.orientation[2]=po.y; parent.orientation[3]=po.z;
    }
}

void solveGrab(GameRagdollSolveV1* s, const GameRagdollGrabV1& g, float beta)
{
    if (!g.active) return;
    if (g.limbIndex >= s->limbCount) return;
    GameRagdollLimbStateV1& hand = s->limbs[g.limbIndex];
    const glm::vec3 handAnchor = limbPos(hand) +
        limbQuat(hand) * glm::vec3(g.handLocalAnchor[0], g.handLocalAnchor[1], g.handLocalAnchor[2]);
    glm::vec3 target(g.grabPoint[0], g.grabPoint[1], g.grabPoint[2]);
    if (g.targetLimb >= 0 && (std::uint32_t)g.targetLimb < s->limbCount) {
        const GameRagdollLimbStateV1& t = s->limbs[g.targetLimb];
        target = limbPos(t) + limbQuat(t) *
            glm::vec3(g.targetLocalAnchor[0], g.targetLocalAnchor[1], g.targetLocalAnchor[2]);
    }
    setLimbPos(hand, limbPos(hand) + (target - handAnchor) * glm::clamp(beta * g.strength, 0.0f, 1.0f));
}

void selfCollision(GameRagdollSolveV1* s, float beta, float skin)
{
    for (std::uint32_t i = 0; i < s->limbCount; ++i) {
        for (std::uint32_t j = i + 1; j < s->limbCount; ++j) {
            if (s->statics[i].parentIndex == j || s->statics[j].parentIndex == i)
                continue;  // directly jointed parts overlap by design
            const float ri = limbRadius(s->statics[i]);
            const float rj = limbRadius(s->statics[j]);
            const glm::vec3 d = limbPos(s->limbs[j]) - limbPos(s->limbs[i]);
            const float dist = glm::length(d);
            const float minDist = ri + rj + skin;
            if (dist > 1e-6f && dist < minDist) {
                const glm::vec3 n = d / dist;
                const float push = (minDist - dist) * beta * 0.5f;
                setLimbPos(s->limbs[i], limbPos(s->limbs[i]) - n * push);
                setLimbPos(s->limbs[j], limbPos(s->limbs[j]) + n * push);
            }
        }
    }
}

void worldCollision(GameRagdollSolveV1* s, float beta, float skin)
{
    if (g_cache.tris.empty())
        return;
    for (std::uint32_t i = 0; i < s->limbCount; ++i) {
        const float r = limbRadius(s->statics[i]);
        const glm::vec3 p = limbPos(s->limbs[i]);
        const glm::ivec3 c0 = cellOf(p - glm::vec3(r));
        const glm::ivec3 c1 = cellOf(p + glm::vec3(r));
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z) {
            auto it = g_cache.grid.find(cellKey(x, y, z));
            if (it == g_cache.grid.end()) continue;
            for (std::uint32_t idx : it->second) {
                const glm::vec3 cp = closestPointOnTri(p, g_cache.tris[idx]);
                const glm::vec3 d = p - cp;
                const float dist = glm::length(d);
                if (dist < 1e-6f || dist >= r + skin) continue;
                const glm::vec3 n = d / dist;
                setLimbPos(s->limbs[i], p + n * (r + skin - dist) * beta);
            }
        }
    }
}

void MIMITA_GAME_CALL ragdollSolveProvider(void* host, GameRagdollSolveV1* s)
{
    if (!s || s->structSize != sizeof(GameRagdollSolveV1))
        return;
    s->handled = 1;
    s->applied = 0;
    if (s->limbCount == 0 || s->limbCount > GAME_MAX_RAGDOLL_LIMBS)
        return;

    ensureWorld(host);

    const int iterations = std::max(1, (int)s->iterations);
    const glm::vec3 gravity(0.0f, 0.0f, -kGravity * s->gravityScale * kTune.gravity);
    integrate(s, gravity, s->dt);

    solveJoints(s, iterations, true, kTune.jointBeta);
    solveJoints(s, iterations, false, kTune.jointBeta);
    solveGrab(s, s->grabLeft, kTune.grabBeta);
    solveGrab(s, s->grabRight, kTune.grabBeta);
    worldCollision(s, kTune.worldBeta, kTune.worldSkin);
    selfCollision(s, kTune.selfBeta, kTune.selfSkin);
    solveRotationLimits(s, kTune.limitBeta);

    const float damping = (s->damping > 0.0f ? s->damping : 1.0f) * kTune.damping;
    for (std::uint32_t i = 0; i < s->limbCount; ++i) {
        GameRagdollLimbStateV1& l = s->limbs[i];
        if (kTune.stopLinearSpeed > 0.0f) {
            const glm::vec3 v(l.linearVelocity[0], l.linearVelocity[1], l.linearVelocity[2]);
            if (glm::length(v) < kTune.stopLinearSpeed) {
                l.linearVelocity[0] = l.linearVelocity[1] = l.linearVelocity[2] = 0.0f;
            }
        }
        if (kTune.stopAngularSpeed > 0.0f) {
            const glm::vec3 w(l.angularVelocity[0], l.angularVelocity[1], l.angularVelocity[2]);
            if (glm::length(w) < kTune.stopAngularSpeed) {
                l.angularVelocity[0] = l.angularVelocity[1] = l.angularVelocity[2] = 0.0f;
            }
        }
        if (damping > 1.0f) {
            const float relax = glm::clamp(1.0f - (damping - 1.0f) * s->dt, 0.0f, 1.0f);
            for (int k = 0; k < 3; ++k) {
                l.linearVelocity[k] *= relax;
                l.angularVelocity[k] *= relax;
            }
        }
    }
    s->applied = 1;
}

const MimitaHotPackage::CapabilityRegistrar s_ragdollSolveProvider{
    {GAME_CAP_RAGDOLL_SOLVE, gameHash("sig.ragdoll.solve.v1"), 0,
     reinterpret_cast<void*>(&ragdollSolveProvider), "ragdoll.solve"}};

} // namespace

#endif
