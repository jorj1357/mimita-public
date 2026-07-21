// 07 21 2026, 17 10
/* purpose
* Tests Stage 3A client-trusting movement validation and snapshot lifecycle helpers.
* Verifies sequence, spawn generation, transform epoch, bounds, wall, and impulse behavior.
* Runs deterministic randomized accepted-report coverage without graphics or sockets.
* Does NOT launch the game, poll network transports, or render interpolation frames.
* Does NOT test damage, ammo, projectile authority, or ICE coordinator behavior.
* Does NOT replace live multi-client networking smoke tests.
*/

#include "network/movement-validation.h"
#include "network/server.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace {

constexpr uint32_t kSeed = 3193003;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::printf("[movement-network-integration-test] FAIL %s\n", message);
        std::exit(1);
    }
}

MovementConfig baseMovementConfig()
{
    MovementConfig config;
    config.groundSpeed = 24.0f;
    config.groundDashImpulse = 80.0f;
    config.downDashVerticalSpeed = -120.0f;
    config.jumpVerticalSpeed = 34.0f;
    config.maximumFallSpeed = 240.0f;
    config.maximumExternalImpulseSpeed = 120.0f;
    config.maximumAirJumps = 1;
    return config;
}

MimitaNet::ServerPlayer activePlayer()
{
    MimitaNet::ServerPlayer player;
    player.id = 7;
    player.spawned = true;
    player.spawnState = MimitaNet::ServerPlayer::Active;
    player.dead = false;
    player.health = 100;
    player.spawnGeneration = 3;
    player.transformEpoch = 2;
    player.pos = {0.0f, 0.0f, 0.0f};
    player.vel = {0.0f, 0.0f, 0.0f};
    player.sizeScale = 1.0f;
    player.lastAcceptedClientPosition = player.pos;
    player.lastAcceptedClientVelocity = player.vel;
    player.lastAcceptedClientTransformMs = 1000;
    player.hasAcceptedClientTransform = true;
    MimitaNet::resetServerMovementForAuthoritativeLifecycle(
        player, baseMovementConfig());
    return player;
}

MimitaNet::MovementValidationContext validationContext(
    const MimitaNet::HeadlessWorld* world = nullptr)
{
    MimitaNet::MovementValidationContext context;
    context.playerExists = true;
    context.connectionActive = true;
    context.connectionOwnsPlayer = true;
    context.serverTick = 100;
    context.nowMs = 1016;
    context.world = world;
    return context;
}

uint32_t defaultMovementFlags()
{
    return MimitaNet::MOVEMENT_REPORT_DASH_AVAILABLE |
           MimitaNet::MOVEMENT_REPORT_DOWN_DASH_AVAILABLE |
           MimitaNet::MOVEMENT_REPORT_FREEZE_AVAILABLE |
           MimitaNet::MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE |
           MimitaNet::MOVEMENT_REPORT_AIR_JUMP_ARMED;
}

MimitaNet::ClientMovementReport baseReport(
    const MimitaNet::ServerPlayer& player,
    uint32_t sequence,
    uint64_t clientTick)
{
    MimitaNet::ClientMovementReport report;
    report.playerId = player.id;
    report.movementSequence = sequence;
    report.clientSimulationTick = clientTick;
    report.lifecycle.spawnGeneration = player.spawnGeneration;
    report.lifecycle.transformEpoch = player.transformEpoch;
    report.moveAxes = {0.2f, 0.0f};
    report.horizontalCameraForward = {1.0f, 0.0f, 0.0f};
    report.position = player.pos + glm::vec3(0.2f, 0.0f, 0.0f);
    report.baseVelocity = {8.0f, 0.0f, 0.0f};
    report.externalImpulse = {0.0f, 0.0f, 0.0f};
    report.yaw = 1.25f;
    report.lookPitch = -4.0f;
    report.sizeScale = 1.0f;
    report.movementFlags = defaultMovementFlags();
    report.clientPingMs = 24;
    return report;
}

void acceptForNextReport(MimitaNet::ServerPlayer& player,
                         const MimitaNet::ClientMovementReport& report,
                         const MimitaNet::MovementValidationResult& result)
{
    player.movement = result.acceptedState;
    player.pos = result.acceptedState.position;
    player.vel = result.acceptedState.baseVelocity +
        result.acceptedState.externalImpulse;
    player.lastAcceptedClientPosition = player.pos;
    player.lastAcceptedClientVelocity = player.vel;
    player.lastAcceptedClientTransformMs += 16;
    player.hasAcceptedClientTransform = true;
    player.lastMovementSequence = report.movementSequence;
    player.hasMovementSequence = true;
    player.movementValidation.lastAcceptedSequence = report.movementSequence;
    player.movementValidation.lastAcceptedClientTick =
        report.clientSimulationTick;
}

