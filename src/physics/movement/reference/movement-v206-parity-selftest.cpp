// 09 21 2026
/* purpose
* Implements the v2.0.6 parity harness and fixture generator.
* See movement-v206-parity-selftest.h for scope and caveats.
*/
#include "physics/movement/reference/movement-v206-parity-selftest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "config/movement-config.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-presets.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/movement-types.h"
#include "physics/movement/reference/movement-v206-reference.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace MimitaV206 {
namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kVelocityEps = 1e-3f;
constexpr float kPositionEps = 1e-3f;
constexpr const char* kFixtureDir = "tests/fixtures/movement/v206";

struct ScenarioInput {
    glm::vec2 wish{0.0f};
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;
    bool movementPressed = false;
    bool grounded = false;
};

struct Scenario {
    const char* name = "";
    int ticks = 120;
    float startZ = 5.0f;
    bool defaultGrounded = false;
};

struct TickTrace {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    bool grounded = false;
};

ScenarioInput inputFor(const Scenario& s, int tick)
{
    ScenarioInput in;
    in.grounded = s.defaultGrounded;
    const std::string name = s.name;

    if (name == "ground_walk") {
        in.wish = glm::vec2(1.0f, 0.0f);
        in.movementPressed = true;
    } else if (name == "ground_walk_diagonal") {
        in.wish = glm::vec2(0.70710678f, 0.70710678f);
        in.movementPressed = true;
    } else if (name == "accelerate_friction") {
        in.wish = (tick < 40) ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f);
        in.movementPressed = (tick < 40);
    } else if (name == "jump_land") {
        in.wish = glm::vec2(1.0f, 0.0f);
        in.movementPressed = true;
        in.jumpHeld = (tick >= 20 && tick <= 26);
        in.jumpPressed = (tick == 20);
        in.grounded = (tick < 20);
    } else if (name == "air_move") {
        in.wish = glm::vec2(1.0f, 0.0f);
        in.movementPressed = true;
    } else if (name == "dash_ground") {
        in.wish = glm::vec2(1.0f, 0.0f);
        in.movementPressed = true;
        in.dashPressed = (tick == 10);
    } else if (name == "dash_air") {
        in.wish = glm::vec2(1.0f, 0.0f);
        in.movementPressed = true;
        in.dashPressed = (tick == 10);
    } else if (name == "down_dash_air") {
        in.downDashPressed = (tick == 10);
    } else if (name == "freeze_air") {
        in.freezeHeld = (tick >= 10 && tick < 60);
    } else if (name == "gravity_fall") {
        // no input
    }
    return in;
}

TickTrace oracleTraceStep(PlayerState& p, const Scenario& s, int tick)
{
    ScenarioInput si = inputFor(s, tick);
    Input in;
    in.dt = kDt;
    in.wishMoveXY = si.wish;
    in.jumpHeld = si.jumpHeld;
    in.jumpPressed = si.jumpPressed;
    in.dashPressed = si.dashPressed;
    in.downDashPressed = si.downDashPressed;
    in.freezeHeld = si.freezeHeld;
    in.movementPressed = si.movementPressed;
    in.camForward = glm::vec3(1.0f, 0.0f, 0.0f);

    World world;
    Triangle floorTri;
    floorTri.a = glm::vec3(-200.0f, -200.0f, 0.0f);
    floorTri.b = glm::vec3(200.0f, -200.0f, 0.0f);
    floorTri.c = glm::vec3(200.0f, 200.0f, 0.0f);
    floorTri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    world.triangles.push_back(floorTri);

    step(p, in, world, Config{});

    TickTrace t;
    t.px = p.pos.x; t.py = p.pos.y; t.pz = p.pos.z;
    t.vx = p.vel.x + p.externalImpulse.x;
    t.vy = p.vel.y + p.externalImpulse.y;
    t.vz = p.vel.z + p.externalImpulse.z;
    t.grounded = p.onGround;
    return t;
}

