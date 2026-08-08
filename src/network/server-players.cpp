// 07 21 2026, 17 10
/* purpose
* Owns authoritative server player names, spawning, collision, simulation, and snapshot entity state.
* Keeps server Player lifecycle, movement parity state, and weapon spawn inventory synchronized.
* Provides small helpers used by the dedicated and listen-server loops.
* Does NOT parse client packets, render players, or define network packet layouts.
* Does NOT own weapon definitions, projectile simulation, or client prediction.
* Does NOT let stale movement sequences survive spawn, respawn, teleport, or death.
*/

#include "network/server.h"
#include "network/network-weapons.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/physics-collision.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace MimitaNet {
namespace {

float safeServerSizeScale(float sizeScale)
{
    return std::isfinite(sizeScale) && sizeScale > 0.0f ? sizeScale : 1.0f;
}

void syncServerMovementRuntime(ServerPlayer& player, bool movementEnabled)
{
    player.movement.lifecycle = MovementLifecycleIdentity{
        player.spawnGeneration,
        static_cast<uint32_t>(player.transformEpoch)};
    player.movement.movementEnabled =
        movementEnabled && !player.dead && player.spawnState == ServerPlayer::Active;
    player.movement.position = player.pos;
    if (!movementIsFinite(player.movement.externalImpulse))
        player.movement.externalImpulse = glm::vec3(0.0f);
    player.movement.baseVelocity = player.vel - player.movement.externalImpulse;
    player.movement.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    player.movement.yaw = player.yaw;
    player.movement.sizeScale = safeServerSizeScale(player.sizeScale);
    player.movement.ground.onGround = player.onGround;
    player.movement.ground.stableOnGround = player.onGround;
    player.movement.ground.hasWorldContact = player.onGround;
    player.movement.dash.dashAvailable = player.dashAvailable;
}

} // namespace

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

