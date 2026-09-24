// 09 12 2026
/* purpose
* Implements the headless live-code self-test.
* Proves UTC formatting, SHA-256 hashing, JSONL journal writing, and the
* GameAPI load + ABI + self-test path that runs before any graphics startup.
* Does NOT own gameplay or the hot-reload activation policy.
*/
#include "live-code/live-code-selftest.h"

#include "hot-reload/hot-reload-system.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-packet-codec.h"
#include "hot-reload/hot-snapshot-codec.h"
#include "hot-reload/hot-movement-validation.h"
#include "hot-reload/hot-physical-contact.h"
#include "hot-reload/hot-damage-application.h"
#include "hot-reload/hot-attack-gates.h"
#include "hot-reload/hot-kill-attribution.h"
#include "hot-reload/hot-npc-targeting.h"
#include "hot-reload/hot-projectile-splash.h"
#include "hot-reload/hot-respawn.h"
#include "hot-reload/hot-attack-claim.h"
#include "hot-reload/hot-npc-ground-clamp.h"
#include "hot-reload/hot-broadcast-interp.h"
#include "hot-reload/hot-join-policy.h"
#include "hot-reload/hot-session-policy.h"
#include "hot-reload/hot-reload-decision.h"
#include "hot-reload/hot-client-snapshot.h"
#include "hot-reload/hot-projectile-correction.h"
#include "hot-reload/hot-connection-health.h"
#include "hot-reload/hot-connection-transition.h"
#include "hot-reload/hot-server-tick.h"
#include "hot-reload/hot-projectile-cancel.h"
#include "hot-reload/hot-server-policy.h"
#include "hot-reload/hot-geometry.h"
#include "network/packet-codec-wire.h"
#include "network/snapshot-chunks.h"
#include "debug/structured-log.h"
#include "live-code/code-hash.h"
#include "live-code/live-actor.h"
#include "live-code/live-behavior.h"
#include "live-code/live-gameplay.h"
#include "live-code/live-journal.h"
#include "live-code/live-presentation.h"
#include "utils/time-format.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

// Observation sink for the hot handler's `net.packet-reply` in headless mode.
// When `sendSocket` is set, the reply is sent over a real loopback UDP socket
// (the "server -> client" leg); otherwise it is captured in-process.
struct ReplyCapture {
    std::vector<std::uint8_t> bytes;
    std::uint32_t connectionId = 0;
    bool got = false;
    SOCKET sendSocket = INVALID_SOCKET;
    sockaddr_in sendTo{};
};
ReplyCapture* gReplyCapture = nullptr;

void captureHotPacketReply(std::uint32_t connectionId, const void* bytes,
                           std::uint32_t size)
{
    if (!gReplyCapture)
        return;
    gReplyCapture->connectionId = connectionId;
    if (gReplyCapture->sendSocket != INVALID_SOCKET && size > 0)
    {
        sendto(gReplyCapture->sendSocket, (const char*)bytes, (int)size, 0,
               (const sockaddr*)&gReplyCapture->sendTo, sizeof(gReplyCapture->sendTo));
    }
    gReplyCapture->bytes.assign(static_cast<const std::uint8_t*>(bytes),
                                static_cast<const std::uint8_t*>(bytes) + size);
    gReplyCapture->got = true;
}

} // namespace

