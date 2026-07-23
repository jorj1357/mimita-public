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
                // Add one shell
                if (rt.magazineAmmo < def->magazineSize && rt.reserveAmmo > 0)
                {
                    rt.magazineAmmo++;
                    rt.reserveAmmo--;
                    rt.stateRevision++;
                }
                // Schedule next shell or finish
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
    // Input is taken from the most recent received input command.
    {
        // Find the most recent valid input command
        MovementCommand cmd;
        bool hasInput = false;
        for (int i = ServerPlayer::INPUT_COMMAND_BUFFER_SIZE - 1; i >= 0; --i)
        {
            auto& entry = p.inputCommandBuffer[i];
            if (entry.valid)
            {
                cmd = entry.command;
                hasInput = true;
                p.lastProcessedInputCommandSequence = cmd.sequence;
                entry.valid = false;
                break;
            }
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
    p.posHistory.push_back({p.pos, p.vel, tick});
    while (p.posHistory.size() > 30)
        p.posHistory.pop_front();
}

bool getPositionAtTick(const ServerPlayer& p, uint32_t targetTick, glm::vec3& outPos)
{
    if (p.posHistory.empty())
        return false;
    if (targetTick >= p.posHistory.back().tick)
    {
        outPos = p.pos;
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

SnapshotEntity makePlayerEntity(const ServerPlayer& player)
{
    SnapshotEntity out{};
    out.networkEntityId = player.id;
    out.entityType = ENTITY_PLAYER;
    out.active = 1;
    out.ownerClientId = player.id;
    if (player.hasAcceptedClientTransform)
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
    out.onGround = player.onGround ? 1 : 0;
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

        // Server retains authority over ON_GROUND
        if (player.onGround)
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
    return out;
}

} // namespace MimitaNet
