// 09 21 2026
/* purpose
* Implements the ragdoll world-contact self-test. See ragdoll-world-selftest.h.
*/
#include "ragdoll/ragdoll-world-selftest.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-reload-system.h"
#include "debug/structured-log.h"
#include "live-code/live-behavior.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void addFloor(World& world)
{
    CollisionTriangle tri;
    tri.a = glm::vec3(-50.0f, -50.0f, 0.0f);
    tri.b = glm::vec3(50.0f, -50.0f, 0.0f);
    tri.c = glm::vec3(0.0f, 50.0f, 0.0f);
    tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    world.collisionMesh.triangles.push_back(tri);
}

void configure(GameRagdollSolveV1& s)
{
    s = GameRagdollSolveV1{};
    s.structSize = sizeof(GameRagdollSolveV1);
    s.limbCount = 2;
    s.dt = 1.0f / 60.0f;
    s.gravityScale = 1.0f;
    s.stiffness = 1.0f;
    s.damping = 1.0f;
    s.iterations = 6;

    s.limbs[0].position[0] = 0.0f;
    s.limbs[0].position[1] = 0.0f;
    s.limbs[0].position[2] = 3.0f;
    s.limbs[0].orientation[0] = 1.0f;
    s.limbs[1].position[0] = 0.5f;
    s.limbs[1].position[1] = 0.0f;
    s.limbs[1].position[2] = 3.0f;
    s.limbs[1].orientation[0] = 1.0f;

    for (int i = 0; i < 2; ++i) {
        s.statics[i].parentIndex = i == 0 ? 0xffffffffu : 0u;
        s.statics[i].inverseMass = 1.0f;
        s.statics[i].radius = 0.2f;
        s.statics[i].halfHeight = 0.0f;
        s.statics[i].restLength = 0.5f;
        s.statics[i].maxStretch = 0.1f;
    }
}

// Runs the hot solver for `ticks` and returns whether it stayed owned throughout.
bool runSolver(GameRagdollSolveFn fn, GameplayContextV1* ctx,
               GameRagdollSolveV1& s, int ticks)
{
    for (int i = 0; i < ticks; ++i) {
        fn(ctx, &s);
        if (s.handled == 0u || !s.applied)
            return false;
    }
    return true;
}

} // namespace

bool runRagdollWorldSelfTest(std::string& report)
{
    bool ok = true;

    World world;
    addFloor(world);
    StructuredLogger::instance().init();
    HotReloadSystem::instance().startup();
    LiveBehavior::setDispatchWorld(&world);

    GameplayContextV1* ctx = LiveBehavior::hostContext(0);
    ok &= check(ctx != nullptr && ctx->resolveCapability != nullptr,
                "gameplay context resolved", report);
    if (!ctx || !ctx->resolveCapability) {
        HotReloadSystem::instance().unloadGameDLL();
        return false;
    }

    auto fn = reinterpret_cast<GameRagdollSolveFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RAGDOLL_SOLVE));
    ok &= check(fn != nullptr, "hot ragdoll.solve capability resolves", report);
    if (!fn) {
        HotReloadSystem::instance().unloadGameDLL();
        return false;
    }

    // Diagnostic: how much world geometry the capability exposes here.
    {
        report += "  worldMeshTris=" +
                  std::to_string(world.collisionMesh.triangles.size()) + "\n";
        using WorldCollisionFn = void (MIMITA_GAME_CALL*)(void*,
                                                          GameWorldCollisionPageV1*);
        auto wfn = reinterpret_cast<WorldCollisionFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_WORLD_COLLISION));
        if (wfn) {
            GameCollisionTriangleV1 buf[16];
            GameWorldCollisionPageV1 pg{};
            pg.maxTriangles = 16;
            pg.out = buf;
            wfn(ctx->host, &pg);
            report += "  worldCapTotal=" + std::to_string(pg.total) +
                      " count=" + std::to_string(pg.count) + "\n";
            if (pg.count > 0) {
                char tbuf[192];
                std::snprintf(
                    tbuf, sizeof(tbuf),
                    "  worldTriA=(%.2f %.2f %.2f) B=(%.2f %.2f %.2f) C=(%.2f %.2f %.2f)\n",
                    buf[0].a[0], buf[0].a[1], buf[0].a[2], buf[0].b[0],
                    buf[0].b[1], buf[0].b[2], buf[0].c[0], buf[0].c[1],
                    buf[0].c[2]);
                report += tbuf;
            }
        } else {
            report += "  worldCap=missing\n";
        }
    }

    // On a floor: limbs must be constrained and settle above it.
    {
        GameRagdollSolveV1 s;
        configure(s);
        bool owned = true;
        for (int tick = 0; tick < 240; ++tick) {
            fn(ctx, &s);
            if (s.handled == 0u || !s.applied) {
                owned = false;
                break;
            }
        }
        ok &= check(owned, "hot solver owns every tick (floor case)", report);
        const float z0 = s.limbs[0].position[2];
        const float z1 = s.limbs[1].position[2];
        ok &= check(std::isfinite(z0) && std::isfinite(z1),
                    "floor case limbs finite", report);
        ok &= check(z0 > -0.1f && z1 > -0.1f,
                    "ragdoll limbs do not fall through the floor", report);
        ok &= check(z0 < 1.0f && z1 < 1.0f,
                    "ragdoll limbs settle near the floor", report);
    }

    // Control: with no world geometry the same ragdoll keeps falling through.
    {
        World empty;
        LiveBehavior::setDispatchWorld(&empty);
        GameRagdollSolveV1 s;
        configure(s);
        runSolver(fn, ctx, s, 240);
        ok &= check(s.limbs[0].position[2] < 0.0f,
                    "control: no floor, ragdoll falls through", report);
    }

    debug::flushEvents();
    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
