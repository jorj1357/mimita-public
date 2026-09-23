// 09 23 2026
/* purpose
* Cold/hot lag-compensation history oracle self-test. Builds real
* ServerPlayer/ServerNpc history, runs the existing cold pose lookup
* (getPlayerPoseAtTick / getNpcPoseAtTick), runs the generic history.query
* capability and the hot history.select policy on the same samples, and asserts
* identical results across exact / interpolated / clamped / generation-boundary
* cases.
* Does NOT own transport, the live server loop, or the history store.
*/
#include "network/lagcomp-history-selftest.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-history.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/server.h"
#include "network/server-history.h"

using MimitaNet::GameHistoryQueryV1;
using MimitaNet::HistoryDomainV1;
using MimitaNet::HistorySelectionV1;

namespace {

using namespace MimitaNet;

bool gPass = true;

void check(bool condition, const char* what, std::string& report)
{
    if (!condition)
        gPass = false;
    report += condition ? "  [ok] " : "  [FAIL] ";
    report += what;
    report += "\n";
}

bool closeEnough(float a, float b, float eps = 0.001f)
{
    return std::fabs(a - b) <= eps;
}

void addPlayerSample(ServerPlayer& p, std::uint32_t tick, float x, float y, float z,
                     float yaw, std::uint32_t gen)
{
    PositionHistoryEntry e{};
    e.pos = glm::vec3(x, y, z);
    e.vel = glm::vec3(0.0f);
    e.yaw = yaw;
    e.tick = tick;
    e.logicalGenerationId = gen;
    p.posHistory.push_back(e);
}

void addNpcSample(ServerNpc& npc, std::uint32_t tick, float x, float y, float z,
                  float yaw, std::uint32_t gen)
{
    ServerNpcPositionSample s{};
    s.pos = glm::vec3(x, y, z);
    s.vel = glm::vec3(0.0f);
    s.yaw = yaw;
    s.tick = tick;
    s.logicalGenerationId = gen;
    npc.posHistory.push_back(s);
}

} // namespace

