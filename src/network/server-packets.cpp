// 07 21 2026, 17 10
/* purpose
* Owns server packet handlers for joins, input, lifecycle requests, and small gameplay commands.
* Converts client movement packets into validated server movement reports.
* Keeps packet parsing close to UDP and ICE transport dispatch code.
* Does NOT own the server tick loop, render clients, or define packet schemas.
* Does NOT simulate shared movement formulas or weapon projectile internals.
* Does NOT bypass lifecycle, ownership, or movement validation before mutating players.
*/

#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "network/snapshot-chunks.h"
#include "network/network-weapons.h"
#include "physics/movement/movement-conversion.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "debug/debug-log.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <random>
#include <unordered_map>

namespace MimitaNet {
namespace {

uint16_t validClientVisualStateFlags(uint16_t flags)
{
    constexpr uint16_t kValid =
        NET_STATE_WALKING |
        NET_STATE_JUMPING |
        NET_STATE_DASHING |
        NET_STATE_DOWN_DASHING |
        NET_STATE_FREEZING |
        NET_STATE_ATTACKING;
    return flags & kValid;
}

const WeaponDefinition* weaponDefinitionForSlot(int slot)
{
    if (slot <= 0)
        return nullptr;
    for (const auto& kv : WeaponRegistry::instance().all())
    {
        if (kv.second.slot == slot)
            return &kv.second;
    }
    return nullptr;
}

bool playerCanEquipSlot(const ServerPlayer& player, int slot)
{
    if (slot == 0)
        return true;
    const WeaponDefinition* def = weaponDefinitionForSlot(slot);
    if (!def)
        return false;
    auto rt = player.weaponRuntimes.find(def->id);
    return rt != player.weaponRuntimes.end() && rt->second.initialized;
}

bool applyEquipIntentFromInput(ServerPlayer& player,
                               const InputPacket& input,
                               const char* source)
{
    const bool serialChanged =
        input.equipSerial != 0 && input.equipSerial != player.lastEquipSerial;
    const bool initialUnsyncedSlot =
        input.equipSerial == 0 &&
        player.lastEquipSerial == 0 &&
        player.equippedSlot == 0 &&
        input.equippedSlot != 0;
    const bool slotChanged =
        input.equippedSlot != player.equippedSlot &&
        (serialChanged || initialUnsyncedSlot);
    if (!serialChanged && !slotChanged)
        return false;

    if (!playerCanEquipSlot(player, input.equippedSlot))
    {
        Debug::logThrottled(
            Debug::Category::Weapons,
            "server-equip-invalid-slot",
            0.5f,
            "[SERVER EQUIP REJECT] playerId=%u requestedSlot=%d "
            "equipSerial=%u source=%s reason=not-owned-or-unknown\n",
            player.id,
            input.equippedSlot,
            input.equipSerial,
            source);
        return false;
    }

    const int oldSlot = player.equippedSlot;
    if (input.equipSerial != 0)
        player.lastEquipSerial = input.equipSerial;
    player.equippedSlot = input.equippedSlot;
    player.weaponState = input.weaponState;
    Debug::log(
        Debug::Category::Weapons,
        "[SERVER EQUIP ACCEPT] playerId=%u oldSlot=%d newSlot=%d "
        "equipSerial=%u source=%s\n",
        player.id,
        oldSlot,
        player.equippedSlot,
        input.equipSerial,
        source);
    return true;
}

uint32_t movementReportFlagsFromInput(const InputPacket& input,
                                      const ServerPlayer& player)
{
    uint32_t flags = input.movementFlags;
    if (flags == 0)
    {
        if (player.onGround || player.movement.ground.onGround)
            flags |= MOVEMENT_REPORT_ON_GROUND |
                MOVEMENT_REPORT_STABLE_ON_GROUND |
                MOVEMENT_REPORT_HAS_WORLD_CONTACT;
        if (player.movement.ground.realWorldContactThisFrame)
            flags |= MOVEMENT_REPORT_REAL_WORLD_CONTACT;
        if (player.movement.jump.airJumpArmed)
            flags |= MOVEMENT_REPORT_AIR_JUMP_ARMED;
        if (player.movement.jump.airJumpLocked)
            flags |= MOVEMENT_REPORT_AIR_JUMP_LOCKED;
        if (player.movement.dash.dashAvailable || player.dashAvailable)
            flags |= MOVEMENT_REPORT_DASH_AVAILABLE;
        if (player.movement.dashMomentumProtection.active)
            flags |= MOVEMENT_REPORT_DASH_PROTECTED;
        if (player.movement.downDash.available)
            flags |= MOVEMENT_REPORT_DOWN_DASH_AVAILABLE;
        if (player.movement.freeze.active)
            flags |= MOVEMENT_REPORT_FREEZE_ACTIVE;
        if (player.movement.freeze.available)
            flags |= MOVEMENT_REPORT_FREEZE_AVAILABLE;
        if (player.movement.groundReturn.available)
            flags |= MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE;
    }

    if ((input.stateFlags & NET_STATE_JUMPING) != 0)
        flags |= MOVEMENT_REPORT_JUMP_HELD;
    if ((input.stateFlags & NET_STATE_DASHING) != 0)
        flags |= MOVEMENT_REPORT_DASH_PRESSED;
    if ((input.stateFlags & NET_STATE_DOWN_DASHING) != 0)
        flags |= MOVEMENT_REPORT_DOWN_DASH_PRESSED;
    if ((input.stateFlags & NET_STATE_FREEZING) != 0)
        flags |= MOVEMENT_REPORT_FREEZE_HELD;
    return flags;
}

ClientMovementReport movementReportFromInputPacket(const InputPacket& input,
                                                   const ServerPlayer& player)
{
    ClientMovementReport report;
    report.playerId = input.header.playerId;
    report.movementSequence = input.movementSequence;
    report.clientSimulationTick = input.clientSimulationTick != 0
        ? input.clientSimulationTick
        : input.header.tick;
    report.lifecycle.spawnGeneration = input.spawnGeneration;
    report.lifecycle.transformEpoch = input.transformEpoch != 0
        ? input.transformEpoch
        : input.header.transformEpoch;
    report.moveAxes = {input.wishX, input.wishY};
    report.horizontalCameraForward = {
        input.camForwardX,
        input.camForwardY,
        input.camForwardZ};
    report.position = {input.clientPx, input.clientPy, input.clientPz};
    report.baseVelocity = {input.clientVx, input.clientVy, input.clientVz};
    report.externalImpulse = {
        input.externalImpulseX,
        input.externalImpulseY,
        input.externalImpulseZ};
    report.yaw = input.yaw;
    report.lookPitch = input.lookPitch;
    report.sizeScale = input.sizeScale;
    report.movementFlags = movementReportFlagsFromInput(input, player);
    report.clientPingMs = input.clientPingMs;
    report.dashSerial = input.dashSerial;
    report.groundJumpSerial = input.groundJumpSerial;
    report.airJumpSerial = input.airJumpSerial;
    report.downDashSerial = input.downDashSerial;
    report.freezeSerial = input.freezeSerial;
    return report;
}

void applyAcceptedInputPresentation(ServerPlayer& player,
                                    const InputPacket& input,
                                    const ClientMovementReport& report)
{
    player.input.wish = movementClampUnitOrZero(report.moveAxes);
    player.input.camForward = report.horizontalCameraForward;
    if (!movementIsFinite(player.input.camForward) ||
        glm::length(player.input.camForward) <= 0.0001f)
    {
        player.input.camForward = {1.0f, 0.0f, 0.0f};
    }
    player.input.yaw = report.yaw;
    player.input.lookPitch = report.lookPitch;
    player.input.tick = static_cast<uint32_t>(report.clientSimulationTick);
    player.input.jumpHeld =
        (report.movementFlags & MOVEMENT_REPORT_JUMP_HELD) != 0;
    player.input.dashPressed =
        (report.movementFlags & MOVEMENT_REPORT_DASH_PRESSED) != 0;
    player.input.downDashPressed =
        (report.movementFlags & MOVEMENT_REPORT_DOWN_DASH_PRESSED) != 0;
    player.input.freezeHeld =
        (report.movementFlags & MOVEMENT_REPORT_FREEZE_HELD) != 0;

    player.weaponState = input.weaponState;
    applyEquipIntentFromInput(player, input, "accepted-input");
    player.pingMs = std::clamp(input.clientPingMs, 0, 9999);
    player.sizeScale = std::max(input.sizeScale, 0.001f);
    player.inputStateFlags = validClientVisualStateFlags(input.stateFlags);

    if (input.dashSerial != 0 && input.dashSerial != player.lastDashSerial)
    {
        player.lastDashSerial = input.dashSerial;
        player.input.dashPressed = true;
    }
    if (input.groundJumpSerial != 0 &&
        input.groundJumpSerial != player.lastPresentationGroundJumpSerial)
        player.lastPresentationGroundJumpSerial = input.groundJumpSerial;
    if (input.airJumpSerial != 0 &&
        input.airJumpSerial != player.lastPresentationAirJumpSerial)
        player.lastPresentationAirJumpSerial = input.airJumpSerial;
    if (input.downDashSerial != 0 &&
        input.downDashSerial != player.lastPresentationDownDashSerial)
        player.lastPresentationDownDashSerial = input.downDashSerial;
    if (input.freezeSerial != 0 &&
        input.freezeSerial != player.lastPresentationFreezeSerial)
        player.lastPresentationFreezeSerial = input.freezeSerial;
    if (input.directionChangeSerial != 0 &&
        input.directionChangeSerial != player.lastPresentationDirectionChangeSerial)
        player.lastPresentationDirectionChangeSerial = input.directionChangeSerial;
    if (input.dashSerial != 0 &&
        input.dashSerial != player.lastPresentationDashSerial)
        player.lastPresentationDashSerial = input.dashSerial;

    const bool attackPressed = input.attackPressed != 0;
    if (attackPressed && !player.input.attackPressed)
        player.attackQueued = true;
    player.input.attackPressed = attackPressed;
}

void logMovementValidation(const ServerPlayer& player,
                           const ClientMovementReport& report,
                           const MovementValidationResult& result)
{
    if (result.decision == MovementValidationDecision::Accept)
        return;

    const MovementCorrectionClass correctionClass =
        classifyMovementCorrection(result.metrics.positionError,
                                   makeMovementValidationConfig(
                                       makeCurrentRuntimeMovementConfig()));
    Debug::logThrottled(
        Debug::Category::Networking,
        "server-movement-validation",
        0.5f,
        "[SERVER MOVEMENT VALIDATION] player=%u decision=%s reason=%s "
        "class=%s seq=%u clientTick=%llu serverEpoch=%u reportEpoch=%u "
        "serverSpawn=%u reportSpawn=%u posError=%.2f hDelta=%.2f vDelta=%.2f "
        "allowH=%.2f allowV=%.2f baseSpeed=%.2f extH=%.2f combined=%.2f\n",
        player.id,
        movementValidationDecisionName(result.decision),
        movementValidationReasonName(result.reason),
        movementCorrectionClassName(correctionClass),
        report.movementSequence,
        (unsigned long long)report.clientSimulationTick,
        (unsigned)player.transformEpoch,
        (unsigned)report.lifecycle.transformEpoch,
        player.spawnGeneration,
        report.lifecycle.spawnGeneration,
        result.metrics.positionError,
        result.metrics.horizontalDelta,
        result.metrics.verticalDelta,
        result.metrics.allowedHorizontalDelta,
        result.metrics.allowedVerticalDelta,
        result.metrics.horizontalBaseSpeed,
        result.metrics.horizontalExternalImpulse,
        result.metrics.combinedSpeed);
}

} // namespace

// ── Global server code for coordinator validation ────────────────────
// Set by the server when it registers with the coordinator.
static std::string gServerCoordinatorCode;
static std::string gServerCoordinatorJoinToken;

// ── Global server map ID ─────────────────────────────────────────────
static std::string gServerMapId = "funworldv3";

void setServerCoordinatorState(const std::string& code, const std::string& joinToken)
{
    gServerCoordinatorCode = code;
    gServerCoordinatorJoinToken = joinToken;
}

const std::string& getServerCoordinatorCode() { return gServerCoordinatorCode; }
const std::string& getServerCoordinatorJoinToken() { return gServerCoordinatorJoinToken; }

void setServerMapId(const std::string& mapId) { gServerMapId = mapId; }
const std::string& getServerMapId() { return gServerMapId; }

const char* transportKindName(TransportKind kind)
{
    switch (kind)
    {
    case TransportKind::Udp: return "udp";
    case TransportKind::Ice: return "ice";
    case TransportKind::Relay: return "relay";
    }
    return "unknown";
}

TransportConnectionId makeUdpConnectionId(const sockaddr_in& endpoint)
{
    const uint64_t ip = static_cast<uint64_t>(ntohl(endpoint.sin_addr.s_addr));
    const uint64_t port = static_cast<uint64_t>(ntohs(endpoint.sin_port));
    return {TransportKind::Udp, (ip << 16) | port};
}

TransportConnectionId allocateIceConnectionId()
{
    static std::atomic<uint64_t> sNextIceConnectionId{1};
    return {TransportKind::Ice, sNextIceConnectionId.fetch_add(1)};
}

sockaddr_in legacyEndpointForTransportConnection(TransportConnectionId id)
{
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    if (id.kind == TransportKind::Udp)
        return endpoint;

    const uint32_t low = static_cast<uint32_t>(id.value & 0x00ffffffu);
    endpoint.sin_addr.s_addr = htonl(0x0a000000u | low);
    endpoint.sin_port = htons(static_cast<uint16_t>(1 + (id.value & 0x7fffu)));
    return endpoint;
}

bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

bool playerOwnsConnectionSource(const ServerPlayer& player,
                                const sockaddr_in* endpoint,
                                const TransportConnectionId* connectionId)
{
    if (connectionId && player.hasConnectionId)
        return player.connectionId == *connectionId;
    return endpoint == nullptr || sameAddress(player.addr, *endpoint);
}

static bool sendToSourceOrPlayer(SOCKET sock,
                                 const sockaddr_in& from,
                                 const ServerPlayer* player,
                                 std::unique_ptr<IGameTransport>* transport,
                                 const void* data,
                                 size_t size)
{
    if (player)
        return serverSendToPlayer(sock, *player, data, size);
    if (transport && transport->get())
        return (*transport)->send(data, size);
    if (sock == INVALID_SOCKET)
        return false;
    int sent = sendto(sock, (const char*)data, (int)size, 0,
                      (sockaddr*)&from, sizeof(from));
    return sent != SOCKET_ERROR;
}

static void bindPlayerConnection(ServerPlayer& player,
                                 const sockaddr_in& from,
                                 const TransportConnectionId* connectionId,
                                 std::unique_ptr<IGameTransport>* claimedTransport)
{
    player.addr = from;
    if (connectionId)
    {
        player.connectionId = *connectionId;
        player.hasConnectionId = true;
    }
    else
    {
        player.connectionId = makeUdpConnectionId(from);
        player.hasConnectionId = true;
    }

    if (claimedTransport && claimedTransport->get())
    {
        if (player.transport)
            player.transport->close();
        player.transport = std::move(*claimedTransport);
    }
}

static bool isKnownPacketType(uint8_t type)
{
    return type >= PACKET_HELLO && type <= PACKET_DAMAGE_CONFIRMED_EVENT;
}

static void countPacketType(ServerPacketStats& stats, uint8_t type)
{
    if (type == PACKET_HELLO)
        ++stats.helloPackets;
    else if (type == PACKET_JOIN_REQUEST)
        ++stats.joinPackets;
    else if (type == PACKET_RECONNECT_REQUEST)
        ++stats.reconnectPackets;
    else if (type == PACKET_INPUT)
        ++stats.inputPackets;
}

bool serverRayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                       const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                       float& outDist)
{
    glm::vec3 e1 = b - a;
    glm::vec3 e2 = c - a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    float inv = 1.0f / det;
    glm::vec3 t = origin - a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    outDist = glm::dot(e2, q) * inv;
    return outDist > 0.0f;
}