std::vector<TickTrace> runOracle(const Scenario& s)
{
    std::vector<TickTrace> out;
    PlayerState p;
    p.pos = glm::vec3(0.0f, 0.0f, s.startZ);
    p.stableOnGround = s.defaultGrounded;
    p.onGround = s.defaultGrounded;
    for (int tick = 0; tick < s.ticks; ++tick)
        out.push_back(oracleTraceStep(p, s, tick));
    return out;
}

std::vector<TickTrace> runCold(const MovementConfig& cfg, const Scenario& s)
{
    std::vector<TickTrace> out;
    MovementState st{};
    st.position = glm::vec3(0.0f, 0.0f, s.startZ);
    st.sizeScale = 1.0f;
    st.ground.onGround = s.defaultGrounded;
    st.ground.stableOnGround = s.defaultGrounded;
    st.jump.airJumpsLeft = cfg.maximumAirJumps;
    st.dash.dashAvailable = true;
    st.downDash.available = true;
    st.freeze.available = true;
    st.groundReturn.available = true;

    for (int tick = 0; tick < s.ticks; ++tick) {
        ScenarioInput si = inputFor(s, tick);
        MovementCommand cmd{};
        cmd.moveAxes = si.wish;
        cmd.lookYaw = 0.0f;
        cmd.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
        cmd.jumpHeld = si.jumpHeld;
        cmd.jumpPressed = si.jumpPressed;
        cmd.dashPressed = si.dashPressed;
        cmd.downDashPressed = si.downDashPressed;
        cmd.freezeHeld = si.freezeHeld;
        cmd.movementDirectionPressed = si.movementPressed;
        cmd.clientSimulationTick = static_cast<std::uint64_t>(tick);

        applyPreCollisionBasicMovement(st, cmd, cfg, kDt);

        // Collision-authoritative cold path: resolve through the universal
        // `collision.main` package (collision.capsuleMove capability), which
        // integrates the tick move and reports ground/contact. Falls back to
        // plain integration + the scenario grounded flag only if unavailable.
        bool grounded = si.grounded;
        bool collided = si.grounded;
        {
            const float inPos[3] = {st.position.x, st.position.y, st.position.z};
            const float inVel[3] = {st.baseVelocity.x + st.externalImpulse.x,
                                    st.baseVelocity.y + st.externalImpulse.y,
                                    st.baseVelocity.z + st.externalImpulse.z};
            float outPos[3] = {inPos[0], inPos[1], inPos[2]};
            float outVel[3] = {inVel[0], inVel[1], inVel[2]};
            if (LiveBehavior::capsuleMove(inPos, inVel, 0.4f, 0.9f, st.yaw,
                                          st.sizeScale, kDt, outPos, outVel,
                                          grounded, collided)) {
                st.position = glm::vec3(outPos[0], outPos[1], outPos[2]);
                st.baseVelocity = glm::vec3(outVel[0], outVel[1], outVel[2]);
                st.externalImpulse = glm::vec3(0.0f);
            } else {
                st.position += (st.baseVelocity + st.externalImpulse) * kDt;
            }
        }

        MovementStepEvents preEvents;
        applySpecialMovementPreCollision(st, cmd, cfg, kDt, preEvents);

        MovementCollisionFeedback collision{};
        collision.onGround = grounded;
        collision.hasWorldContact = collided;
        collision.realWorldContactThisFrame = collided;
        collision.simulationTick = static_cast<std::uint64_t>(tick);
        collision.groundNormal = glm::vec3(0.0f, 0.0f, 1.0f);

        MovementStepResult res = applyPostCollisionMovementWithSpecials(
            st, cmd, cfg, collision, kDt, preEvents);
        st = res.state;

        TickTrace t;
        t.px = st.position.x; t.py = st.position.y; t.pz = st.position.z;
        t.vx = st.baseVelocity.x + st.externalImpulse.x;
        t.vy = st.baseVelocity.y + st.externalImpulse.y;
        t.vz = st.baseVelocity.z + st.externalImpulse.z;
        t.grounded = st.ground.onGround;
        out.push_back(t);
    }
    return out;
}