void testValidationDecisions()
{
    MimitaNet::ServerPlayer player = activePlayer();
    const MovementConfig movementConfig = baseMovementConfig();
    MimitaNet::MovementValidationConfig config =
        MimitaNet::makeMovementValidationConfig(movementConfig);

    MimitaNet::ClientMovementReport report = baseReport(player, 1, 100);
    MimitaNet::MovementValidationResult result =
        MimitaNet::validateClientMovementReport(
            player, report, validationContext(), config);
    check(result.decision == MimitaNet::MovementValidationDecision::Accept,
          "valid report accepted");
    check(result.acceptedState.externalImpulse == glm::vec3(0.0f),
          "accepted state preserves zero impulse");
    MimitaNet::applyMovementValidationCounters(
        player.movementValidation, result, report);
    acceptForNextReport(player, report, result);

    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(), config);
    check(result.reason == MimitaNet::MovementValidationReason::DuplicateSequence,
          "duplicate sequence rejected");

    report = baseReport(player, 2, 101);
    report.lifecycle.spawnGeneration = player.spawnGeneration - 1;
    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(), config);
    check(result.reason ==
              MimitaNet::MovementValidationReason::SpawnGenerationMismatch,
          "old spawn generation rejected");

    report = baseReport(player, 2, 101);
    report.lifecycle.transformEpoch = player.transformEpoch + 1;
    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(), config);
    check(result.reason ==
              MimitaNet::MovementValidationReason::TransformEpochMismatch,
          "wrong transform epoch rejected");

    report = baseReport(player, 2, 500);
    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(), config);
    check(result.reason == MimitaNet::MovementValidationReason::FutureClientTick,
          "future client tick rejected");

    report = baseReport(player, 2, 101);
    report.externalImpulse = {30.0f, 0.0f, 0.0f};
    report.position = player.pos + glm::vec3(2.0f, 0.0f, 0.0f);
    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(), config);
    check(result.decision == MimitaNet::MovementValidationDecision::Accept,
          "bounded external impulse accepted");
    check(std::abs(result.acceptedState.externalImpulse.x - 30.0f) < 0.001f,
          "accepted state preserves external impulse");
}

void testCorrections()
{
    MimitaNet::ServerPlayer player = activePlayer();
    MovementConfig movementConfig = baseMovementConfig();
    MimitaNet::MovementValidationConfig config =
        MimitaNet::makeMovementValidationConfig(movementConfig);
    config.worldBoundsMin = {-1.0f, -1.0f, -1.0f};
    config.worldBoundsMax = {1.0f, 1.0f, 1.0f};
    config.worldBoundsPadding = 0.0f;

    MimitaNet::ClientMovementReport report = baseReport(player, 1, 100);
    report.position = {2.0f, 0.0f, 0.0f};
    MimitaNet::MovementValidationResult result =
        MimitaNet::validateClientMovementReport(
            player, report, validationContext(), config);
    check(result.decision == MimitaNet::MovementValidationDecision::Correct,
          "out-of-bounds report corrected");
    check(result.reason == MimitaNet::MovementValidationReason::OutOfBounds,
          "out-of-bounds correction reason");
    check(result.acceptedState.position.x <= 1.001f,
          "out-of-bounds position clamped");

    MimitaNet::HeadlessWorld world;
    world.boundsMin = {-10.0f, -10.0f, -10.0f};
    world.boundsMax = {10.0f, 10.0f, 10.0f};
    CollisionTriangle wall;
    wall.a = {1.0f, -2.0f, -2.0f};
    wall.b = {1.0f, 2.0f, -2.0f};
    wall.c = {1.0f, 0.0f, 2.0f};
    wall.normal = {-1.0f, 0.0f, 0.0f};
    world.triangles.push_back(wall);
    config = MimitaNet::makeMovementValidationConfig(movementConfig, &world);
    report = baseReport(player, 1, 100);
    report.position = {2.0f, 0.0f, 0.0f};
    result = MimitaNet::validateClientMovementReport(
        player, report, validationContext(&world), config);
    check(result.decision == MimitaNet::MovementValidationDecision::Correct,
          "wall-crossing report corrected");
    check(result.reason == MimitaNet::MovementValidationReason::BlockingGeometry,
          "wall-crossing correction reason");
    check(result.acceptedState.position == player.pos,
          "wall correction holds previous position");
}