bool serverRaycastWorld(const glm::vec3& origin, const glm::vec3& direction,
                         float maxDist, const HeadlessWorld& world,
                         glm::vec3& outHitPos, glm::vec3& outNormal)
{
    float closest = maxDist;
    bool hit = false;
    for (const CollisionTriangle& tri : world.triangles)
    {
        float d;
        if (serverRayTriangle(origin, direction, tri.a, tri.b, tri.c, d) && d < closest)
        {
            closest = d;
            outNormal = tri.normal;
            hit = true;
        }
    }
    if (hit)
        outHitPos = origin + direction * closest;
    return hit;
}

void logSnapshotEntity(const SnapshotEntity& entity)
{
    printf("  entityId=%u type=%s ownerClientId=%u position=(%.2f,%.2f,%.2f) "
           "rotation=%.2f health=%d\n",
           entity.networkEntityId,
           entity.entityType == ENTITY_PLAYER ? "Player" : "NPC",
           entity.ownerClientId,
           entity.px, entity.py, entity.pz,
           entity.yaw, entity.health);
}

void handleHello(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                 std::unordered_map<uint32_t, ServerPlayer>& players,
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                 const HeadlessWorld* world,
                 const TransportConnectionId* connectionId,
                 std::unique_ptr<IGameTransport>* claimedTransport)
{
    if (bytes < (int)sizeof(HelloPacket))
        return;
    uint32_t existingId = 0;
    for (const auto& kv : players)
        if (playerOwnsConnectionSource(kv.second, &from, connectionId))
            existingId = kv.first;

    uint32_t id = existingId ? existingId : nextPlayerId++;
    ServerPlayer& p = players[id];
    p.id = id;
    bindPlayerConnection(p, from, connectionId, claimedTransport);
    p.lastHeardMs = nowMs();
    p.lastShotSerial = 0;
    p.lastProjectileFireSerial = 0;
    p.lastMeleeAttackSerial = 0;
    p.projectileFireCooldown = 0.0f;
    p.name = uniquePlayerName(
        players, reinterpret_cast<const HelloPacket*>(buffer)->name, id);

    if (!existingId)
    {
        // Use map spawnpoints if available
        glm::vec3 spawnPos;
        float spawnYaw = 0.0f;
        if (world && !world->spawnPoints.empty())
        {
            size_t idx = (id - 1) % world->spawnPoints.size();
            spawnPos = world->spawnPoints[idx].position;
            spawnYaw = world->spawnPoints[idx].yaw;
            printf("%s [SERVER PLAYER SPAWN] reason=initial_join id=%u name=\"%s\" "
                   "spawnpoint=%zu position=(%.2f,%.2f,%.2f) yaw=%.1f\n",
                   serverTimestamp(), id, p.name.c_str(), idx,
                   spawnPos.x, spawnPos.y, spawnPos.z, glm::degrees(spawnYaw));
        }
        else
        {
            spawnPos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
            printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s spawn=(%.1f,%.1f,%.1f) "
                   "(no spawnpoints in map)\n",
                   serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str(),
                   spawnPos.x, spawnPos.y, spawnPos.z);
        }
        beginAuthoritativeTransform(p, spawnPos, glm::vec3(0.0f), spawnYaw, "initial_join");
        // resetPlayerForSpawn and inventory setup happen in completeAuthoritativeSpawn
        // when the client sends ClientMapReady and spawned becomes true.
        // Wait for ClientMapReady before including in snapshots.
        // The client must load the required map first.
        p.spawned = false;
    }

    p.reconnectToken = generateReconnectToken();
    WelcomePacket welcome{};
    welcome.header.type = PACKET_WELCOME;
    welcome.header.tick = tick;
    welcome.header.playerId = id;
    welcome.header.transformEpoch = p.transformEpoch;
    welcome.assignedPlayerId = id;
    welcome.reliableEventSessionId = reliableGameplayEventSessionForPlayer(p);
    welcome.tickRate = SERVER_TICK_RATE;
    copyName(welcome.approvedName, p.name);
    std::memset(welcome.reconnectToken, 0, sizeof(welcome.reconnectToken));
    std::strncpy(welcome.reconnectToken, p.reconnectToken.c_str(), sizeof(welcome.reconnectToken) - 1);
    std::memset(welcome.mapId, 0, sizeof(welcome.mapId));
    std::strncpy(welcome.mapId, gServerMapId.c_str(), sizeof(welcome.mapId) - 1);
    if (sendToSourceOrPlayer(sock, from, &p, nullptr, &welcome, sizeof(welcome)))
        ++totalPacketsOut;
}

