// 09 16 2026
/* purpose
* physics.capsuleSolve: the hot capsule collision solve. It owns integration,
* swept substepping, slide (contact depenetration + velocity projection),
* step-up, slope classification, and ground snap for every actor. The kernel
* only provides world triangles through the world.collision capability.
* All tuning constants below are hot: edit and hot-activate live.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdint>

namespace {

// ── Hot tuning (edit live) ──────────────────────────────────────────────────
constexpr bool  kHotCapsuleSolveEnabled = true;

constexpr float kGravity              = 9.81f;  // only when gravityScale > 0
constexpr int   kMaxTris              = 4096;
constexpr int   kResolvePasses        = 3;
constexpr int   kMaxSubsteps          = 8;      // swept movement resolution
constexpr float kMinSubstepMove       = 0.05f;  // min advance per substep
constexpr float kWalkableSlopeDot     = 0.70f;  // n.z >= this => walkable floor
constexpr float kGroundNormalZ        = 0.35f;  // min n.z to count as ground
constexpr float kGroundSnapEpsilon    = 0.05f;  // |v.z| below this snaps to 0
constexpr float kStepHeight           = 0.25f;  // max step-up height
constexpr float kBlockedFraction      = 0.5f;   // progress below this = blocked
constexpr float kSkin                 = 0.001f; // depenetration skin

thread_local GameCollisionTriangleV1 g_tris[kMaxTris];

float dot3(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
void sub3(const float a[3], const float b[3], float o[3]) { o[0]=a[0]-b[0]; o[1]=a[1]-b[1]; o[2]=a[2]-b[2]; }

// Closest point on triangle abc to p (Ericson, Real-Time Collision Detection).
void closestPointTriangle(const float p[3], const float a[3], const float b[3],
                          const float c[3], float out[3])
{
    float ab[3], ac[3], ap[3];
    sub3(b, a, ab); sub3(c, a, ac); sub3(p, a, ap);
    const float d1 = dot3(ab, ap);
    const float d2 = dot3(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) { out[0]=a[0]; out[1]=a[1]; out[2]=a[2]; return; }

    float bp[3]; sub3(p, b, bp);
    const float d3 = dot3(ab, bp);
    const float d4 = dot3(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) { out[0]=b[0]; out[1]=b[1]; out[2]=b[2]; return; }

    const float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        out[0]=a[0]+ab[0]*v; out[1]=a[1]+ab[1]*v; out[2]=a[2]+ab[2]*v; return;
    }

    float cp[3]; sub3(p, c, cp);
    const float d5 = dot3(ab, cp);
    const float d6 = dot3(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) { out[0]=c[0]; out[1]=c[1]; out[2]=c[2]; return; }

    const float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        out[0]=a[0]+ac[0]*w; out[1]=a[1]+ac[1]*w; out[2]=a[2]+ac[2]*w; return;
    }

    const float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        out[0]=b[0]+(c[0]-b[0])*w; out[1]=b[1]+(c[1]-b[1])*w; out[2]=b[2]+(c[2]-b[2])*w; return;
    }

    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    out[0]=a[0]+ab[0]*v+ac[0]*w;
    out[1]=a[1]+ab[1]*v+ac[1]*w;
    out[2]=a[2]+ab[2]*v+ac[2]*w;
}

// Depenetrate the capsule and project velocity out of every contact
// (slide). Sets `grounded` on any walkable contact and `collided` on any touch.
void resolveContacts(float pos[3], float vel[3], float radius, float halfSeg,
                     const GameCollisionTriangleV1* tris, std::uint32_t triCount,
                     bool& grounded, bool& collided)
{
    const float sampleZ[3] = {-halfSeg, 0.0f, halfSeg};
    for (int pass = 0; pass < kResolvePasses; ++pass) {
        for (int s = 0; s < 3; ++s) {
            float sample[3] = {pos[0], pos[1], pos[2] + sampleZ[s]};
            for (std::uint32_t t = 0; t < triCount; ++t) {
                const GameCollisionTriangleV1& tri = tris[t];
                float cp[3];
                closestPointTriangle(sample, tri.a, tri.b, tri.c, cp);
                float d[3] = {sample[0]-cp[0], sample[1]-cp[1], sample[2]-cp[2]};
                const float dist = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
                if (dist >= radius || dist < 0.00001f)
                    continue;
                float n[3] = {d[0]/dist, d[1]/dist, d[2]/dist};
                if (dot3(n, tri.normal) < 0.0f) { n[0]=-n[0]; n[1]=-n[1]; n[2]=-n[2]; }
                const float penetration = radius - dist;
                pos[0] += n[0]*(penetration + kSkin);
                pos[1] += n[1]*(penetration + kSkin);
                pos[2] += n[2]*(penetration + kSkin);
                const float into = dot3(vel, n);
                if (into < 0.0f) { vel[0]-=n[0]*into; vel[1]-=n[1]*into; vel[2]-=n[2]*into; }
                collided = true;
                if (n[2] >= kWalkableSlopeDot)
                    grounded = true;
            }
        }
    }
}

// One swept capsule move: substep the integration so fast parts cannot tunnel,
// resolving contacts after each substep (sweep + slide).
void sweptMove(float pos[3], float vel[3], float dt, float radius, float halfSeg,
               const GameCollisionTriangleV1* tris, std::uint32_t triCount,
               bool& grounded, bool& collided)
{
    const float speed = std::sqrt(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]);
    if (speed * dt < 1e-6f)
        return;
    int steps = (int)std::ceil(speed * dt / kMinSubstepMove);
    if (steps < 1) steps = 1;
    if (steps > kMaxSubsteps) steps = kMaxSubsteps;
    const float stepDt = dt / (float)steps;
    for (int i = 0; i < steps; ++i) {
        pos[0] += vel[0]*stepDt;
        pos[1] += vel[1]*stepDt;
        pos[2] += vel[2]*stepDt;
        resolveContacts(pos, vel, radius, halfSeg, tris, triCount, grounded, collided);
    }
}

void MIMITA_GAME_CALL onCapsuleSolve(void* /*host*/, GameCapsuleSolveV1* q)
{
    if (!q)
        return;
    q->handled = 0u;
    if (!kHotCapsuleSolveEnabled)
        return;

    float pos[3] = {q->position[0], q->position[1], q->position[2]};
    float vel[3] = {q->velocity[0], q->velocity[1], q->velocity[2]};

    if (q->gravityScale > 0.0f)
        vel[2] -= kGravity * q->gravityScale * q->dt;

    const float radius = q->radius > 0.0f ? q->radius : 0.4f;
    const float halfSeg = q->halfHeight > radius ? q->halfHeight - radius : 0.0f;

    // Fetch world geometry only when solving.
    std::uint32_t triCount = 0;
    if (q->collisionFn) {
        GameWorldCollisionPageV1 page{};
        page.offset = 0;
        page.maxTriangles = (std::uint32_t)kMaxTris;
        page.out = g_tris;
        q->collisionFn(q->collisionHost, &page);
        triCount = page.count < (std::uint32_t)kMaxTris ? page.count : (std::uint32_t)kMaxTris;
    }

    bool grounded = false;
    bool collided = false;

    // Establish whether we start on the ground (contact-based, no motion).
    if (triCount) {
        bool g0 = false, c0 = false;
        resolveContacts(pos, vel, radius, halfSeg, g_tris, triCount, g0, c0);
        grounded = g0;
        collided = c0;
    }

    float start[3] = {pos[0], pos[1], pos[2]};
    const float desiredH[2] = {vel[0]*q->dt, vel[1]*q->dt};

    sweptMove(pos, vel, q->dt, radius, halfSeg, g_tris, triCount, grounded, collided);

    // Step-up: if horizontal progress was mostly blocked and we are on the
    // ground, try the same move lifted by the step height, then settle back down.
    if (triCount && grounded) {
        const float actualHx = pos[0] - start[0];
        const float actualHy = pos[1] - start[1];
        const float desiredLen = std::sqrt(desiredH[0]*desiredH[0] + desiredH[1]*desiredH[1]);
        const float actualLen = std::sqrt(actualHx*actualHx + actualHy*actualHy);
        if (desiredLen > 1e-4f && actualLen < desiredLen * kBlockedFraction) {
            float cpos[3] = {start[0], start[1], start[2] + kStepHeight};
            float cvel[3] = {vel[0], vel[1], vel[2]};
            bool cg = false, cc = false;
            sweptMove(cpos, cvel, q->dt, radius, halfSeg, g_tris, triCount, cg, cc);
            // Settle down onto the step.
            float down[3] = {0.0f, 0.0f, -kStepHeight * 2.0f};
            cpos[2] += down[2] * q->dt; // one small descent step
            resolveContacts(cpos, cvel, radius, halfSeg, g_tris, triCount, cg, cc);
            const float cActual = std::sqrt((cpos[0]-start[0])*(cpos[0]-start[0]) +
                                            (cpos[1]-start[1])*(cpos[1]-start[1]));
            if (cActual > actualLen) {
                pos[0] = cpos[0]; pos[1] = cpos[1]; pos[2] = cpos[2];
                vel[0] = cvel[0]; vel[1] = cvel[1]; vel[2] = cvel[2];
                grounded = cg;
                collided = collided || cc;
            }
        }
    }

    // Ground snap: kill tiny residual vertical velocity while grounded so the
    // actor does not jitter/sink on contact.
    if (grounded && vel[2] > -kGroundSnapEpsilon && vel[2] < kGroundSnapEpsilon)
        vel[2] = 0.0f;

    q->outPosition[0] = pos[0];
    q->outPosition[1] = pos[1];
    q->outPosition[2] = pos[2];
    q->outVelocity[0] = vel[0];
    q->outVelocity[1] = vel[1];
    q->outVelocity[2] = vel[2];
    q->grounded = grounded ? 1u : 0u;
    q->collided = collided ? 1u : 0u;
    q->handled = 1u;
}

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_capsuleSolveProvider{
    {GAME_CAP_PHYSICS_CAPSULE_SOLVE, gameHash("sig.physics.capsule-solve.v1"), 0,
     reinterpret_cast<void*>(&onCapsuleSolve), "physics.capsuleSolve"}};

#endif