bool runLiveCodeSelfTest(std::string& report)
{
    bool ok = true;

    const std::string timestamp = MiMitaTime::utcIso8601Millis();
    ok &= check(timestamp.size() == 24 && timestamp[10] == 'T' &&
                    timestamp.back() == 'Z',
                "utc millisecond format", report);

    const std::string emptyHash = LiveCodeHash::sha256Bytes("", 0);
    ok &= check(
        emptyHash ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "sha256 empty string", report);

    LiveEventJournal::instance().init();
    const std::string journalPath = LiveEventJournal::instance().path();
    ok &= check(LiveEventJournal::instance().active(), "journal active", report);

    LiveEventJournal::Fields fields;
    fields.result = "selftest";
    LiveEventJournal::instance().record("test_finished", fields);

    std::ifstream journal(journalPath);
    std::string lastLine;
    std::string line;
    while (std::getline(journal, line)) {
        if (!line.empty())
            lastLine = line;
    }
    ok &= check(lastLine.find("\"type\":\"test_finished\"") != std::string::npos,
                "journal line written", report);
    ok &= check(lastLine.find("\"ts_utc\":") != std::string::npos,
                "journal utc field", report);
    ok &= check(!lastLine.empty() && lastLine.front() == '{' && lastLine.back() == '}',
                "journal valid JSON object", report);
    LiveEventJournal::instance().shutdown();

    // ── Events JSONL self-test ──────────────────────────────────────────────
    // One events.jsonl, one clean record per message, no category .txt, every
    // line independent JSON, strictly increasing seq, UTC Z timestamps, and
    // bounded duplicate aggregation.
    {
        StructuredLogger::instance().init();
        const std::string eventsPath = StructuredLogger::instance().eventsPath();
        ok &= check(!eventsPath.empty(), "events.jsonl created", report);
        ok &= check(eventsPath.find("events.jsonl") != std::string::npos,
                    "events.jsonl path is jsonl", report);
        {
            // The run directory must contain only the JSONL stream.
            const std::filesystem::path runDir =
                std::filesystem::path(eventsPath).parent_path();
            bool onlyJsonl = true;
            std::error_code dirEc;
            for (const auto& entry :
                 std::filesystem::directory_iterator(runDir, dirEc)) {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().filename().string() != "events.jsonl")
                    onlyJsonl = false;
            }
            ok &= check(onlyJsonl, "no category .txt files", report);
        }

        // 500 identical events must collapse to one summary with count 500.
        for (int i = 0; i < 500; ++i) {
            debug::Event ev;
            ev.category = "LEGACY";
            ev.name = "selftest.repeat";
            ev.level = debug::Level::Info;
            ev.message = "repeated";
            ev.aggregationKey = "SELFTEST:repeat";
            debug::logEvent(ev);
        }
        debug::Event other;
        other.category = "NETWORK";
        other.name = "selftest.keychange";
        other.level = debug::Level::Info;
        other.message = "key change flushes the prior bucket";
        other.aggregationKey = "SELFTEST:keychange";
        debug::logEvent(other);
        debug::flushEvents();

        // Errors must never be aggregated away.
        debug::Event err;
        err.category = "EXECUTABLE";
        err.name = "selftest.error";
        err.level = debug::Level::Error;
        err.message = "error must stand alone";
        debug::logEvent(err);

        StructuredLogger::instance().shutdown();

        std::ifstream events(eventsPath);
        uint64_t lastSeq = 0;
        bool seqMonotonic = true;
        bool allJson = true;
        bool utcZ = true;
        bool sawSummary500 = false;
        bool sawError = false;
        uint32_t lineCount = 0;
        if (events.is_open()) {
            while (std::getline(events, line)) {
                if (line.empty())
                    continue;
                ++lineCount;
                allJson &= line.front() == '{' && line.back() == '}';
                try {
                    const nlohmann::json record = nlohmann::json::parse(line);
                    const uint64_t seq = record.value("seq", (uint64_t)0);
                    if (seq <= lastSeq)
                        seqMonotonic = false;
                    lastSeq = seq;
                    const std::string wall = record.value("wall_time", std::string());
                    if (wall.empty() || wall.back() != 'Z')
                        utcZ = false;
                    if (record.value("event", std::string()) == "selftest.repeat.summary" &&
                        record.value("count", 0) == 500)
                        sawSummary500 = true;
                    if (record.value("event", std::string()) == "selftest.error")
                        sawError = record.value("level", std::string()) == "ERROR";
                } catch (...) {
                    allJson = false;
                }
            }
        }
        ok &= check(lineCount > 0, "events.jsonl has records", report);
        ok &= check(allJson, "every line is valid JSON", report);
        ok &= check(seqMonotonic, "seq strictly increases", report);
        ok &= check(utcZ, "timestamps are UTC with Z", report);
        ok &= check(sawSummary500, "500 repeats -> one summary count=500", report);
        ok &= check(sawError, "errors are never aggregated", report);
    }

    HotReloadSystem::instance().startup();
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    report += "  activeGeneration=" + std::to_string(status.activeGeneration) + "\n";
    report += "  activeHash=" + status.activeHash + "\n";
    report += "  reloadCount=" + std::to_string(status.reloadCount) + "\n";
    ok &= check(status.loaded, "GameAPI load + ABI + self-test", report);

    if (status.loaded) {
        ok &= check(LiveActor::available(), "actor module present", report);
        ActorStateV1 actor{};
        actor.id = 7;
        actor.kind = 1;
        actor.health = 100.0f;
        actor.maxHealth = 100.0f;
        actor.emotionConfidence = 0.5f;
        actor.emotionFear = 0.5f;
        actor.distanceToTarget = 3.0f;
        actor.tick = 123;
        ActorCommandV1 command{};
        ok &= check(LiveActor::chooseCommand(actor, command) &&
                        command.speedScale > 0.0f && command.speedScale <= 1.6f,
                    "actor chooseCommand", report);
        ok &= check(LiveActor::chooseRole(actor) != 0, "actor chooseRole", report);

        DamageNumberStyleV1 damageBase{};
        damageBase.scale = 1.0f;
        damageBase.endScale = 1.0f;
        damageBase.alpha = 1.0f;
        damageBase.lifetime = 1.0f;
        damageBase.moveSpeed = 1.0f;
        damageBase.visible = 1;
        std::snprintf(damageBase.text, sizeof(damageBase.text), "150");
        DamageNumberStyleV1 damageOut{};
        ok &= check(LivePresentation::formatDamage(damageBase, 150, 0u, damageOut) &&
                        damageOut.scale > damageBase.scale,
                    "presentation damage format", report);

        RocketTrailStyleV1 trailBase{};
        trailBase.size = 0.25f;
        trailBase.endSize = 0.8f;
        trailBase.enabled = 1;
        RocketTrailStyleV1 trailOut{};
        ok &= check(LivePresentation::rocketTrail(trailBase, trailOut) &&
                        trailOut.size > trailBase.size,
                    "presentation rocket trail", report);

        RocketFlightStateV1 flightState{};
        RocketFlightParamsV1 flightBase{};
        flightBase.speedScale = 1.0f;
        flightBase.lifetime = 5.0f;
        RocketFlightParamsV1 flightOut{};
        // Assert the policy is callable and returns a usable result. Do NOT pin
        // the tuned output: the developer edits this policy live on purpose.
        ok &= check(LiveGameplay::rocketFlight(flightState, flightBase, flightOut) &&
                        std::isfinite(flightOut.speedScale) && flightOut.speedScale > 0.0f,
                    "gameplay rocket flight params", report);

        // Generic behavior path: the kernel emits a damage-policy event and the
        // hot behavior must handle it. Do NOT pin the returned damage value.
        DamagePolicyV1 policy{};
        policy.baseDamage = 123;
        policy.outDamage = 123;
        policy.source = GAME_DAMAGE_SOURCE_EXPLOSION;
        const bool handled = LiveBehavior::dispatchDamagePolicy(policy, 42);
        ok &= check(handled && policy.handled == 1 &&
                        std::isfinite((float)policy.outDamage) && policy.outDamage >= 0,
                    "hot damage policy dispatch", report);

        ActorLifecycleStateV1 actorLifecycle{};
        actorLifecycle.entityId = 42;
        actorLifecycle.actorKind = 2;
        actorLifecycle.lifeGeneration = 1;
        actorLifecycle.position[0] = 3.0f;
        actorLifecycle.maxHealth = 100;
        actorLifecycle.health = 100;
        ok &= check(LiveBehavior::dispatchActorLifecycle(actorLifecycle, 42) &&
                        actorLifecycle.handled == 1 &&
                        actorLifecycle.entityId == 42 &&
                        actorLifecycle.lifeGeneration == 1 &&
                        actorLifecycle.health == 100,
                    "generic hot actor lifecycle boundary", report);

        ActorAvatarPolicyV1 avatarPolicy{};
        avatarPolicy.entityId = 42;
        avatarPolicy.lifeGeneration = 1;
        avatarPolicy.candidateCount = 3;
        std::snprintf(avatarPolicy.candidates[0], sizeof(avatarPolicy.candidates[0]), "avatar-a");
        std::snprintf(avatarPolicy.candidates[1], sizeof(avatarPolicy.candidates[1]), "avatar-b");
        std::snprintf(avatarPolicy.candidates[2], sizeof(avatarPolicy.candidates[2]), "avatar-c");
        ok &= check(LiveBehavior::dispatchActorAvatarPolicy(avatarPolicy, 42) &&
                        avatarPolicy.handled == 1 &&
                        avatarPolicy.selectedIndex < avatarPolicy.candidateCount &&
                        avatarPolicy.selectedAvatar[0] != '\0',
                    "hot actor avatar policy", report);
        const std::string selectedAvatar = avatarPolicy.selectedAvatar;
        ActorAvatarPolicyV1 repeatAvatarPolicy = avatarPolicy;
        repeatAvatarPolicy.handled = 0;
        repeatAvatarPolicy.selectedAvatar[0] = '\0';
        ok &= check(LiveBehavior::dispatchActorAvatarPolicy(repeatAvatarPolicy, 42) &&
                        selectedAvatar == repeatAvatarPolicy.selectedAvatar,
                    "hot actor avatar policy deterministic per life", report);

        // ── Hot-module log capability round-trip ───────────────────────────
        // Proves the exact path the collision package uses: a hot caller
        // resolves `log.event` through the gameplay context and the record
        // lands in the same events.jsonl, so collision diagnostics are
        // observable live while the game runs.
        StructuredLogger::instance().init();
        const std::string capEventsPath = StructuredLogger::instance().eventsPath();
        bool emitted = false;
        if (GameplayContextV1* ctx = LiveBehavior::hostContext(1)) {
            auto logFn = reinterpret_cast<GameLogEventFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
            if (logFn) {
                GameLogEventV1 ev{};
                ev.level = 2u;
                ev.simulationTick = 1u;
                // NETWORK is enabled at "important" by default, so this proves
                // delivery independently of the COLLISION level setting.
                std::snprintf(ev.category, sizeof(ev.category), "NETWORK");
                std::snprintf(ev.name, sizeof(ev.name), "selftest.capability_log");
                std::snprintf(ev.message, sizeof(ev.message),
                              "hot log capability round-trip");
                std::snprintf(ev.result, sizeof(ev.result), "ok");
                logFn(ctx->host, &ev);
                emitted = true;
            }
        }
        StructuredLogger::instance().shutdown();
        ok &= check(emitted, "log.event capability resolves", report);
        bool sawCapabilityLog = false;
        std::ifstream probe(capEventsPath);
        while (std::getline(probe, line)) {
            if (line.find("selftest.capability_log") != std::string::npos)
                sawCapabilityLog = true;
        }
        ok &= check(sawCapabilityLog, "hot capability log reached events.jsonl",
                    report);

        // The cold `debug::logEvent` API must also route through the hot
        // provider while the package is active, so ordinary gameplay
        // diagnostics share the hot filtering/field/destination policy.
        {
            StructuredLogger::instance().init();
            const std::string providerEventsPath =
                StructuredLogger::instance().eventsPath();
            debug::Event ev;
            ev.category = "NETWORK";
            ev.name = "selftest.provider_route";
            ev.level = debug::Level::Info;
            ev.message = "cold logEvent routed through the hot provider";
            debug::logEvent(ev);
            StructuredLogger::instance().shutdown();

            bool sawProviderRoute = false;
            std::ifstream probe(providerEventsPath);
            while (std::getline(probe, line)) {
                if (line.find("selftest.provider_route") != std::string::npos)
                    sawProviderRoute = true;
            }
            ok &= check(sawProviderRoute,
                        "cold logEvent routed through hot provider", report);
        }

        // ── Hot packet-codec registry through the same generic doorway ─────
        // The hot module registers a real codec; the cold dispatcher resolves it
        // by capability id and looks it up by (schemaId, schemaVersion). This
        // proves a packet schema is a hot source edit with no EXE call site.
        {
            auto lookup = reinterpret_cast<MimitaNet::GamePacketCodecLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_PACKET_CODECS));
            const MimitaNet::GamePacketCodecDescriptorV1* codec =
                lookup ? lookup(nullptr, gameHash("packet.hot.ping"), 1) : nullptr;
            ok &= check(codec && codec->schemaVersion == 1 && codec->encode &&
                            codec->decode && codec->validate,
                        "hot packet-codecs provider resolves a live codec", report);

            // packet.ping: a core gameplay schema declared in the hot schema
            // layer (hot-packet-schemas.h), resolved through the same doorway.
            const MimitaNet::GamePacketCodecDescriptorV1* pingCodec =
                lookup ? lookup(nullptr, gameHash("packet.ping"), 1) : nullptr;
            ok &= check(pingCodec && pingCodec->encode && pingCodec->decode &&
                            pingCodec->validate,
                        "hot packet-schemas resolves packet.ping", report);
        }

        // ── Hot snapshot-codec registry through the same generic doorway ───
        // The hot module registers the snapshot codec; the cold dispatcher in
        // network/snapshot-chunks.cpp resolves it by capability id. This proves
        // snapshot serialization is a hot source edit with no EXE call site.
        {
            auto lookup = reinterpret_cast<MimitaNet::GameSnapshotCodecLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    GAME_CAP_SNAPSHOT_CODECS));
            const MimitaNet::GameSnapshotCodecV1* codec =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(codec && codec->build && codec->parse && codec->reassemble,
                        "hot snapshot-codecs provider resolves a live codec", report);
            if (codec && codec->build && codec->parse)
            {
                MimitaNet::CompactEntityData entity{};
                entity.networkEntityId = 1;
                entity.entityType = MimitaNet::ENTITY_PLAYER;
                entity.active = 1;
                entity.px = 1.0f; entity.py = 2.0f; entity.pz = 3.0f;
                entity.aimX = 1.0f;
                MimitaNet::SnapshotChunkPacket chunk{};
                MimitaNet::GameSnapshotBuildV1 build{};
                build.structSize = sizeof(MimitaNet::GameSnapshotBuildV1);
                build.entityCount = 1;
                build.serverTick = 99;
                build.ownerPlayerId = 3;
                build.entities = &entity;
                build.outChunks = &chunk;
                build.maxChunks = 1;
                codec->build(nullptr, &build);
                ok &= check(build.result == 1u && build.outChunkCount == 1u,
                            "hot snapshot codec builds a chunk", report);
                MimitaNet::SnapshotChunkPacket parsed{};
                MimitaNet::GameSnapshotParseV1 parse{};
                parse.structSize = sizeof(MimitaNet::GameSnapshotParseV1);
                parse.data = &chunk;
                parse.bytes = (std::uint32_t)MimitaNet::snapshotChunkWireSize(1);
                parse.out = &parsed;
                codec->parse(nullptr, &parse);
                ok &= check(parse.result == 1u && parsed.entityCount == 1u &&
                                parsed.entities[0].networkEntityId == 1,
                            "hot snapshot codec parses a chunk", report);
            }
        }

        // ── Hot movement-validation policy through the same generic doorway ─
        // The hot module registers the server movement-report policy; the cold
        // bridge in network/movement-validation.cpp resolves it by capability id.
        {
            auto validateFn = reinterpret_cast<MimitaNet::GameMovementValidateFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_MOVEMENT_VALIDATE));
            ok &= check(validateFn != nullptr,
                        "hot movement-validation provider resolves", report);
            if (validateFn)
            {
                auto fillBase = [](MimitaNet::GameMovementValidateV1& req) {
                    req.structSize = sizeof(MimitaNet::GameMovementValidateV1);
                    req.playerExists = 1; req.connectionActive = 1;
                    req.connectionOwnsPlayer = 1; req.spawned = 1;
                    req.spawnStateActive = 1; req.movementEnabled = 1;
                    req.spawnGeneration = 3; req.transformEpoch = 4;
                    req.reportSizeScale = 1.0f; req.acceptedStateFinite = 1;
                    for (int i = 0; i < 3; ++i) {
                        req.worldBoundsMin[i] = -100.0f;
                        req.worldBoundsMax[i] = 100.0f;
                    }
                    req.reportPosition[0] = 1.0f;
                    req.reportPosition[1] = 2.0f;
                    req.reportPosition[2] = 3.0f;
                };

                MimitaNet::GameMovementValidateV1 accept{};
                fillBase(accept);
                accept.reportSpawnGeneration = 3;
                accept.reportTransformEpoch = 4;
                validateFn(nullptr, &accept);
                ok &= check(accept.result == 1u && accept.decision == 0u,
                            "hot movement-validation accepts a valid report", report);

                MimitaNet::GameMovementValidateV1 stale{};
                fillBase(stale);
                stale.reportSpawnGeneration = 2;
                stale.reportTransformEpoch = 4;
                validateFn(nullptr, &stale);
                ok &= check(
                    stale.result == 1u && stale.decision == 2u &&
                        stale.reason == (std::uint32_t)
                            MimitaNet::MovementValidationReason::SpawnGenerationMismatch,
                    "hot movement-validation rejects a stale generation", report);
            }
        }

        // ── Hot physical-contact policy through the same generic doorway ───
        {
            auto lookup = reinterpret_cast<MimitaNet::GamePhysicalContactLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_PHYSICAL_CONTACT));
            const MimitaNet::GamePhysicalContactPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->damage && policy->knockback &&
                            policy->intervalTicks && policy->shouldConfirm,
                        "hot physical-contact provider resolves", report);
            if (policy && policy->intervalTicks)
                ok &= check(policy->intervalTicks(nullptr, 0.05f, 60.0f) == 3u,
                            "hot physical-contact interval ticks", report);
            if (policy && policy->damage)
            {
                MimitaNet::GamePhysicalContactDamageV1 dmg{};
                dmg.structSize = sizeof(MimitaNet::GamePhysicalContactDamageV1);
                dmg.kind = MimitaNet::GAME_PHYSICAL_KIND_SWORD;
                dmg.baseDamage = 10.0f;
                policy->damage(nullptr, &dmg);
                ok &= check(dmg.result == 1u && dmg.outDamage == 10,
                            "hot physical-contact sword damage", report);
            }
        }

        // ── Hot damage-application rules through the same generic doorway ──
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameDamageApplicationFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_DAMAGE_APPLICATION));
            ok &= check(evaluate != nullptr,
                        "hot damage-application provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameDamageApplicationV1 friendly{};
                friendly.structSize = sizeof(MimitaNet::GameDamageApplicationV1);
                friendly.attackerFound = 1;
                friendly.targetTeam = 1;
                friendly.attackerTeam = 1;
                friendly.targetHealth = 100;
                friendly.damage = 10;
                evaluate(nullptr, &friendly);
                ok &= check(friendly.accept == 0u &&
                                friendly.rejectReason == (std::uint32_t)
                                    MimitaNet::GAME_DAMAGE_REJECT_FRIENDLY_FIRE,
                            "hot damage-application rejects friendly fire", report);

                MimitaNet::GameDamageApplicationV1 lethal{};
                lethal.structSize = sizeof(MimitaNet::GameDamageApplicationV1);
                lethal.attackerFound = 1;
                lethal.targetTeam = 0;
                lethal.attackerTeam = 1;
                lethal.targetHealth = 10;
                lethal.damage = 30;
                lethal.respawnsEnabled = 1;
                lethal.respawnSeconds = 3.0f;
                evaluate(nullptr, &lethal);
                ok &= check(lethal.accept == 1u && lethal.killed == 1u &&
                                lethal.healthAfter == 0 &&
                                lethal.outRespawnSeconds == 3.0f,
                            "hot damage-application applies lethal + respawn rule",
                            report);
            }
        }

        // ── Hot attack gates through the same generic doorway ─────────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameAttackGatesFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_ATTACK_GATES));
            ok &= check(evaluate != nullptr,
                        "hot attack-gates provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameAttackGatesV1 cooldown{};
                cooldown.structSize = sizeof(MimitaNet::GameAttackGatesV1);
                cooldown.tick = 100;
                cooldown.nextAllowedFireTick = 110;
                cooldown.cooldownGraceTicks = 2;
                cooldown.maxShotsPerTick = 2;
                evaluate(nullptr, &cooldown);
                ok &= check(cooldown.cooldownReject == 1u,
                            "hot attack-gates reject cooldown", report);

                MimitaNet::GameAttackGatesV1 geometry{};
                geometry.structSize = sizeof(MimitaNet::GameAttackGatesV1);
                geometry.hotStateOwns = 1;
                geometry.isHitscan = 1;
                geometry.shotsThisTick = 0;
                geometry.maxShotsPerTick = 1;
                geometry.pingMs = 0.0f;
                geometry.origin[0] = 100.0f;
                geometry.direction[0] = 1.0f;
                evaluate(nullptr, &geometry);
                ok &= check(geometry.geometryReject == 1u,
                            "hot attack-gates reject far muzzle geometry", report);
            }
        }

        // ── Hot kill-attribution through the same generic doorway ─────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameKillAttributionFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_KILL_ATTRIBUTION));
            ok &= check(evaluate != nullptr,
                        "hot kill-attribution provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameKillAttributionV1 reattrib{};
                reattrib.structSize = sizeof(MimitaNet::GameKillAttributionV1);
                reattrib.victimId = 5;
                reattrib.lastNpcDamageSourceId = 7;
                reattrib.lastNpcDamageTick = 100;
                reattrib.tick = 110;
                reattrib.windowTicks = 120;
                evaluate(nullptr, &reattrib);
                ok &= check(reattrib.reattributed == 1u &&
                                reattrib.outAttackerNpcId == 7u,
                            "hot kill-attribution credits the recent NPC", report);
            }
        }

        // ── Hot NPC targeting through the same generic doorway ────────────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameNpcTargetingLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_NPC_TARGETING));
            const MimitaNet::GameNpcTargetingPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->hostile && policy->score,
                        "hot npc-targeting provider resolves", report);
            if (policy && policy->hostile)
            {
                MimitaNet::GameNpcHostilityV1 across{};
                across.structSize = sizeof(MimitaNet::GameNpcHostilityV1);
                across.teamA = 1;
                across.teamB = 2;
                policy->hostile(nullptr, &across);
                ok &= check(across.hostile == 1u,
                            "hot npc-targeting hostile across teams", report);

                MimitaNet::GameNpcHostilityV1 same{};
                same.structSize = sizeof(MimitaNet::GameNpcHostilityV1);
                same.teamA = 1;
                same.teamB = 1;
                policy->hostile(nullptr, &same);
                ok &= check(same.hostile == 0u,
                            "hot npc-targeting same team not hostile", report);
            }
        }

        // ── Hot projectile splash through the same generic doorway ────────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameProjectileSplashLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_PROJECTILE_SPLASH));
            const MimitaNet::GameProjectileSplashPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->damage && policy->knockScale,
                        "hot projectile-splash provider resolves", report);
            if (policy && policy->damage)
            {
                MimitaNet::GameSplashFalloffV1 r{};
                r.structSize = sizeof(MimitaNet::GameSplashFalloffV1);
                r.distance = 1.0f;
                r.fullDamageRadius = 3.0f;
                r.splashRadius = 6.0f;
                r.splashDamage = 100.0f;
                r.edgeDamage = 20.0f;
                r.splashExponent = 1.0f;
                policy->damage(nullptr, &r);
                ok &= check(r.outDamage == 100.0f,
                            "hot projectile-splash full-radius damage", report);
            }
        }

        // ── Hot respawn rule through the same generic doorway ─────────────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameRespawnLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_RESPAWN));
            const MimitaNet::GameRespawnPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->initialTimer && policy->tick,
                        "hot respawn provider resolves", report);
            if (policy && policy->tick)
            {
                MimitaNet::GameRespawnRuleV1 oneLife{};
                oneLife.structSize = sizeof(MimitaNet::GameRespawnRuleV1);
                oneLife.respawnsEnabled = 0u;
                policy->tick(nullptr, &oneLife);
                ok &= check(oneLife.stayDead == 1u,
                            "hot respawn one-life stays dead", report);

                MimitaNet::GameRespawnRuleV1 count{};
                count.structSize = sizeof(MimitaNet::GameRespawnRuleV1);
                count.respawnsEnabled = 1u;
                count.timer = 1.0f;
                count.dt = 2.0f;
                policy->tick(nullptr, &count);
                ok &= check(count.readyToRespawn == 1u,
                            "hot respawn countdown reaches ready", report);
            }
        }

        // ── Hot attack claim through the same generic doorway ─────────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameAttackClaimFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_ATTACK_CLAIM));
            ok &= check(evaluate != nullptr,
                        "hot attack-claim provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameAttackClaimV1 claim{};
                claim.structSize = sizeof(MimitaNet::GameAttackClaimV1);
                claim.claimedTargetId = 9u;
                claim.claimedDistance = 10.0f;
                claim.maxRange = 100.0f;
                claim.worldBlockDistance = 50.0f;
                claim.rewindHitTolerance = 0.5f;
                claim.claimLagAllowance = 0.25f;
                evaluate(nullptr, &claim);
                ok &= check(claim.eligible == 1u && claim.tolerance == 0.75f,
                            "hot attack-claim accepts a clear claim", report);
            }
        }

        // ── Hot NPC ground clamp through the same generic doorway ─────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameNpcGroundClampFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_NPC_GROUND_CLAMP));
            ok &= check(evaluate != nullptr,
                        "hot npc-ground-clamp provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameNpcGroundClampV1 clamp{};
                clamp.structSize = sizeof(MimitaNet::GameNpcGroundClampV1);
                clamp.haveFloor = 1u;
                clamp.floorZ = 10.0f;
                clamp.posZ = 5.0f;
                clamp.velZ = -3.0f;
                clamp.restHeight = 1.8f;
                evaluate(nullptr, &clamp);
                ok &= check(clamp.clamp == 1u && clamp.outPosZ == 11.8f &&
                                clamp.outVelZ == 0.0f,
                            "hot npc-ground-clamp pins below floor", report);
            }
        }

        // ── Hot broadcast interpolation through the same generic doorway ──
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameBroadcastInterpFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_BROADCAST_INTERP));
            ok &= check(evaluate != nullptr,
                        "hot broadcast-interp provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameBroadcastInterpV1 off{};
                off.structSize = sizeof(MimitaNet::GameBroadcastInterpV1);
                off.configSmoothing = 0u;
                evaluate(nullptr, &off);
                ok &= check(off.useSmoothing == 0u,
                            "hot broadcast-interp disabled when config off", report);

                MimitaNet::GameBroadcastInterpV1 on{};
                on.structSize = sizeof(MimitaNet::GameBroadcastInterpV1);
                on.configSmoothing = 1u;
                on.configMaxSpeed = 100.0f;
                on.dt = 1.0f / 60.0f;
                evaluate(nullptr, &on);
                ok &= check(on.useSmoothing == 1u &&
                                on.maxDelta > 0.0f && on.maxDelta < 2.0f,
                            "hot broadcast-interp speed cap from config", report);
            }
        }

        // ── Hot join policy through the same generic doorway ──────────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameJoinPolicyFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_JOIN_POLICY));
            ok &= check(evaluate != nullptr,
                        "hot join-policy provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameJoinPolicyV1 local{};
                local.structSize = sizeof(MimitaNet::GameJoinPolicyV1);
                local.coordinatorIsLocal = 1u;
                evaluate(nullptr, &local);
                ok &= check(local.accept == 1u,
                            "hot join-policy accepts a local join", report);

                MimitaNet::GameJoinPolicyV1 badPassword{};
                badPassword.structSize = sizeof(MimitaNet::GameJoinPolicyV1);
                badPassword.coordinatorIsLocal = 1u;
                badPassword.passwordProtected = 1u;
                badPassword.passwordMatches = 0u;
                evaluate(nullptr, &badPassword);
                ok &= check(
                    badPassword.accept == 0u &&
                        badPassword.rejectReason == (std::uint32_t)
                            MimitaNet::GAME_JOIN_REJECT_WRONG_PASSWORD,
                    "hot join-policy rejects wrong password", report);
            }
        }

        // ── Hot session policy through the same generic doorway ───────────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameSessionLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_SESSION_POLICY));
            const MimitaNet::GameSessionPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->reconnectGrace && policy->mapReady,
                        "hot session-policy provider resolves", report);
            if (policy && policy->mapReady)
            {
                MimitaNet::GameMapReadyV1 fresh{};
                fresh.structSize = sizeof(MimitaNet::GameMapReadyV1);
                fresh.mapMatches = 1u;
                policy->mapReady(nullptr, &fresh);
                ok &= check(fresh.spawn == 1u && fresh.rearm == 0u,
                            "hot session-policy spawns a fresh player", report);

                MimitaNet::GameMapReadyV1 rearm{};
                rearm.structSize = sizeof(MimitaNet::GameMapReadyV1);
                rearm.alreadySpawned = 1u;
                rearm.mapMatches = 1u;
                policy->mapReady(nullptr, &rearm);
                ok &= check(rearm.spawn == 0u && rearm.rearm == 1u,
                            "hot session-policy re-arms an existing player", report);
            }
        }

        // ── Hot reload decision through the same generic doorway ──────────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameReloadDecisionFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_RELOAD_DECISION));
            ok &= check(evaluate != nullptr,
                        "hot reload-decision provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameReloadDecisionV1 full{};
                full.structSize = sizeof(MimitaNet::GameReloadDecisionV1);
                full.currentAmmo = 10;
                full.magazineSize = 10;
                full.reserveAmmo = 30;
                evaluate(nullptr, &full);
                ok &= check(full.accept == 0u &&
                                full.reason == MimitaNet::GAME_RELOAD_REASON_MAG_FULL,
                            "hot reload-decision rejects full magazine", report);

                MimitaNet::GameReloadDecisionV1 begin{};
                begin.structSize = sizeof(MimitaNet::GameReloadDecisionV1);
                begin.currentAmmo = 3;
                begin.magazineSize = 10;
                begin.reserveAmmo = 30;
                evaluate(nullptr, &begin);
                ok &= check(begin.accept == 1u && begin.beginReload == 1u,
                            "hot reload-decision begins a reload", report);
            }
        }

        // ── Hot client snapshot-apply through the same generic doorway ────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameSnapshotApplyFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_CLIENT_SNAPSHOT));
            ok &= check(evaluate != nullptr,
                        "hot client-snapshot provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameSnapshotApplyV1 staleLocal{};
                staleLocal.structSize = sizeof(MimitaNet::GameSnapshotApplyV1);
                staleLocal.isLocal = 1u;
                staleLocal.incomingEpoch = 5u;
                staleLocal.localServerEpoch = 5u;
                staleLocal.serverTick = 100u;
                staleLocal.latestLocalSnapshotTick = 120u;
                evaluate(nullptr, &staleLocal);
                ok &= check(
                    staleLocal.acceptLifecycle == 0u &&
                        staleLocal.dropReason == (std::uint32_t)
                            MimitaNet::GAME_SNAPSHOT_DROP_STALE_LOCAL,
                    "hot client-snapshot drops a stale local sample", report);

                MimitaNet::GameSnapshotApplyV1 noCreate{};
                noCreate.structSize = sizeof(MimitaNet::GameSnapshotApplyV1);
                noCreate.membershipAllowed = 0u;
                noCreate.existsBefore = 0u;
                evaluate(nullptr, &noCreate);
                ok &= check(noCreate.mayCreate == 0u,
                            "hot client-snapshot blocks stale membership create", report);
            }
        }

        // ── Hot projectile correction through the same generic doorway ────
        {
            auto evaluate = reinterpret_cast<MimitaNet::GameProjectileCorrectionFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_PROJECTILE_CORRECTION));
            ok &= check(evaluate != nullptr,
                        "hot projectile-correction provider resolves", report);
            if (evaluate)
            {
                MimitaNet::GameProjectileCorrectionV1 bigError{};
                bigError.structSize = sizeof(MimitaNet::GameProjectileCorrectionV1);
                bigError.serverHasSentUpdate = 1u;
                bigError.positionError = 6.0f;
                bigError.errorThreshold = 4.0f;
                evaluate(nullptr, &bigError);
                ok &= check(bigError.correct == 1u,
                            "hot projectile-correction corrects a large error", report);

                MimitaNet::GameProjectileCorrectionV1 small{};
                small.structSize = sizeof(MimitaNet::GameProjectileCorrectionV1);
                small.serverHasSentUpdate = 1u;
                small.positionError = 2.0f;
                small.errorThreshold = 4.0f;
                evaluate(nullptr, &small);
                ok &= check(small.correct == 0u,
                            "hot projectile-correction ignores a small error", report);
            }
        }

        // ── Hot connection-health through the same generic doorway ────────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameConnectionLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_CONNECTION_POLICY));
            const MimitaNet::GameConnectionPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->nextState && policy->cadence,
                        "hot connection-policy provider resolves", report);
            if (policy && policy->nextState)
            {
                // Connected (6) + last packet 5000ms ago > hardTimeout 4000
                // -> Reconnecting (7).
                MimitaNet::GameConnectionHealthV1 health{};
                health.structSize = sizeof(MimitaNet::GameConnectionHealthV1);
                health.current = 6u;
                health.lastHeardAge = 5000u;
                health.staleThresholdMs = 1000u;
                health.hardTimeoutMs = 4000u;
                policy->nextState(nullptr, &health);
                ok &= check(health.next == 7u,
                            "hot connection-policy enters reconnect on hard timeout",
                            report);
            }
        }

        // ── Hot server tick orchestration through the generic doorway ─────
        {
            auto fn = reinterpret_cast<GameServerTickFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    GAME_CAP_SERVER_TICK));
            ok &= check(fn != nullptr, "hot server-tick provider resolves", report);
            if (fn)
            {
                GameServerTickV1 tickReq{};
                tickReq.structSize = sizeof(GameServerTickV1);
                tickReq.abiVersion = GAME_SERVER_TICK_VERSION;
                tickReq.tick = 42u;
                tickReq.snapshotDue = 1u;
                tickReq.gameplayDomainDue = 1u;
                fn(nullptr, &tickReq);
                ok &= check(tickReq.handled == 1u && tickReq.outSnapshotDue == 1u &&
                                tickReq.outRunGameplayDomain == 1u,
                            "hot server-tick preserves snapshot/gameplay decisions",
                            report);
                GameServerTickV1 exitReq{};
                exitReq.structSize = sizeof(GameServerTickV1);
                exitReq.autoExitDue = 1u;
                fn(nullptr, &exitReq);
                ok &= check(exitReq.outRequestShutdown == 1u,
                            "hot server-tick honors the auto-exit request", report);
            }
        }

        // ── Hot connection transition through the generic doorway ─────────
        {
            auto fn = reinterpret_cast<GameConnectionTransitionFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    GAME_CAP_CONNECTION_TRANSITION));
            ok &= check(fn != nullptr,
                        "hot connection-transition provider resolves", report);
            if (fn)
            {
                GameConnectionTransitionV1 t{};
                t.structSize = sizeof(GameConnectionTransitionV1);
                t.currentState = 1u;
                t.requestedState = 6u;
                t.reconnectTokenPresent = 1u;
                t.attempt = 0u;
                t.maxAttempts = 3u;
                fn(nullptr, &t);
                ok &= check(t.handled == 1u && t.outState == 6u && t.outRetry == 1u,
                            "hot connection-transition applies state and retry",
                            report);
            }
        }

        // ── Hot projectile cancellation through the generic doorway ───────
        {
            auto fn = reinterpret_cast<GameProjectileCancelFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    GAME_CAP_PROJECTILE_CANCEL));
            ok &= check(fn != nullptr,
                        "hot projectile-cancel provider resolves", report);
            if (fn)
            {
                GameProjectileCancelV1 dead{};
                dead.structSize = sizeof(GameProjectileCancelV1);
                dead.ownerDead = 1u;
                fn(nullptr, &dead);
                ok &= check(dead.outCancel == 1u,
                            "hot projectile-cancel cancels a dead-owner projectile",
                            report);
                GameProjectileCancelV1 alive{};
                alive.structSize = sizeof(GameProjectileCancelV1);
                alive.ownerAlive = 1u;
                fn(nullptr, &alive);
                ok &= check(alive.outCancel == 0u,
                            "hot projectile-cancel keeps a live-owner projectile",
                            report);
            }
        }

        // ── Damage-application v2 suicide outcome ─────────────────────────
        {
            auto fn = reinterpret_cast<MimitaNet::GameDamageApplicationFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_DAMAGE_APPLICATION));
            ok &= check(fn != nullptr,
                        "hot damage-application provider resolves", report);
            if (fn)
            {
                MimitaNet::GameDamageApplicationV1 suicide{};
                suicide.structSize = sizeof(MimitaNet::GameDamageApplicationV1);
                suicide.version = 2u;
                suicide.attackerIsTarget = 1u;
                suicide.damage = 999;
                suicide.targetHealth = 100;
                suicide.respawnsEnabled = 1u;
                suicide.respawnSeconds = 3.0f;
                fn(nullptr, &suicide);
                ok &= check(suicide.accept == 1u && suicide.killed == 1u &&
                                suicide.outSuicide == 1u &&
                                suicide.outScoreEligible == 0u,
                            "hot damage-application marks suicide with no score",
                            report);
            }
        }

        // ── Hot server startup policy through the same generic doorway ────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameServerPolicyLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_SERVER_POLICY));
            const MimitaNet::GameServerPolicyV1* policy =
                lookup ? lookup(nullptr) : nullptr;
            ok &= check(policy && policy->mode && policy->startupNpc,
                        "hot server-policy provider resolves", report);
            if (policy && policy->mode)
            {
                MimitaNet::GameServerModeV1 community{};
                community.structSize = sizeof(MimitaNet::GameServerModeV1);
                community.gameModeIsSandbox = 0u;
                policy->mode(nullptr, &community);
                ok &= check(community.useDuel == 0u && community.startMatch == 1u,
                            "hot server-policy community starts a match", report);

                MimitaNet::GameServerStartupNpcV1 plan{};
                plan.structSize = sizeof(MimitaNet::GameServerStartupNpcV1);
                plan.npcsEnabled = 1u;
                plan.requestedCount = 5u;
                plan.spawnPointCount = 3u;
                plan.maxSpawn = 256u;
                policy->startupNpc(nullptr, &plan);
                ok &= check(plan.count == 5u && plan.useSpawnPoints == 1u,
                            "hot server-policy clamps startup NPCs", report);
            }
        }

        // ── Hot geometry primitives through the same generic doorway ──────
        {
            auto lookup = reinterpret_cast<MimitaNet::GameGeometryLookupFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    MimitaNet::GAME_CAP_GEOMETRY));
            const MimitaNet::GameGeometryPrimitiveV1* prim =
                lookup ? lookup(nullptr, MimitaNet::GAME_GEOM_RAY_AABB) : nullptr;
            ok &= check(lookup && prim && prim->invoke,
                        "hot geometry provider resolves ray.aabb", report);

            // Ray from z=-10 toward +z hits a unit box at the origin at t=9.
            MimitaNet::GameGeometryQueryV1 q{};
            q.primitiveId = MimitaNet::GAME_GEOM_RAY_AABB;
            q.origin[2] = -10.0f;
            q.direction[2] = 1.0f;
            q.boxMin[0] = -1.0f; q.boxMin[1] = -1.0f; q.boxMin[2] = -1.0f;
            q.boxMax[0] = 1.0f; q.boxMax[1] = 1.0f; q.boxMax[2] = 1.0f;
            q.maxDistance = 100.0f;
            MimitaNet::runGeometryPrimitive(q);
            ok &= check(q.hit == 1u && q.outDistance > 8.9f && q.outDistance < 9.1f,
                        "hot geometry ray.aabb hits a box", report);

            // Point inside the same box is contained.
            MimitaNet::GameGeometryQueryV1 p{};
            p.primitiveId = MimitaNet::GAME_GEOM_POINT_IN_AABB;
            p.boxMin[0] = -1.0f; p.boxMin[1] = -1.0f; p.boxMin[2] = -1.0f;
            p.boxMax[0] = 1.0f; p.boxMax[1] = 1.0f; p.boxMax[2] = 1.0f;
            p.point[0] = 0.5f; p.point[1] = 0.0f; p.point[2] = 0.0f;
            MimitaNet::runGeometryPrimitive(p);
            ok &= check(p.hit == 1u, "hot geometry point.aabb contains", report);
        }

        // ── End-to-end hot round trip ───────────────────────────────────────
        // Build a hot-coded ping datagram, decode it through the hot codec, hand
        // the bytes to the hot `net.packet` handler, and observe the handler's
        // reply through the kernel reply capability. No EXE schema knowledge.
        {
            ReplyCapture capture;
            gReplyCapture = &capture;
            LiveBehavior::setPacketReplySink(&captureHotPacketReply);
            ok &= check(
                MimitaRuntime::GenericRuntime::instance().hasCapability(
                    MimitaNet::GAME_CAP_HOT_PACKET_REPLY),
                "net.packet-reply capability registered", report);

            // Real two-leg loopback: the "client" sends the ping datagram over a
            // real UDP socket; the "server" receives it, decodes and dispatches
            // through the hot codec/handler; the handler's reply is sent back over
            // the same socket and parsed as the client would parse it.
            WSADATA wsa{};
            WSAStartup(MAKEWORD(2, 2), &wsa);
            SOCKET serverSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            SOCKET clientSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            serverAddr.sin_port = 0;
            bind(serverSock, (const sockaddr*)&serverAddr, sizeof(serverAddr));
            int serverAddrLen = sizeof(serverAddr);
            getsockname(serverSock, (sockaddr*)&serverAddr, &serverAddrLen);
            const DWORD timeoutMs = 2000;
            setsockopt(serverSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs,
                       sizeof(timeoutMs));
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs,
                       sizeof(timeoutMs));
            capture.sendSocket = serverSock;
            capture.sendTo = serverAddr;

            // Build the client's ping. The inner layout is hot-owned, but a
            // raw 8-byte ping payload is enough to exercise the path.
            MimitaNet::PacketCodecEnvelopeV1 in{};
            in.connectionId = 21;
            in.serverTick = 4242;
            in.packetSequence = 9;
            std::uint32_t ping[2] = {7u, 100u};
            std::vector<std::uint8_t> datagram;
            MimitaNet::PacketCompatibilityV1 reason =
                MimitaNet::PacketCompatibilityV1::Malformed;
            const bool built = MimitaNet::buildHotCodecDatagram(
                gameHash("packet.hot.ping"), 1, ping, (std::uint32_t)sizeof(ping),
                in, datagram, &reason);
            ok &= check(built, "hot round trip: datagram builds", report);

            if (built)
            {
                sendto(clientSock, (const char*)datagram.data(), (int)datagram.size(),
                       0, (const sockaddr*)&serverAddr, sizeof(serverAddr));
                std::vector<std::uint8_t> received(2048);
                sockaddr_in from{};
                int fromLen = sizeof(from);
                int n = recvfrom(serverSock, (char*)received.data(),
                                 (int)received.size(), 0, (sockaddr*)&from, &fromLen);
                ok &= check(n > 0, "hot round trip: server received the datagram",
                            report);
                // Reply to the client's source address, not the server's.
                capture.sendTo = from;
                // Mirror production: strip the outer header, decode through the
                // hot codec, and dispatch [envelope][payload] to the hot handler.
                std::uint32_t decodedSize = 0;
                MimitaNet::PacketCodecEnvelopeV1 envelope{};
                std::uint8_t decodedPayload[16] = {};
                const bool decoded = n > 0 && MimitaNet::parseHotCodecDatagram(
                    received.data(), (std::uint32_t)n, decodedPayload,
                    (std::uint32_t)sizeof(decodedPayload), &decodedSize, &envelope,
                    &reason);
                ok &= check(decoded, "hot round trip: server decodes the packet",
                            report);
                if (decoded)
                {
                    std::vector<std::uint8_t> eventBytes(
                        sizeof(envelope) + decodedSize);
                    std::memcpy(eventBytes.data(), &envelope, sizeof(envelope));
                    std::memcpy(eventBytes.data() + sizeof(envelope), decodedPayload,
                                decodedSize);
                    const bool dispatched = LiveBehavior::dispatchGameplayEvent64(
                        MimitaNet::GAME_EVENT_HOT_PACKET, eventBytes.data(),
                        (std::uint32_t)eventBytes.size(), 1, 0, 0);
                    ok &= check(dispatched, "hot round trip: net.packet dispatched",
                                report);
                    ok &= check(capture.got && capture.connectionId == 21,
                                "hot net.packet handler replies on the origin connection",
                                report);
                    // Receive the reply on the client socket and parse it.
                    std::vector<std::uint8_t> replyBytes(2048);
                    sockaddr_in replyFrom{};
                    int replyFromLen = sizeof(replyFrom);
                    int rn = recvfrom(clientSock, (char*)replyBytes.data(),
                                      (int)replyBytes.size(), 0,
                                      (sockaddr*)&replyFrom, &replyFromLen);
                    std::uint32_t reply[2] = {0u, 0u};
                    std::uint32_t replySize = 0;
                    const bool decodedReply = rn > 0 &&
                        MimitaNet::parseHotCodecDatagram(
                            replyBytes.data(), (std::uint32_t)rn, reply,
                            (std::uint32_t)sizeof(reply), &replySize, nullptr, &reason);
                    ok &= check(decodedReply && reply[0] == 7u && reply[1] == 101u,
                                "hot round trip: client decodes the hot-handled reply",
                                report);
                }
            }
            closesocket(serverSock);
            closesocket(clientSock);
            WSACleanup();
            LiveBehavior::clearPacketReplySink();
            gReplyCapture = nullptr;
        }
    }

    HotReloadSystem::instance().unloadGameDLL();

    return ok;
}