void handleInputPacket(const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t& nextEntityId,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       const sockaddr_in* from,
                       uint32_t serverTick,
                       const TransportConnectionId* connectionId)
{
    if (bytes < (int)sizeof(InputPacket))
        return;

    const InputPacket* in = reinterpret_cast<const InputPacket*>(buffer);
    auto it = players.find(in->header.playerId);
    if (it == players.end())
        return;

    ServerPlayer& p = it->second;
    const bool ownsConnection =
        playerOwnsConnectionSource(p, from, connectionId);
    const uint64_t currentMs = nowMs();
    if (ownsConnection)
        p.lastHeardMs = currentMs;

    // Store input command for server-side movement simulation (spec: input-command authority)
    if (ownsConnection && in->inputCommandSequence != 0 &&
        in->inputCommandSequence > p.lastInputCommandSequence)
    {
        p.lastInputCommandSequence = in->inputCommandSequence;
        auto& slot = p.inputCommandBuffer[p.nextInputCommandSlot];
        slot.command.sequence = in->inputCommandSequence;
        slot.command.clientSimulationTick = in->clientSimulationTick;
        slot.command.lifecycle.spawnGeneration = in->spawnGeneration;
        slot.command.lifecycle.transformEpoch =
            in->transformEpoch != 0 ? in->transformEpoch : in->header.transformEpoch;
        slot.command.moveAxes = movementClampUnitOrZero({in->wishX, in->wishY});
        slot.command.horizontalCameraForward = {
            in->camForwardX, in->camForwardY, in->camForwardZ};
        slot.command.lookYaw = in->yaw;
        slot.command.lookPitch = in->lookPitch;
        slot.command.jumpHeld = (in->stateFlags & NET_STATE_JUMPING) != 0;
        slot.command.dashPressed = (in->stateFlags & NET_STATE_DASHING) != 0;
        slot.command.downDashPressed = (in->stateFlags & NET_STATE_DOWN_DASHING) != 0;
        slot.command.freezeHeld = (in->stateFlags & NET_STATE_FREEZING) != 0;
        slot.command.movementDirectionPressed =
            std::abs(in->wishX) > 0.001f || std::abs(in->wishY) > 0.001f;
        slot.valid = true;
        p.nextInputCommandSlot = (p.nextInputCommandSlot + 1) % ServerPlayer::INPUT_COMMAND_BUFFER_SIZE;
    }

    const ClientMovementReport report = movementReportFromInputPacket(*in, p);
    const MovementConfig movementConfig = makeCurrentRuntimeMovementConfig();
    const MovementValidationConfig validationConfig =
        makeMovementValidationConfig(movementConfig, &world);
    MovementValidationContext validationContext;
    validationContext.playerExists = true;
    validationContext.connectionActive = true;
    validationContext.connectionOwnsPlayer = ownsConnection;
    validationContext.serverTick = serverTick;
    validationContext.nowMs = currentMs;
    validationContext.world = &world;

    // Process respawn serial before dead-state movement rejection. Ownership
    // still applies; spoofed packets must not request another player's respawn.
    if (ownsConnection &&
        in->respawnSerial != 0 &&
        in->respawnSerial != p.lastRespawnSerial)
    {
        p.lastRespawnSerial = in->respawnSerial;
        Debug::warn(Debug::Category::Networking,
            "[SERVER RESPAWN REQUEST] playerId=%u dead=%d respawnSerial=%u\n",
            p.id, (int)p.dead, (unsigned)in->respawnSerial);
        if (p.dead)
            p.instantRespawnRequested = true;
    }

    const bool equipLifecycleMatches =
        ownsConnection &&
        p.spawned &&
        p.spawnState == ServerPlayer::Active &&
        !p.dead &&
        in->spawnGeneration == p.spawnGeneration &&
        ((in->transformEpoch != 0 ? in->transformEpoch : in->header.transformEpoch) ==
            p.transformEpoch);
    if (equipLifecycleMatches)
        applyEquipIntentFromInput(p, *in, "owned-input");

    MovementValidationResult result =
        validateClientMovementReport(p, report, validationContext, validationConfig);
    applyMovementValidationCounters(p.movementValidation, result, report);
    logMovementValidation(p, report, result);
    if (connectionId)
    {
        static uint64_t sLastMovementDecisionLogMs = 0;
        static int sLastDecision = -1;
        static int sLastReason = -1;
        static uint64_t sLastDecisionHash = 0;
        uint64_t decisionHash = (uint64_t)(uint8_t)result.decision * 1000u +
                                (uint64_t)(uint8_t)result.reason;

        bool logThis = false;
        if (decisionHash != sLastDecisionHash)
        {
            logThis = true;
            sLastDecisionHash = decisionHash;
        }
        else if (result.decision == MovementValidationDecision::Reject &&
                 currentMs - sLastMovementDecisionLogMs >= 500)
        {
            logThis = true;
        }
        if (result.decision == MovementValidationDecision::Accept &&
            sLastDecision == (int)MovementValidationDecision::Reject)
        {
            // First acceptance after a rejection — always log.
            logThis = true;
        }

        if (logThis)
        {
            sLastMovementDecisionLogMs = currentMs;
            sLastDecision = (int)result.decision;
            sLastReason = (int)result.reason;

            Debug::warn(Debug::Category::Networking,
                "[SERVER MOVEMENT DECISION] "
                "playerId=%u seq=%u clientTick=%llu lastAcceptedTick=%llu serverTick=%u "
                "decision=%s reason=%s recovered=%d "
                "spawnGen=%u epoch=%u "
                "reportPos=(%.2f,%.2f,%.2f) serverPos=(%.2f,%.2f,%.2f)\n",
                p.id,
                report.movementSequence,
                (unsigned long long)report.clientSimulationTick,
                (unsigned long long)p.movementValidation.lastAcceptedClientTick,
                serverTick,
                movementValidationDecisionName(result.decision),
                movementValidationReasonName(result.reason),
                (result.decision == MovementValidationDecision::Accept &&
                 sLastDecision == (int)MovementValidationDecision::Reject) ? 1 : 0,
                report.lifecycle.spawnGeneration,
                (unsigned)report.lifecycle.transformEpoch,
                report.position.x, report.position.y, report.position.z,
                p.pos.x, p.pos.y, p.pos.z);
        }
    }

    if (result.decision == MovementValidationDecision::Reject)
    {
        if (p.dead)
            p.input.attackPressed = false;
        return;
    }

    if (result.clearsAuthoritativeTransformAck)
    {
        p.awaitingAuthoritativeTransformAck = false;
        Debug::warn(Debug::Category::Networking,
            "[SERVER TRANSFORM ACK] playerId=%u epoch=%u seq=%u "
            "distance=%.2f assignedMsAgo=%llu\n",
            p.id,
            (unsigned)p.transformEpoch,
            report.movementSequence,
            glm::length(report.position - p.authoritativeTransformPosition),
            (unsigned long long)(currentMs - p.authoritativeTransformAssignedMs));
    }

    applyMovementStateToServerPlayer(result.acceptedState, p);
    applyAcceptedInputPresentation(p, *in, report);

    if (!p.hasAcceptedClientTransform)
    {
        Debug::warn(Debug::Category::Networking,
            "[SERVER MOVEMENT BASELINE] playerId=%u seq=%u clientTick=%llu "
            "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) spawnGen=%u epoch=%u\n",
            p.id, report.movementSequence,
            (unsigned long long)report.clientSimulationTick,
            report.position.x, report.position.y, report.position.z,
            report.baseVelocity.x, report.baseVelocity.y, report.baseVelocity.z,
            report.lifecycle.spawnGeneration, (unsigned)report.lifecycle.transformEpoch);
    }

    p.clientStateUpdated = true;
    p.lastAcceptedClientPosition = result.acceptedState.position;
    p.lastAcceptedClientVelocity =
        result.acceptedState.baseVelocity + result.acceptedState.externalImpulse;
    p.lastAcceptedClientTransformMs = currentMs;
    p.hasAcceptedClientTransform = true;
    p.lastMovementSequence = report.movementSequence;
    p.hasMovementSequence = true;

    if (in->spawnNpcPressed)
    {
        ServerNpc npc;
        npc.entityId = nextEntityId++;
        npc.name = "NPC " + std::to_string(npc.entityId);
        npc.pos = p.pos + glm::vec3(2.0f, 0.0f, 0.0f);
        npcs[npc.entityId] = npc;
        printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f)\n",
               serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z);
    }
}

