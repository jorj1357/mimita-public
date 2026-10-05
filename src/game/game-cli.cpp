// 08 16 2026, 01 35
/* purpose
* Owns lightweight command-line self-tests and diagnostic launch paths.
* Runs requested checks before normal graphics or gameplay startup.
* Keeps dedicated verification commands process-bounded and script-friendly.
* Does NOT own the main game loop, rendering lifecycle, or packet schemas.
* Does NOT implement gameplay systems or long-running server/client loops.
* Does NOT replace feature-owned test logic.
*/

#include "game/game-cli.h"
#include "combat/weapon-runtime.h"
#include "network/snapshot-chunks.h"
#include "network/destruction-replication-selftest.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <chrono>
#include <thread>
#include <windows.h>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "engine/engine.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "camera.h"
#include "terminal/terminal-state.h"
#include "gui/menus/online-menu.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-export-target.h"
#include "replay/replay-factory.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/actor-triangle-spike.h"
#include "physics/physical-entity.h"
#include "impact/impact-system.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/actor-triangle-solver.h"
#include "debug/crash-handler.h"
#include <exception>
#include <stdexcept>
#include <fstream>
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/ice/ice-test.h"
#include "network/coordinator-client.h"
#include "network/badconn/badconn.h"
#include "gamemode/match-roles.h"
#include "gamemode/gamemode.h"
#include "gamemode/map-config.h"
#include "gamemode/mode-pack-registry.h"
#include "gamemode/action-graph.h"
#include "gamemode/capability-registry.h"
#include "gamemode/disaster-runtime.h"
#include "map/map-loader-collision.h"
#include "network/server-gamemode.h"
#include "npc/npc-nav-request.h"
#include "npc/npc-navigation.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-surface.h"
#include "npc/npc-navigation-settings.h"
#include "npc/npc-targeting.h"
#include "entities/aim-fov.h"
#include "combat/grenade-registry.h"
#include "combat/area-effect.h"

extern DuelManager gDuelManager;
extern bool gMainmenuDebug;

void forceMainMenu()
{
    auto startTime = std::chrono::steady_clock::now();
    Debug::log(Debug::Category::General, "[MAINMENU] requested");

    printf("[MAINMENU] entering\n");

    auto logPhase = [&](const char* phase) {
        if (!gMainmenuDebug) return;
        auto now = std::chrono::steady_clock::now();
        double ms = (double)std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000.0;
        Debug::log(Debug::Category::General, "[MAINMENU] %s: %.2fms", phase, ms);
    };

    Debug::log(Debug::Category::General, "[MAINMENU] currentState=%s",
        gDuelManager.phase() != DuelPhase::Off ? "DUEL" :
        REPLAY_PLAYER.isPlaying() ? "REPLAY" :
        MP_CONTEXT.active ? "MULTIPLAYER" : "GAMEPLAY");

    Debug::log(Debug::Category::General, "[MAINMENU] transitioning");

    // 1. Stop replay playback
    if (REPLAY_PLAYER.isPlaying()) {
        printf("[MAINMENU] cleaning replay\n");
        REPLAY_PLAYER.stopPlayback();
        logPhase("Replay Cleanup");
    }

    // 1b. Clear replay actor/weapon models
    printf("[MAINMENU] clearing replay models\n");
    if (gpReplayActorModels) gpReplayActorModels->clear();
    if (gpReplayWeaponModels) gpReplayWeaponModels->clear();
    if (gpReplayChatStates) gpReplayChatStates->clear();
    logPhase("Replay Models Clear");

    // 2. Stop replay recording
    if (REPLAY_RECORDER.isRecording()) {
        REPLAY_RECORDER.stopRecording();
        logPhase("Replay Recording Stop");
    }

    // 3. Stop duel
    if (gDuelManager.phase() != DuelPhase::Off) {
        printf("[MAINMENU] cleaning duel\n");
        gDuelManager.stopDuel();
        logPhase("Duel Cleanup");
    }

    // 4. Destroy NPCs
    THE_NPC_SYSTEM.destroyAll();
    logPhase("NPC Cleanup");

    // 5. Disconnect multiplayer
    if (MP_CONTEXT.active) {
        printf("[MAINMENU] cleaning network\n");
        Debug::log(Debug::Category::General, "[MAINMENU] disconnecting multiplayer");
        MimitaNet::mpShutdown(MP_CONTEXT);
        onlineMenuSetServerCode("");
        onlineMenuSetServerRunning(false);
        logPhase("Network Cleanup");
        Debug::log(Debug::Category::General, "[MAINMENU] client disconnected");
    }

    // 6. Reset freecam
    FREECAM_ENABLED = false;

    // 7. Reset player state
    resetAllWeaponRuntimesForSpawn(THE_PLAYER, "forceMainMenu");
    THE_PLAYER.dead = false;
    THE_PLAYER.currentHp = THE_PLAYER.maxHp;
    THE_PLAYER.vel = glm::vec3(0.0f);
    THE_PLAYER.externalImpulse = glm::vec3(0.0f);
    THE_PLAYER.proceduralFrozen = false;
    THE_PLAYER.respawnTimer = 0.0f;
    THE_PLAYER.killedBy.clear();

    // 8. Cancel any ongoing replay export
    cancelReplayExport();

    // 9. Force cursor visible
    GLFWwindow* win = glfwGetCurrentContext();
    if (win)
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // 10. Transition to menu
    printf("[MAINMENU] switching scene\n");
    GAME_STATE = GAME_MENU;

    logPhase("GUI Load");
    Debug::log(Debug::Category::General, "[MAINMENU] success");
}