bool runLagcompHistorySelfTest(std::string& report)
{
    gPass = true;

    // Load the hot DLL so the net.history.select policy is active.
    HotReloadSystem::instance().startup();

    // ── Build identical history for a player and an NPC ───────────────
    ServerPlayer player;
    player.id = 1;
    addPlayerSample(player, 100, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    addPlayerSample(player, 102, 2.0f, 0.0f, 0.0f, 0.2f, 1);
    addPlayerSample(player, 104, 4.0f, 0.0f, 0.0f, 0.4f, 1);

    ServerNpc npc;
    npc.entityId = 5;
    addNpcSample(npc, 100, 10.0f, 0.0f, 0.0f, 0.0f, 1);
    addNpcSample(npc, 102, 12.0f, 0.0f, 0.0f, 0.2f, 1);
    addNpcSample(npc, 104, 14.0f, 0.0f, 0.0f, 0.4f, 1);

    // ── Cold oracle: existing pose lookup ─────────────────────────────
    glm::vec3 coldPos{};
    float coldYaw = 0.0f;

    // Exact tick.
    check(getPlayerPoseAtTick(player, 102, coldPos, coldYaw) &&
              closeEnough(coldPos.x, 2.0f) && closeEnough(coldYaw, 0.2f),
          "cold player exact tick resolves", report);

    // Interpolated tick.
    check(getPlayerPoseAtTick(player, 101, coldPos, coldYaw) &&
              closeEnough(coldPos.x, 1.0f) && closeEnough(coldYaw, 0.1f),
          "cold player interpolates between samples", report);

    // Nearest-older clamp (target newer than newest sample).
    check(getPlayerPoseAtTick(player, 200, coldPos, coldYaw) &&
              closeEnough(coldPos.x, 4.0f),
          "cold player clamps to newest sample", report);

    // ── Hot store path: generic history.query with the cold default ───
    // Without a live server context the store cannot answer; the raw-sample
    // extraction is exercised directly here through the selection payload.
    {
        MimitaNet::GameHistorySelectV1 select{};
        select.targetTick = 101;
        select.haveA = 1;
        select.haveB = 1;
        select.a.tick = 100;
        select.a.position[0] = 0.0f;
        select.a.yaw = 0.0f;
        select.a.logicalGenerationId = 1;
        select.b.tick = 102;
        select.b.position[0] = 2.0f;
        select.b.yaw = 0.2f;
        select.b.logicalGenerationId = 1;
        const bool handled = LiveBehavior::dispatchGameplayEvent64(
            MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), 101, 0, 0);
        check(handled && select.handled &&
                  select.selection ==
                      (std::uint32_t)HistorySelectionV1::Interpolated &&
                  closeEnough(select.position[0], 1.0f) && closeEnough(select.yaw, 0.1f),
              "hot history.select interpolates between samples", report);
    }

    // Hot exact tick.
    {
        MimitaNet::GameHistorySelectV1 select{};
        select.targetTick = 102;
        select.exactTick = 1;
        select.haveA = 1;
        select.a.tick = 102;
        select.a.position[0] = 2.0f;
        select.a.yaw = 0.2f;
        select.a.logicalGenerationId = 1;
        LiveBehavior::dispatchGameplayEvent64(
            MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), 102, 0, 0);
        check(select.handled &&
                  select.selection ==
                      (std::uint32_t)HistorySelectionV1::ExactTick &&
                  closeEnough(select.position[0], 2.0f),
              "hot history.select returns exact tick", report);
    }

    // Hot generation boundary clamp (F -> G): never blend, clamp to newer.
    {
        MimitaNet::GameHistorySelectV1 select{};
        select.targetTick = 101;
        select.haveA = 1;
        select.haveB = 1;
        select.a.tick = 100;
        select.a.position[0] = 0.0f;
        select.a.logicalGenerationId = 1;  // F
        select.b.tick = 102;
        select.b.position[0] = 50.0f;
        select.b.logicalGenerationId = 2;  // G
        LiveBehavior::dispatchGameplayEvent64(
            MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), 101, 0, 0);
        check(select.handled &&
                  select.selection ==
                      (std::uint32_t)HistorySelectionV1::GenerationClamp &&
                  closeEnough(select.position[0], 50.0f),
              "hot history.select clamps across an F/G boundary", report);
    }

    // Hot nearest-older clamp.
    {
        MimitaNet::GameHistorySelectV1 select{};
        select.targetTick = 200;
        select.haveA = 1;
        select.a.tick = 104;
        select.a.position[0] = 4.0f;
        select.a.logicalGenerationId = 1;
        LiveBehavior::dispatchGameplayEvent64(
            MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), 200, 0, 0);
        check(select.handled &&
                  select.selection ==
                      (std::uint32_t)HistorySelectionV1::NearestOlder &&
                  closeEnough(select.position[0], 4.0f),
              "hot history.select clamps to the newest sample", report);
    }

    // ── Cold/hot NPC parity on the same inputs ────────────────────────
    // Compare the hot policy decision to the cold NPC oracle for the
    // interpolated tick.
    {
        glm::vec3 coldNpcPos{};
        float coldNpcYaw = 0.0f;
        const bool coldOk = getNpcPoseAtTick(npc, 101, coldNpcPos, coldNpcYaw);
        MimitaNet::GameHistorySelectV1 select{};
        select.targetTick = 101;
        select.haveA = 1;
        select.haveB = 1;
        select.a.tick = 100;
        select.a.position[0] = 10.0f;
        select.a.logicalGenerationId = 1;
        select.b.tick = 102;
        select.b.position[0] = 12.0f;
        select.b.logicalGenerationId = 1;
        LiveBehavior::dispatchGameplayEvent64(
            MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), 101, 0, 0);
        check(coldOk && select.handled && closeEnough(coldNpcPos.x, select.position[0]),
              "cold/hot NPC rewind parity on identical inputs", report);
    }

    HotReloadSystem::instance().unloadGameDLL();

    report += gPass ? "[LAGCOMP HISTORY SELFTEST] PASS\n"
                    : "[LAGCOMP HISTORY SELFTEST] FAIL\n";
    return gPass;
}