void handleDisconnect(std::unordered_map<uint32_t, ServerPlayer>& players,
                      const char* buffer,
                      std::vector<uint32_t>* pendingRemovals)
{
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    auto it = players.find(header->playerId);
    if (it != players.end())
    {
        printf("%s [SERVER LEAVE] id=%u name=\"%s\"\n",
               serverTimestamp(), it->second.id, it->second.name.c_str());

        if (it->second.transport)
            it->second.transport->close();

        if (pendingRemovals)
        {
            pendingRemovals->push_back(header->playerId);
            printf("%s [SERVER POST-LEAVE] players=%zu — pending removal (deferred)\n",
                   serverTimestamp(), players.size());
        }
        else
        {
            players.erase(it);
            printf("%s [SERVER POST-LEAVE] players=%zu — server continuing\n",
                   serverTimestamp(), players.size());
        }
        fflush(stdout);
    }
}

void handleSpawnNpcRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t& nextEntityId,
                           std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (bytes < (int)sizeof(SpawnNpcRequestPacket))
        return;
    const SpawnNpcRequestPacket* request =
        reinterpret_cast<const SpawnNpcRequestPacket*>(buffer);
    if (players.find(request->header.playerId) == players.end())
        return;

    ServerNpc npc;
    npc.entityId = nextEntityId++;
    npc.name = "NPC " + std::to_string(npc.entityId);
    npc.pos = {request->px, request->py, request->pz};
    npc.difficulty = request->difficulty;
    npcs[npc.entityId] = npc;
    printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f) difficulty=%.1f\n",
           serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z, npc.difficulty);
}

void handleTeleportRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           const HeadlessWorld& world)
{
    if (bytes < (int)sizeof(TeleportRequestPacket))
        return;
    const TeleportRequestPacket* request =
        reinterpret_cast<const TeleportRequestPacket*>(buffer);
    auto it = players.find(request->header.playerId);
    if (it == players.end() || it->second.dead)
        return;

    const glm::vec3 requestedPosition{
        request->px, request->py, request->pz};
    if (!std::isfinite(requestedPosition.x) ||
        !std::isfinite(requestedPosition.y) ||
        !std::isfinite(requestedPosition.z))
    {
        printf("%s [SERVER TELEPORT] playerId=%u rejected=non-finite\n",
               serverTimestamp(), request->header.playerId);
        return;
    }

    ServerPlayer& p = it->second;
    const glm::vec3 clampedPos = glm::clamp(
        requestedPosition,
        world.boundsMin - glm::vec3(2.0f),
        world.boundsMax + glm::vec3(2.0f));
    beginAuthoritativeTransform(p, clampedPos, glm::vec3(0.0f), p.yaw, "teleport");
    p.onGround = false;
    printf("%s [SERVER TELEPORT] playerId=%u position=(%.2f,%.2f,%.2f) epoch=%u\n",
           serverTimestamp(), p.id, clampedPos.x, clampedPos.y, clampedPos.z, (unsigned)p.transformEpoch);
}

void handleExplodeRequest(const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players)
{
    (void)bytes;
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    auto it = players.find(header->playerId);
    if (it == players.end() || it->second.dead)
        return;

    ServerPlayer& p = it->second;
    p.health = 0;
    p.dead = true;
    p.respawnSeconds = 2.0f;
    p.vel = glm::vec3(0.0f);
    printf("%s [SERVER DEATH] playerId=%u cause=explode respawn=2.0s\n",
           serverTimestamp(), p.id);
}

void handleJoinRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                       const HeadlessWorld* world,
                       const TransportConnectionId* connectionId,
                       std::unique_ptr<IGameTransport>* claimedTransport)
{
    if (bytes < (int)sizeof(JoinRequestPacket))
        return;
    const JoinRequestPacket* join = reinterpret_cast<const JoinRequestPacket*>(buffer);

    // Check if server is full
    if (players.size() >= MAX_PLAYERS)
    {
        JoinRejectPacket reject{};
        reject.header.type = PACKET_JOIN_REJECT;
        reject.header.tick = tick;
        reject.reason = 1; // full
        if (sendToSourceOrPlayer(sock, from, nullptr, claimedTransport, &reject, sizeof(reject)))
            ++totalPacketsOut;
        if (claimedTransport && claimedTransport->get())
        {
            (*claimedTransport)->close();
            claimedTransport->reset();
        }
        printf("%s [SERVER JOIN REJECT] reason=server-full\n", serverTimestamp());
        return;
    }

    // Validate join token — coordinator validation
    const std::string joinTokenStr = join->joinToken;
    if (joinTokenStr.empty())
    {
        JoinRejectPacket reject{};
        reject.header.type = PACKET_JOIN_REJECT;
        reject.header.tick = tick;
        reject.reason = 2; // bad token
        if (sendToSourceOrPlayer(sock, from, nullptr, claimedTransport, &reject, sizeof(reject)))
            ++totalPacketsOut;
        if (claimedTransport && claimedTransport->get())
        {
            (*claimedTransport)->close();
            claimedTransport->reset();
        }
        printf("%s [SERVER JOIN REJECT] reason=empty-token\n", serverTimestamp());
        return;
    }

    // Validate ICE join token with coordinator
    // (skip validation for local-only servers)
    if (gServerCoordinatorCode == "LOCAL")
    {
        printf("%s [SERVER JOIN] local server — accepting join for %s\n",
               serverTimestamp(), join->name);
    }
    else if (!gServerCoordinatorCode.empty())
    {
        printf("[ICE TOKEN VALIDATE] code=%s tokenPrefix=%s\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        if (!coordinatorIceValidateJoin(gServerCoordinatorCode, joinTokenStr))
        {
            printf("[ICE TOKEN VALIDATE] code=%s tokenPrefix=%s REJECTED\n",
                   gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
            JoinRejectPacket reject{};
            reject.header.type = PACKET_JOIN_REJECT;
            reject.header.tick = tick;
            reject.reason = 2;
            if (sendToSourceOrPlayer(sock, from, nullptr, claimedTransport, &reject, sizeof(reject)))
                ++totalPacketsOut;
            if (claimedTransport && claimedTransport->get())
            {
                (*claimedTransport)->close();
                claimedTransport->reset();
            }
            printf("%s [SERVER JOIN REJECT] reason=coordinator-rejected-token\n", serverTimestamp());
            return;
        }
        printf("[ICE TOKEN VALIDATE] code=%s tokenPrefix=%s valid\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        printf("%s [SERVER JOIN] coordinator validated token for %s\n",
               serverTimestamp(), join->name);
    }
    else
    {
        printf("%s [SERVER JOIN] no coordinator code — rejecting\n", serverTimestamp());
        JoinRejectPacket reject{};
        reject.header.type = PACKET_JOIN_REJECT;
        reject.header.tick = tick;
        reject.reason = 2;
        if (sendToSourceOrPlayer(sock, from, nullptr, claimedTransport, &reject, sizeof(reject)))
            ++totalPacketsOut;
        if (claimedTransport && claimedTransport->get())
        {
            (*claimedTransport)->close();
            claimedTransport->reset();
        }
        printf("%s [SERVER JOIN REJECT] reason=no-coordinator-code\n", serverTimestamp());
        return;
    }

    // Check for existing reconnection
    uint32_t existingId = 0;
    for (auto& kv : players)
    {
        if (kv.second.reconnectToken == join->joinToken && !kv.second.dead)
        {
            existingId = kv.first;
            break;
        }
    }

    uint32_t id = existingId ? existingId : nextPlayerId++;
    ServerPlayer& p = players[id];
    p.id = id;
    bindPlayerConnection(p, from, connectionId, claimedTransport);
    p.lastHeardMs = nowMs();
    p.lastShotSerial = 0;
    p.lastProjectileFireSerial = 0;
    p.lastMeleeAttackSerial = 0;
    p.projectileFireCooldown = 0.0f;
    p.joinToken = join->joinToken;
    p.joinTokenValidated = true;
    p.reconnectToken = generateReconnectToken();
    p.name = uniquePlayerName(players, join->name, id);

    if (!existingId)
    {
        // Use map spawnpoints if available
        glm::vec3 spawnPos;
        float spawnYaw = 0.0f;
        if (world && !world->spawnPoints.empty())
        {
            size_t idx = (id - 1) % world->spawnPoints.size();
            spawnPos = world->spawnPoints[idx].position;
            spawnYaw = world->spawnPoints[idx].yaw;
            printf("%s [SERVER PLAYER SPAWN] reason=join_request id=%u name=\"%s\" "
                   "spawnpoint=%zu position=(%.2f,%.2f,%.2f) yaw=%.1f token=%s\n",
                   serverTimestamp(), id, p.name.c_str(), idx,
                   spawnPos.x, spawnPos.y, spawnPos.z, glm::degrees(spawnYaw),
                   p.reconnectToken.c_str());
        }
        else
        {
            spawnPos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
            printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s spawn=(%.1f,%.1f,%.1f) "
                   "(no spawnpoints) token=%s\n",
                   serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str(),
                   spawnPos.x, spawnPos.y, spawnPos.z, p.reconnectToken.c_str());
        }
        beginAuthoritativeTransform(p, spawnPos, glm::vec3(0.0f), spawnYaw, "join_request");
        // Player registered but not yet spawned — waits for ClientMapReady.
        // Server will simulate and include in snapshots only after spawned=true.
        p.spawned = false;
    }
    else
    {
        printf("%s [SERVER REJOIN] id=%u name=\"%s\" addr=%s\n",
               serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str());
        beginAuthoritativeTransform(p, p.pos, p.vel, p.yaw, "rejoin");
        p.spawned = true;
    }

    JoinAcceptPacket accept{};
    accept.header.type = PACKET_JOIN_ACCEPT;
    accept.header.tick = tick;
    accept.header.playerId = id;
    accept.header.transformEpoch = p.transformEpoch;
    accept.assignedPlayerId = id;
    accept.reliableEventSessionId = reliableGameplayEventSessionForPlayer(p);
    accept.tickRate = SERVER_TICK_RATE;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    std::memset(accept.mapId, 0, sizeof(accept.mapId));
    std::strncpy(accept.mapId, gServerMapId.c_str(), sizeof(accept.mapId) - 1);
    if (sendToSourceOrPlayer(sock, from, &p, nullptr, &accept, sizeof(accept)))
        ++totalPacketsOut;
}

void handleReconnectRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick, uint64_t& totalPacketsOut,
                            const TransportConnectionId* connectionId,
                            std::unique_ptr<IGameTransport>* claimedTransport)
{
    if (bytes < (int)sizeof(ReconnectRequestPacket))
        return;
    const ReconnectRequestPacket* req = reinterpret_cast<const ReconnectRequestPacket*>(buffer);
    const std::string requestedToken = req->reconnectToken;
    const uint64_t currentMs = nowMs();

    // Find player with matching reconnect token
    uint32_t foundId = 0;
    bool resendExistingAccept = false;
    for (auto& kv : players)
    {
        if (kv.second.reconnectToken == requestedToken)
        {
            foundId = kv.first;
            break;
        }
        if (!kv.second.previousReconnectToken.empty() &&
            kv.second.previousReconnectToken == requestedToken &&
            currentMs <= kv.second.previousReconnectTokenValidUntilMs)
        {
            foundId = kv.first;
            resendExistingAccept = true;
            break;
        }
    }

    if (foundId == 0)
    {
        printf("%s [SERVER RECONNECT] rejected token=%s not-found\n",
               serverTimestamp(), requestedToken.c_str());
        if (claimedTransport && claimedTransport->get())
        {
            (*claimedTransport)->close();
            claimedTransport->reset();
        }
        return;
    }

    ServerPlayer& p = players[foundId];
    bindPlayerConnection(p, from, connectionId, claimedTransport);
    p.lastHeardMs = currentMs;
    if (!resendExistingAccept)
    {
        p.pendingReliableEvents.clear();
        p.reliableEventSessionId = 0;
        p.previousReconnectToken = requestedToken;
        p.previousReconnectTokenValidUntilMs = currentMs + 5000;
        p.reconnectToken = generateReconnectToken(); // rotate token
        beginAuthoritativeTransform(p, p.pos, p.vel, p.yaw, "reconnect");
        p.awaitingAuthoritativeTransformAck = false;
    }

    ReconnectAcceptPacket accept{};
    accept.header.type = PACKET_RECONNECT_ACCEPT;
    accept.header.tick = tick;
    accept.header.playerId = foundId;
    accept.header.transformEpoch = p.transformEpoch;
    accept.assignedPlayerId = foundId;
    accept.reliableEventSessionId = reliableGameplayEventSessionForPlayer(p);
    accept.tickRate = SERVER_TICK_RATE;
    accept.spawnGeneration = p.spawnGeneration;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    accept.restoredHealth = p.health;
    accept.restoredKills = p.kills;
    accept.restoredDeaths = p.deaths;
    accept.restorePx = p.pos.x;
    accept.restorePy = p.pos.y;
    accept.restorePz = p.pos.z;
    const int acceptCopies = resendExistingAccept ? 1 : 4;
    for (int i = 0; i < acceptCopies; ++i)
    {
        if (sendToSourceOrPlayer(sock, from, &p, nullptr, &accept, sizeof(accept)))
            ++totalPacketsOut;
    }

    printf("%s [SERVER RECONNECT] %s id=%u name=\"%s\" health=%d copies=%d\n",
           serverTimestamp(), resendExistingAccept ? "resent" : "accepted",
           foundId, p.name.c_str(), p.health, acceptCopies);
}