struct CompareResult {
    int firstVelTick = -1;
    int firstGroundTick = -1;
    int firstPosTick = -1;
    float maxVelDev = 0.0f;
    float finalPosDev = 0.0f;
};

CompareResult compare(const std::vector<TickTrace>& a,
                      const std::vector<TickTrace>& b)
{
    CompareResult r;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const float dv = std::sqrt((a[i].vx - b[i].vx) * (a[i].vx - b[i].vx) +
                                   (a[i].vy - b[i].vy) * (a[i].vy - b[i].vy) +
                                   (a[i].vz - b[i].vz) * (a[i].vz - b[i].vz));
        const float dp = std::sqrt((a[i].px - b[i].px) * (a[i].px - b[i].px) +
                                   (a[i].py - b[i].py) * (a[i].py - b[i].py) +
                                   (a[i].pz - b[i].pz) * (a[i].pz - b[i].pz));
        r.maxVelDev = std::max(r.maxVelDev, dv);
        if (r.firstVelTick < 0 && dv > kVelocityEps)
            r.firstVelTick = static_cast<int>(i);
        if (r.firstGroundTick < 0 && a[i].grounded != b[i].grounded)
            r.firstGroundTick = static_cast<int>(i);
        if (r.firstPosTick < 0 && dp > kPositionEps)
            r.firstPosTick = static_cast<int>(i);
        r.finalPosDev = dp;
    }
    return r;
}

std::vector<TickTrace> oracleFixture(const Scenario& s)
{
    return runOracle(s);
}

bool writeFixture(const Scenario& s, std::string& report)
{
    std::error_code ec;
    std::filesystem::create_directories(kFixtureDir, ec);
    const std::string path = std::string(kFixtureDir) + "/" + s.name + ".csv";
    std::ofstream out(path);
    if (!out) {
        report += std::string("[FAIL] could not write ") + path + "\n";
        return false;
    }
    out << "tick,px,py,pz,vx,vy,vz,grounded\n";
    char line[160];
    const std::vector<TickTrace> trace = oracleFixture(s);
    for (size_t i = 0; i < trace.size(); ++i) {
        const TickTrace& t = trace[i];
        std::snprintf(line, sizeof(line),
                      "%zu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", i, t.px, t.py,
                      t.pz, t.vx, t.vy, t.vz, t.grounded ? 1 : 0);
        out << line;
    }
    report += std::string("[ok] fixture ") + s.name + " (" +
              std::to_string(trace.size()) + " ticks)\n";
    return true;
}

const Scenario kScenarios[] = {
    {"gravity_fall", 120, 5.0f, false},
    {"ground_walk", 120, 1.8f, true},
    {"ground_walk_diagonal", 120, 1.8f, true},
    {"accelerate_friction", 120, 1.8f, true},
    {"jump_land", 120, 1.8f, false},
    {"air_move", 120, 5.0f, false},
    {"dash_ground", 120, 1.8f, true},
    {"dash_air", 120, 5.0f, false},
    {"down_dash_air", 120, 5.0f, false},
    {"freeze_air", 120, 5.0f, false},
};

void appendRow(std::string& report, const char* scenario, const char* path,
               const CompareResult& r)
{
    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "%-12s %-11s firstVelTick=%-4d firstGroundTick=%-4d "
                  "firstPosTick=%-4d maxVelDev=%.4f finalPosDev=%.4f\n",
                  scenario, path, r.firstVelTick, r.firstGroundTick,
                  r.firstPosTick, r.maxVelDev, r.finalPosDev);
    report += buf;
}