bool handleGameCLI(int argc, char** argv)
{
    if (argc <= 1) return false;

    const std::string command = argv[1];
    if (command == "--ice-host-only" ||
        command == "--ice-join-only" ||
        command == "--ice-game-host" ||
        command == "--ice-game-client" ||
        command == "--ice-server")
    {
        printf("[ICE LEGACY DISABLED] command=%s reason=Stage3C-unified-server-path\n", argv[1]);
        printf("[ICE LEGACY DISABLED] use --server --ice --timeout <secs> for hosting and --ice-connect <room-code> for a client probe\n");
        std::exit(2);
    }

    // ── ICE Connect (client connects via ICE to an ICE server) ────
    if (std::string(argv[1]) == "--ice-connect") {
        if (argc < 3) {
            printf("[ICE] usage: mimita.exe --ice-connect <room-code>\n");
            return true;
        }
        IceTestOptions opts;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--disable-relay") opts.disableRelay = true;
            else if (a == "--timeout-seconds" && i + 1 < argc)
                opts.timeoutSeconds = std::max(1, std::atoi(argv[++i]));
            else if (a == "--death-respawn-cycles" && i + 1 < argc)
                opts.deathRespawnCycles = std::max(0, std::atoi(argv[++i]));
            else if (a == "--client-index" && i + 1 < argc)
                opts.clientIndex = std::max(0, std::atoi(argv[++i]));
            else if (a == "--reconnect-token" && i + 1 < argc)
                opts.reconnectToken = argv[++i];
            else if (a == "--badconn-preset" && i + 1 < argc)
                opts.badconnPreset = argv[++i];
        }
        if (!opts.badconnPreset.empty()) {
            badconn::loadConfig(badconn::configPath());
            if (opts.badconnPreset == "0")
                badconn::disable();
            else if (!badconn::activatePreset(opts.badconnPreset)) {
                printf("[BADCONN CLI] preset %s not found\n", opts.badconnPreset.c_str());
                std::exit(2);
            }
            printf("[BADCONN CLI] preset=%s active=%d name=%s\n",
                   opts.badconnPreset.c_str(), (int)badconn::active(),
                   badconn::activePresetName().c_str());
        }
        runIceConnect(argv[2], opts);
        return true;
    }

    if (std::string(argv[1]) == "--snapshot-chunk-selftest") {
        std::string report;
        const bool ok = MimitaNet::runSnapshotChunkSelfTest(&report);
        printf("%s", report.c_str());
        printf("[SNAPSHOT CHUNK SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--actor-preset-selftest") {
        const std::string directory = argc > 2 ? argv[2] : "config/actor-presets";
        std::error_code ec;
        const auto cwd = std::filesystem::current_path(ec);
        printf("[ACTOR PRESET SELFTEST] cwd=%s\n",
               ec ? "unknown" : cwd.string().c_str());
        printf("[ACTOR PRESET SELFTEST] requested_directory=%s\n", directory.c_str());

        size_t jsonFiles = 0;
        if (std::filesystem::is_directory(directory, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
                if (ec) break;
                std::error_code entryEc;
                if (entry.is_regular_file(entryEc) && !entryEc &&
                    entry.path().extension() == ".json") {
                    ++jsonFiles;
                    printf("[ACTOR PRESET SELFTEST] file=%s\n",
                           entry.path().string().c_str());
                }
            }
        }
        printf("[ACTOR PRESET SELFTEST] json_files=%zu\n", jsonFiles);

        auto& registry = MatchRoleRegistry::instance();
        const bool loaded = registry.loadActorPresets(directory);
        const auto presets = registry.actorPresets();
        printf("[ACTOR PRESET SELFTEST] load_return=%d resolved_directory=%s count=%zu\n",
               (int)loaded, registry.actorPresetDirectory().c_str(), presets.size());
        for (const auto* preset : presets)
            printf("[ACTOR PRESET SELFTEST] id=%s fov=%.1f first_person=%d movement=%s weapon_set=%s\n",
                   preset->id.c_str(), preset->cameraFov, (int)preset->forceFirstPerson,
                   preset->movementPreset.c_str(), preset->weaponSet.c_str());

        const auto* counterStrike = registry.getActorPreset("counter_strike");
        const auto revolver = counterStrike && counterStrike->weaponOverrides.find("revolver") != counterStrike->weaponOverrides.end()
            ? &counterStrike->weaponOverrides.at("revolver") : nullptr;
        const auto shotgun = counterStrike && counterStrike->weaponOverrides.find("shotgun") != counterStrike->weaponOverrides.end()
            ? &counterStrike->weaponOverrides.at("shotgun") : nullptr;
        const auto rifle = counterStrike && counterStrike->weaponOverrides.find("hitscan_rifle") != counterStrike->weaponOverrides.end()
            ? &counterStrike->weaponOverrides.at("hitscan_rifle") : nullptr;
        // The generic movement policy parsed from "npc_behavior".
        const NpcMovementPolicy* mp = counterStrike ? &counterStrike->movementPolicy : nullptr;
        const bool policyOk = mp && mp->configured &&
            mp->travelStyle == "forward" && mp->combatStyle == "forward" &&
            !mp->allowCircle && !mp->allowRandomWalk && !mp->allowStrafe &&
            !mp->allowZigZag && !mp->allowHoldPosition &&
            mp->retreatStyle == "low_health" &&
            std::abs(mp->retreatHealthFraction - 0.35f) < 1e-4f &&
            mp->jumpStyle == "obstacle_or_navigation" &&
            mp->dashStyle == "attack_or_navigation" &&
            mp->worldKnowledge == "local_sensing" &&
            mp->blockedBehavior == "turn_then_repath" &&
            std::abs(mp->movementNoise) < 1e-4f;
        const bool ok = loaded && counterStrike != nullptr &&
            counterStrike->cameraFov == 70.0f &&
            counterStrike->forceFov && counterStrike->forceFirstPerson &&
            !counterStrike->movementPreset.empty() &&
            counterStrike->weaponSet == "counterstrike" &&
            revolver && revolver->hasDamage && revolver->damage == 100.0f &&
            revolver->hasFireDelay && revolver->fireDelay == 0.3f &&
            revolver->hasReloadTime && revolver->reloadTime == 1.5f &&
            revolver->hasMagazineSize && revolver->magazineSize == 6 &&
            revolver->hasReserveAmmo && revolver->reserveAmmo == 36 &&
            revolver->hasBeamThickness && revolver->beamThickness == 0.0f &&
            revolver->hasWorldThickness && revolver->worldThickness == 0.0f &&
            shotgun && shotgun->hasDamage && shotgun->hasFireDelay &&
            shotgun->hasBeamThickness && shotgun->beamThickness == 0.0f &&
            // New override fields present and correct.
            rifle && rifle->hasMagazineSize && rifle->magazineSize == 30 &&
            rifle->hasReserveAmmo && rifle->reserveAmmo == 180 &&
            rifle->hasDamage && rifle->damage == 30.0f &&
            rifle->hasHeadshotMultiplier && rifle->headshotMultiplier == 4.0f &&
            rifle->hasBeamThickness && rifle->beamThickness == 0.0f &&
            rifle->hasWorldThickness && rifle->worldThickness == 0.0f &&
            !counterStrike->presentation.damageNumbers &&
            !counterStrike->presentation.hitEffects &&
            !counterStrike->presentation.worldImpactEffects &&
            counterStrike->presentation.hasHitMarkers &&
            !counterStrike->presentation.hitMarkers &&
            counterStrike->presentation.hasHitSounds &&
            !counterStrike->presentation.hitSounds &&
            counterStrike->presentation.bloodEffects &&
            policyOk;
        printf("[ACTOR PRESET SELFTEST] policy configured=%d travel=%s combat=%s circle=%d strafe=%d zigzag=%d randomwalk=%d jump=%s dash=%s noise=%.2f retreat=%.2f\n",
               mp && mp->configured, mp ? mp->travelStyle.c_str() : "-",
               mp ? mp->combatStyle.c_str() : "-",
               mp ? (int)mp->allowCircle : -1, mp ? (int)mp->allowStrafe : -1,
               mp ? (int)mp->allowZigZag : -1, mp ? (int)mp->allowRandomWalk : -1,
               mp ? mp->jumpStyle.c_str() : "-", mp ? mp->dashStyle.c_str() : "-",
               mp ? mp->movementNoise : -1.0f, mp ? mp->retreatHealthFraction : -1.0f);
        printf("[ACTOR PRESET SELFTEST] counter_strike=%s revolver=%.0f/6/%d shotgun=%.0f/%d/%d rifle=%.0f/%d/%d hs=%.0f thick=%.1f/%.1f hit_markers=%d hit_sounds=%d\n",
               counterStrike ? "found" : "missing",
               revolver ? revolver->damage : -1.0f, revolver ? revolver->reserveAmmo : -1,
               shotgun ? shotgun->damage : -1.0f, shotgun ? shotgun->magazineSize : -1, shotgun ? shotgun->reserveAmmo : -1,
               rifle ? rifle->damage : -1.0f, rifle ? rifle->magazineSize : -1, rifle ? rifle->reserveAmmo : -1,
               rifle ? rifle->headshotMultiplier : -1.0f,
               rifle ? rifle->beamThickness : -1.0f, rifle ? rifle->worldThickness : -1.0f,
               counterStrike ? (int)counterStrike->presentation.hitMarkers : -1,
               counterStrike ? (int)counterStrike->presentation.hitSounds : -1);
        printf("[ACTOR PRESET SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-movement-policy-selftest") {
        // Builds a real NpcSystem on a minimal collision world and drives it
        // through the actual update path, proving the actor-preset policy
        // restricts state selection and recovers from a wall locally.
        std::string report;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            return cond;
        };
        bool ok = true;

        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
        const MatchRoleDefinition* preset =
            MatchRoleRegistry::instance().getActorPreset("counter_strike");
        ok &= check(preset && preset->movementPolicy.configured,
                    "counter_strike preset has a configured npc_behavior policy");
        if (!preset || !preset->movementPolicy.configured) {
            printf("%s", report.c_str());
            printf("[NPC POLICY SELFTEST] FAIL\n");
            std::exit(1);
        }

        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            w.collisionMesh.triangles.push_back(t);
        };

        World world;
        // Floor: two triangles over [-25,25]^2 at z=0 (normal +z).
        addTri(world, {25,25,0}, {-25,25,0}, {-25,-25,0});
        addTri(world, {25,25,0}, {-25,-25,0}, {25,-25,0});
        buildCollisionChunks(world, nullptr);

        NpcSystem npcs;
        const uint32_t npcId = 9001;
        npcs.spawnNpc(npcId, 5.0f, glm::vec3(0.0f, 0.0f, 2.0f));
        Npc* npc = nullptr;
        for (Npc& n : npcs.all())
            if (n.id == npcId) { npc = &n; break; }
        if (!npc) {
            printf("[NPC POLICY SELFTEST] FAIL: NPC was not spawned\n");
            std::exit(1);
        }
        npc->actorPresetId = "counter_strike";
        npc->body.maxHp = 100;
        npc->body.currentHp = 100;
        npc->wakeupTimer = 0.0f;
        npc->body.pos = glm::vec3(0.0f, 0.0f, 2.0f);

        Player target;
        target.pos = glm::vec3(60.0f, 0.0f, 2.0f);
        target.currentHp = 100;
        target.maxHp = 100;
        target.dead = false;

        bool sawCircle = false, sawRandom = false, sawStrafe = false, sawZigZag = false;
        bool sawAdvance = false;
        float startX = npc->body.pos.x;

        // Phase 1: open floor. Must advance forward, never pick a special state.
        for (int tick = 0; tick < 150; ++tick) {
            npcs.updateOneWithTarget(npcId, world, target, 1.0f / 60.0f);
            const NpcState st = npc->stateMachine.currentState;
            sawCircle |= st == NpcState::Circle;
            sawRandom |= st == NpcState::RandomWalk;
            sawStrafe |= st == NpcState::Strafe;
            sawZigZag |= st == NpcState::ZigZag;
            sawAdvance |= st == NpcState::Advance;
        }
        ok &= check(sawAdvance, "normal movement selects Advance");
        ok &= check(!sawCircle, "Circle is never selected");
        ok &= check(!sawRandom, "RandomWalk is never selected");
        ok &= check(!sawStrafe, "Strafe is never selected");
        ok &= check(!sawZigZag, "ZigZag is never selected");
        ok &= check(npc->body.pos.x > startX + 3.0f, "NPC moved forward continuously");

        // Phase 2: wall ahead. Place it relative to the NPC's current position
        // so it is always directly in front regardless of how far Phase 1
        // advanced; an absolute X becomes stale as movement tuning changes and
        // would leave the wall behind the actor. The NPC must make a local
        // correction (lateral movement) and never fall back to random wandering.
        const float wallX = npc->body.pos.x + 3.0f;
        addTri(world, {wallX,-3,0}, {wallX,3,0}, {wallX,3,4});
        addTri(world, {wallX,-3,0}, {wallX,3,4}, {wallX,-3,4});
        buildCollisionChunks(world, nullptr);
        sawRandom = false;
        sawCircle = false;
        float maxLateral = 0.0f;
        for (int tick = 0; tick < 420; ++tick) {
            npcs.updateOneWithTarget(npcId, world, target, 1.0f / 60.0f);
            const NpcState st = npc->stateMachine.currentState;
            sawRandom |= st == NpcState::RandomWalk;
            sawCircle |= st == NpcState::Circle;
            maxLateral = std::max(maxLateral, std::fabs(npc->body.pos.y));
        }
        ok &= check(!sawRandom, "wall recovery never selects RandomWalk");
        ok &= check(!sawCircle, "wall recovery never selects Circle");
        ok &= check(maxLateral > 0.5f, "NPC made a local lateral correction at the wall");

        // Percentage health: retreat is emergent and probability-based. Low
        // health must clearly dominate; full health must never retreat.
        const NpcMovementPolicy& policy = preset->movementPolicy;
        ok &= check(npcRetreatChance(10, 100, policy) >= 0.75f,
                    "20% health gives a much higher retreat chance");
        ok &= check(npcRetreatChance(100, 100, policy) == 0.0f,
                    "full health gives zero retreat chance");
        ok &= check(std::fabs(npcRetreatChance(20, 1000, policy) -
                              npcRetreatChance(20000, 1000000, policy)) < 1e-6f,
                    "retreat uses current/max percentage at any scale");

        npc->sensors.hasTarget = true;
        npc->sensors.targetPos = target.pos;
        npc->sensors.predictedTarget = target.pos;
        npc->sensors.targetDistance = glm::length(target.pos - npc->body.pos);
        npc->lastMoveInput = glm::vec2(0.0f);
        int fullRetreats = 0, fullAdvances = 0;
        npc->body.currentHp = 100;
        for (int i = 0; i < 300; ++i) {
            const NpcState s = pickNextState(*npc);
            if (s == NpcState::Retreat) ++fullRetreats;
            else if (s == NpcState::Advance) ++fullAdvances;
        }
        ok &= check(fullRetreats == 0 && fullAdvances > 0,
                    "full health always advances");
        int lowRetreats = 0, lowAdvances = 0;
        npc->body.currentHp = 10;
        for (int i = 0; i < 300; ++i) {
            const NpcState s = pickNextState(*npc);
            if (s == NpcState::Retreat) ++lowRetreats;
            else if (s == NpcState::Advance) ++lowAdvances;
        }
        ok &= check(lowRetreats > 0, "low health can retreat (emergent)");
        ok &= check(lowAdvances > 0, "low health still advances sometimes");

        printf("[NPC POLICY SELFTEST]\n%s", report.c_str());
        printf("[NPC POLICY SELFTEST] lowHealthRetreats=%d/%d fullHealthRetreats=%d lateral=%.2f %s\n",
               lowRetreats, lowRetreats + lowAdvances, fullRetreats, maxLateral,
               ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-search-behavior-selftest") {
        // Real NpcSystem on a minimal collision world, driving the actual
        // update path, to prove: walkable slopes are climbed instead of being
        // treated as walls; a blocked NPC explores sideways rather than
        // ramming the same wall; and it never stalls in one spot for long.
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };
        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            w.collisionMesh.triangles.push_back(t);
        };
        auto addFloor = [&](World& w) {
            addTri(w, {40,40,0}, {-40,40,0}, {-40,-40,0});
            addTri(w, {40,40,0}, {-40,-40,0}, {40,-40,0});
        };

        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");

        // ── Phase 1: walkable slope must be climbed ─────────────────
        {
            World world;
            addFloor(world);
            // Ramp from x=6 (z=0) to x=12 (z=2), then a flat top to x=22.
            addTri(world, {6,-4,0}, {12,-4,2}, {12,4,2});
            addTri(world, {6,-4,0}, {12,4,2}, {6,4,0});
            addTri(world, {12,-4,2}, {22,-4,2}, {22,4,2});
            addTri(world, {12,-4,2}, {22,4,2}, {12,4,2});
            buildCollisionChunks(world, nullptr);

            NpcSystem npcs;
            const uint32_t id = 9201;
            npcs.spawnNpc(id, 5.0f, glm::vec3(0.0f, 0.0f, 1.9f));
            Npc* npc = nullptr;
            for (Npc& n : npcs.all()) if (n.id == id) { npc = &n; break; }
            if (!npc) { printf("[NPC SEARCH SELFTEST] FAIL: no NPC\n"); std::exit(1); }
            npc->actorPresetId = "counter_strike";
            npc->wakeupTimer = 0.0f;
            npc->body.pos = glm::vec3(0.0f, 0.0f, 1.9f);
            npc->stateMachine.currentState = NpcState::Patrol;
            npc->navigator.commitmentActive = true;
            npc->navigator.committedDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            npc->navigator.commitmentTimeRemaining = 100.0f;

            Player target;
            target.pos = glm::vec3(-300.0f, 0.0f, 2.0f);
            target.currentHp = 100; target.maxHp = 100; target.dead = false;

            for (int tick = 0; tick < 420; ++tick)
                npcs.updateOneWithTarget(id, world, target, 1.0f / 60.0f);

            check(npc->body.pos.x > 7.0f, "NPC advances onto the ramp");
            check(npc->body.pos.z > 2.4f, "NPC climbs the walkable slope (z rises)");
        }

        // ── Phase 2: steep wall -> explore sideways, never stall ────
        {
            World world;
            addFloor(world);
            // Vertical wall at x=4 (steep), spanning y=[-12,12], z=[0,5].
            addTri(world, {4,-12,0}, {4,12,0}, {4,12,5});
            addTri(world, {4,-12,0}, {4,12,5}, {4,-12,5});
            buildCollisionChunks(world, nullptr);

            NpcSystem npcs;
            const uint32_t id = 9202;
            npcs.spawnNpc(id, 5.0f, glm::vec3(0.0f, 0.0f, 1.9f));
            Npc* npc = nullptr;
            for (Npc& n : npcs.all()) if (n.id == id) { npc = &n; break; }
            if (!npc) { printf("[NPC SEARCH SELFTEST] FAIL: no NPC\n"); std::exit(1); }
            npc->actorPresetId = "counter_strike";
            npc->wakeupTimer = 0.0f;
            npc->body.pos = glm::vec3(0.0f, 0.0f, 1.9f);
            npc->stateMachine.currentState = NpcState::Patrol;
            npc->navigator.commitmentActive = true;
            npc->navigator.committedDirection = glm::vec3(1.0f, 0.0f, 0.0f);
            npc->navigator.commitmentTimeRemaining = 100.0f;

            Player target;
            target.pos = glm::vec3(-300.0f, 0.0f, 2.0f);
            target.currentHp = 100; target.maxHp = 100; target.dead = false;

            bool forbid = false;
            float maxAbsY = 0.0f, minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            glm::vec3 ref = npc->body.pos;
            int stall = 0, maxStall = 0;
            for (int tick = 0; tick < 720; ++tick) {
                npcs.updateOneWithTarget(id, world, target, 1.0f / 60.0f);
                const NpcState st = npc->stateMachine.currentState;
                forbid |= st == NpcState::RandomWalk || st == NpcState::Circle ||
                          st == NpcState::Strafe || st == NpcState::ZigZag;
                const glm::vec3 p = npc->body.pos;
                maxAbsY = std::max(maxAbsY, std::fabs(p.y));
                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
                if (glm::length(glm::vec2(p.x - ref.x, p.y - ref.y)) > 0.75f) {
                    ref = p; stall = 0;
                } else {
                    ++stall;
                    maxStall = std::max(maxStall, stall);
                }
            }
            check(!forbid, "blocked search never uses random/circle/strafe/zigzag");
            check(maxAbsY > 1.0f, "NPC turns and explores sideways along the wall");
            check((maxX - minX) > 2.0f || (maxY - minY) > 2.0f,
                  "NPC keeps covering ground instead of pacing one spot");
            check(maxStall < 360, "NPC never stalls in one spot longer than ~6s");
        }

        printf("[NPC SEARCH SELFTEST]\n%s", report.c_str());
        printf("[NPC SEARCH SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-navigation-selftest") {
        // Drives the real NpcNavigator on minimal collision worlds to prove the
        // automatic surface planner: classifies surfaces, routes around a wall,
        // reaches a higher platform only through a legal jump/ramp, rejects an
        // unreachable wall, handles stacked floors, and never steers directly
        // into a confirmed blocking wall.
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };
        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            const glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            t.normal = len > 1e-6f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            w.collisionMesh.triangles.push_back(t);
        };
        auto addFloor = [&](World& w, float z) {
            addTri(w, {40,40,z}, {-40,40,z}, {-40,-40,z});
            addTri(w, {40,40,z}, {-40,-40,z}, {40,-40,z});
        };

        {
            std::string r;
            ok &= check(npcSurfaceSelfTest(r), "surface classification (floor/ramp/steep/wall/ceiling)");
            r.clear();
            ok &= check(npcNavigationSettingsSelfTest(r), "navigation settings parse/default/clamp");
        }

        MovementConfig cfg;
        cfg.gravityZ = -58.0f;
        cfg.jumpVerticalSpeed = 19.0f;   // ~3.1 m jump cap
        cfg.dashEnabled = true;

        NpcNavigationSettings ns;
        ns.configured = true;
        ns.searchRadius = 20.0f;
        ns.maxWalkableSlopeDot = 0.0f;    // shared collision rule
        ns.allowNavigationJumps = true;
        ns.allowWallJump = false;

        NpcMovementPolicy jumpPolicy;
        jumpPolicy.configured = true;
        jumpPolicy.jumpStyle = "obstacle_or_navigation";
        NpcMovementPolicy neverJump;
        neverJump.configured = true;
        neverJump.jumpStyle = "never";

        auto spawnNavNpc = [&](NpcSystem& npcs, uint32_t id, glm::vec3 pos) -> Npc* {
            npcs.spawnNpc(id, 5.0f, pos);
            for (Npc& n : npcs.all()) {
                if (n.id != id) continue;
                n.wakeupTimer = 0.0f;
                n.body.pos = pos;
                n.body.ground.onGround = true;
                return &n;
            }
            return nullptr;
        };
        auto runNav = [&](Npc& npc, const World& world, glm::vec3 dest,
                          const NpcNavigationSettings& s,
                          const NpcMovementPolicy* pol) {
            NpcGoal goal;
            goal.kind = NpcGoalKind::ReachPosition;
            goal.targetPos = dest;
            goal.tolerance = 1.0f;
            return npc.navigator.update(npc, goal, world, &cfg, 1.0f / 60.0f, &s, pol);
        };
        auto pathUsesCapability = [](const Npc& npc, NavCapability cap) {
            for (uint8_t c : npc.navigator.pathCapability)
                if (c == (uint8_t)cap) return true;
            return false;
        };

        // ── Wall detour ─────────────────────────────────────────────
        {
            World world;
            addFloor(world, 0.0f);
            addTri(world, {4,-3,0}, {4,3,0}, {4,3,4});
            addTri(world, {4,-3,0}, {4,3,4}, {4,-3,4});
            buildCollisionChunks(world, nullptr);
            NpcSystem npcs;
            Npc* npc = spawnNavNpc(npcs, 9301, glm::vec3(0, 0, 1.0f));
            if (!npc) check(false, "spawn nav npc (wall)");
            else {
                const NpcNavResult r = runNav(*npc, world, glm::vec3(8, 0, 1.0f), ns, &jumpPolicy);
                check(r.hasPath, "routes around a wall");
                float maxY = 0.0f;
                for (const glm::vec3& p : npc->navigator.path)
                    maxY = std::max(maxY, std::fabs(p.y));
                check(maxY > 2.0f, "wall route detours sideways instead of through the wall");
            }
        }

        // ── Sealed box: no route -> blocked, never push the wall ─────
        {
            World world;
            addFloor(world, 0.0f);
            addTri(world, {1.5f,-30,0}, {1.5f,30,0}, {1.5f,30,8});
            addTri(world, {1.5f,-30,0}, {1.5f,30,8}, {1.5f,-30,8});
            addTri(world, {-1.5f,-30,0}, {-1.5f,30,0}, {-1.5f,30,8});
            addTri(world, {-1.5f,-30,0}, {-1.5f,30,8}, {-1.5f,-30,8});
            addTri(world, {-30,1.5f,0}, {30,1.5f,0}, {30,1.5f,8});
            addTri(world, {-30,1.5f,0}, {30,1.5f,8}, {-30,1.5f,8});
            addTri(world, {-30,-1.5f,0}, {30,-1.5f,0}, {30,-1.5f,8});
            addTri(world, {-30,-1.5f,0}, {30,-1.5f,8}, {-30,-1.5f,8});
            buildCollisionChunks(world, nullptr);
            NpcSystem npcs;
            Npc* npc = spawnNavNpc(npcs, 9302, glm::vec3(0, 0, 1.0f));
            if (!npc) check(false, "spawn nav npc (box)");
            else {
                const NpcNavResult r = runNav(*npc, world, glm::vec3(10, 0, 1.0f), ns, &jumpPolicy);
                check(!r.hasPath, "no route across a sealed wall");
                check(r.blocked, "blocked route is reported");
                const float len = glm::length(glm::vec2(r.dir.x, r.dir.y));
                const float toward = len > 0.001f ? glm::dot(r.dir / len, glm::vec3(1, 0, 0)) : 0.0f;
                check(toward < 0.3f, "does not steer directly into the blocking wall");
            }
        }

        // ── Legal jump onto a higher platform ───────────────────────
        {
            World world;
            addFloor(world, 0.0f);
            addTri(world, {6,-40,1.5f}, {40,-40,1.5f}, {40,40,1.5f});
            addTri(world, {6,-40,1.5f}, {40,40,1.5f}, {6,40,1.5f});
            addTri(world, {6,-40,0}, {6,40,0}, {6,40,1.5f});
            addTri(world, {6,-40,0}, {6,40,1.5f}, {6,-40,1.5f});
            buildCollisionChunks(world, nullptr);
            NpcSystem npcs;
            Npc* npc = spawnNavNpc(npcs, 9303, glm::vec3(0, 0, 1.0f));
            if (!npc) check(false, "spawn nav npc (platform)");
            else {
                const NpcNavResult r = runNav(*npc, world, glm::vec3(10, 0, 1.5f), ns, &jumpPolicy);
                check(r.hasPath, "reaches a higher platform");
                check(pathUsesCapability(*npc, NavCapability::Jump),
                      "the route uses a jump connection");
            }
            NpcSystem npcs2;
            Npc* npc2 = spawnNavNpc(npcs2, 9304, glm::vec3(0, 0, 1.0f));
            if (!npc2) check(false, "spawn nav npc (never-jump)");
            else {
                const NpcNavResult r2 = runNav(*npc2, world, glm::vec3(10, 0, 1.5f), ns, &neverJump);
                check(!r2.hasPath, "jump_style never rejects the jump route");
            }
        }

        // ── Wall too high: no jump route ────────────────────────────
        {
            World world;
            addFloor(world, 0.0f);
            addTri(world, {6,-40,5}, {40,-40,5}, {40,40,5});
            addTri(world, {6,-40,5}, {40,40,5}, {6,40,5});
            addTri(world, {6,-40,0}, {6,40,0}, {6,40,5});
            addTri(world, {6,-40,0}, {6,40,5}, {6,-40,5});
            buildCollisionChunks(world, nullptr);
            NpcSystem npcs;
            Npc* npc = spawnNavNpc(npcs, 9305, glm::vec3(0, 0, 1.0f));
            if (!npc) check(false, "spawn nav npc (high wall)");
            else {
                const NpcNavResult r = runNav(*npc, world, glm::vec3(10, 0, 5.0f), ns, &jumpPolicy);
                check(!r.hasPath, "a wall above the jump cap is not a jump route");
            }
        }

        // ── Stacked floors reached by a ramp ────────────────────────
        {
            World world;
            addFloor(world, 0.0f);
            // Ramp x=[-2,10] rising z=0 -> z=4, then upper floor at z=4.
            addTri(world, {-2,-40,0}, {10,-40,4}, {10,40,4});
            addTri(world, {-2,-40,0}, {10,40,4}, {-2,40,0});
            addTri(world, {10,-40,4}, {40,-40,4}, {40,40,4});
            addTri(world, {10,-40,4}, {40,40,4}, {10,40,4});
            buildCollisionChunks(world, nullptr);
            NpcSystem npcs;
            Npc* npc = spawnNavNpc(npcs, 9306, glm::vec3(-8, 0, 1.0f));
            if (!npc) check(false, "spawn nav npc (ramp)");
            else {
                const NpcNavResult r = runNav(*npc, world, glm::vec3(20, 0, 4.0f), ns, &jumpPolicy);
                check(r.hasPath, "stacked floor reached via a ramp");
                const float topZ = npc->navigator.path.empty()
                    ? 0.0f : npc->navigator.path.back().z;
                check(topZ > 3.5f, "route ends on the upper floor surface");
            }
        }

        printf("[NPC NAVIGATION SELFTEST]\n%s", report.c_str());
        printf("[NPC NAVIGATION SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-targeting-selftest") {
        std::string report;
        const bool ok = npcTargetingSelfTest(report);
        printf("[NPC TARGETING SELFTEST]\n%s", report.c_str());
        printf("[NPC TARGETING SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--aim-fov-selftest") {
        std::string report;
        const bool ok = aimFovSelfTest(report);
        printf("[AIM FOV SELFTEST]\n%s", report.c_str());
        printf("[AIM FOV SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-movement-executor-selftest") {
        // Proves Sandbox and Counter-Strike reach the same shared movement
        // executor, that a mode objective changes only the goal, and that the
        // executor is resolved from the actor preset.
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };
        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
        const MatchRoleDefinition* preset =
            MatchRoleRegistry::instance().getActorPreset("counter_strike");
        ok &= check(preset != nullptr, "counter_strike preset loads");
        if (preset)
            ok &= check(preset->movementPolicy.movementExecutor == "sandbox_shared",
                        "counter_strike preset uses movement_executor=sandbox_shared");

        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            const glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            t.normal = len > 1e-6f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            w.collisionMesh.triangles.push_back(t);
        };
        World world;
        addTri(world, {60,60,0}, {-60,60,0}, {-60,-60,0});
        addTri(world, {60,60,0}, {-60,-60,0}, {60,-60,0});
        buildCollisionChunks(world, nullptr);

        NpcSystem npcs;
        const uint32_t csId = 9401, sbId = 9402;
        npcs.spawnNpc(csId, 5.0f, glm::vec3(0.0f, 0.0f, 1.9f));
        npcs.spawnNpc(sbId, 5.0f, glm::vec3(6.0f, 0.0f, 1.9f));
        Npc* cs = nullptr;
        Npc* sb = nullptr;
        for (Npc& n : npcs.all()) {
            if (n.id == csId) cs = &n;
            else if (n.id == sbId) sb = &n;
        }
        ok &= check(cs && sb, "both NPCs spawned");
        if (cs && sb) {
            cs->actorPresetId = "counter_strike";
            cs->wakeupTimer = 0.0f;
            sb->wakeupTimer = 0.0f;
            cs->body.pos = glm::vec3(0.0f, 0.0f, 1.9f);
            sb->body.pos = glm::vec3(6.0f, 0.0f, 1.9f);

            Player target;
            target.pos = glm::vec3(40.0f, 0.0f, 2.0f);
            target.currentHp = 100; target.maxHp = 100; target.dead = false;

            const float csStart = cs->body.pos.x;
            const float sbStart = sb->body.pos.x;
            for (int t = 0; t < 150; ++t) {
                npcs.updateOneWithTarget(csId, world, target, 1.0f / 60.0f);
                npcs.updateOneWithTarget(sbId, world, target, 1.0f / 60.0f);
            }
            ok &= check(cs->lastMovementExecutor == NpcMovementExecutor::SandboxShared,
                        "CS NPC runs the shared Sandbox executor");
            ok &= check(sb->lastMovementExecutor == NpcMovementExecutor::SandboxShared,
                        "Sandbox NPC runs the shared Sandbox executor");
            ok &= check(cs->lastMovementExecutor == sb->lastMovementExecutor,
                        "both modes reach the same movement executor");
            ok &= check(cs->body.pos.x > csStart + 1.0f, "CS NPC moved");
            ok &= check(sb->body.pos.x > sbStart + 1.0f, "Sandbox NPC moved");

            // Objective context changes only the goal, never the executor.
            NpcMovementContext ctx;
            ctx.target = &target;
            ctx.hasTarget = true;
            ctx.hasObjective = true;
            ctx.objective.objectiveKnown = true;
            ctx.objective.objectivePos = glm::vec3(-10.0f, 0.0f, 2.0f);
            npcs.updateOneNpc(*cs, world, ctx, 1.0f / 60.0f);
            ok &= check(cs->utilityContext.objectiveKnown &&
                        glm::length(cs->utilityContext.objectivePos - ctx.objective.objectivePos) < 0.01f,
                        "objective context reaches the shared utility context");
            ok &= check(cs->lastMovementExecutor == NpcMovementExecutor::SandboxShared,
                        "objective context does not change the movement executor");
        }

        printf("[NPC MOVEMENT EXECUTOR SELFTEST]\n%s", report.c_str());
        printf("[NPC MOVEMENT EXECUTOR SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-wall-escape-event-selftest") {
        // Proves the `npc.wall-escape` diagnostic actually reaches the canonical
        // logs/<date>/<run>/events.jsonl sink through the real, level-gated
        // StructuredLogger. The actor is a real counter_strike-preset NPC driven
        // through the live NpcSystem::updateOneNpc path inside a closed wall
        // pocket, so it must backtrack / recover and emit the event. Reading the
        // file back is the whole point: without the npc_movement category level
        // gate in config/debuglogger.json the record is silently dropped.
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };

        StructuredLogger::instance().init();
        const std::string eventsPath = StructuredLogger::instance().eventsPath();
        check(!eventsPath.empty(), "canonical events path is available");
        check(StructuredLogger::instance().shouldLog(
                  StructuredCategory::NpcMovement, StructuredLevel::Important),
              "npc_movement category allows Important (config/debuglogger.json)");

        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
        NpcDifficultyConfig::instance().load("config/npc-difficulty.json");

        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            const glm::vec3 n = glm::cross(b - a, c - a);
            const float len = glm::length(n);
            t.normal = len > 1e-6f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
            w.collisionMesh.triangles.push_back(t);
        };

        World world;
        addTri(world, {25,25,0}, {-25,25,0}, {-25,-25,0});
        addTri(world, {25,25,0}, {-25,-25,0}, {25,-25,0});
        // Closed pocket: four vertical walls around the actor so every local
        // escape direction is blocked and recovery must fire. The pocket is
        // wider than the navigator's short-probe clearance so the actor first
        // commits to a blocked direction instead of holding with no input.
        const float h = 3.0f, top = 20.0f;
        addTri(world, { h,-h,0}, { h, h,0}, { h, h,top});
        addTri(world, { h,-h,0}, { h, h,top}, { h,-h,top});
        addTri(world, {-h, h,0}, {-h,-h,0}, {-h,-h,top});
        addTri(world, {-h, h,0}, {-h,-h,top}, {-h, h,top});
        addTri(world, {-h, h,0}, { h, h,0}, { h, h,top});
        addTri(world, {-h, h,0}, { h, h,top}, {-h, h,top});
        addTri(world, { h,-h,0}, {-h,-h,0}, {-h,-h,top});
        addTri(world, { h,-h,0}, {-h,-h,top}, { h,-h,top});
        buildCollisionChunks(world, nullptr);

        NpcSystem npcs;
        const uint32_t npcId = 9501;
        npcs.spawnNpc(npcId, 5.0f, glm::vec3(0.0f, 0.0f, 2.0f));
        Npc* npc = nullptr;
        for (Npc& n : npcs.all())
            if (n.id == npcId) { npc = &n; break; }
        ok &= check(npc != nullptr, "counter_strike test actor spawned");
        if (npc) {
            npc->actorPresetId = "counter_strike";
            npc->body.maxHp = 100;
            npc->body.currentHp = 100;
            npc->body.matchTeam = 0;  // Counter-Terrorists
            npc->wakeupTimer = 0.0f;
            npc->body.pos = glm::vec3(0.0f, 0.0f, 2.0f);

            Player target;
            target.pos = glm::vec3(10.0f, 0.0f, 2.0f);
            target.currentHp = 100;
            target.maxHp = 100;
            target.dead = false;

            // Deterministic mode goal: push through the +x wall. This is the
            // same NpcMovementContext boundary Counter-Strike uses; the mode
            // owns the goal, the shared executor owns the escape.
            NpcMovementContext ctx;
            ctx.target = &target;
            ctx.hasTarget = true;
            ctx.hasGoalOverride = true;
            ctx.goalOverride.kind = NpcGoalKind::ReachPosition;
            ctx.goalOverride.targetPos = glm::vec3(10.0f, 0.0f, 2.0f);
            ctx.goalOverride.tolerance = 0.5f;

            if (npc) {
                for (int tick = 0; tick < 180; ++tick)
                    npcs.updateOneNpc(*npc, world, ctx, 1.0f / 60.0f);
            }
        }

        StructuredLogger::instance().shutdown();

        int escapeEvents = 0;
        int counterStrikeEvents = 0;
        std::ifstream in(eventsPath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("\"event\":\"npc.wall-escape\"") != std::string::npos) {
                ++escapeEvents;
                if (line.find("\"preset\":\"counter_strike\"") != std::string::npos)
                    ++counterStrikeEvents;
            }
        }
        report += "  info  events path: " + eventsPath + "\n";
        report += "  info  npc.wall-escape records=" + std::to_string(escapeEvents) +
                  " counter_strike=" + std::to_string(counterStrikeEvents) + "\n";
        ok &= check(escapeEvents >= 1,
                    "at least one npc.wall-escape record landed in events.jsonl");
        ok &= check(counterStrikeEvents >= 1,
                    "the recorded escape came from the counter_strike actor");
        ok &= check(escapeEvents < 180,
                    "backtrack diagnostic is rate-limited (not once per tick)");

        printf("[NPC WALL ESCAPE EVENT SELFTEST]\n%s", report.c_str());
        printf("[NPC WALL ESCAPE EVENT SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-behavior-profile-selftest") {
        std::string report;
        const bool ok = behaviorProfileSelfTest(report);
        printf("[NPC BEHAVIOR PROFILE SELFTEST]\n%s", report.c_str());
        printf("[NPC BEHAVIOR PROFILE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-movement-commitment-selftest") {
        // Drives the real NpcNavigator commitment algorithm on minimal collision
        // worlds: open-floor persistence, no reverse, visible-enemy bypass,
        // replacement when blocked, replacement after failed progress, and
        // hidden-target forward pursuit.
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };
        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            w.collisionMesh.triangles.push_back(t);
        };
        auto addFloor = [&](World& w) {
            addTri(w, {60,60,0}, {-60,60,0}, {-60,-60,0});
            addTri(w, {60,60,0}, {-60,-60,0}, {60,-60,0});
        };

        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
        BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
        NpcDifficultyConfig::instance().load("config/npc-difficulty.json");

        World world;
        addFloor(world);
        buildCollisionChunks(world, nullptr);
        std::vector<int> tris;
        for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
            tris.push_back(i);

        NpcSystem npcs;
        const uint32_t npcId = 9601;
        npcs.spawnNpc(npcId, 5.0f, glm::vec3(0.0f, 0.0f, 1.9f));
        Npc* npc = nullptr;
        for (Npc& n : npcs.all()) if (n.id == npcId) { npc = &n; break; }
        ok &= check(npc != nullptr, "commitment test actor spawned");
        if (npc) {
            npc->actorPresetId = "counter_strike";
            npc->behaviorProfileId = "balanced";
            npc->behavior = resolveNpcBehavior("balanced");
            npc->body.maxHp = 100;
            npc->body.currentHp = 100;
            npc->body.pos = glm::vec3(0.0f, 0.0f, 1.9f);

            const NpcBehaviorTuning& b = npc->behavior;
            MovementCommitmentSettings s;
            s.repathIntervalSeconds = b.repathIntervalSeconds;
            s.goalMoveThresholdMeters = b.goalMoveThresholdMeters;
            s.enabled = b.commitmentEnabled;
            s.directionCommitSeconds = b.commitmentDirectionSeconds;
            s.progressCheckSeconds = b.commitmentProgressCheckSeconds;
            s.minimumProgressMeters = b.commitmentMinimumProgressMeters;
            s.candidateDistanceMeters = b.commitmentCandidateDistanceMeters;
            s.allowReverse = b.commitmentAllowReverse;
            s.avoidRecentPath = b.commitmentAvoidRecentPath;
            s.recentPathAvoidRadius = b.commitmentRecentPathAvoidRadius;
            s.visibleEnemyAllowsCombatMovement = b.commitmentVisibleEnemyAllowsCombatMovement;
            s.forwardBias = b.commitmentForwardBias;
            s.targetProgressBias = b.commitmentTargetProgressBias;
            s.openDistanceBias = b.commitmentOpenDistanceBias;
            s.reversePenalty = b.commitmentReversePenalty;
            const float dt = 1.0f / 60.0f;

            // Open floor: forward wins and reverse is not chosen.
            const glm::vec3 open = npc->navigator.chooseBestOpenDirection(
                *npc, glm::vec3(1,0,0), glm::vec3(0,0,0), world, tris, s, 0.0f, 12.0f);
            ok &= check(glm::length(open) > 0.001f, "open floor yields a direction");
            ok &= check(glm::dot(glm::normalize(open), glm::vec3(1,0,0)) > 0.5f,
                        "open floor keeps forward (reverse disabled)");

            // Hidden-target goal direction pulls the commitment toward it.
            const glm::vec3 towardTarget = npc->navigator.chooseBestOpenDirection(
                *npc, glm::vec3(0,1,0), glm::vec3(1,0,0), world, tris, s, 0.0f, 12.0f);
            ok &= check(glm::dot(glm::normalize(towardTarget), glm::vec3(1,0,0)) > 0.2f,
                        "target-progress bias steers the committed direction toward the goal");

            // Create a commitment, then prove a visible enemy preserves it.
            npc->navigator.reset();
            NpcCommitmentUpdate c1 = npc->navigator.updateCommitment(
                *npc, glm::vec3(1,0,0), glm::vec3(1,0,0), false, world, tris, s, dt, 0.0f, 12.0f);
            ok &= check(c1.created && npc->navigator.commitmentActive,
                        "first commitment is created");
            const glm::vec3 persisted = npc->navigator.committedDirection;
            NpcCommitmentUpdate visible = npc->navigator.updateCommitment(
                *npc, glm::vec3(1,0,0), glm::vec3(1,0,0), true, world, tris, s, dt, 0.0f, 12.0f);
            ok &= check(!visible.created && !visible.replaced && !visible.blocked,
                        "a visible enemy does not replace the commitment");
            ok &= check(glm::length(npc->navigator.committedDirection - persisted) < 1e-4f,
                        "visible enemy preserves the committed direction");

            // Blocked: add a wall in front; the commitment must be replaced.
            addTri(world, {2.2f,-8,0}, {2.2f,8,0}, {2.2f,8,5});
            addTri(world, {2.2f,-8,0}, {2.2f,8,5}, {2.2f,-8,5});
            buildCollisionChunks(world, nullptr);
            tris.clear();
            for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
                tris.push_back(i);
            NpcCommitmentUpdate blocked = npc->navigator.updateCommitment(
                *npc, glm::vec3(1,0,0), glm::vec3(1,0,0), false, world, tris, s, dt, 0.0f, 12.0f);
            ok &= check(blocked.blocked || blocked.replaced,
                        "a blocked committed direction is replaced");
            ok &= check(!NpcNavigation::obstacleInDirection(
                            *npc, npc->navigator.committedDirection, 2.0f, world, tris),
                        "the replacement direction is not immediately blocked");

            // Failed progress: hold the actor still and let the timer expire.
            npc->navigator.reset();
            npc->navigator.updateCommitment(
                *npc, glm::vec3(1,0,0), glm::vec3(1,0,0), false, world, tris, s, dt, 0.0f, 12.0f);
            bool sawProgressFailed = false;
            for (int i = 0; i < 90; ++i) {
                NpcCommitmentUpdate cu = npc->navigator.updateCommitment(
                    *npc, glm::vec3(1,0,0), glm::vec3(1,0,0), false, world, tris, s, dt, 0.0f, 12.0f);
                sawProgressFailed |= cu.progressFailed;
            }
            ok &= check(sawProgressFailed, "failed progress is detected and reported");
        }

        printf("[NPC MOVEMENT COMMITMENT SELFTEST]\n%s", report.c_str());
        printf("[NPC MOVEMENT COMMITMENT SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-movement-commitment-event-selftest") {
        // Proves the new commitment / plan / goal diagnostics reach the
        // canonical events.jsonl through the real logger and are rate-limited
        // (never once per tick).
        std::string report;
        bool ok = true;
        auto check = [&](bool cond, const char* what) {
            report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
            ok = ok && cond;
            return cond;
        };
        auto addTri = [](World& w, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            CollisionTriangle t;
            t.a = a; t.b = b; t.c = c;
            w.collisionMesh.triangles.push_back(t);
        };

        StructuredLogger::instance().init();
        const std::string eventsPath = StructuredLogger::instance().eventsPath();
        check(!eventsPath.empty(), "canonical events path is available");
        check(StructuredLogger::instance().shouldLog(
                  StructuredCategory::NpcMovement, StructuredLevel::Important),
              "npc_movement category allows Important");

        MatchRoleRegistry::instance().load("config/roles.json");
        MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
        BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
        NpcDifficultyConfig::instance().load("config/npc-difficulty.json");

        World world;
        addTri(world, {300,300,0}, {-300,300,0}, {-300,-300,0});
        addTri(world, {300,300,0}, {-300,-300,0}, {300,-300,0});
        addTri(world, {8,-6,0}, {8,6,0}, {8,6,4});
        addTri(world, {8,-6,0}, {8,6,4}, {8,-6,4});
        buildCollisionChunks(world, nullptr);

        NpcSystem npcs;
        const uint32_t npcId = 9602;
        npcs.spawnNpc(npcId, 5.0f, glm::vec3(0.0f, 0.0f, 1.9f));
        for (Npc& n : npcs.all()) {
            if (n.id != npcId) continue;
            n.actorPresetId = "counter_strike";
            n.behaviorProfileId = "balanced";
            n.behavior = resolveNpcBehavior("balanced");
            n.body.maxHp = 100;
            n.body.currentHp = 100;
            n.wakeupTimer = 0.0f;
            n.body.pos = glm::vec3(0.0f, 0.0f, 1.9f);
            n.stateMachine.currentState = NpcState::Patrol;
            break;
        }

        // Target far outside sight so the actor patrols (no visible enemy).
        Player target;
        target.pos = glm::vec3(280.0f, 0.0f, 2.0f);
        target.currentHp = 100;
        target.maxHp = 100;
        target.dead = false;

        for (int tick = 0; tick < 300; ++tick)
            npcs.updateOneWithTarget(npcId, world, target, 1.0f / 60.0f);

        StructuredLogger::instance().shutdown();

        int commitmentCreated = 0, commitmentReplaced = 0, planCreated = 0,
            planFailed = 0, goalChanged = 0, targetChanged = 0;
        std::ifstream in(eventsPath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("\"event\":\"npc.movement-commitment-created\"") != std::string::npos) ++commitmentCreated;
            if (line.find("\"event\":\"npc.movement-commitment-replaced\"") != std::string::npos) ++commitmentReplaced;
            if (line.find("\"event\":\"npc.nav-plan-created\"") != std::string::npos) ++planCreated;
            if (line.find("\"event\":\"npc.nav-plan-failed\"") != std::string::npos) ++planFailed;
            if (line.find("\"event\":\"npc.goal-changed\"") != std::string::npos) ++goalChanged;
            if (line.find("\"event\":\"npc.target-changed\"") != std::string::npos) ++targetChanged;
        }
        report += "  info  events path: " + eventsPath + "\n";
        report += "  info  commitmentCreated=" + std::to_string(commitmentCreated) +
                  " replaced=" + std::to_string(commitmentReplaced) +
                  " planCreated=" + std::to_string(planCreated) +
                  " planFailed=" + std::to_string(planFailed) +
                  " goalChanged=" + std::to_string(goalChanged) + "\n";
        ok &= check(commitmentCreated >= 1,
                    "npc.movement-commitment-created reached events.jsonl");
        ok &= check(goalChanged >= 1, "npc.goal-changed reached events.jsonl");
        ok &= check(planCreated + planFailed >= 1,
                    "npc.nav-plan-created/failed reached events.jsonl");
        ok &= check(commitmentCreated < 60,
                    "commitment events are rate-limited (not once per tick)");
        ok &= check(goalChanged < 60, "goal events are rate-limited (not once per tick)");

        printf("[NPC MOVEMENT COMMITMENT EVENT SELFTEST]\n%s", report.c_str());
        printf("[NPC MOVEMENT COMMITMENT EVENT SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--grenade-reasoning-selftest") {
        std::string report;
        const bool ok = npcGrenadeReasoningSelfTest(report);
        printf("[GRENADE REASONING SELFTEST]\n%s", report.c_str());
        printf("[GRENADE REASONING SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--team-brain-selftest") {
        std::string report;
        const bool ok = teamBrainSelfTest(report);
        printf("[TEAM BRAIN SELFTEST]\n%s", report.c_str());
        printf("[TEAM BRAIN SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--grenade-selftest") {
        std::string report;
        const bool ok = grenadeRegistrySelfTest(report);
        printf("[GRENADE SELFTEST]\n%s", report.c_str());
        printf("[GRENADE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--area-effect-selftest") {
        std::string report;
        const bool ok = areaEffectSelfTest(report);
        printf("[AREA EFFECT SELFTEST]\n%s", report.c_str());
        printf("[AREA EFFECT SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--map-config-selftest") {
        std::string report;
        const bool ok = mapConfigSelfTest(report);
        printf("[MAP CONFIG SELFTEST]\n%s", report.c_str());
        printf("[MAP CONFIG SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--objective-selftest") {
        std::string report;
        const bool ok = objectiveSelfTest(report);
        printf("[OBJECTIVE SELFTEST]\n%s", report.c_str());
        printf("[OBJECTIVE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-utility-selftest") {
        std::string report;
        const bool ok = npcUtilitySelfTest(report);
        printf("[NPC UTILITY SELFTEST]\n%s", report.c_str());
        printf("[NPC UTILITY SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-nav-request-selftest") {
        std::string report;
        const bool ok = npcNavRequestSelfTest(report);
        printf("[NPC NAV REQUEST SELFTEST]\n%s", report.c_str());
        printf("[NPC NAV REQUEST SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-perception-selftest") {
        std::string report;
        const bool ok = npcPerceptionSelfTest(report);
        printf("[NPC PERCEPTION SELFTEST]\n%s", report.c_str());
        printf("[NPC PERCEPTION SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--npc-radar-selftest") {
        bool ok = true;
        auto check = [&](bool condition, const char* name) {
            printf("  %s %s\n", condition ? "ok  " : "FAIL", name);
            ok = ok && condition;
        };

        auto& registry = BehaviorProfileRegistry::instance();
        check(registry.load("config/behavior-profiles.json"),
              "behavior profiles load");
        const NpcBehaviorTuning rage2 = resolveNpcBehavior("rage2");
        check(rage2.active, "rage2 profile resolves");
        check(rage2.informationMode == "perfect_radar",
              "rage2 uses perfect radar");
        check(rage2.radarDelayTicks == 0 && rage2.radarErrorMeters == 0.0f,
              "rage2 radar has no delay or error");
        check(rage2.radarMemoryMode == "never" && rage2.rememberedPathPoints == 32,
              "rage2 radar memory is persistent with a target trail");
        check(rage2.continuePredictedPath,
              "rage2 continues along predicted target movement");

        MemoryRecord memory;
        PerceptionSnapshot radar;
        radar.radarKnown = true;
        radar.position = glm::vec3(20.0f, 4.0f, 0.0f);
        updateMemory(memory, radar, radar.position, glm::vec3(2.0f, 0.0f, 0.0f),
                     1.0f / 60.0f, PerceptionTuning{});
        const BeliefState belief = buildBelief(
            PerceptionSnapshot{}, memory, 0, 0.0f, PerceptionTuning{});
        check(belief.hasTarget && !belief.hasVisibleTarget,
              "radar knowledge creates pursuit memory, not wall vision");

        printf("[NPC RADAR SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--counterstrike-acceptance-selftest") {
        // Consolidated pure-rule acceptance for every Counter-Strike subsystem.
        // This proves the data/rules load and behave; it does NOT prove live
        // gameplay or visuals. See docs/features/gamemodes/counterstrike.md.
        struct Case { const char* name; bool (*fn)(std::string&); };
        // Load the shared registries/weapons the rule checks depend on.
        // serverCounterStrikeRoundSelfTest loads roles/presets and builtin
        // weapons; the gamemode registry must be loaded here.
        GamemodeRegistry::instance().loadDirectory("config/gamemodes");
        std::string report;
        const bool roundOk = MimitaNet::serverCounterStrikeRoundSelfTest(report);
        std::string spawnReport;
        const bool spawnOk = MimitaNet::serverSpawnTagSelfTest(spawnReport);
        printf("[ACCEPTANCE] round+weapons: %s\n", roundOk ? "PASS" : "FAIL");
        printf("[ACCEPTANCE] spawn-tags   : %s\n", spawnOk ? "PASS" : "FAIL");
        bool all = roundOk && spawnOk;

        const Case cases[] = {
            {"objective",       objectiveSelfTest},
            {"map-config",      mapConfigSelfTest},
            {"grenade",         grenadeRegistrySelfTest},
            {"area-effect",     areaEffectSelfTest},
            {"team-brain",      teamBrainSelfTest},
            {"npc-perception",  npcPerceptionSelfTest},
            {"npc-utility",     npcUtilitySelfTest},
            {"npc-grenade",     npcGrenadeReasoningSelfTest},
            {"npc-nav-request", npcNavRequestSelfTest},
        };
        for (const Case& c : cases) {
            std::string r;
            const bool ok = c.fn(r);
            printf("[ACCEPTANCE] %-16s: %s\n", c.name, ok ? "PASS" : "FAIL");
            all = all && ok;
        }
        printf("[ACCEPTANCE] %s\n", all ? "PASS" : "FAIL");
        std::exit(all ? 0 : 1);
    }

    if (std::string(argv[1]) == "--spawn-tag-selftest") {
        std::string report;
        const bool ok = MimitaNet::serverSpawnTagSelfTest(report);
        printf("[SPAWN TAG SELFTEST]\n%s", report.c_str());
        printf("[SPAWN TAG SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--cs-round-selftest") {
        auto& registry = GamemodeRegistry::instance();
        registry.loadDirectory("config/gamemodes");
        std::string report;
        const bool ok = MimitaNet::serverCounterStrikeRoundSelfTest(report);
        printf("[CS ROUND SELFTEST]\n%s", report.c_str());
        printf("[CS ROUND SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--gamemode-selftest") {
        auto& registry = GamemodeRegistry::instance();
        registry.loadDirectory(argc > 2 ? argv[2] : "config/gamemodes");
        const Gamemode& cs = registry.get("counterstrike");
        printf("[GAMEMODE SELFTEST] loaded=%zu\n", registry.ids().size());
        printf("[GAMEMODE SELFTEST] id=%s teams=%zu\n", cs.id.c_str(), cs.teams.size());
        for (size_t i = 0; i < cs.teams.size(); ++i)
            printf("[GAMEMODE SELFTEST] team[%zu] id=%s display=%s capacity=%d role=%s spawn=%s\n",
                   i, cs.teams[i].id.c_str(), cs.teams[i].displayName.c_str(),
                   cs.teams[i].capacity, cs.teams[i].role.c_str(), cs.teams[i].spawnGroup.c_str());
        printf("[GAMEMODE SELFTEST] rounds_to_win=%d round_seconds=%.0f countdown=%.0f\n",
               cs.rounds.roundsToWin, cs.rounds.roundSeconds, cs.rounds.countdownSeconds);
        printf("[GAMEMODE SELFTEST] victory=%s objectives=%zu spawn_groups=%zu\n",
               cs.victoryCondition.c_str(), cs.objectives.size(), cs.spawnGroups.size());

        auto& roles = MatchRoleRegistry::instance();
        roles.load("config/roles.json");
        roles.loadActorPresets("config/actor-presets");
        const auto* ct = roles.get("counter_terrorist");
        const auto* t = roles.get("terrorist");
        const auto* preset = roles.getActorPreset("counter_strike");
        printf("[GAMEMODE SELFTEST] role counter_terrorist=%s actor_preset=%s avatar=%s team_id=%s\n",
               ct ? "found" : "missing", ct ? ct->actorPresetId.c_str() : "-",
               ct ? ct->avatarName.c_str() : "-", ct ? ct->teamId.c_str() : "-");
        printf("[GAMEMODE SELFTEST] role terrorist=%s actor_preset=%s avatar=%s team_id=%s\n",
               t ? "found" : "missing", t ? t->actorPresetId.c_str() : "-",
               t ? t->avatarName.c_str() : "-", t ? t->teamId.c_str() : "-");

        // Three ordered teams: CT, T, and the built-in Spectator team.
        const bool teamsOk = cs.teams.size() == 3 &&
            cs.teams[0].id == "ct" && cs.teams[0].displayName == "Counter-Terrorists" &&
            cs.teams[1].id == "t" && cs.teams[1].displayName == "Terrorists" &&
            cs.teams[2].id == "spec" && cs.teams[2].displayName == "Spectator" &&
            cs.teams[2].capacity == 0;
        const bool rolesOk = ct && t && ct->actorPresetId == "counter_strike" &&
            t->actorPresetId == "counter_strike" && preset != nullptr;
        const bool roundsOk = cs.rounds.roundsToWin == 8 && cs.rounds.maxRounds == 15;
        const bool objOk = cs.objectives.size() == 1 && cs.objectives[0].id == "bomb" &&
            cs.objectives[0].carrierTeam == "t";
        // Objective pulse visual is parsed from the objective's `visual` block.
        const bool visualOk = cs.objectives.size() == 1 &&
            cs.objectives[0].visual.enabled &&
            cs.objectives[0].visual.radius > 0.0f &&
            cs.objectives[0].visual.pulseAmplitude > 0.0f &&
            cs.objectives[0].visual.periodSeconds > 0.0f;
        printf("[GAMEMODE SELFTEST] objective_visual enabled=%d radius=%.2f amp=%.2f period=%.2f\n",
               (int)cs.objectives[0].visual.enabled, cs.objectives[0].visual.radius,
               cs.objectives[0].visual.pulseAmplitude, cs.objectives[0].visual.periodSeconds);
        const bool ok = teamsOk && rolesOk && roundsOk && objOk && visualOk;
        printf("[GAMEMODE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--mode-pack-selftest") {
        using namespace MimitaGamemode;
        std::vector<std::string> diagnostics;
        const bool loaded = ModePackRegistry::instance().loadDirectory(
            argc > 2 ? argv[2] : "config/mode-packs", &diagnostics);
        printf("[MODE PACK SELFTEST] loaded=%zu ok=%d\n",
               ModePackRegistry::instance().size(), (int)loaded);
        for (const std::string& line : diagnostics)
            printf("[MODE PACK SELFTEST] %s\n", line.c_str());
        for (const std::string& id : ModePackRegistry::instance().ids())
            printf("[MODE PACK SELFTEST] pack=%s\n", id.c_str());
        std::string disasterReport;
        const bool disasterOk = disasterRuntimeSelfTest(disasterReport);
        printf("[DISASTER SELFTEST]\n%s", disasterReport.c_str());
        printf("[DISASTER SELFTEST] %s\n", disasterOk ? "PASS" : "FAIL");
        const bool ok = loaded && disasterOk;
        printf("[MODE PACK SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--survive-disasters-selftest") {
        using namespace MimitaGamemode;
        std::vector<std::string> diagnostics;
        const bool loaded =
            ModePackRegistry::instance().loadDirectory("config/mode-packs", &diagnostics);
        const ModePack* pack = ModePackRegistry::instance().get("survive_disasters");
        printf("[SURVIVE DISASTERS] pack=%s schema=%d capabilities=%zu disasters=%zu\n",
               pack ? pack->id.c_str() : "missing", pack ? pack->schemaVersion : -1,
               pack ? pack->capabilities.size() : 0, pack ? pack->disasters.size() : 0);

        bool ok = loaded && pack != nullptr;
        if (pack) {
            const ActionGraph graph = ActionGraph::build(*pack);
            const bool hasWeapon = graph.hasCapability(Cap::kInventoryRandomPerActor);
            const bool hasWin = graph.hasCapability(Cap::kWinLastActorAlive);
            const auto activeActions = graph.dueAt((uint8_t)ActionPhase::Active, 1000, 1000);
            ok &= hasWeapon && hasWin && !activeActions.empty();

            DisasterState state;
            disasterConfigure(state, *pack, 42);
            disasterBegin(state, {7, 9, 11}, 1000);
            const bool assigned = state.weaponByActor.size() == 3 &&
                disasterWeaponForActor(state, 7) && disasterWeaponForActor(state, 9);
            const bool durationOk = !disasterDurationElapsed(state, 1000) &&
                disasterDurationElapsed(state, 1000 + state.durationTicks);
            const bool timeoutOk = disasterPickTimeoutWinner(42, {7, 9, 11}) != 0;
            printf("[SURVIVE DISASTERS] disaster=%s duration=%.0fs weapons=%zu actions=%zu\n",
                   state.disasterId.c_str(), (float)state.durationTicks / 60.0f,
                   state.weaponByActor.size(), graph.actions().size());
            ok &= assigned && durationOk && timeoutOk;
        }
        printf("[SURVIVE DISASTERS] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--collision-selftest") {
        std::string summary;
        const bool ok = collisionStressSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[COLLISION SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--actor-triangle-spike") {
        std::string summary;
        const bool ok = actorTriangleSpike(&summary);
        printf("%s", summary.c_str());
        printf("[ACTOR TRIANGLE SPIKE] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--actor-collision-mesh-selftest") {
        std::string summary;
        const bool ok = actorCollisionMeshSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[ACTOR COLLISION MESH SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--actor-triangle-solve-selftest") {
        std::string summary;
        const bool ok = actorTriangleSolverSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[ACTOR TRIANGLE SOLVE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--canonical-contact-selftest") {
        std::string summary;
        const bool ok = canonicalContactSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[CANONICAL CONTACT SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--moving-crate-selftest") {
        std::string summary;
        const bool ok = physicalEntitySelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[MOVING CRATE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--physical-perf-selftest") {
        std::string summary;
        const bool ok = physicalEntityPerfSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[PHYSICAL PERF SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--destructible-selftest") {
        std::string summary;
        const bool ok = MimitaImpact::destructibleSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[DESTRUCTIBLE SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--crash-exception-selftest") {
        // Validates the crash diagnostics: throws an uncaught C++ exception so
        // the text report must contain the exception type, what(), breadcrumbs,
        // and stack. The dialog is suppressed; the process still aborts. CLI
        // modes run before the engine installs the handler, so install it here.
        installCrashHandler();
        setCrashHandlerTestMode(true);
        recordCrashBreadcrumb("crash-test",
            "throwing uncaught exception to validate crash diagnostics");
        throw std::runtime_error(
            "intentional crash-handler test: uncaught std::runtime_error");
    }

    if (std::string(argv[1]) == "--destruction-stress-selftest") {
        std::string summary;
        const bool ok = MimitaImpact::destructionStressSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[DESTRUCTION STRESS SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--destruction-replication-selftest") {
        std::string summary;
        const bool ok = MimitaNet::destructionReplicationSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[DESTRUCTION REPLICATION SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--collision-subgrid-selftest") {
        std::string summary;
        const bool ok = collisionSubGridSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[COLLISION SUBGRID SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--collision-stress") {
        const std::string caseName = argc > 2 ? argv[2] : "wedge5";
        printf("%s\n", collisionStressRun(caseName).c_str());
        return true;
    }

    if (std::string(argv[1]) == "--ice-test") {
        printf("[ICE TEST] starting ICE candidate gathering test\n");

        IceConfiguration iceConfig = loadIceConfig();

        if (iceConfig.turn.password.empty())
        {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig))
        {
            printf("[ICE TEST] agent initialization failed\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        printf("[ICE TEST] agent initialized, starting gather...\n");

        if (!agent.gatherCandidates())
        {
            printf("[ICE TEST] gather failed\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        // Wait for gathering to complete (or timeout)
        const int maxWaitMs = 10000;
        int waited = 0;
        while (waited < maxWaitMs)
        {
            if (agent.state() == IceAgentState::GatheringComplete ||
                agent.state() == IceAgentState::Failed)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }

        auto candidates = agent.candidates();

        int hostCount = 0, srflxCount = 0, relayCount = 0;
        for (const auto& c : candidates)
        {
            switch (c.type)
            {
            case IceCandidateType::Host:            hostCount++; break;
            case IceCandidateType::ServerReflexive:  srflxCount++; break;
            case IceCandidateType::Relay:           relayCount++; break;
            default: break;
            }
        }

        printf("\n[ICE TEST] === CANDIDATE SUMMARY ===\n");
        printf("[ICE TEST] Host candidates:  %d\n", hostCount);
        printf("[ICE TEST] SRFLX candidates: %d\n", srflxCount);
        printf("[ICE TEST] Relay candidates: %d\n", relayCount);
        printf("[ICE TEST] Total candidates: %zu\n", candidates.size());
        printf("[ICE TEST] State: %d\n", static_cast<int>(agent.state()));

        bool ok = (hostCount >= 1 && srflxCount >= 1 && relayCount >= 1);
        printf("[ICE TEST] %s\n", ok ? "PASS" : "FAIL");

        if (!ok)
        {
            printf("[ICE TEST] Missing candidate types - check STUN/TURN configuration.\n");
            if (srflxCount == 0)
                printf("[ICE TEST]   No server-reflexive candidates - is STUN reachable?\n");
            if (relayCount == 0)
                printf("[ICE TEST]   No relay candidates - is TURN configured correctly?\n");
        }

        agent.shutdown();
        return true;
    }

    // ── ICE Host Test ────────────────────────────────────────
    if (std::string(argv[1]) == "--ice-host-test") {
        printf("[ICE TEST HOST START] processId=%lu\n", (unsigned long)GetCurrentProcessId());
        fflush(stdout);

        IceConfiguration iceConfig = loadIceConfig();
        if (iceConfig.turn.password.empty()) {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\nFAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig)) { printf("[ICE TEST] agent init failed\nFAIL\n"); return true; }
        if (!agent.gatherCandidates()) { printf("[ICE TEST] gather failed\nFAIL\n"); return true; }

        // Wait for gathering to finish
        int waited = 0;
        while (waited < 15000) {
            if (agent.state() == IceAgentState::GatheringComplete ||
                agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (agent.state() != IceAgentState::GatheringComplete) {
            printf("[ICE TEST] gather timeout\nFAIL\n"); return true;
        }

        // Register with coordinator and upload description
        std::string sessionId = "host_" + std::to_string(GetCurrentProcessId());
        std::string sdp = agent.localSdp();

        printf("[ICE DEBUG] Host SDP first 100 chars: %.*s\n", 100, sdp.c_str());
        fflush(stdout);

        auto hostResult = MimitaNet::coordinatorIceHost(sessionId, sdp);
        if (!hostResult.ok) { printf("[ICE TEST] coordinator register failed\nFAIL\n"); return true; }

        printf("[ICE TEST ROOM] code=%s\n", hostResult.roomCode.c_str());
        fflush(stdout);
        printf("[ICE SIGNAL UPLOAD] role=host code=%s descBytes=%zu\n", hostResult.roomCode.c_str(), sdp.size());
        fflush(stdout);

        // Poll for client
        printf("[ICE TEST] waiting for joiner...\n");
        fflush(stdout);
        std::string clientDesc;
        bool gotClient = false;
        waited = 0;
        while (waited < 60000) {
            auto pollResult = MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
            if (pollResult.ok && pollResult.status == "client_ready" && !pollResult.clientIceDescription.empty()) {
                clientDesc = pollResult.clientIceDescription;
                gotClient = true;
                printf("[ICE SIGNAL RECEIVE] role=host code=%s descBytes=%zu\n", hostResult.roomCode.c_str(), clientDesc.size());
                printf("[ICE DEBUG] Client SDP first 100: %.*s\n", 100, clientDesc.c_str());
                fflush(stdout);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            waited += 500;
        }
        if (!gotClient) { printf("[ICE TEST] client poll timeout\nFAIL\n"); return true; }

        // Set remote description
        if (!agent.setRemoteDescription(clientDesc)) {
            printf("[ICE TEST] setRemoteDescription failed\nFAIL\n"); return true;
        }

        // Wait for connected state
        printf("[ICE TEST] waiting for ICE connection...\n");
        waited = 0;
        bool connected = false;
        while (waited < 15000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::StateChanged) {
                    printf("[ICE STATE] role=host old=%d new=%d\n",
                           static_cast<int>(agent.state()), static_cast<int>(ev.newState));
                    if (ev.newState == IceAgentState::Connected || ev.newState == IceAgentState::Completed)
                        connected = true;
                }
            }
            if (connected || agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (!connected) { printf("[ICE TEST] connection timeout\nFAIL\n"); return true; }
        printf("[ICE CONNECTED] role=host durationMs=%d\n", waited);

        // Small stabilization delay
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        printf("[ICE TEST] waiting for data from joiner...\n");
        waited = 0;
        std::string recvd;
        while (waited < 10000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::Recv) {
                    recvd.assign(ev.data.data(), ev.data.size());
                }
            }
            if (!recvd.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
        if (recvd.empty()) { printf("[ICE TEST] no data received\nFAIL\n"); return true; }
        printf("[ICE TEST RECEIVE] role=host bytes=%zu payload=%.*s\n", recvd.size(), (int)recvd.size(), recvd.c_str());

        // Reply
        const char* reply = "hello from host";
        if (!agent.send(reply, strlen(reply) + 1)) {
            printf("[ICE TEST] host send failed\nFAIL\n"); return true;
        }
        printf("[ICE TEST SEND] role=host bytes=%zu\n", strlen(reply) + 1);

        MimitaNet::coordinatorIceDone(hostResult.roomCode);
        printf("[ICE TEST RESULT] role=host pass=1\nPASS\n");
        agent.shutdown();
        return true;
    }

    // ── ICE Join Test ────────────────────────────────────────
    if (std::string(argv[1]) == "--ice-join-test") {
        if (argc < 3) {
            printf("[ICE JOIN] usage: mimita.exe --ice-join-test <room-code>\nFAIL\n");
            return true;
        }

        std::string roomCode = argv[2];
        printf("[ICE TEST JOIN START] roomCode=%s processId=%lu\n",
               roomCode.c_str(), (unsigned long)GetCurrentProcessId());

        IceConfiguration iceConfig = loadIceConfig();
        if (iceConfig.turn.password.empty()) {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\nFAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig)) { printf("[ICE TEST] agent init failed\nFAIL\n"); return true; }
        if (!agent.gatherCandidates()) { printf("[ICE TEST] gather failed\nFAIL\n"); return true; }

        int waited = 0;
        while (waited < 15000) {
            if (agent.state() == IceAgentState::GatheringComplete ||
                agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (agent.state() != IceAgentState::GatheringComplete) {
            printf("[ICE TEST] gather timeout\nFAIL\n"); return true;
        }

        // Join via coordinator, get host SDP
        std::string clientSessionId = "client_" + std::to_string(GetCurrentProcessId());
        std::string sdp = agent.localSdp();

        printf("[ICE DEBUG] Join SDP first 100 chars: %.*s\n", 100, sdp.c_str());
        fflush(stdout);

        auto joinResult = MimitaNet::coordinatorIceJoin(roomCode, clientSessionId, sdp);
        if (!joinResult.ok || joinResult.hostIceDescription.empty()) {
            printf("[ICE TEST] coordinator join failed (room not found or expired)\nFAIL\n");
            return true;
        }

        printf("[ICE SIGNAL RECEIVE] role=join code=%s descBytes=%zu\n",
               roomCode.c_str(), joinResult.hostIceDescription.size());

        printf("[ICE DEBUG] Host SDP first 100 chars: %.*s\n", 100, joinResult.hostIceDescription.c_str());
        fflush(stdout);

        // Set remote (host) SDP
        if (!agent.setRemoteDescription(joinResult.hostIceDescription)) {
            printf("[ICE TEST] setRemoteDescription failed\nFAIL\n"); return true;
        }

        // Wait for connected
        printf("[ICE TEST] waiting for ICE connection...\n");
        waited = 0;
        bool connected = false;
        while (waited < 15000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::StateChanged) {
                    printf("[ICE STATE] role=join old=%d new=%d\n",
                           static_cast<int>(agent.state()), static_cast<int>(ev.newState));
                    if (ev.newState == IceAgentState::Connected || ev.newState == IceAgentState::Completed)
                        connected = true;
                }
            }
            if (connected || agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (!connected) { printf("[ICE TEST] connection timeout\nFAIL\n"); return true; }
        printf("[ICE CONNECTED] role=join durationMs=%d\n", waited);

        // Small stabilization delay
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Send test payload
        const char* msg = "hello from client";
        bool sendOk = agent.send(msg, strlen(msg) + 1);
        printf("[ICE TEST SEND] role=join bytes=%zu result=%d\n", strlen(msg) + 1, sendOk);
        fflush(stdout);
        if (!sendOk) {
            printf("[ICE TEST] join send failed\nFAIL\n"); return true;
        }

        // Wait for reply
        printf("[ICE TEST] waiting for reply...\n");
        waited = 0;
        std::string reply;
        while (waited < 10000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::Recv) {
                    reply.assign(ev.data.data(), ev.data.size());
                }
            }
            if (!reply.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
        if (reply.empty()) { printf("[ICE TEST] no reply received\nFAIL\n"); return true; }
        printf("[ICE TEST RECEIVE] role=join bytes=%zu payload=%.*s\n", reply.size(), (int)reply.size(), reply.c_str());

        printf("[ICE TEST RESULT] role=join pass=1\nPASS\n");
        agent.shutdown();
        return true;
    }

    if (std::string(argv[1]) == "--replay-selftest") {
        ReplayClip clip;
        clip.header.tickRate = 60;
        clip.header.tickCount = 2;
        clip.mapPath = "assets/maps/mimita-duels-map-v3.glb";
        clip.killerId = "player";
        clip.victimId = "npc_100";
        clip.killTick = 30;

        ReplayActorState actorA;
        actorA.id = "player";
        actorA.name = "player";
        actorA.type = "player";
        actorA.position = {0.0f, 0.0f, 0.0f};
        actorA.weaponName = "revolver";
        ReplaySceneFrame frameA;
        frameA.tick = 0;
        frameA.actors.push_back(actorA);

        ReplayActorState actorB = actorA;
        actorB.position = {10.0f, 0.0f, 0.0f};
        ReplaySceneFrame frameB;
        frameB.tick = 60;
        frameB.time = 1.0f;
        frameB.actors.push_back(actorB);
        clip.sceneFrames = {frameA, frameB};

        const std::filesystem::path path =
            std::filesystem::path("build") / "replay-selftest.mclip.json";
        ReplayPlayer playerTest;
        const bool saved = clip.save(path.string());
        const bool loaded = saved && playerTest.loadFromJSON(path.string());
        playerTest.setTimescale(0.25f);
        playerTest.beginPlayback();
        playerTest.update(1.0f);
        const ReplaySceneFrame* interpolated =
            playerTest.currentSceneFrame();
        const bool interpolationOk =
            interpolated && !interpolated->actors.empty() &&
            std::fabs(interpolated->actors.front().position.x - 2.5f) < 0.01f;
        const bool camerasOk =
            playerTest.cameraController().setMode("fp") &&
            playerTest.cameraController().setMode("victim") &&
            playerTest.cameraController().setMode("orbit") &&
            playerTest.cameraController().setMode("freecam");
        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        printf("[REPLAY SELFTEST] save=%d load=%d interpolation=%d cameras=%d\n",
               (int)saved, (int)loaded, (int)interpolationOk, (int)camerasOk);
        return true;
    }

    if (std::string(argv[1]) == "--replay-export-fbo-test") {
        printf("[REPLAY EXPORT FBO TEST] Starting...\n");
        // Verifies the offscreen capture FBO (create/render/read) used by Step 1
        // export. Creates a hidden GL context so it runs headlessly.
        int failures = 0, total = 0;
        auto check = [&](bool cond, const char* name) {
            total++;
            if (!cond) { printf("  FAIL: %s\n", name); failures++; }
            else { printf("  PASS: %s\n", name); }
        };
        if (!glfwInit()) {
            printf("[REPLAY EXPORT FBO TEST] glfwInit failed (no display) — SKIP\n");
            return true;
        }
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        GLFWwindow* win = glfwCreateWindow(64, 64, "mimita-fbo-test", nullptr, nullptr);
        if (!win) {
            printf("[REPLAY EXPORT FBO TEST] cannot create hidden context — SKIP\n");
            glfwTerminate();
            return true;
        }
        glfwMakeContextCurrent(win);
        bool glOk = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0;
        check(glOk, "load OpenGL (glad)");
        if (glOk) {
            check(replayExportTargetInit(32, 32), "create offscreen export FBO (32x32)");
            if (replayExportTarget().ready()) {
                auto& tgt = replayExportTarget();
                glBindFramebuffer(GL_FRAMEBUFFER, tgt.fbo);
                glViewport(0, 0, tgt.width, tgt.height);
                glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                std::vector<uint8_t> px(tgt.width * tgt.height * 3, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, tgt.fbo);
                glReadPixels(0, 0, tgt.width, tgt.height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                // Pixel (0,0) should be red (255,0,0).
                bool red = px[0] > 200 && px[1] < 60 && px[2] < 60;
                check(red, "read red pixel back from offscreen FBO");
                printf("    sample pixel(0,0) = (%u,%u,%u)\n", px[0], px[1], px[2]);
            } else {
                check(false, "offscreen FBO ready");
            }
            replayExportTargetDestroy();
            check(!replayExportTarget().ready(), "offscreen FBO destroyed");
        }
        glfwDestroyWindow(win);
        glfwTerminate();
        printf("\n[REPLAY EXPORT FBO TEST] Results: %d/%d passed, %d failed\n",
               total - failures, total, failures);
        return true;
    }

    if (std::string(argv[1]) == "--replay-export-selftest") {
        printf("[REPLAY EXPORT SELFTEST] Starting...\n");

        int failures = 0;
        int total = 0;
        auto check = [&](bool cond, const char* name) {
            total++;
            if (!cond) { printf("  FAIL: %s\n", name); failures++; }
            else { printf("  PASS: %s\n", name); }
        };

        // 1. Create a tiny replay file
        printf("\n--- Creating test replay ---\n");
        ReplayClip clip;
        clip.header.tickRate = 60;
        clip.header.tickCount = 10;
        clip.mapPath = "assets/maps/mimita-duels-map-v3.glb";
        clip.killerId = "player";
        clip.victimId = "npc_100";
        clip.killTick = 5;

        ReplayActorState actorA;
        actorA.id = "player";
        actorA.name = "player";
        actorA.type = "player";
        actorA.position = {0.0f, 0.0f, 0.0f};
        actorA.weaponName = "revolver";
        actorA.bodyPartCount = 2;
        actorA.bodyParts[0].partId = (uint8_t)ReplayBodyPartId::LeftLeg;
        actorA.bodyParts[0].parentPartId = (uint8_t)ReplayBodyPartId::Torso;
        actorA.bodyParts[1].partId = (uint8_t)ReplayBodyPartId::RightLeg;
        actorA.bodyParts[1].parentPartId = (uint8_t)ReplayBodyPartId::Torso;
        for (uint32_t i = 0; i < 10; i++) {
            ReplaySceneFrame f;
            f.tick = (int)i;
            f.time = (float)i / 60.0f;
            actorA.position.x = (float)i * 2.0f;
            f.camera.position = {100.0f + (float)i * 3.0f, 200.0f, 300.0f};
            f.camera.rotation = {5.0f, 0.0f, 90.0f + (float)i * 2.0f};
            f.camera.fov = 100.0f;
            f.actors.push_back(actorA);
            if (i == 2) {
                ReplayEffectEvent rocketEvent;
                rocketEvent.eventId = 9001;
                rocketEvent.type = "projectile_spawn";
                rocketEvent.assetId = "rocket_launcher";
                rocketEvent.sourceActorId = "player";
                rocketEvent.spawnTick = 2;
                rocketEvent.position = {1.0f, 2.0f, 3.0f};
                rocketEvent.velocity = {45.0f, 0.0f, 0.0f};
                rocketEvent.lifetime = 5.0f;
                f.effects.push_back(rocketEvent);
            }
            clip.sceneFrames.push_back(f);
        }

        const std::filesystem::path selftestDir =
            std::filesystem::path("build") / "replay-export-selftest";
        std::error_code ec;
        std::filesystem::create_directories(selftestDir, ec);
        std::filesystem::path replayPath = selftestDir / "selftest.mclip.json";
        bool saved = clip.save(replayPath.string());
        check(saved, "create test replay file");
        if (!saved) { printf("[REPLAY EXPORT SELFTEST] FAILED: cannot create replay\n"); return true; }

        // 2. Verify replay loads and advances
        printf("\n--- Verifying replay advances ---\n");
        ReplayPlayer player;
        bool loaded = player.loadFromJSON(replayPath.string());
        check(loaded, "load test replay into player");
        if (!loaded) { printf("[REPLAY EXPORT SELFTEST] FAILED: cannot load replay\n"); return true; }

        player.beginPlayback();
        check(player.isPlaying(), "beginPlayback sets isPlaying");

        player.seekToTick(0);
        const ReplaySceneFrame* frame0 = player.currentSceneFrame();
        check(frame0 != nullptr && !frame0->actors.empty(), "seekToTick(0) returns scene frame with actors");
        check(frame0 && frame0->actors[0].position.x == 0.0f, "actor0.x at tick 0 == 0.0");
        check(frame0 && frame0->camera.position == glm::vec3(100.0f, 200.0f, 300.0f),
              "camera at tick 0 matches recorded player camera");

        player.seekToTick(5);
        player.update(0.0f);
        const ReplaySceneFrame* frame5 = player.currentSceneFrame();
        check(frame5 != nullptr, "seekToTick(5) returns scene frame");
        check(frame5 && frame5->actors[0].position.x > 0.0f, "actor0.x at tick 5 > 0.0 (advancing)");
        check(frame5 && frame5->camera.position.x > 100.0f,
              "camera position advances with replay tick");
        const float cameraXAtTick5 = frame5 ? frame5->camera.position.x : 0.0f;

        player.seekToTick(9);
        player.update(0.0f);
        const ReplaySceneFrame* frame9 = player.currentSceneFrame();
        check(frame9 != nullptr, "seekToTick(9) returns scene frame");
        check(frame9 && frame9->camera.position.x > cameraXAtTick5,
              "camera continues moving at final replay tick");

        check(player.currentTick() > 0, "currentTick > 0 (replay advances)");
        const ReplaySceneFrame* eventFrame = player.currentSceneFrame();
        check(eventFrame && eventFrame->actors[0].bodyParts[0].parentPartId ==
                  (uint8_t)ReplayBodyPartId::Torso,
              "replay body-part parent relationship survives serialization");
        bool foundRocketEvent = false;
        for (const ReplayEffectEvent& event : clip.sceneFrames[2].effects)
            foundRocketEvent |= event.eventId == 9001 && event.assetId == "rocket_launcher";
        check(foundRocketEvent, "rocket event keeps stable event ID and weapon identity");
        bool foundRocketOwner = false;
        for (const ReplayEffectEvent& event : clip.sceneFrames[2].effects)
            foundRocketOwner |= event.eventId == 9001 && event.sourceActorId == "player";
        check(foundRocketOwner, "rocket event keeps source actor identity");

        // 3. Verify the player-default Windows encoder without launching the game.
#ifdef _WIN32
        printf("\n--- Verifying Media Foundation MP4 writer ---\n");
        std::filesystem::path mfPath = selftestDir / "selftest_mf.mp4";
        MfMp4Writer* mfWriter = nullptr;
        std::string mfError;
        bool mfStarted = startMfReplayExport(mfWriter, mfPath.string(), 320, 240, 1000, mfError);
        check(mfStarted, "Media Foundation writer starts");
        for (int i = 0; mfWriter && !mfReplayInitReady(mfWriter) && i < 200; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bool mfInitOk = mfStarted && mfWriter && mfReplayInitSucceeded(mfWriter);
        check(mfInitOk, "Media Foundation encoder initializes");
        if (mfWriter) printf("    Media Foundation encoder path: %s\n",
                             mfReplayEncoderMode(mfWriter).c_str());
        bool mfFramesOk = mfInitOk;
        std::vector<uint8_t> mfFrame(320 * 240 * 3);
        for (uint32_t frame = 0; frame < 30 && mfFramesOk && mfWriter; ++frame) {
            for (size_t i = 0; i < mfFrame.size(); i += 3) {
                mfFrame[i] = frame == 0 ? 255 : 0;
                mfFrame[i + 1] = frame == 1 ? 255 : 0;
                mfFrame[i + 2] = frame == 2 ? 255 : 0;
            }
            bool accepted = false;
            bool okCall = false;
            for (int tries = 0; tries < 2000 && mfWriter; ++tries) {
                okCall = writeMfReplayVideoFrame(
                    mfWriter, mfFrame.data(), 320, 240, frame, &accepted, mfError);
                if (okCall && accepted) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!(okCall && accepted)) { mfFramesOk = false; break; }
        }
        check(mfFramesOk, "Media Foundation accepts synthetic NV12 frames");
        std::filesystem::path mfWav = selftestDir / "selftest_mf.wav";
        std::vector<int16_t> mfSilence(24000 * 2, 0);
        bool mfWavOk = writeReplayExportWav(
            mfWav.string(), mfSilence.data(), mfSilence.size(), 48000, 2);
        check(mfWavOk, "create Media Foundation PCM input");
        bool mfFinished = false;
        if (mfStarted && mfInitOk && mfFramesOk && mfWavOk && mfWriter) {
            finishMfReplayExport(mfWriter, mfWav.string(),
                                 std::filesystem::exists("assets/video/mimitaoutrov1.mp4")
                                     ? "assets/video/mimitaoutrov1.mp4" : std::string());
            bool ok = false;
            bool outroMissing = false;
            std::string err;
            int waits = 0;
            while (mfWriter && !pollMfReplayExport(mfWriter, ok, outroMissing, err)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (++waits > 600) break;
            }
            mfFinished = ok;
            if (!err.empty()) mfError = err;
        }
        if (mfWriter) cancelMfReplayExport(mfWriter);
        check(mfFinished, "Media Foundation finalizes MP4");
        bool mfOutput = std::filesystem::exists(mfPath, ec) &&
            std::filesystem::file_size(mfPath, ec) > 0;
        check(mfOutput, "Media Foundation MP4 exists and is non-empty");
        if (!mfError.empty()) printf("    Media Foundation detail: %s\n", mfError.c_str());

        // 3b. Cancel mid-encode must not corrupt memory or hang.
        {
            MfMp4Writer* cw = nullptr;
            std::string cerr;
            bool cwStarted = startMfReplayExport(cw, (selftestDir / "cancel_mf.mp4").string(),
                                                 320, 240, 1000, cerr);
            for (int i = 0; cw && !mfReplayInitReady(cw) && i < 200; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            for (uint32_t frame = 0; frame < 8 && cw; ++frame) {
                bool acc = false;
                std::string e;
                writeMfReplayVideoFrame(cw, mfFrame.data(), 320, 240, frame, &acc, e);
            }
            if (cw) cancelMfReplayExport(cw);
            check(cwStarted && cw == nullptr, "Media Foundation cancels cleanly");
        }
        std::string ffmpegProbe = defaultFfmpegPath();
        if (!ffmpegProbe.empty() && std::filesystem::exists(ffmpegProbe)) {
            // ffprobe is not shipped; verify streams by parsing `ffmpeg -i` output.
            // Wrap the whole command in an extra pair of quotes: `_popen` runs it
            // via `cmd.exe /c`, which otherwise mangles `"quoted exe" ... 2>&1`.
            std::string probeCmd = "\"\"" + ffmpegProbe + "\" -hide_banner -i \"" +
                std::filesystem::absolute(mfPath).string() + "\" 2>&1\"";
            FILE* pipe = _popen(probeCmd.c_str(), "r");
            std::string info;
            char probeLine[256];
            while (pipe && fgets(probeLine, sizeof(probeLine), pipe)) info += probeLine;
            if (pipe) _pclose(pipe);
            const bool videoOk = info.find("h264") != std::string::npos &&
                info.find("Video") != std::string::npos;
            const bool audioOk = info.find("aac") != std::string::npos &&
                info.find("Audio") != std::string::npos;
            if (!videoOk || !audioOk) {
                printf("    ffmpeg -i output:\n%s\n", info.c_str());
            }
            check(videoOk, "ffmpeg -i verifies H.264 video stream");
            check(audioOk, "ffmpeg -i verifies AAC audio stream");
        }
#endif

        // 4. Verify optional developer FFmpeg when it is installed.
        printf("\n--- Verifying ffmpeg ---\n");
        std::string ffmpegPath = defaultFfmpegPath();
        bool ffmpegExists = std::filesystem::exists(ffmpegPath);
        if (gExportConfig.encoder == "ffmpeg")
            check(ffmpegExists, "ffmpeg exists through dynamic resolution");
        if (!ffmpegExists) {
            printf("[REPLAY EXPORT SELFTEST] ffmpeg not found, skipping ffmpeg tests\n");
        } else {
            // 4. Create a synthetic raw file and encode it with ffmpeg
            printf("\n--- Creating synthetic raw file ---\n");
            int rawW = 320, rawH = 240, rawFrames = 3;
            std::filesystem::path rawPath = selftestDir / "selftest_raw.rgb";
            std::filesystem::path mp4Path = selftestDir / "selftest_output.mp4";
            std::filesystem::path stderrPath = selftestDir / "selftest_ffmpeg_stderr.txt";

            // Write 3 frames of raw RGB data (solid red/green/blue)
            FILE* rawFile = fopen(rawPath.string().c_str(), "wb");
            bool rawOpened = (rawFile != nullptr);
            check(rawOpened, "open synthetic raw file");
            if (rawOpened) {
                for (int f = 0; f < rawFrames; f++) {
                    std::vector<uint8_t> rawFrame(rawW * rawH * 3);
                    uint8_t r = (f == 0) ? 255 : (f == 1) ? 0 : 0;
                    uint8_t g = (f == 0) ? 0 : (f == 1) ? 255 : 0;
                    uint8_t b = (f == 0) ? 0 : (f == 1) ? 0 : 255;
                    for (size_t i = 0; i < rawFrame.size(); i += 3) {
                        rawFrame[i] = r;
                        rawFrame[i+1] = g;
                        rawFrame[i+2] = b;
                    }
                    fwrite(rawFrame.data(), 1, rawFrame.size(), rawFile);
                }
                fclose(rawFile);
            }

            uint64_t rawSize = std::filesystem::file_size(rawPath, ec);
            check(rawSize > 0, "raw file exists and size > 0");
            printf("    raw file bytes=%llu\n", (unsigned long long)rawSize);

            // 5. Run ffmpeg on synthetic raw file
            printf("\n--- Running ffmpeg on synthetic raw file ---\n");
            std::string rawStr = rawPath.make_preferred().string();
            std::string mp4Str = mp4Path.make_preferred().string();

            // Write a batch file to avoid cmd.exe quoting issues with std::system()
            std::filesystem::path batPath = selftestDir / "encode.bat";
            std::string batContent = "@echo off\r\n"
                "\"" + ffmpegPath + "\" -y -f rawvideo -pixel_format rgb24 "
                "-video_size " + std::to_string(rawW) + "x" + std::to_string(rawH) + " "
                "-framerate 60 -i \"" + rawStr + "\" "
                "-c:v libx264 -preset fast -pix_fmt yuv420p "
                "-crf 23 \"" + mp4Str + "\" "
                "-loglevel error\r\n"
                "exit /b %ERRORLEVEL%\r\n";
            FILE* batFile = fopen(batPath.string().c_str(), "w");
            if (batFile) {
                fwrite(batContent.c_str(), 1, batContent.size(), batFile);
                fclose(batFile);
            }
            int ffmpegResult = std::system(batPath.string().c_str());
            printf("    ffmpeg exit code=%d\n", ffmpegResult);

            // 6. Verify mp4 exists and has size > 0
            printf("\n--- Verifying mp4 output ---\n");
            bool mp4Exists = std::filesystem::exists(mp4Path, ec);
            check(mp4Exists, "mp4 file exists");
            bool mp4SizeOk = false;
            if (mp4Exists) {
                uint64_t mp4Size = std::filesystem::file_size(mp4Path, ec);
                mp4SizeOk = mp4Size > 0;
                check(mp4SizeOk, "mp4 file size > 0");
                printf("    mp4 bytes=%llu\n", (unsigned long long)mp4Size);
            } else {
                check(false, "mp4 file size > 0");
            }
        }

        // Cleanup
        printf("\n--- Cleanup ---\n");
        std::filesystem::remove_all(selftestDir, ec);

        printf("\n[REPLAY EXPORT SELFTEST] Results: %d/%d passed, %d failed\n",
               total - failures, total, failures);
        return true;
    }

    if (std::string(argv[1]) == "-exportdiagnostic") {
        printf("[EXPORTDIAG] Running replay export diagnostic...\n");
        // Find newest replay
        std::vector<std::string> clips = listReplayClips();
        std::string reportPath = "replays/exports/export-diagnostic-report.txt";
        std::error_code ec;
        std::filesystem::create_directories("replays/exports", ec);
        FILE* report = fopen(reportPath.c_str(), "w");
        if (!report) {
            printf("[EXPORTDIAG] FAILED: cannot write report\n");
            return true;
        }
        fprintf(report, "=== EXPORT DIAGNOSTIC REPORT ===\n");
        fprintf(report, "Timestamp: %lld\n", (long long)std::time(nullptr));

        // CHECK A: Replay exists
        fprintf(report, "\n--- CHECK A: Replay exists ---\n");
        if (clips.empty()) {
            fprintf(report, "FAIL: No replays found\n");
            fclose(report);
            return true;
        }
        std::string newestPath = clips.front();
        fprintf(report, "PASS: Newest replay: %s\n", newestPath.c_str());

        // CHECK B: Replay loads
        fprintf(report, "\n--- CHECK B: Replay loads ---\n");
        ReplayClip clip;
        if (!clip.load(newestPath)) {
            fprintf(report, "FAIL: Cannot load replay\n");
            fclose(report);
            return true;
        }
        fprintf(report, "PASS: tickCount=%u duration=%.1fs map=%s\n",
                clip.header.tickCount, (float)clip.header.tickCount / 60.0f,
                clip.header.mapName);

        // CHECK C: FFmpeg exists
        fprintf(report, "\n--- CHECK C: FFmpeg ---\n");
        std::string ffmpeg = defaultFfmpegPath();
        if (!std::filesystem::exists(ffmpeg)) {
            fprintf(report, "FAIL: ffmpeg not found at %s\n", ffmpeg.c_str());
            fclose(report);
            return true;
        }
        fprintf(report, "PASS: ffmpeg found at %s\n", ffmpeg.c_str());

        // CHECK D: ffmpeg -version
        fprintf(report, "\n--- CHECK D: ffmpeg -version ---\n");
        std::string versionCmd = "\"" + ffmpeg + "\" -version 2>&1";
        FILE* vp = _popen(versionCmd.c_str(), "r");
        if (vp) {
            char vbuf[256];
            if (fgets(vbuf, sizeof(vbuf), vp))
                fprintf(report, "PASS: %s", vbuf);
            _pclose(vp);
        } else {
            fprintf(report, "FAIL: cannot run ffmpeg -version\n");
        }

        // CHECK E: Load into gReplayPlayer
        fprintf(report, "\n--- CHECK E: Load into gReplayPlayer ---\n");
        {
            ReplayPlayer diagnosticPlayer;
            if (!diagnosticPlayer.loadFromJSON(newestPath)) {
                fprintf(report, "FAIL: Cannot load replay into ReplayPlayer\n");
            } else {
                fprintf(report, "PASS: totalTicks=%u\n", diagnosticPlayer.totalTicks());
                diagnosticPlayer.beginPlayback();
                fprintf(report, "PASS: beginPlayback OK. isPlaying=%d\n", (int)diagnosticPlayer.isPlaying());
                diagnosticPlayer.seekToTick(0);
                const ReplaySceneFrame* frame = diagnosticPlayer.currentSceneFrame();
                fprintf(report, "PASS: seekToTick(0). hasSceneFrame=%d\n", frame ? 1 : 0);
                if (frame)
                    fprintf(report, "PASS: actors=%zu\n", frame->actors.size());
            }
        }

        // CHECK F: Output path
        fprintf(report, "\n--- CHECK F: Output path ---\n");
        std::string outputPath = generateExportOutputPath();
        fprintf(report, "PASS: outputPath=%s\n", outputPath.c_str());

        fprintf(report, "\n=== DIAGNOSTIC COMPLETE ===\n");
        fclose(report);
        printf("[EXPORTDIAG] Report written to %s\n", reportPath.c_str());
        return true;
    }

    return false;
}