ServerPacketProcessResult processServerPacket(
    SOCKET sock,
    const TransportReceiveEvent& event,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextPlayerId,
    uint32_t& nextEntityId,
    uint32_t& nextProjectileId,
    const HeadlessWorld& world,
    uint32_t tick,
    uint64_t& totalPacketsIn,
    uint64_t& totalPacketsOut,
    ServerPacketStats* stats,
    DisagreementRetransmitState* retransmitState,
    ServerPlayer* authenticatedPlayer,
    std::unique_ptr<IGameTransport>* claimedTransport,
    std::vector<uint32_t>* pendingRemovals)
{
    ServerPacketProcessResult result{};
    ++totalPacketsIn;

    char buffer[2048];
    const std::string source = addressToString(event.remoteEndpoint);

    if (!event.payload || event.payloadBytes <= 0)
    {
        if (stats)
            ++stats->malformedPackets;
        printf("%s [SERVER PACKET] rejected reason=empty-payload transport=%s connection=%llu source=%s\n",
               serverTimestamp(), transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value, source.c_str());
        return result;
    }

    if (event.payloadBytes < (int)sizeof(PacketHeader))
    {
        if (stats)
            ++stats->malformedPackets;
        printf("%s [SERVER PACKET] rejected reason=too-small bytes=%d transport=%s "
               "connection=%llu source=%s minBytes=%zu\n",
               serverTimestamp(), event.payloadBytes,
               transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value,
               source.c_str(), sizeof(PacketHeader));
        return result;
    }

    if (event.payloadBytes > (int)sizeof(buffer) ||
        event.payloadBytes > MAX_GAME_DATAGRAM_BYTES)
    {
        if (stats)
            ++stats->malformedPackets;
        printf("%s [SERVER PACKET] rejected reason=too-large bytes=%d maxBytes=%d "
               "transport=%s connection=%llu source=%s\n",
               serverTimestamp(), event.payloadBytes, MAX_GAME_DATAGRAM_BYTES,
               transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value,
               source.c_str());
        return result;
    }

    std::memcpy(buffer, event.payload, event.payloadBytes);
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    result.decoded = true;

    if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
    {
        if (stats)
            ++stats->protocolMismatches;
        printf("%s [SERVER PACKET] rejected reason=protocol-mismatch bytes=%d "
               "transport=%s connection=%llu source=%s magic=0x%08x "
               "expectedMagic=0x%08x version=%u expectedVersion=%u type=%u\n",
               serverTimestamp(), event.payloadBytes,
               transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value,
               source.c_str(), header->magic, PROTOCOL_MAGIC,
               header->version, PROTOCOL_VERSION, header->type);
        return result;
    }

    if (!isKnownPacketType(header->type))
    {
        if (stats)
            ++stats->unknownPacketTypes;
        printf("%s [SERVER PACKET] rejected reason=unknown-type bytes=%d "
               "transport=%s connection=%llu source=%s type=%u\n",
               serverTimestamp(), event.payloadBytes,
               transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value,
               source.c_str(), header->type);
        return result;
    }

    if (stats)
        countPacketType(*stats, header->type);

    if (header->type != PACKET_HELLO)
    {
        auto it = players.find(header->playerId);
        if (it != players.end() &&
            playerOwnsConnectionSource(
                it->second, &event.remoteEndpoint, &event.connectionId))
        {
            it->second.lastHeardMs = nowMs();
        }
    }

    const TransportConnectionId* sourceConnection = &event.connectionId;
    const sockaddr_in& from = event.remoteEndpoint;
    const int bytes = event.payloadBytes;
    result.playerId = header->playerId;

    if (header->type == PACKET_HELLO)
    {
        handleHello(sock, from, buffer, bytes, players, nextPlayerId, tick,
                    totalPacketsOut, &world, sourceConnection, claimedTransport);
        result.handled = true;
        result.transportConsumed = claimedTransport && !claimedTransport->get();
    }
    else if (header->type == PACKET_JOIN_REQUEST)
    {
        handleJoinRequest(sock, from, buffer, bytes, players, nextPlayerId, tick,
                          totalPacketsOut, &world, sourceConnection,
                          claimedTransport);
        result.handled = true;
        result.transportConsumed = claimedTransport && !claimedTransport->get();
    }
    else if (header->type == PACKET_RECONNECT_REQUEST)
    {
        handleReconnectRequest(sock, from, buffer, bytes, players, tick,
                               totalPacketsOut, sourceConnection,
                               claimedTransport);
        result.handled = true;
        result.transportConsumed = claimedTransport && !claimedTransport->get();
    }
    else if (header->type == PACKET_INPUT)
    {
        handleInputPacket(buffer, bytes, players, world, nextEntityId, npcs,
                          &from, tick, sourceConnection);
        result.handled = true;
    }
    else if (header->type == PACKET_DISCONNECT)
    {
        handleDisconnect(players, buffer, pendingRemovals);
        result.handled = true;
    }
    else if (header->type == PACKET_SPAWN_NPC_REQUEST)
    {
        handleSpawnNpcRequest(buffer, bytes, players, nextEntityId, npcs);
        result.handled = true;
    }
    else if (header->type == PACKET_TELEPORT_REQUEST)
    {
        handleTeleportRequest(buffer, bytes, players, world);
        result.handled = true;
    }
    else if (header->type == PACKET_EXPLODE_REQUEST)
    {
        handleExplodeRequest(buffer, bytes, players);
        result.handled = true;
    }
    else if (header->type == PACKET_SHOT_REQUEST)
    {
        handleShotRequest(sock, from, buffer, bytes, players, world, tick,
                          totalPacketsOut, retransmitState);
        result.handled = true;
    }
    else if (header->type == PACKET_PELLET_BLAST_REQUEST)
    {
        handlePelletBlastRequest(sock, from, buffer, bytes, players, world, tick,
                                 totalPacketsOut, retransmitState);
        result.handled = true;
    }
    else if (header->type == PACKET_PROJECTILE_FIRE_REQUEST)
    {
        handleProjectileFireRequest(sock, from, buffer, bytes, players,
                                    projectiles, nextProjectileId, world, tick,
                                    totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_ATTACK_REQUEST)
    {
        handleAttackRequest(sock, from, buffer, bytes, players, npcs, projectiles,
                            nextProjectileId, world, tick, totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_MELEE_HIT_REQUEST)
    {
        handleMeleeHitRequest(sock, from, buffer, bytes, players, tick,
                              totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_CHAT_MESSAGE)
    {
        handleChatMessage(sock, buffer, bytes, players, tick, totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_PING)
    {
        handlePing(sock, from, buffer, bytes, tick, authenticatedPlayer);
        result.handled = true;
    }
    else if (header->type == PACKET_RELOAD_REQUEST)
    {
        handleReloadRequest(sock, from, buffer, bytes, players, tick,
                            totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_GODBALL_STATE)
    {
        handleGodballState(sock, players, buffer, bytes);
        result.handled = true;
    }
    else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
    {
        handleNpcDamageRequest(sock, buffer, bytes, from, players, npcs, tick,
                               totalPacketsOut);
        result.handled = true;
    }
    else if (header->type == PACKET_SERVER_COMMAND)
    {
        handleServerCommand(buffer, bytes, players, npcs);
        result.handled = true;
    }
    else if (header->type == PACKET_SPAWN_ACK &&
             bytes >= (int)sizeof(SpawnAckPacket))
    {
        handleSpawnAck(sock, buffer, bytes, players, tick);
        result.handled = true;
    }
    else if (header->type == PACKET_RELIABLE_EVENT_ACK &&
             bytes >= (int)sizeof(ReliableEventAckPacket))
    {
        handleReliableEventAck(buffer, bytes, players, &from,
                               authenticatedPlayer);
        result.handled = true;
    }
    else if (header->type == PACKET_CLIENT_MAP_READY &&
             bytes >= (int)sizeof(ClientMapReadyPacket))
    {
        const ClientMapReadyPacket* ready =
            reinterpret_cast<const ClientMapReadyPacket*>(buffer);
        if (ready->header.playerId != ready->assignedPlayerId)
        {
            printf("%s [SERVER MAP READY REJECT] reason=assignedPlayerId-mismatch "
                   "headerPlayerId=%u assignedPlayerId=%u\n",
                   serverTimestamp(), ready->header.playerId,
                   ready->assignedPlayerId);
        }
        else
        {
            auto it = players.find(ready->assignedPlayerId);
            if (it == players.end())
            {
                printf("%s [SERVER MAP READY REJECT] reason=player-not-found id=%u\n",
                       serverTimestamp(), ready->assignedPlayerId);
            }
            else if (!playerOwnsConnectionSource(
                it->second, &from, sourceConnection))
            {
                printf("%s [SERVER MAP READY REJECT] reason=connection-mismatch "
                       "id=%u transport=%s connection=%llu\n",
                       serverTimestamp(), ready->assignedPlayerId,
                       transportKindName(event.transportKind),
                       (unsigned long long)event.connectionId.value);
            }
            else
            {
                std::string readyMap = normalizeMapId(ready->mapId);
                std::string serverMap = normalizeMapId(getServerMapId());
                if (readyMap != serverMap)
                {
                    printf("%s [SERVER MAP READY REJECT] reason=map-mismatch id=%u "
                           "readyMap=%s serverMap=%s\n",
                           serverTimestamp(), ready->assignedPlayerId,
                           readyMap.c_str(), serverMap.c_str());
                }
                else if (!it->second.spawned)
                {
                    it->second.spawned = true;
                    it->second.vel = glm::vec3(0.0f);
                    it->second.clientStateUpdated = false;
                    completeAuthoritativeSpawn(sock, it->second, true);
                    printf("%s [SERVER MAP READY] transport=%s connection=%llu "
                           "id=%u name=\"%s\"\n",
                           serverTimestamp(), transportKindName(event.transportKind),
                           (unsigned long long)event.connectionId.value,
                           it->second.id, it->second.name.c_str());
                }
            }
        }
        result.handled = true;
    }
    else
    {
        if (stats)
            ++stats->unknownPacketTypes;
        printf("%s [SERVER PACKET] rejected reason=unsupported-or-short type=%u "
               "bytes=%d transport=%s connection=%llu source=%s\n",
               serverTimestamp(), header->type, bytes,
               transportKindName(event.transportKind),
               (unsigned long long)event.connectionId.value,
               source.c_str());
    }

    return result;
}

void beginAuthoritativeTransform(ServerPlayer& player,
    const glm::vec3& position, const glm::vec3& velocity, float yaw,
    const char* reason)
{
    player.pos = position;
    player.vel = velocity;
    player.yaw = yaw;
    ++player.transformEpoch;
    player.awaitingAuthoritativeTransformAck = true;
    player.authoritativeTransformPosition = position;
    player.authoritativeTransformEpoch = player.transformEpoch;
    player.authoritativeTransformAssignedMs = nowMs();
    player.clientStateUpdated = false;

    // Reset accepted-client-state tracking for the new transform
    player.lastAcceptedClientPosition = position;
    player.lastAcceptedClientVelocity = velocity;
    player.lastAcceptedClientTransformMs = nowMs();
    player.hasAcceptedClientTransform = true;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    printf("%s [SERVER AUTHORITATIVE TRANSFORM] playerId=%u reason=%s "
           "epoch=%u position=(%.2f,%.2f,%.2f) awaitingAck=1\n",
           serverTimestamp(), player.id, reason,
           (unsigned)player.transformEpoch,
           position.x, position.y, position.z);
}

std::string generateReconnectToken()
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::mt19937 rng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 61);
    std::string token;
    for (int i = 0; i < 24; ++i)
        token += chars[dist(rng)];
    return token;
}

void buildAndSendSnapshot(SOCKET sock,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          const std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick, uint64_t& totalPacketsOut)
{
    // Build compact entity list
    CompactEntityData entities[MAX_SNAPSHOT_ENTITIES];
    uint32_t entityCount = 0;

    for (const auto& kv : players)
    {
        if (entityCount >= MAX_SNAPSHOT_ENTITIES)
            break;
        if (!kv.second.spawned)
        {
            static std::unordered_map<uint32_t, uint64_t> lastSkipLogMs;
            uint64_t nowSkip = nowMs();
            uint64_t& lastLog = lastSkipLogMs[kv.first];
            if (nowSkip - lastLog >= 1000)
            {
                printf("%s [SERVER SNAPSHOT SKIP] playerId=%u reason=not-spawned "
                       "connected=1 lastHeardAgoMs=%llu mapReadyReceived=0 serverMap=%s\n",
                       serverTimestamp(), kv.first,
                       (unsigned long long)(nowSkip - kv.second.lastHeardMs),
                       getServerMapId().c_str());
                lastLog = nowSkip;
            }
            continue;
        }
        entities[entityCount++] = compactEntityFromSnapshot(makePlayerEntity(kv.second));
    }
    for (const auto& kv : npcs)
    {
        if (entityCount >= MAX_SNAPSHOT_ENTITIES)
            break;
        entities[entityCount++] = compactEntityFromSnapshot(makeNpcEntity(kv.second));
    }

    if (tick % 120 == 0)
    {
        printf("%s [SERVER SNAPSHOT BUILD] tick=%u entities=%u\n",
               serverTimestamp(), tick, entityCount);
    }

    // Build chunks from compact entities
    std::vector<std::vector<uint8_t>> chunks;
    if (!buildSnapshotChunks(entities, entityCount, tick, 0, chunks))
    {
        printf("%s [SERVER SNAPSHOT] chunk build failed for tick=%u\n",
               serverTimestamp(), tick);
        return;
    }

    if (tick % 120 == 0)
    {
        printf("%s [SERVER SNAPSHOT CHUNKS] tick=%u chunks=%zu maxChunkBytes=%zu\n",
               serverTimestamp(), tick, chunks.size(),
               chunks.empty() ? 0 : chunks[0].size());
    }

    for (const auto& kv : players)
    {
        for (const auto& chunk : chunks)
        {
            bool sent = false;
            if (kv.second.transport)
            {
                sent = kv.second.transport->send(chunk.data(), chunk.size());
            }
            else
            {
                int bytesSent = sendto(
                    sock, (const char*)chunk.data(), (int)chunk.size(), 0,
                    (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
                sent = (bytesSent != SOCKET_ERROR);
                if (!sent)
                    printf("%s [NET TX ERROR] sendto failed id=%u error=%d\n",
                           serverTimestamp(), kv.first, WSAGetLastError());
            }
            ++totalPacketsOut;
        }
        if (tick % 120 == 0)
            printf("%s [SERVER SNAPSHOT SEND] toClientId=%u chunks=%zu\n",
                   serverTimestamp(), kv.first, chunks.size());
    }
}

// ── Send disagreement event to all connected players ─────────────────
void sendDisagreementToAll(SOCKET sock,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           DisagreementReason reason,
                           uint32_t eventId,
                           uint32_t relatedSerial,
                           uint32_t sourcePlayerId,
                           uint32_t targetPlayerId,
                           glm::vec3 position,
                           glm::vec3 correction,
                           const char* description,
                           uint32_t tick,
                           uint64_t& totalPacketsOut,
                           DisagreementRetransmitState* retransmitState)
{
    DisagreementPacket packet{};
    packet.header.type = PACKET_DISAGREEMENT;
    packet.header.tick = tick;
    packet.reason = (uint8_t)reason;
    packet.eventId = eventId;
    packet.relatedSerial = relatedSerial;
    packet.sourcePlayerId = sourcePlayerId;
    packet.targetPlayerId = targetPlayerId;
    packet.posX = position.x;
    packet.posY = position.y;
    packet.posZ = position.z;
    packet.correctionX = correction.x;
    packet.correctionY = correction.y;
    packet.correctionZ = correction.z;
    std::memset(packet.description, 0, sizeof(packet.description));
    if (description)
        std::strncpy(packet.description, description, sizeof(packet.description) - 1);

    printf("%s [SERVER DISAGREEMENT SEND] eventId=%u shotSerial=%u shooter=%u target=%u "
           "reason=%u pos=(%.1f,%.1f,%.1f) correction=(%.1f,%.1f,%.1f) desc=\"%s\" players=%zu\n",
           serverTimestamp(), eventId, relatedSerial, sourcePlayerId, targetPlayerId,
           (unsigned)reason,
           position.x, position.y, position.z,
           correction.x, correction.y, correction.z,
           description ? description : "",
           players.size());

    for (const auto& kv : players)
    {
        bool ok = false;
        if (kv.second.transport)
            ok = kv.second.transport->send(&packet, sizeof(packet));
        else
        {
            int bytesSent = sendto(
                sock, (const char*)&packet, sizeof(packet), 0,
                (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            ok = (bytesSent != SOCKET_ERROR);
            if (!ok)
                printf("%s [NET TX ERROR] sendDisagreementToAll failed id=%u error=%d\n",
                       serverTimestamp(), kv.first, WSAGetLastError());
        }
        ++totalPacketsOut;
    }

    // Queue for best-effort retransmission over the next few ticks
    if (retransmitState)
    {
        for (int i = 0; i < DISAGREEMENT_RETRANSMIT_MAX; ++i)
        {
            if (!retransmitState->events[i].active)
            {
                retransmitState->events[i].packet = packet;
                retransmitState->events[i].retransmitsLeft = DISAGREEMENT_RETRANSMIT_TICKS;
                retransmitState->events[i].active = true;
                break;
            }
        }
    }
}

// ── Retransmit pending disagreements (best-effort reliability) ────────
void tickDisagreementRetransmit(SOCKET sock,
                                const std::unordered_map<uint32_t, ServerPlayer>& players,
                                DisagreementRetransmitState& state,
                                uint64_t& totalPacketsOut)
{
    for (int i = 0; i < DISAGREEMENT_RETRANSMIT_MAX; ++i)
    {
        PendingDisagreement& pd = state.events[i];
        if (!pd.active)
            continue;

        printf("%s [SERVER DISAGREEMENT SEND] eventId=%u attempt=%d recipients=%zu "
               "reason=%u\n",
               serverTimestamp(), pd.packet.eventId,
               DISAGREEMENT_RETRANSMIT_TICKS - pd.retransmitsLeft + 1,
               players.size(), (unsigned)pd.packet.reason);

        for (const auto& kv : players)
        {
            if (kv.second.transport)
                kv.second.transport->send(&pd.packet, sizeof(pd.packet));
            else
                sendto(sock, (const char*)&pd.packet, sizeof(pd.packet), 0,
                       (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            ++totalPacketsOut;
        }

        --pd.retransmitsLeft;
        if (pd.retransmitsLeft <= 0)
            pd.active = false;
    }
}

// ── Reload request (with idempotent caching) ─────────────────────────
void handleReloadRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t tick, uint64_t& totalPacketsOut)
{
    (void)totalPacketsOut;
    if (bytes < (int)sizeof(ReloadRequestPacket))
        return;

    const ReloadRequestPacket* req = reinterpret_cast<const ReloadRequestPacket*>(buffer);

    auto it = players.find(req->header.playerId);
    if (it == players.end() || !sameAddress(it->second.addr, from))
        return;

    ServerPlayer& p = it->second;
    const std::string* wepId = weaponIdForDefNetworkId(req->weaponDefNetworkId);
    if (!wepId)
        return;

    Debug::log(Debug::Category::Weapons, "[RELOAD REQUEST RX] playerId=%u requestId=%u spawnGen=%u weapon=%s tick=%u\n",
               p.id, req->requestId, req->spawnGeneration, wepId->c_str(), tick);

    // Validate spawn generation
    if (req->spawnGeneration != p.spawnGeneration)
    {
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u stale spawnGeneration req=%u cur=%u\n",
                   p.id, req->spawnGeneration, p.spawnGeneration);
        return;
    }

    // Validate spawn state
    if (p.spawnState != ServerPlayer::Active)
    {
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u spawnState not Active — rejected\n", p.id);
        return;
    }

    // ── Idempotent cache lookup ────────────────────────────────────
    // Cache reload results by (playerId, spawnGeneration, requestId)
    // Similar to projectile fire result caching.
    // For the initial implementation, just deduplicate with a simple set.
    static std::unordered_set<uint64_t> s_processedReloads;
    uint64_t cacheKey = ((uint64_t)p.id << 32) | ((uint64_t)req->spawnGeneration << 16) | req->requestId;
    if (s_processedReloads.count(cacheKey))
    {
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u duplicate requestId=%u (already processed)\n",
                   p.id, req->requestId);
        return;
    }
    s_processedReloads.insert(cacheKey);
    if (s_processedReloads.size() > 256)
        s_processedReloads.clear();

    auto rtIt = p.weaponRuntimes.find(*wepId);
    if (rtIt == p.weaponRuntimes.end() || !rtIt->second.initialized)
        return;

    ServerPlayer::ServerWeaponRuntime& rt = rtIt->second;
    const WeaponDefinition* def = WeaponRegistry::instance().get(*wepId);

    ReloadResultPacket result{};
    result.header.type = PACKET_RELOAD_RESULT;
    result.header.tick = tick;
    result.header.playerId = p.id;
    result.requestId = req->requestId;
    result.spawnGeneration = req->spawnGeneration;
    result.weaponDefNetworkId = req->weaponDefNetworkId;

    if (p.dead)
    {
        result.accepted = 0;
        result.reason = 2;
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u dead — rejected\n", p.id);
    }
    else if (rt.magazineAmmo >= (def ? def->magazineSize : 999))
    {
        result.accepted = 0;
        result.reason = 3;
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u magazine already full (%d/%d)\n",
                   p.id, rt.magazineAmmo, def ? def->magazineSize : -1);
    }
    else if (rt.reserveAmmo <= 0)
    {
        result.accepted = 0;
        result.reason = 4;
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u no reserve ammo\n", p.id);
    }
    else if (rt.reloading)
    {
        // Already reloading — accept but report current state
        result.accepted = 1;
        result.reason = 5; // already reloading
        Debug::log(Debug::Category::Weapons, "[RELOAD] playerId=%u already reloading — report current state\n", p.id);
    }
    else
    {
        rt.reloading = true;
        float reloadTime = def ? def->reloadTime : 0.55f;
        uint32_t reloadTicks = (uint32_t)std::ceil(reloadTime * 60.0f);
        rt.reloadCompleteTick = tick + reloadTicks;
        rt.stateRevision++;
        result.accepted = 1;
        result.reason = 0;
        Debug::log(Debug::Category::Weapons, "[RELOAD ACCEPT] playerId=%u weapon=%s reloadTicks=%u completeTick=%llu stateRev=%u ammo=%d/%d\n",
                   p.id, wepId->c_str(), reloadTicks, (unsigned long long)rt.reloadCompleteTick,
                   rt.stateRevision, rt.magazineAmmo, rt.reserveAmmo);
    }

    result.magazineAmmo = rt.magazineAmmo;
    result.reserveAmmo = rt.reserveAmmo;
    result.reloadCompleteTick = rt.reloadCompleteTick;
    result.nextAllowedFireTick = rt.nextAllowedFireTick;
    result.reloading = rt.reloading ? 1 : 0;
    result.stateRevision = rt.stateRevision;

    // Send result back to the requesting player only
    serverSendToPlayer(sock, p, &result, sizeof(result));
}

// ── Spawn acknowledgement ────────────────────────────────────────────
void handleSpawnAck(SOCKET sock, const char* buffer, int bytes,
                     std::unordered_map<uint32_t, ServerPlayer>& players,
                     uint32_t tick)
{
    if (bytes < (int)sizeof(SpawnAckPacket))
        return;
    const SpawnAckPacket* ack = reinterpret_cast<const SpawnAckPacket*>(buffer);
    auto it = players.find(ack->header.playerId);
    if (it == players.end())
        return;

    ServerPlayer& p = it->second;

    // Log every ACK received, even if stale
    if (ack->spawnGeneration != p.spawnGeneration || ack->transformEpoch != p.transformEpoch || p.spawnState != ServerPlayer::AwaitingSpawnAck)
    {
        Debug::log(Debug::Category::Weapons, "[SPAWN ACK REJECT] playerId=%u recvGen=%u recvEpoch=%u expectGen=%u expectEpoch=%u state=%d\n",
                   p.id, ack->spawnGeneration, ack->transformEpoch, p.spawnGeneration, p.transformEpoch, (int)p.spawnState);
        return;
    }

    // Duplicate matching ACK after already Active — harmless
    if (p.spawnState == ServerPlayer::Active)
    {
        Debug::log(Debug::Category::Weapons, "[SPAWN ACK] playerId=%u spawnGen=%u epoch=%u — already Active, idempotent\n",
                   p.id, ack->spawnGeneration, ack->transformEpoch);
        return;
    }

    p.spawnState = ServerPlayer::Active;
    resetServerMovementForAuthoritativeLifecycle(
        p, makeCurrentRuntimeMovementConfig());
    Debug::log(Debug::Category::Weapons, "[SPAWN ACK ACCEPT] playerId=%u spawnGen=%u epoch=%u — now Active\n",
               p.id, ack->spawnGeneration, ack->transformEpoch);

    // Send SpawnActivated confirmation
    SpawnActivatedPacket act{};
    act.header.type = PACKET_SPAWN_ACTIVATED;
    act.header.tick = tick;
    act.header.playerId = p.id;
    act.spawnGeneration = p.spawnGeneration;
    act.transformEpoch = p.transformEpoch;
    act.serverTick = tick;
    if (p.transport)
        p.transport->send(&act, sizeof(act));
    else
        sendto(sock, (const char*)&act, sizeof(act), 0,
               (sockaddr*)&p.addr, sizeof(p.addr));
    Debug::log(Debug::Category::Weapons, "[SPAWN ACTIVATED SEND] playerId=%u spawnGen=%u epoch=%u\n",
               p.id, act.spawnGeneration, act.transformEpoch);
}

} // namespace MimitaNet