// Focused check: the loaded hot freeze policy must reproduce the v2.0.6
// piecewise-quadratic all-axis multiplier (physics-freeze.cpp) at known times.
bool checkFreezeCurve(std::string& report)
{
    struct Case {
        float t;
        float expect;
    };
    const Case cases[] = {
        {0.0f, 0.0f}, {1.25f, 0.05f}, {2.5f, 0.2f}, {3.75f, 0.4f}, {5.0f, 1.0f}};

    bool ok = true;
    for (const Case& c : cases) {
        GameFreezePolicyV1 p{};
        p.velocity[0] = 1.0f;
        p.velocity[1] = 1.0f;
        p.velocity[2] = 1.0f;
        p.dt = 0.0f;
        p.durationSeconds = 5.0f;
        p.freezePressed = 0u;
        p.freezeHeld = 1u;
        p.freezeHeldPreviously = 1u;
        p.freezeEnabled = 1u;
        p.freezeActive = 1u;
        p.freezeAvailable = 0u;
        p.freezeTimerSeconds = c.t;
        p.movementModel = 1u;  // v2.0.6 destructive freeze curve
        p.handled = 0u;

        const bool handled =
            LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_FREEZE, &p,
                                                  sizeof(p), 0, 0, 0) &&
            p.handled != 0u;
        const float got = p.outVelocity[2];
        const bool pass = handled && std::fabs(got - c.expect) < 1e-3f;

        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "[%s] freeze curve t=%.2f expected=%.4f hot=%.4f\n",
                      pass ? "ok" : "FAIL", c.t, c.expect, got);
        report += buf;
        ok = ok && pass;
    }
    return ok;
}

// Switch fixture: prove the behaviorSource selector and the JSON loader respond
// to config/movement.json, and that the JSON preset can express the compiled
// source preset's key values. Restores the original file afterwards.
bool checkMovementSourceSwitch(std::string& report)
{
    const char* path = "config/movement.json";
    std::string original;
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            report += "[FAIL] switch fixture: config/movement.json missing\n";
            return false;
        }
        original.assign(std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>());
    }

    struct Restore {
        const char* path;
        const std::string& original;
        ~Restore() {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << original;
        }
    } restore{path, original};

    auto write = [&](const char* text) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    };
    auto check = [&](bool cond, const char* name) {
        report += std::string(cond ? "[ok] " : "[FAIL] ") + name + "\n";
        return cond;
    };

    bool ok = true;

    write("{\"behaviorSource\":\"json\",\"preset\":\"source\"}");
    {
        std::string name;
        const auto s = MimitaHotMovement::movementBehaviorSourceFromJson(&name);
        ok &= check(s == MimitaHotMovement::MovementBehaviorSource::Json &&
                        name == "source",
                    "switch fixture: behaviorSource=json resolves json/source");
    }

    write("{\"behaviorSource\":\"cpp\",\"preset\":\"source\"}");
    ok &= check(MimitaHotMovement::movementBehaviorSourceFromJson() ==
                    MimitaHotMovement::MovementBehaviorSource::Cpp,
                "switch fixture: behaviorSource=cpp resolves cpp");

    write("{ this is not valid json ");
    ok &= check(MimitaHotMovement::movementBehaviorSourceFromJson() ==
                    MimitaHotMovement::MovementBehaviorSource::Cpp,
                "switch fixture: invalid movement.json falls back to cpp");

    write("{\"behaviorSource\":\"json\",\"preset\":\"source\"}");
    {
        GameMovementTuningV1 jsonTuning{};
        const bool loaded =
            MimitaHotMovement::loadJsonMovementPreset("source", jsonTuning);
        const auto& cppTuning = MimitaHotMovement::getMovementPreset(
            MimitaHotMovement::movementPresetIdFromName("source")).tuning;
        const bool matches =
            loaded &&
            static_cast<int>(jsonTuning.walkMode) ==
                static_cast<int>(cppTuning.walkMode) &&
            std::fabs(jsonTuning.groundAcceleration -
                      cppTuning.groundAcceleration) < 1e-3f &&
            std::fabs(jsonTuning.airAcceleration -
                      cppTuning.airAcceleration) < 1e-3f &&
            std::fabs(jsonTuning.gravityMagnitude -
                      cppTuning.gravityMagnitude) < 1e-3f &&
            std::fabs(jsonTuning.groundDashImpulse -
                      cppTuning.groundDashImpulse) < 1e-3f &&
            jsonTuning.maximumAirJumps == cppTuning.maximumAirJumps;
        ok &= check(matches,
                    "switch fixture: JSON source preset matches compiled source");
    }

    return ok;
}

} // namespace

