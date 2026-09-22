// 09 22 2026
/* purpose
* Implements the afad20a oracle-vs-hot movement parity harness and fixture
* generator. Does NOT run the game or own movement policy.
*/
#include "physics/movement/reference/afad20a-parity-selftest.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "physics/movement/reference/movement-afad20a-reference.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace MimitaAfad20a {
namespace {

constexpr float kDt = 1.0f / 60.0f;

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

bool approxEqual(float a, float b, float eps)
{
    return std::fabs(a - b) <= eps;
}

struct HotResult {
    bool got = false;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {0.0f, 0.0f, 0.0f};
};

// Drives the registered hot movement system for `ticks` fixed steps with a
// constant intent and returns the last applied override.
HotResult runHotMovement(EntityId entity, int ticks, float moveX, float moveY,
                         bool pressed, bool jump, bool dash, bool downDash,
                         bool freeze, const glm::vec3& startVel)
{
    Ecs::setTransform(entity, glm::vec3(0.0f, 0.0f, 2.0f),
                      glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
    Ecs::setVelocity(entity, startVel, glm::vec3(0.0f));
    Ecs::setMovementIntent(entity, moveX, moveY, pressed, jump, dash, downDash,
                           freeze);
    MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
    HotResult result;
    for (int i = 0; i < ticks; ++i) {
        runtime.beginMovementTick();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, (std::uint64_t)i, kDt,
                          LiveBehavior::hostContext((std::uint64_t)i));
        float yaw = 0.0f;
        if (runtime.consumeMovementOverride(result.pos, result.vel, yaw))
            result.got = true;
    }
    return result;
}

State runReference(int ticks, const Input& in, State s, const Config& c)
{
    for (int i = 0; i < ticks; ++i)
        step(s, in, c);
    return s;
}

} // namespace

bool runAfad20aParitySelfTest(std::string& report)
{
    bool ok = true;
    const Config c{};

    World world;  // no triangles: collision declines, movement math only
    EntityRegistry::instance().destroyAll();
    HotReloadSystem::instance().startup();

    const EntityId entity = Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 1);
    Ecs::setBody(entity, 1.0f, 0.4f, 1.8f);
    Ecs::setMovementIntent(entity, 0.0f, 0.0f, false, false, false, false, false);
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState()) {
        shared->magic = GAME_SHARED_MAGIC;
        shared->modeFlags = GAME_MODE_FLAG_HOT_MOVEMENT;
        shared->localPlayerEntity = (std::uint64_t)entity;
    }
    LiveBehavior::setDispatchWorld(&world);

    ok &= check(MimitaRuntime::GenericRuntime::instance().systemCount() > 0,
                "hot package active with systems", report);

    const float tol = 1e-3f;

    // 1. Free fall for one second.
    {
        const HotResult hot = runHotMovement(entity, 60, 0.0f, 0.0f, false, false,
                                             false, false, false, glm::vec3(0.0f));
        const State ref = runReference(60, Input{}, State{}, c);
        ok &= check(hot.got && approxEqual(hot.vel[2], ref.velocity.z, tol),
                    "fall: hot matches afad20a vertical velocity", report);
    }

    // 2. Air dash: additive horizontal impulse along camera forward.
    {
        Input in{};
        in.dashPressed = true;
        const State ref = runReference(1, in, State{}, c);
        const HotResult hot = runHotMovement(entity, 1, 0.0f, 0.0f, false, false,
                                             true, false, false, glm::vec3(0.0f));
        ok &= check(hot.got && approxEqual(hot.vel[0], ref.velocity.x, tol) &&
                        approxEqual(hot.vel[1], ref.velocity.y, tol),
                    "air dash: hot matches afad20a horizontal velocity", report);
    }

    // 3. Down-dash replaces vertical velocity.
    {
        Input in{};
        in.downDashPressed = true;
        const State ref = runReference(1, in, State{}, c);
        const HotResult hot = runHotMovement(entity, 1, 0.0f, 0.0f, false, false,
                                             false, true, false, glm::vec3(0.0f));
        ok &= check(hot.got && approxEqual(hot.vel[2], ref.velocity.z, tol),
                    "down-dash: hot matches afad20a vertical velocity", report);
    }

    // 4. Freeze activation stops velocity on the activation tick.
    {
        Input in{};
        in.freezeHeld = true;
        in.freezePressed = true;
        const State ref = runReference(1, in, State{}, c);
        const HotResult hot = runHotMovement(entity, 1, 0.0f, 0.0f, false, false,
                                             false, false, true,
                                             glm::vec3(5.0f, 3.0f, -2.0f));
        ok &= check(hot.got && approxEqual(hot.vel[0], ref.velocity.x, tol) &&
                        approxEqual(hot.vel[1], ref.velocity.y, tol) &&
                        approxEqual(hot.vel[2], ref.velocity.z, tol),
                    "freeze: hot matches afad20a suppressed velocity", report);
    }

    // 5. Air-strafe: a perpendicular wish adds speed.
    {
        Input in{};
        in.moveAxes = glm::vec2(0.0f, 1.0f);
        State s{};
        s.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
        const State ref = runReference(1, in, s, c);
        const HotResult hot = runHotMovement(entity, 1, 0.0f, 1.0f, true, false,
                                             false, false, false,
                                             glm::vec3(10.0f, 0.0f, 0.0f));
        ok &= check(hot.got && approxEqual(hot.vel[0], ref.velocity.x, tol) &&
                        approxEqual(hot.vel[1], ref.velocity.y, tol),
                    "air-strafe: hot matches afad20a horizontal velocity", report);
    }

    HotReloadSystem::instance().unloadGameDLL();
    EntityRegistry::instance().destroyAll();
    return ok;
}

bool generateAfad20aFixtures(std::string& report)
{
    const Config c{};
    const std::filesystem::path dir("tests/fixtures/movement/afad20a");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // A deterministic scripted trace: 60 ticks of free fall, then 60 ticks of
    // air-strafe input, recorded tick-by-tick from the oracle.
    State s{};
    Input in{};
    std::ofstream out(dir / "fall-and-strafe.json");
    if (!out) {
        report += "[FAIL] could not open fixture output\n";
        return false;
    }
    out << "{\n  \"source\": \"afad20a-reference\",\n  \"dt\": " << c.dt
        << ",\n  \"ticks\": [\n";
    for (int i = 0; i < 120; ++i) {
        if (i == 60) {
            in.moveAxes = glm::vec2(0.0f, 1.0f);
            in.jumpHeld = false;
        }
        step(s, in, c);
        out << "    {\"tick\": " << i << ", \"pos\": [" << s.position.x << ", "
            << s.position.y << ", " << s.position.z << "], \"vel\": ["
            << s.velocity.x << ", " << s.velocity.y << ", " << s.velocity.z
            << "], \"grounded\": " << (s.grounded ? "true" : "false") << "}"
            << (i + 1 < 120 ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    out.close();

    report += "[ok] wrote " + (dir / "fall-and-strafe.json").string() + "\n";
    return true;
}

} // namespace MimitaAfad20a