void testLifecycleHelpers()
{
    check(MimitaNet::movementReportSequenceIsNewer(1u, 0xfffffffeu),
          "sequence wrap treated as newer");
    check(!MimitaNet::movementReportSequenceIsNewer(20u, 20u),
          "same sequence is not newer");
    check(!MimitaNet::movementReportSequenceIsNewer(19u, 20u),
          "older sequence is not newer");

    check(MimitaNet::movementSnapshotIsFresh(11, 4, 2, 10, 4, 2),
          "newer snapshot tick accepted");
    check(!MimitaNet::movementSnapshotIsFresh(10, 4, 2, 10, 4, 2),
          "duplicate snapshot tick rejected");
    check(!MimitaNet::movementSnapshotIsFresh(12, 3, 2, 10, 4, 2),
          "older spawn generation rejected");
    check(MimitaNet::movementSnapshotIsFresh(9, 5, 2, 10, 4, 2),
          "new spawn generation accepted despite lower tick");
    check(!MimitaNet::movementSnapshotIsFresh(12, 4, 1, 10, 4, 2),
          "older transform epoch rejected");
    check(MimitaNet::movementSnapshotIsFresh(9, 4, 3, 10, 4, 2),
          "new transform epoch accepted despite lower tick");

    MimitaNet::ServerPlayer player = activePlayer();
    player.hasMovementSequence = true;
    player.lastMovementSequence = 44;
    player.spawnState = MimitaNet::ServerPlayer::AwaitingSpawnAck;
    MimitaNet::resetServerMovementForAuthoritativeLifecycle(
        player, baseMovementConfig());
    check(!player.movement.movementEnabled,
          "awaiting spawn ack disables movement");
    check(!player.hasMovementSequence && player.lastMovementSequence == 0,
          "lifecycle reset clears movement sequence");

    player.spawnState = MimitaNet::ServerPlayer::Active;
    MimitaNet::resetServerMovementForAuthoritativeLifecycle(
        player, baseMovementConfig());
    check(player.movement.movementEnabled,
          "active spawn lifecycle enables movement");
}

void testExternalImpulseBridge()
{
    MimitaNet::ServerPlayer player = activePlayer();
    player.vel = {15.0f, 0.0f, 0.0f};
    MimitaNet::recordServerMovementExternalImpulse(
        player, glm::vec3(5.0f, 0.0f, 0.0f));
    check(std::abs(player.movement.externalImpulse.x - 5.0f) < 0.001f,
          "server impulse stored");
    check(std::abs(player.movement.baseVelocity.x - 10.0f) < 0.001f,
          "server impulse does not double-count effective velocity");
}

void testRandomizedAcceptedReports()
{
    std::mt19937 rng(kSeed);
    std::uniform_real_distribution<float> axis(-0.8f, 0.8f);
    std::uniform_real_distribution<float> stepDelta(-0.08f, 0.08f);
    std::uniform_real_distribution<float> velocity(-12.0f, 12.0f);

    MimitaNet::ServerPlayer player = activePlayer();
    MimitaNet::MovementValidationConfig config =
        MimitaNet::makeMovementValidationConfig(baseMovementConfig());

    for (uint32_t i = 0; i < 256; ++i)
    {
        MimitaNet::ClientMovementReport report =
            baseReport(player, i + 1, 100 + i);
        report.moveAxes = {axis(rng), axis(rng)};
        report.position = player.pos + glm::vec3(
            stepDelta(rng), stepDelta(rng), stepDelta(rng));
        report.baseVelocity = {velocity(rng), velocity(rng), velocity(rng)};
        report.externalImpulse = {0.0f, 0.0f, 0.0f};

        MimitaNet::MovementValidationContext context = validationContext();
        context.serverTick = 100 + i;
        context.nowMs = 1016 + (uint64_t)i * 16;
        MimitaNet::MovementValidationResult result =
            MimitaNet::validateClientMovementReport(
                player, report, context, config);
        check(result.decision == MimitaNet::MovementValidationDecision::Accept,
              "random bounded report accepted");
        acceptForNextReport(player, report, result);
    }
}

} // namespace

int main()
{
    testValidationDecisions();
    testCorrections();
    testLifecycleHelpers();
    testExternalImpulseBridge();
    testRandomizedAcceptedReports();
    std::printf("[movement-network-integration-test] PASS seed=%u randomizedCases=256\n",
                kSeed);
    return 0;
}