bool runMovementV206ParitySelfTest(std::string& report)
{
    report += "v2.0.6 oracle vs current cold shared movement kernel\n";
    report += "path 'cpp' = makeMovementConfigForPreset(source); "
              "'json' = config/movement/movement-source.json\n";
    report +=
        "--------------------------------------------------------------------------------\n";

    HotReloadSystem::instance().startup();

    // Collision-authoritative cold path: bind a floor world so the cold path
    // resolves through collision.main exactly like the oracle floor.
    ::World parityWorld;
    {
        ::CollisionTriangle tri;
        tri.a = glm::vec3(-200.0f, -200.0f, 0.0f);
        tri.b = glm::vec3(200.0f, -200.0f, 0.0f);
        tri.c = glm::vec3(200.0f, 200.0f, 0.0f);
        tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        parityWorld.collisionMesh.triangles.push_back(tri);
    }
    LiveBehavior::setDispatchWorld(&parityWorld);

    const MovementConfig cppConfig = makeMovementConfigForPreset(
        static_cast<std::uint32_t>(MimitaHotMovement::movementPresetIdFromName(
            "source")));
    MovementConfig jsonConfig;
    const bool jsonLoaded =
        MovementJsonConfig::instance().loadPresetInto("source", jsonConfig);
    if (jsonLoaded)
        jsonConfig = applyRuntimeMovementTuning(jsonConfig);

    int divergenceCount = 0;
    for (const Scenario& s : kScenarios) {
        const std::vector<TickTrace> oracle = runOracle(s);
        const std::vector<TickTrace> cpp = runCold(cppConfig, s);
        appendRow(report, s.name, "cpp", compare(oracle, cpp));
        if (jsonLoaded) {
            const std::vector<TickTrace> json = runCold(jsonConfig, s);
            appendRow(report, s.name, "json", compare(oracle, json));
        }
        const CompareResult rc = compare(oracle, cpp);
        if (rc.firstVelTick >= 0)
            ++divergenceCount;
    }

    report +=
        "--------------------------------------------------------------------------------\n";
    report += "summary: " + std::to_string(divergenceCount) + "/" +
              std::to_string(static_cast<int>(sizeof(kScenarios) /
                                              sizeof(kScenarios[0]))) +
              " scenarios diverge from the v2.0.6 oracle on cpp velocity\n";
    if (!jsonLoaded)
        report += "[FAIL] JSON preset config/movement/movement-source.json did not load\n";

    const bool freezeOk = checkFreezeCurve(report);
    const bool switchOk = checkMovementSourceSwitch(report);

    HotReloadSystem::instance().unloadGameDLL();

    // The harness always "runs"; it reports divergence rather than failing on
    // it, because divergence is the expected Phase A work list. Only a load
    // failure, a wrong freeze curve, or a broken source switch is a hard error.
    return jsonLoaded && freezeOk && switchOk;
}

bool generateMovementV206Fixtures(std::string& report)
{
    bool ok = true;
    for (const Scenario& s : kScenarios)
        ok = writeFixture(s, report) && ok;
    report += std::string("[MOVEMENT V206 FIXTURES] ") + (ok ? "PASS" : "FAIL") +
              "\n";
    return ok;
}

} // namespace MimitaV206