std::string uniquePlayerName(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& requested,
    uint32_t ownId)
{
    const std::string base = requested.empty() ? "player" + std::to_string(ownId) : requested;
    std::string candidate = base;
    int suffix = 2;
    for (;;)
    {
        bool used = false;
        for (const auto& kv : players)
        {
            if (kv.first != ownId && kv.second.name == candidate)
            {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
        candidate = base + "(" + std::to_string(suffix++) + ")";
    }
}

glm::vec3 closestPointTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;
    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 >= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

void resolveWorldCollision(ServerPlayer& p, const HeadlessWorld& world)
{
    p.onGround = false;

    for (int pass = 0; pass < 3; ++pass)
    {
        glm::vec3 samples[3] = {
            p.pos + glm::vec3(0, 0, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS),
            p.pos,
            p.pos + glm::vec3(0, 0, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS)
        };

        for (glm::vec3 sample : samples)
        {
            // Broadphase: gather only triangles near the sample point
            AABB queryBounds;
            queryBounds.min = sample - glm::vec3(PLAYER_RADIUS + 0.1f);
            queryBounds.max = sample + glm::vec3(PLAYER_RADIUS + 0.1f);
            thread_local std::vector<int> s_candidates;
            s_candidates.clear();
            gatherHeadlessTrianglesForAABB(world, queryBounds, PLAYER_RADIUS * 0.1f, s_candidates);

            for (int triIdx : s_candidates)
            {
                if (triIdx < 0 || triIdx >= (int)world.triangles.size())
                    continue;
                const CollisionTriangle& tri = world.triangles[triIdx];

                glm::vec3 cp = closestPointTriangle(sample, tri.a, tri.b, tri.c);
                glm::vec3 delta = sample - cp;
                float dist = glm::length(delta);
                if (dist >= PLAYER_RADIUS || dist < 0.00001f)
                    continue;

                glm::vec3 n = delta / dist;
                if (glm::dot(n, tri.normal) < 0.0f)
                    n = -n;
                float penetration = PLAYER_RADIUS - dist;
                p.pos += n * (penetration + 0.001f);
                float into = glm::dot(p.vel, n);
                if (into < 0.0f)
                    p.vel -= n * into;
                if (n.z > 0.35f)
                    p.onGround = true;
            }
        }
    }

    // Debug log when a real triangle collision resolves below the map bounds
    if (p.pos.z < world.boundsMin.z)
    {
        static uint64_t lastBelowBoundsCollisionLogMs = 0;
        uint64_t nowBc = nowMs();
        if (nowBc - lastBelowBoundsCollisionLogMs >= 1000)
        {
            printf("%s [SERVER BELOW-MAP COLLISION] playerId=%u "
                   "posZBefore=%.2f posZAfter=%.2f onGround=%d\n",
                   serverTimestamp(), p.id,
                   p.pos.z, p.pos.z, (int)p.onGround);
            lastBelowBoundsCollisionLogMs = nowBc;
        }
    }

    // Note: the world.boundsMin.z global floor clamp has been intentionally
    // removed.  world.boundsMin is map metadata, not collision geometry.
    // Players below bounds must continue falling naturally until the
    // void-death threshold.  Actual collision triangles handle platforms.
}

// ═══ Player‑to‑player de‑penetration ═══════════════════════════════
// TEMPORARY: active client‑controlled players use validated client‑transform
// authority.  Direct server positional pushes conflict with that model
// because the clients have not applied the same push.
// This function is preserved for future NPC or server‑controlled entities.
void resolvePlayerCollision(std::unordered_map<uint32_t, ServerPlayer>& players)
{
    // Client-transform authority: do not silently shift accepted player positions.
    (void)players;
}

// ── Initial inventory: temporary — grants non-restricted weapons ─────
// Documented as: temporary until a proper inventory/ownership system exists.
// Excludes weapons with `restricted=true` (admin_revolver, op_revolver).
static void getInitialInventory(std::vector<std::string>& out)
{
    out.clear();
    for (const auto& kv : WeaponRegistry::instance().all())
    {
        if (!kv.second.restricted)
            out.push_back(kv.first);
    }
}

// ── Centralized spawn/respawn reset ──────────────────────────────────
void resetPlayerForSpawn(ServerPlayer& player, bool isInitialSpawn)
{
    // Health and death
    player.dead = false;
    player.health = 100;

    // Increment spawn generation (never decremented, never reset)
    ++player.spawnGeneration;

    // Build or preserve owned weapon inventory
    if (isInitialSpawn)
        getInitialInventory(player.ownedWeaponIds);

    // Reset weapon runtimes for every owned weapon
    for (const std::string& wepId : player.ownedWeaponIds)
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(wepId);
        if (!def)
            continue;

        ServerPlayer::ServerWeaponRuntime rt;
        rt.magazineAmmo = def->magazineSize;
        rt.reserveAmmo = initialReserveAmmoForDefinition(*def);
        rt.nextAllowedFireTick = 0;  // cooldown cleared — new life may fire immediately
        rt.reloading = false;
        rt.reloadCompleteTick = 0;
        rt.stateRevision = 0;
        rt.initialized = true;
        player.weaponRuntimes[wepId] = rt;
    }

    // Clear transient combat/reload state
    player.projectileFireCooldown = 0.0f;
    player.nextProjectileFireTick = 0;
    player.lastShotSerial = 0;
    player.lastProjectileFireSerial = 0;
    player.lastMeleeAttackSerial = 0;
    player.swordswordState = SwordswordState{};
    player.meleeCooldownTimer = 0.0f;
    player.godballActive = false;
    player.godballX = player.godballY = player.godballZ = 0.0f;
    player.hasLastPhysicalWeaponShape = false;
    player.lastPhysicalWeaponDefNetworkId = 0;
    player.nextPhysicalContactSerial = 1;
    player.physicalContactEpisodes.clear();
    player.input = {};
    player.inputStateFlags = 0;
    player.attackQueued = false;
    player.clientStateUpdated = false;
    player.dashAvailable = true;
    player.onGround = false;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    // Hard-reset broadcast interpolation so a fresh life starts at the spawn
    // position instead of lerping from the previous life.
    player.hasBroadcastTransform = false;
    player.hasInterpSegment = false;
    player.interpFromPos = player.pos;
    player.interpToPos = player.pos;
    player.interpToTick = 0;
    player.interpDurationTicks = 2;
    player.interpSegmentStartTick = 0;
    player.hasSimBroadcastPos = false;
    player.simBroadcastPos = player.pos;

    printf("[SERVER SPAWN RESET] playerId=%u spawnGeneration=%u ownedWeapons=%zu"
           " isInitialSpawn=%d\n",
           player.id, player.spawnGeneration, player.ownedWeaponIds.size(),
           (int)isInitialSpawn);
}

// ── Complete authoritative spawn and notify client ────────────────────
// Called from every spawn path: initial spawn, auto-respawn, instant-respawn.
// Sends PlayerRespawnedPacket with authoritative generation and inventory.
void completeAuthoritativeSpawn(SOCKET sock, ServerPlayer& player, bool isInitialSpawn)
{
    resetPlayerForSpawn(player, isInitialSpawn);
    player.spawnState = ServerPlayer::AwaitingSpawnAck;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    PlayerRespawnedPacket spawnSync{};
    spawnSync.header.type = PACKET_PLAYER_RESPAWNED;
    spawnSync.header.tick = 0;
    spawnSync.header.playerId = player.id;
    spawnSync.spawnGeneration = player.spawnGeneration;
    spawnSync.transformEpoch = player.transformEpoch;
    spawnSync.health = player.health;
    spawnSync.weaponCount = 0;
    for (const auto& wkv : player.weaponRuntimes)
    {
        if (spawnSync.weaponCount >= 16) break;
        uint16_t wid = weaponDefNetworkIdFor(wkv.first);
        if (wid == 0) continue;
        auto& ws = spawnSync.weapons[spawnSync.weaponCount++];
        ws.weaponDefNetworkId = wid;
        ws.magazineAmmo = wkv.second.magazineAmmo;
        ws.reserveAmmo = wkv.second.reserveAmmo;
        ws.nextAllowedFireTick = wkv.second.nextAllowedFireTick;
        ws.stateRevision = wkv.second.stateRevision;
        ws.reloading = wkv.second.reloading ? 1 : 0;
    }

    // Send to this player only
    if (player.transport)
        player.transport->send(&spawnSync, sizeof(spawnSync));
    else
        sendto(sock, (const char*)&spawnSync, sizeof(spawnSync), 0,
               (sockaddr*)&player.addr, sizeof(player.addr));

    printf("[SPAWN TX CREATE] id=%u spawnGen=%u epoch=%u reason=%s health=%d ownedWeapons=%zu\n",
           player.id, player.spawnGeneration, player.transformEpoch,
           isInitialSpawn ? "initial" : "respawn", player.health, player.ownedWeaponIds.size());
}

// ── Retransmit SpawnConfirmed while awaiting ack ─────────────────────
void retrySpawnSync(SOCKET sock, ServerPlayer& player)
{
    if (player.spawnState != ServerPlayer::AwaitingSpawnAck)
        return;

    // Rebuild and resend the identical spawn transaction
    PlayerRespawnedPacket spawnSync{};
    spawnSync.header.type = PACKET_PLAYER_RESPAWNED;
    spawnSync.header.tick = 0;
    spawnSync.header.playerId = player.id;
    spawnSync.spawnGeneration = player.spawnGeneration;
    spawnSync.transformEpoch = player.transformEpoch;
    spawnSync.health = player.health;
    spawnSync.weaponCount = 0;
    for (const auto& wkv : player.weaponRuntimes)
    {
        if (spawnSync.weaponCount >= 16) break;
        uint16_t wid = weaponDefNetworkIdFor(wkv.first);
        if (wid == 0) continue;
        auto& ws = spawnSync.weapons[spawnSync.weaponCount++];
        ws.weaponDefNetworkId = wid;
        ws.magazineAmmo = wkv.second.magazineAmmo;
        ws.reserveAmmo = wkv.second.reserveAmmo;
        ws.nextAllowedFireTick = wkv.second.nextAllowedFireTick;
        ws.stateRevision = wkv.second.stateRevision;
        ws.reloading = wkv.second.reloading ? 1 : 0;
    }

    if (player.transport)
        player.transport->send(&spawnSync, sizeof(spawnSync));
    else
        sendto(sock, (const char*)&spawnSync, sizeof(spawnSync), 0,
               (sockaddr*)&player.addr, sizeof(player.addr));

    Debug::log(Debug::Category::Weapons, "[SPAWN TX RETRY] id=%u spawnGen=%u epoch=%u\n",
               player.id, player.spawnGeneration, player.transformEpoch);
}

// ── Advance all reload timers — called once per server tick ─────────
void tickWeaponRuntimes(std::unordered_map<uint32_t, ServerPlayer>& players, uint32_t currentTick)
{
    for (auto& kv : players)
    {
        ServerPlayer& p = kv.second;
        for (auto& rtKv : p.weaponRuntimes)
        {
            ServerPlayer::ServerWeaponRuntime& rt = rtKv.second;
            if (!rt.initialized || !rt.reloading)
                continue;
            if (currentTick >= rt.reloadCompleteTick)
            {
                const WeaponDefinition* def = WeaponRegistry::instance().get(rtKv.first);
                if (!def)
                {
                    rt.reloading = false;
                    continue;
                }

                bool oneAtATime = false;
                auto cit = def->customParams.find("reloadOneAtATime");
                if (cit != def->customParams.end()) oneAtATime = (cit->second != 0.0f);

                if (oneAtATime)
                {
                    // Load +1 shell, schedule next if more needed
                    if (rt.magazineAmmo < def->magazineSize && rt.reserveAmmo > 0)
                    {
                        rt.magazineAmmo++;
                        rt.reserveAmmo--;
                        rt.stateRevision++;
                    }
                    if (rt.magazineAmmo < def->magazineSize && rt.reserveAmmo > 0)
                    {
                        float reloadInterval = def->customParams.count("reloadTimePerShell")
                            ? def->customParams.at("reloadTimePerShell") : (def->reloadTime > 0 ? def->reloadTime : 0.55f);
                        rt.reloadCompleteTick = currentTick + (uint32_t)std::ceil(reloadInterval * 60.0f);
                    }
                    else
                    {
                        rt.reloading = false;
                    }
                }
                else
                {
                    // Bulk reload — fill all missing shells at once
                    int needed = def->magazineSize - rt.magazineAmmo;
                    int loaded = std::min(needed, rt.reserveAmmo);
                    rt.magazineAmmo += loaded;
                    rt.reserveAmmo -= loaded;
                    rt.stateRevision++;
                    rt.reloading = false;
                }
            }
        }
    }
}

void simulatePlayer(ServerPlayer& p, const HeadlessWorld& world)
{
    // Apply input yaw BEFORE any non-dead early return.
    // Orientation comes from current input and must update every frame,
    // even when clientStateUpdated causes an early return.
    p.yaw = p.input.yaw;

    {
        static uint64_t lastLookApplyLogMs = 0;
        uint64_t nowLookApply = nowMs();
        if (nowLookApply - lastLookApplyLogMs >= 1000)
        {
            printf("[LOOK SERVER APPLY] playerId=%u inputYaw=%.2f oldYaw=%.2f newYaw=%.2f "
                   "clientStateUpdated=%d\n",
                   p.id, p.input.yaw, p.yaw, p.input.yaw, (int)p.clientStateUpdated);
            lastLookApplyLogMs = nowLookApply;
        }
    }

    if (p.dead)
    {
        p.vel = glm::vec3(0.0f);
        p.movement.externalImpulse = glm::vec3(0.0f);
        syncServerMovementRuntime(p, false);
        p.projectileFireCooldown = std::max(0.0f, p.projectileFireCooldown - SERVER_DT);
        // Instant respawn request from client Space press
        if (p.instantRespawnRequested)
        {
            p.instantRespawnRequested = false;
            p.respawnSeconds = 0.0f;
            printf("%s [SERVER RESPAWN INSTANT] playerId=%u trigger=instantRespawnRequested\n",
                   serverTimestamp(), p.id);
        }
        p.respawnSeconds -= SERVER_DT;
        if (p.respawnSeconds <= 0.0f)
        {
            glm::vec3 respawnPos;
            float respawnYaw = 0.0f;
            if (!world.spawnPoints.empty())
            {
                size_t idx = (p.id - 1) % world.spawnPoints.size();
                respawnPos = world.spawnPoints[idx].position;
                respawnYaw = world.spawnPoints[idx].yaw;
            }
            else
            {
                respawnPos = {1.0f + (float)(p.id - 1) * 1.5f, 5.0f, 30.0f};
            }
            beginAuthoritativeTransform(p, respawnPos, glm::vec3(0.0f), respawnYaw, "respawn");
            // resetPlayerForSpawn is called by completeAuthoritativeSpawn
            // which is triggered by justRespawned flag in the server pump.
            p.justRespawned = true;  // signal caller to send spawn sync
            p.input = {};
            p.attackQueued = false;
            p.dashAvailable = true;
            p.onGround = false;
            p.respawnSeconds = 0.0f;
            syncServerMovementRuntime(p, false);
            printf("%s [SERVER RESPAWN] playerId=%u position=(%.2f,%.2f,%.2f) epoch=%u spawnpoints=%zu spawnGeneration=%u health=%d\n",
                   serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z, (unsigned)p.transformEpoch,
                   world.spawnPoints.size(), p.spawnGeneration, p.health);
        }
        return;
    }

    // If the lifecycle/transform handshake is not complete, freeze movement.
    if (p.spawnState != ServerPlayer::Active ||
        p.awaitingAuthoritativeTransformAck)
    {
        p.vel = glm::vec3(0.0f);
        p.movement.externalImpulse = glm::vec3(0.0f);
        p.clientStateUpdated = false;
        syncServerMovementRuntime(
            p, p.spawnState == ServerPlayer::Active);
        return;
    }

    p.projectileFireCooldown = std::max(0.0f, p.projectileFireCooldown - SERVER_DT);

    // ── Server-side movement simulation from input commands (spec) ─────
    // The server simulates movement using the SAME kernel as the client.
    // Input commands are replayed in CHRONOLOGICAL order (lowest sequence
    // first), so the simulated path follows the client's exact intended
    // trajectory. Consuming the newest command first instead would replay a
    // burst of commands in reverse order, kinking the simulated position and
    // making the broadcast stream jittery (visible as jitter to other clients).
    {
        // Find the oldest valid input command (FIFO replay). Redundant command
        // slots can insert out of ring order, so scan all slots for the lowest
        // sequence instead of relying on ring position.
        MovementCommand cmd;
        bool hasInput = false;
        uint32_t lowestSeq = 0;
        int bestIndex = -1;
        for (int i = 0; i < ServerPlayer::INPUT_COMMAND_BUFFER_SIZE; ++i)
        {
            const auto& entry = p.inputCommandBuffer[i];
            if (!entry.valid)
                continue;
            if (bestIndex < 0 || entry.command.sequence < lowestSeq)
            {
                lowestSeq = entry.command.sequence;
                bestIndex = i;
            }
        }
        if (bestIndex >= 0)
        {
            auto& entry = p.inputCommandBuffer[bestIndex];
            cmd = entry.command;
            hasInput = true;
            p.lastProcessedInputCommandSequence = cmd.sequence;
            entry.valid = false;
        }
        if (!hasInput)
        {
            cmd = movementCommandFromServerInput(
                p.input, p.lastMovementSequence,
                MovementLifecycleIdentity{p.spawnGeneration, p.transformEpoch});
        }

        const MovementConfig cfg = makeCurrentRuntimeMovementConfig();
        MovementState state = movementStateFromServerPlayer(p);

        // Phase 1: Pre-collision movement (gravity, walk, jump, dash)
        applyPreCollisionBasicMovement(state, cmd, cfg, SERVER_DT);
        MovementStepEvents preEvents;
        applySpecialMovementPreCollision(state, cmd, cfg, SERVER_DT, preEvents);
        applyMovementStateToServerPlayer(state, p);

        // Phase 2: World collision resolve
        resolveWorldCollision(p, world);

        // Phase 3: Build collision feedback and run post-collision movement
        state = movementStateFromServerPlayer(p);
        MovementCollisionFeedback collision;
        collision.onGround = p.onGround;
        collision.hasWorldContact = p.onGround;
        collision.realWorldContactThisFrame = p.onGround;
        collision.groundNormal = {0.0f, 0.0f, 1.0f};
        collision.simulationTick = cmd.clientSimulationTick;

        MovementStepResult stepResult = applyPostCollisionMovementWithSpecials(
            state, cmd, cfg, collision, SERVER_DT, preEvents);
        applyMovementStateToServerPlayer(stepResult.state, p);
        p.clientStateUpdated = false;

        {
            static uint64_t lastDivergenceLogMs = 0;
            uint64_t nowDiv = nowMs();
            const float simDivergence = glm::length(p.pos - p.lastAcceptedClientPosition);
            if (simDivergence > 2.0f && nowDiv - lastDivergenceLogMs >= 1000)
            {
                lastDivergenceLogMs = nowDiv;
                Debug::warn(Debug::Category::Networking,
                    "[SERVER SIM DIVERGENCE] playerId=%u simPos=(%.1f,%.1f,%.1f) "
                    "acceptedPos=(%.1f,%.1f,%.1f) distance=%.1f seq=%u tick=%llu\n",
                    p.id,
                    p.pos.x, p.pos.y, p.pos.z,
                    p.lastAcceptedClientPosition.x,
                    p.lastAcceptedClientPosition.y,
                    p.lastAcceptedClientPosition.z,
                    simDivergence, cmd.sequence,
                    (unsigned long long)cmd.clientSimulationTick);
            }
        }

        if (DebugConfig::DEBUG_MOVEMENT_SIM)
        {
            Debug::log(Debug::Category::Networking,
                "[SERVER MOVE SIM] playerId=%u seq=%u tick=%llu "
                "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) ground=%d\n",
                p.id, cmd.sequence,
                (unsigned long long)cmd.clientSimulationTick,
                p.pos.x, p.pos.y, p.pos.z,
                p.vel.x, p.vel.y, p.vel.z,
                (int)p.onGround);
        }
    }
    syncServerMovementRuntime(p, true);
}

void pushPositionHistory(ServerPlayer& p, uint32_t tick)
{
    const auto& netCfg = NetworkingConfig::instance().data();
    // Record the position actually broadcast to clients (the smoothed
    // interpolated one), so hit rewind reads exactly what attackers saw.
    // In server_sim broadcast mode that is the authoritative simulation pos,
    // optionally smoothed via server_sim_smooth_ticks.
    const bool serverSimBroadcast =
        netCfg.remotePlayers.broadcastSource == "server_sim";

    // Server-sim broadcast smoothing: ease the broadcast position toward the
    // authoritative sim position so the snapshot stream is clean (removes the
    // tiny per-tick resting jitter from the collision solver). Real movement
    // passes through with ~server_sim_smooth_ticks of easing (negligible lag).
    const uint32_t smoothTicks = netCfg.remotePlayers.serverSimSmoothTicks;
    glm::vec3 simBroadcast = p.pos;
    if (serverSimBroadcast)
    {
        if (!p.hasSimBroadcastPos)
        {
            p.simBroadcastPos = p.pos;
            p.hasSimBroadcastPos = true;
        }
        if (smoothTicks > 0)
        {
            const float k = 1.0f / (float)(smoothTicks + 1);
            p.simBroadcastPos += (p.pos - p.simBroadcastPos) * k;
        }
        else
        {
            p.simBroadcastPos = p.pos;
        }
        simBroadcast = p.simBroadcastPos;
    }

    const glm::vec3 histPos = serverSimBroadcast ? simBroadcast
        : (p.hasBroadcastTransform ? p.broadcastPosition : p.pos);
    const glm::vec3 histVel = serverSimBroadcast
        ? (p.hasAcceptedClientTransform ? p.lastAcceptedClientVelocity : p.vel)
        : (p.hasBroadcastTransform ? p.broadcastVelocity : p.vel);
    p.posHistory.push_back({histPos, histVel, tick});
    const std::size_t historyLimit = NetworkingConfig::instance()
        .data().bufferLimits.serverPositionHistoryTicks;
    while (p.posHistory.size() > historyLimit)
        p.posHistory.pop_front();
}

bool getPositionAtTick(const ServerPlayer& p, uint32_t targetTick, glm::vec3& outPos)
{
    if (p.posHistory.empty())
        return false;
    if (targetTick >= p.posHistory.back().tick)
    {
        outPos = p.posHistory.back().pos;
        return true;
    }
    if (targetTick <= p.posHistory.front().tick)
    {
        outPos = p.posHistory.front().pos;
        return true;
    }
    for (int i = (int)p.posHistory.size() - 1; i > 0; --i)
    {
        if (p.posHistory[i].tick == targetTick)
        {
            outPos = p.posHistory[i].pos;
            return true;
        }
        if (p.posHistory[i].tick < targetTick)
        {
            const auto& a = p.posHistory[i];
            const auto& b = p.posHistory[i + 1];
            float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = p.posHistory.front().pos;
    return true;
}

void beginServerBroadcastInterp(ServerPlayer& player, uint32_t serverTick)
{
    if (!player.hasAcceptedClientTransform)
        return;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

    // When smoothing is off the broadcast is simply the newest accepted report
    // — coherent with the animation state carried in the same snapshot.
    if (!interpCfg.serverSmoothing)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        player.hasInterpSegment = false;
        player.broadcastSamples.clear();
        player.broadcastSamplesSeeded = false;
        return;
    }

    // Lifecycle reset (respawn / teleport / map change): drop all history so
    // the broadcast snaps to the new life instead of smearing across it.
    if (!player.broadcastSamplesSeeded)
    {
        player.broadcastSamplesSeeded = true;
        player.broadcastSamplesSpawnGeneration = player.spawnGeneration;
        player.broadcastSamplesEpoch = player.transformEpoch;
    }
    else if (player.spawnGeneration != player.broadcastSamplesSpawnGeneration ||
             player.transformEpoch != player.broadcastSamplesEpoch)
    {
        player.broadcastSamples.clear();
        player.broadcastSamplesSpawnGeneration = player.spawnGeneration;
        player.broadcastSamplesEpoch = player.transformEpoch;
    }

    // Buffer the accepted report. Interpolation is keyed by the client's own
    // movement tick (uniform 60Hz), not by server arrival time, so reordered
    // and bursty arrivals render as uniform motion.
    const uint32_t acceptedClientTick =
        player.movementValidation.lastAcceptedClientTick;
    player.broadcastSamples.push_back({
        nowMs(), acceptedClientTick,
        player.lastAcceptedClientPosition, player.lastAcceptedClientVelocity});
    const std::size_t sampleLimit = NetworkingConfig::instance()
        .data().bufferLimits.serverBroadcastSampleLimit;
    while (player.broadcastSamples.size() > sampleLimit)
        player.broadcastSamples.pop_front();

    // Keep the fixed-window lerp fields fresh as the fallback path used when
    // serverBroadcastDelaySeconds == 0.
    const uint32_t newTick = acceptedClientTick;
    if (player.hasInterpSegment)
        player.interpFromPos = player.broadcastPosition;
    else
        player.interpFromPos = player.lastAcceptedClientPosition;
    player.interpDurationTicks = std::max(1u, interpCfg.serverSmoothingDurationTicks);
    player.interpToPos = player.lastAcceptedClientPosition;
    player.interpToTick = newTick;
    player.interpSegmentStartTick = serverTick;
    player.hasInterpSegment = true;
    player.broadcastVelocity = player.lastAcceptedClientVelocity;
    player.hasBroadcastTransform = true;
}

void updateServerBroadcastInterp(ServerPlayer& player, uint32_t serverTick)
{
    if (!player.hasAcceptedClientTransform)
        return;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

    if (!interpCfg.serverSmoothing)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        return;
    }

    // ── Client-tick broadcast rendering (primary path) ───────────────
    // Render the broadcast at `newestClientTick - delayTicks`, lerping between
    // the two accepted reports whose client movement ticks bracket that
    // instant. Client ticks are uniform 60Hz, so even reordered/bursty
    // arrivals produce uniform motion — no arrival-cluster "flash-then-hold".
    if (interpCfg.serverBroadcastDelaySeconds > 0.0 &&
        player.broadcastSamples.size() >= 2)
    {
        const uint32_t newestClientTick = player.broadcastSamples.back().clientTick;
        const double delayTicks =
            interpCfg.serverBroadcastDelaySeconds * (double)GAMEPLAY_SIMULATION_HZ;
        const double targetTick = (double)newestClientTick - delayTicks;

        // Optional speed clamp: cap how far the broadcast may move per tick so
        // a burst report cannot make the body lurch even through the buffer.
        // 0 = unlimited.
        const float maxDelta = interpCfg.serverBroadcastMaxSpeed > 0.0
            ? (float)(interpCfg.serverBroadcastMaxSpeed * (double)SERVER_DT)
            : 0.0f;
        const auto clampDelta = [&](const glm::vec3& target) {
            if (maxDelta <= 0.0f)
            {
                player.broadcastPosition = target;
                return;
            }
            const glm::vec3 delta = target - player.broadcastPosition;
            const float len = glm::length(delta);
            player.broadcastPosition = (len > maxDelta)
                ? player.broadcastPosition + delta * (maxDelta / len)
                : target;
        };

        const ServerPlayer::BroadcastSample* older = nullptr;
        const ServerPlayer::BroadcastSample* newer = nullptr;
        for (const auto& s : player.broadcastSamples)
        {
            if ((double)s.clientTick <= targetTick)
                older = &s;   // keep the latest one at/before targetTick
            else
            {
                newer = &s;   // first sample after targetTick
                break;
            }
        }

        if (older && newer)
        {
            double alpha = 1.0;
            if (newer->clientTick > older->clientTick)
            {
                alpha = (targetTick - (double)older->clientTick) /
                        (double)(newer->clientTick - older->clientTick);
                alpha = std::clamp(alpha, 0.0, 1.0);
            }
            clampDelta(older->position +
                (newer->position - older->position) * (float)alpha);
            player.broadcastVelocity = older->velocity +
                (newer->velocity - older->velocity) * (float)alpha;
            player.hasBroadcastTransform = true;
            return;
        }

        if (!older)
        {
            // targetTick older than everything buffered (fresh start): hold the
            // oldest sample until enough history accumulates.
            const auto& newest = player.broadcastSamples.front();
            clampDelta(newest.position);
            player.broadcastVelocity = newest.velocity;
            player.hasBroadcastTransform = true;
            return;
        }

        // Buffer ran dry (no fresh reports): extrapolate along the newest
        // velocity for a short wall-clock time, then hold.
        const auto& newest = player.broadcastSamples.back();
        const double dryTicks = targetTick - (double)newest.clientTick;
        const double dryMs = dryTicks / (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
        if (interpCfg.serverBroadcastExtrapolationSeconds > 0.0 && dryMs > 0.0)
        {
            const double extrapMs = std::min(
                dryMs, interpCfg.serverBroadcastExtrapolationSeconds * 1000.0);
            clampDelta(newest.position +
                newest.velocity * (float)(extrapMs / 1000.0));
            player.broadcastVelocity = newest.velocity;
            player.hasBroadcastTransform = true;
            return;
        }
        clampDelta(newest.position);
        player.broadcastVelocity = newest.velocity;
        player.hasBroadcastTransform = true;
        return;
    }

    // ── Fallback: fixed-window lerp (serverBroadcastDelaySeconds == 0) ──
    if (!player.hasInterpSegment)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        return;
    }

    const uint32_t duration = std::max(1u, player.interpDurationTicks);
    const uint32_t elapsed = serverTick >= player.interpSegmentStartTick
        ? (serverTick - player.interpSegmentStartTick)
        : 0u;
    const float alpha = elapsed >= duration
        ? 1.0f
        : (float)elapsed / (float)duration;
    player.broadcastPosition = player.interpFromPos +
        (player.interpToPos - player.interpFromPos) * alpha;
    player.broadcastVelocity = player.lastAcceptedClientVelocity;
    player.hasBroadcastTransform = true;
}

uint32_t estimateServerRewindTick(const ServerPlayer& attacker,
                                  uint32_t clientSimulationTick,
                                  uint32_t serverTick)
{
    // clientSimulationTick carries the newest server tick the attacker had
    // rendered (the client stamps it from the local-player snapshot tick), so
    // it is already in the server tick domain. Their view is that tick minus
    // the remote interpolation buffer.
    // rewind_compensation_ms adds an extra tunable offset (in server ticks)
    // so hits land on the body the shooter actually rendered under jitter and
    // server broadcast smoothing. Positive = rewind further back.
    const int64_t compTicks = (int64_t)std::llround(
        NetworkingConfig::instance().data().remotePlayers
            .rewindCompensationSeconds * (double)GAMEPLAY_SIMULATION_HZ);
    int64_t rewind;
    if (clientSimulationTick != 0)
        rewind = (int64_t)clientSimulationTick - (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks;
    else if (attacker.movementValidation.lastAcceptedClientTick != 0 &&
             attacker.lastAcceptedServerTick != 0)
        rewind = (int64_t)attacker.lastAcceptedServerTick -
                 (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks;
    else
        rewind = (int64_t)serverTick - (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks;
    rewind = std::clamp<int64_t>(rewind, 0, (int64_t)serverTick);
    return (uint32_t)rewind;
}

SnapshotEntity makePlayerEntity(const ServerPlayer& player)
{
    SnapshotEntity out{};
    out.networkEntityId = player.id;
    out.entityType = ENTITY_PLAYER;
    out.active = 1;
    out.ownerClientId = player.id;
    // server_sim broadcast source: stream the server's own 60Hz input-driven
    // simulation. Always smooth and uniform regardless of the player's network
    // quality (client reports arrive gappy under loss/jitter/reorder). The
    // broadcast position is the eased simBroadcastPos (see pushPositionHistory)
    // so the snapshot stream is clean for other clients' linear interpolation.
    // Velocity + grounded use the VALIDATED client report: the server's own
    // re-sim cannot detect ground contact for a resting client (collision
    // dimensions differ) so gravity accumulates a bogus negative vz each tick —
    // broadcasting that makes observers extrapolate bodies into the floor.
    const bool serverSimBroadcast =
        NetworkingConfig::instance().data().remotePlayers.broadcastSource ==
        "server_sim";
    if (serverSimBroadcast)
    {
        const glm::vec3 bp = player.hasSimBroadcastPos
            ? player.simBroadcastPos : player.pos;
        out.px = bp.x;
        out.py = bp.y;
        out.pz = bp.z;
        const glm::vec3 bv = player.hasAcceptedClientTransform
            ? player.lastAcceptedClientVelocity : player.vel;
        out.vx = bv.x;
        out.vy = bv.y;
        out.vz = bv.z;
    }
    else if (player.hasBroadcastTransform)
    {
        out.px = player.broadcastPosition.x;
        out.py = player.broadcastPosition.y;
        out.pz = player.broadcastPosition.z;
        out.vx = player.broadcastVelocity.x;
        out.vy = player.broadcastVelocity.y;
        out.vz = player.broadcastVelocity.z;
    }
    else if (player.hasAcceptedClientTransform)
    {
        out.px = player.lastAcceptedClientPosition.x;
        out.py = player.lastAcceptedClientPosition.y;
        out.pz = player.lastAcceptedClientPosition.z;
        out.vx = player.lastAcceptedClientVelocity.x;
        out.vy = player.lastAcceptedClientVelocity.y;
        out.vz = player.lastAcceptedClientVelocity.z;
    }
    else
    {
        out.px = player.pos.x; out.py = player.pos.y; out.pz = player.pos.z;
        out.vx = player.vel.x; out.vy = player.vel.y; out.vz = player.vel.z;
    }
    out.yaw = player.yaw;
    out.health = player.health;
    out.onGround = (serverSimBroadcast ? player.lastAcceptedClientOnGround : player.onGround) ? 1 : 0;
    out.equippedSlot = (int16_t)player.equippedSlot;
    out.weaponState = player.weaponState;
    out.transformEpoch = player.transformEpoch;
    out.aimX = player.input.camForward.x;
    out.aimY = player.input.camForward.y;
    out.aimZ = player.input.camForward.z;
    out.pingMs = player.pingMs;
    out.sizeScale = player.sizeScale;
    out.spawnGeneration = player.spawnGeneration;

    {
        static uint64_t lastLookSnapshotLogMs = 0;
        uint64_t nowLookSnap = nowMs();
        if (nowLookSnap - lastLookSnapshotLogMs >= 1000)
        {
            printf("[LOOK SNAPSHOT BUILD] playerId=%u yaw=%.2f aim=(%.2f,%.2f,%.2f) "
                   "pos=(%.2f,%.2f,%.2f)\n",
                   player.id, player.yaw,
                   player.input.camForward.x, player.input.camForward.y, player.input.camForward.z,
                   player.pos.x, player.pos.y, player.pos.z);
            lastLookSnapshotLogMs = nowLookSnap;
        }
    }

    // Build snapshot state flags from client input state + server authoritative state.
    // Use client-validated visual flags for cosmetic animation replication
    // so that walking animation works even when server onGround is temporarily wrong.
    {
        constexpr uint16_t CLIENT_VISUAL_FLAGS =
            NET_STATE_WALKING | NET_STATE_JUMPING |
            NET_STATE_DASHING | NET_STATE_DOWN_DASHING |
            NET_STATE_FREEZING | NET_STATE_ATTACKING;

        uint16_t flags = player.inputStateFlags & CLIENT_VISUAL_FLAGS;

        // Server retains authority over ON_GROUND (in server_sim broadcast the
        // server's own ground detection is broken for resting clients, so fall
        // back to the validated client-reported grounded flag).
        const bool broadcastOnGround = serverSimBroadcast
            ? player.lastAcceptedClientOnGround : player.onGround;
        if (broadcastOnGround)
            flags |= NET_STATE_ON_GROUND;
        else
            flags &= ~NET_STATE_ON_GROUND;

        out.stateFlags = flags;

        static uint64_t lastWalkBuildLogMs = 0;
        uint64_t nowWalkBuild = nowMs();
        if (nowWalkBuild - lastWalkBuildLogMs >= 1000)
        {
            printf("[WALK SERVER BUILD] playerId=%u inputStateFlags=0x%04x "
                   "snapshotStateFlags=0x%04x walkingInputBit=%d "
                   "serverOnGround=%d planarSpeed=%.2f\n",
                   player.id, (unsigned)player.inputStateFlags,
                   (unsigned)out.stateFlags,
                   (int)((out.stateFlags & NET_STATE_WALKING) != 0),
                   (int)player.onGround,
                   glm::length(glm::vec2(player.vel.x, player.vel.y)));
            lastWalkBuildLogMs = nowWalkBuild;
        }
    }
    // Emit presentation serials (replicated from client, pass through unchanged).
    // These are the canonical event-ordered serials for remote VFX/anim trigger.
    out.dashSerial = player.lastPresentationDashSerial;
    out.groundJumpSerial = player.lastPresentationGroundJumpSerial;
    out.airJumpSerial = player.lastPresentationAirJumpSerial;
    out.downDashSerial = player.lastPresentationDownDashSerial;
    out.directionChangeSerial = player.lastPresentationDirectionChangeSerial;
    out.equipSerial = player.lastEquipSerial;
    out.freezeSerial = player.lastPresentationFreezeSerial;
    copyName(out.displayName, player.name);
    MimitaVip::copyAppearanceToBytes(
        player.vipAppearance, out.vipTier, out.vipStyleKind,
        out.vipColorR, out.vipColorG, out.vipColorB, out.vipFlags);
    out.vipStyleEpoch = (uint8_t)std::min<uint32_t>(player.vipStyleEpoch, 255);
    return out;
}

} // namespace MimitaNet
