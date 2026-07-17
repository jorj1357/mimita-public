#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <limits>
#include <random>
#include <unordered_map>

namespace MimitaNet {

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

bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
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
                 const HeadlessWorld* world)
{
    if (bytes < (int)sizeof(HelloPacket))
        return;
    uint32_t existingId = 0;
    for (const auto& kv : players)
        if (sameAddress(kv.second.addr, from))
            existingId = kv.first;

    uint32_t id = existingId ? existingId : nextPlayerId++;
    ServerPlayer& p = players[id];
    p.id = id;
    p.addr = from;
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
    welcome.tickRate = SERVER_TICK_RATE;
    copyName(welcome.approvedName, p.name);
    std::memset(welcome.reconnectToken, 0, sizeof(welcome.reconnectToken));
    std::strncpy(welcome.reconnectToken, p.reconnectToken.c_str(), sizeof(welcome.reconnectToken) - 1);
    std::memset(welcome.mapId, 0, sizeof(welcome.mapId));
    std::strncpy(welcome.mapId, gServerMapId.c_str(), sizeof(welcome.mapId) - 1);
    sendto(sock, (const char*)&welcome, sizeof(welcome), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;
}

void handleInputPacket(const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t& nextEntityId,
                       std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (bytes < (int)sizeof(InputPacket))
        return;
    const InputPacket* in = reinterpret_cast<const InputPacket*>(buffer);
    auto it = players.find(in->header.playerId);
    if (it == players.end())
        return;
    ServerPlayer& p = it->second;
    p.lastHeardMs = nowMs();

    // ── Skip unspawned players (ClientMapReady not yet received) ──────
    if (!p.spawned)
    {
        static uint64_t lastSpawnedLogMs = 0;
        uint64_t nowSpawned = nowMs();
        if (nowSpawned - lastSpawnedLogMs >= 1000)
        {
            printf("%s [SERVER INPUT SKIP] playerId=%u reason=not-spawned "
                   "connected=1 lastHeardMs=%llu\n",
                   serverTimestamp(), p.id,
                   (unsigned long long)(nowSpawned - p.lastHeardMs));
            lastSpawnedLogMs = nowSpawned;
        }
        return;
    }

    // ── Transform epoch check ─────────────────────────────────────
    // Discard packets from before the player's most recent spawn/respawn.
    if (in->header.transformEpoch != 0 && in->header.transformEpoch < p.transformEpoch)
    {
        static uint64_t lastEpochLogMs = 0;
        uint64_t now = nowMs();
        if (now - lastEpochLogMs >= 1000)
        {
            printf("%s [SERVER INPUT DROP] reason=OLD_TRANSFORM_EPOCH playerId=%u "
                   "packetEpoch=%u currentEpoch=%u dead=%d serverPos=(%.2f,%.2f,%.2f)\n",
                   serverTimestamp(), p.id, in->header.transformEpoch, p.transformEpoch,
                   (int)p.dead, p.pos.x, p.pos.y, p.pos.z);
            lastEpochLogMs = now;
        }
        return;
    }

    // Log first accepted packet matching the new epoch
    if (in->header.transformEpoch == p.transformEpoch)
    {
        static uint64_t lastEpochAcceptLogMs = 0;
        uint64_t nowAccept = nowMs();
        if (nowAccept - lastEpochAcceptLogMs >= 5000)
        {
            printf("%s [SERVER INPUT EPOCH ACCEPT] playerId=%u epoch=%u "
                   "clientPos=(%.2f,%.2f,%.2f) serverPosBefore=(%.2f,%.2f,%.2f)\n",
                   serverTimestamp(), p.id, in->header.transformEpoch,
                   in->clientPx, in->clientPy, in->clientPz,
                   p.pos.x, p.pos.y, p.pos.z);
            lastEpochAcceptLogMs = nowAccept;
        }
    }

    // Process respawn serial even when dead — Space press requests instant respawn
    if (in->respawnSerial != 0 && in->respawnSerial != p.lastRespawnSerial)
    {
        p.lastRespawnSerial = in->respawnSerial;
        printf("%s [SERVER RESPAWN REQUEST] playerId=%u dead=%d respawnSerial=%u\n",
               serverTimestamp(), p.id, (int)p.dead, (unsigned)in->respawnSerial);
        if (p.dead)
        {
            p.instantRespawnRequested = true;
            printf("%s [SERVER RESPAWN ACCEPT] playerId=%u reason=instant-respawn-serial\n",
                   serverTimestamp(), p.id);
        }
    }

    if (p.dead)
    {
        p.input.attackPressed = false;
        return;
    }
    p.input.wish = {in->wishX, in->wishY};
    p.input.camForward = {in->camForwardX, in->camForwardY, in->camForwardZ};
    p.input.yaw = in->yaw;
    p.input.tick = in->header.tick;

    {
        static uint64_t lastLookServerRxLogMs = 0;
        uint64_t nowLookRx = nowMs();
        if (nowLookRx - lastLookServerRxLogMs >= 1000)
        {
            printf("[LOOK SERVER RX] playerId=%u serverTick=%u yaw=%.2f forward=(%.2f,%.2f,%.2f) "
                   "pos=(%.2f,%.2f,%.2f)\n",
                   p.id, nowMs() % 65536,
                   in->yaw, in->camForwardX, in->camForwardY, in->camForwardZ,
                   p.pos.x, p.pos.y, p.pos.z);
            lastLookServerRxLogMs = nowLookRx;
        }
    }
    p.equippedSlot = in->equippedSlot;
    p.weaponState = in->weaponState;
    p.pingMs = std::clamp(in->clientPingMs, 0, 9999);
    p.sizeScale = std::max(in->sizeScale, 0.001f);

    // Reconstruct per-input booleans from state flags for existing server code
    p.input.jumpHeld = (in->stateFlags & NET_STATE_JUMPING) != 0;
    p.input.dashPressed = (in->stateFlags & NET_STATE_DASHING) != 0;
    p.input.freezeHeld = (in->stateFlags & NET_STATE_FREEZING) != 0;

    // Store the received visual state flags by replacement (not OR).
    // This preserves client walking/freezing/dashing state for snapshot replication.
    {
        constexpr uint16_t VALID_STATE_FLAGS =
            NET_STATE_WALKING | NET_STATE_JUMPING |
            NET_STATE_DASHING | NET_STATE_DOWN_DASHING |
            NET_STATE_FREEZING | NET_STATE_ATTACKING;
        p.inputStateFlags = in->stateFlags & VALID_STATE_FLAGS;

        static uint64_t lastWalkStoreLogMs = 0;
        uint64_t nowWalkStore = nowMs();
        if (nowWalkStore - lastWalkStoreLogMs >= 1000)
        {
            printf("[WALK SERVER STORE] playerId=%u storedStateFlags=0x%04x walkingBit=%d\n",
                   p.id, (unsigned)p.inputStateFlags,
                   (int)((p.inputStateFlags & NET_STATE_WALKING) != 0));
            lastWalkStoreLogMs = nowWalkStore;
        }
    }

    {
        static uint64_t lastWalkRxLogMs = 0;
        uint64_t nowWalkRx = nowMs();
        if (nowWalkRx - lastWalkRxLogMs >= 1000)
        {
            printf("[WALK SERVER RX] playerId=%u receivedStateFlags=0x%04x "
                   "walkingBit=%d wish=(%.2f,%.2f) clientVel=(%.2f,%.2f,%.2f)\n",
                   p.id, (unsigned)in->stateFlags,
                   (int)((in->stateFlags & NET_STATE_WALKING) != 0),
                   in->wishX, in->wishY, in->clientVx, in->clientVy, in->clientVz);
            lastWalkRxLogMs = nowWalkRx;
        }
    }

    // Update event serials from input
    // Dash serial: first new serial also triggers server-side dash impulse
    if (in->dashSerial != 0 && in->dashSerial != p.lastDashSerial)
    {
        p.lastDashSerial = in->dashSerial;
        p.input.dashPressed = true;
    }
    if (in->groundJumpSerial != 0 && in->groundJumpSerial != p.lastPresentationGroundJumpSerial)
        p.lastPresentationGroundJumpSerial = in->groundJumpSerial;
    if (in->airJumpSerial != 0 && in->airJumpSerial != p.lastPresentationAirJumpSerial)
        p.lastPresentationAirJumpSerial = in->airJumpSerial;
    if (in->downDashSerial != 0 && in->downDashSerial != p.lastPresentationDownDashSerial)
        p.lastPresentationDownDashSerial = in->downDashSerial;
    if (in->freezeSerial != 0 && in->freezeSerial != p.lastPresentationFreezeSerial)
        p.lastPresentationFreezeSerial = in->freezeSerial;
    if (in->directionChangeSerial != 0 && in->directionChangeSerial != p.lastPresentationDirectionChangeSerial)
        p.lastPresentationDirectionChangeSerial = in->directionChangeSerial;
    if (in->equipSerial != 0 && in->equipSerial != p.lastEquipSerial)
    {
        p.lastEquipSerial = in->equipSerial;
        p.equippedSlot = in->equippedSlot;
    }

    // Also update presentation dash serial from input (separate from simulation serial)
    if (in->dashSerial != 0 && in->dashSerial != p.lastPresentationDashSerial)
    {
        p.lastPresentationDashSerial = in->dashSerial;
        printf("%s [DASH PRESENTATION SERIAL] playerId=%u dashSerial=%u lastSimSerial=%u\n",
               serverTimestamp(), p.id, (unsigned)in->dashSerial, (unsigned)p.lastDashSerial);
    }

    const bool attackPressed = in->attackPressed != 0;
    if (attackPressed && !p.input.attackPressed)
        p.attackQueued = true;
    p.input.attackPressed = attackPressed;

    const glm::vec3 reportedPosition{
        in->clientPx, in->clientPy, in->clientPz};
    const glm::vec3 reportedVelocity{
        in->clientVx, in->clientVy, in->clientVz};
    const bool finiteState =
        std::isfinite(reportedPosition.x) &&
        std::isfinite(reportedPosition.y) &&
        std::isfinite(reportedPosition.z) &&
        std::isfinite(reportedVelocity.x) &&
        std::isfinite(reportedVelocity.y) &&
        std::isfinite(reportedVelocity.z);
    const float reportedSpeed = finiteState
        ? glm::length(reportedVelocity)
        : std::numeric_limits<float>::infinity();
    const float stateDelta = finiteState
        ? glm::length(reportedPosition - p.pos)
        : std::numeric_limits<float>::infinity();

    bool allowClientTransform = true;
    const char* rejectReason = nullptr;

    // ── Authoritative transform gate ──────────────────────────────────
    // After a spawn, respawn, teleport, or epoch change, the server waits
    // for the client to acknowledge the new authoritative position before
    // accepting any client-reported transform.
    if (p.awaitingAuthoritativeTransformAck)
    {
        const bool epochMatches =
            in->header.transformEpoch == p.authoritativeTransformEpoch;
        const float distanceFromAuthoritative = finiteState
            ? glm::length(reportedPosition - p.authoritativeTransformPosition)
            : std::numeric_limits<float>::infinity();
        constexpr float AUTHORITATIVE_ACK_DISTANCE = 5.0f;
        const bool acknowledged =
            epochMatches && finiteState &&
            distanceFromAuthoritative <= AUTHORITATIVE_ACK_DISTANCE;

        if (!acknowledged)
        {
            allowClientTransform = false;
            if (!epochMatches)
                rejectReason = "wrong-epoch";
            else if (!finiteState)
                rejectReason = "non-finite";
            else
                rejectReason = "too-far-from-authoritative";
        }
        else
        {
            p.awaitingAuthoritativeTransformAck = false;
            printf("%s [SERVER TRANSFORM ACK] playerId=%u epoch=%u "
                   "position=(%.2f,%.2f,%.2f) distance=%.2f timeSinceAssignment=%llums\n",
                   serverTimestamp(), p.id,
                   (unsigned)p.transformEpoch,
                   reportedPosition.x, reportedPosition.y, reportedPosition.z,
                   distanceFromAuthoritative,
                   (unsigned long long)(nowMs() - p.authoritativeTransformAssignedMs));
        }
    }

    // ── Component-aware movement validation ────────────────────────────
    // Validate horizontal, upward, and downward velocity separately so
    // that a fast fall cannot be blocked by a combined total-speed limit.
    const glm::vec2 reportedHorizontalVelocity{
        reportedVelocity.x, reportedVelocity.y};
    const float horizontalSpeed = glm::length(reportedHorizontalVelocity);
    const float upwardSpeed = std::max(0.0f, reportedVelocity.z);
    const float downwardSpeed = std::max(0.0f, -reportedVelocity.z);

    bool speedValid = false;
    const char* speedFailReason = nullptr;

    if (horizontalSpeed <= MAX_HORIZONTAL_SPEED &&
        upwardSpeed <= MAX_UPWARD_SPEED &&
        downwardSpeed <= MAX_DOWNWARD_SPEED)
    {
        speedValid = true;
    }
    else if (horizontalSpeed > MAX_HORIZONTAL_SPEED)
        speedFailReason = "horizontal-speed";
    else if (upwardSpeed > MAX_UPWARD_SPEED)
        speedFailReason = "upward-speed";
    else
        speedFailReason = "downward-speed";

    // ── Elapsed-time trajectory validation ─────────────────────────────
    // Compare the reported position against the last *accepted* client
    // transform, not against the server-simulated p.pos.  This prevents
    // a single rejected packet from freezing movement permanently.
    const uint64_t currentMs = nowMs();
    const float elapsedSeconds =
        p.hasAcceptedClientTransform
            ? std::clamp(
                float(currentMs - p.lastAcceptedClientTransformMs) / 1000.0f,
                1.0f / 240.0f,
                0.5f)
            : SERVER_DT;

    const glm::vec3 acceptedDelta =
        reportedPosition - p.lastAcceptedClientPosition;

    const float horizontalDelta =
        glm::length(glm::vec2(acceptedDelta.x, acceptedDelta.y));
    const float verticalDelta = std::abs(acceptedDelta.z);

    const float allowedHorizontalDelta =
        MAX_HORIZONTAL_SPEED * elapsedSeconds + HORIZONTAL_NET_TOL;
    const float allowedVerticalDelta =
        MAX_DOWNWARD_SPEED * elapsedSeconds + VERTICAL_NET_TOL;

    const bool trajectoryValid =
        horizontalDelta <= allowedHorizontalDelta &&
        verticalDelta <= allowedVerticalDelta;

    // ── Continuous-fall recovery path ──────────────────────────────────
    // If the raw position delta exceeds the envelope, but the client is
    // clearly falling (descending, downward velocity, horizontal within
    // bounds), re-acquire via gravity prediction.
    bool accept = false;
    const char* acceptReason = nullptr;

    if (allowClientTransform && finiteState && speedValid)
    {
        if (trajectoryValid)
        {
            accept = true;
            acceptReason = "normal";
        }
        else if (downwardSpeed > 0.0f &&
                 reportedPosition.z <= p.lastAcceptedClientPosition.z + 2.0f &&
                 horizontalDelta <= allowedHorizontalDelta &&
                 !p.dead &&
                 !p.awaitingAuthoritativeTransformAck)
        {
            // Predict Z from last accepted state using server gravity
            const float predictedZ =
                p.lastAcceptedClientPosition.z +
                p.lastAcceptedClientVelocity.z * elapsedSeconds +
                0.5f * (-58.0f) * elapsedSeconds * elapsedSeconds;

            if (std::abs(reportedPosition.z - predictedZ) <= FALL_PREDICTION_TOL)
            {
                accept = true;
                acceptReason = "continuous-fall";
            }
        }
    }

    if (accept)
    {
        p.pos = reportedPosition;
        p.vel = reportedVelocity;
        p.clientStateUpdated = true;

        p.lastAcceptedClientPosition = reportedPosition;
        p.lastAcceptedClientVelocity = reportedVelocity;
        p.lastAcceptedClientTransformMs = currentMs;
        p.hasAcceptedClientTransform = true;

        // Log continuous-fall acceptances below map bounds
        if (strcmp(acceptReason, "continuous-fall") == 0 &&
            reportedPosition.z < 0.0f)
        {
            static uint64_t lastFallAcceptLogMs = 0;
            if (currentMs - lastFallAcceptLogMs >= 250)
            {
                printf("%s [SERVER FALL ACCEPT] playerId=%u "
                       "posZ=%.2f velZ=%.2f horizontalSpeed=%.2f "
                       "downwardSpeed=%.2f elapsedMs=%llu "
                       "horizontalDelta=%.2f verticalDelta=%.2f "
                       "allowedHorizontal=%.2f allowedVertical=%.2f\n",
                       serverTimestamp(), p.id,
                       reportedPosition.z, reportedVelocity.z,
                       horizontalSpeed, downwardSpeed,
                       (unsigned long long)(currentMs - p.lastAcceptedClientTransformMs),
                       horizontalDelta, verticalDelta,
                       allowedHorizontalDelta, allowedVerticalDelta);
                lastFallAcceptLogMs = currentMs;
            }
        }
    }
    else if (allowClientTransform)
    {
        static uint64_t lastRejectedStateLogMs = 0;
        if (currentMs - lastRejectedStateLogMs >= 500)
        {
            printf("%s [SERVER MOVEMENT REJECT] playerId=%u "
                   "reason=%s "
                   "horizontalSpeed=%.2f upwardSpeed=%.2f downwardSpeed=%.2f "
                   "horizontalDelta=%.2f verticalDelta=%.2f "
                   "allowedHorizontal=%.2f allowedVertical=%.2f "
                   "elapsedMs=%llu "
                   "lastAcceptedPos=(%.2f,%.2f,%.2f) "
                   "serverPos=(%.2f,%.2f,%.2f) "
                   "clientPos=(%.2f,%.2f,%.2f)\n",
                   serverTimestamp(), p.id,
                   speedValid
                       ? (trajectoryValid ? "unknown" : "trajectory")
                       : speedFailReason,
                   horizontalSpeed, upwardSpeed, downwardSpeed,
                   horizontalDelta, verticalDelta,
                   allowedHorizontalDelta, allowedVerticalDelta,
                   (unsigned long long)(currentMs - p.lastAcceptedClientTransformMs),
                   p.lastAcceptedClientPosition.x,
                   p.lastAcceptedClientPosition.y,
                   p.lastAcceptedClientPosition.z,
                   p.pos.x, p.pos.y, p.pos.z,
                   reportedPosition.x, reportedPosition.y, reportedPosition.z);
            lastRejectedStateLogMs = currentMs;
        }
    }
    else
    {
        static uint64_t lastTransformGateLogMs = 0;
        const uint64_t gateNowMs = nowMs();
        if (gateNowMs - lastTransformGateLogMs >= 500)
        {
            printf("%s [SERVER TRANSFORM GATE] playerId=%u "
                   "accepted=0 packetEpoch=%u expectedEpoch=%u "
                   "clientPos=(%.2f,%.2f,%.2f) authoritativePos=(%.2f,%.2f,%.2f) "
                   "distance=%.2f reason=%s\n",
                   serverTimestamp(), p.id,
                   in->header.transformEpoch, (unsigned)p.authoritativeTransformEpoch,
                   reportedPosition.x, reportedPosition.y, reportedPosition.z,
                   p.authoritativeTransformPosition.x,
                   p.authoritativeTransformPosition.y,
                   p.authoritativeTransformPosition.z,
                   finiteState
                       ? glm::length(reportedPosition - p.authoritativeTransformPosition)
                       : std::numeric_limits<float>::infinity(),
                   rejectReason ? rejectReason : "unknown");
            lastTransformGateLogMs = gateNowMs;
        }
    }
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
                      const char* buffer)
{
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    auto it = players.find(header->playerId);
    if (it != players.end())
    {
        printf("%s [SERVER LEAVE] id=%u name=\"%s\"\n",
               serverTimestamp(), it->second.id, it->second.name.c_str());
        players.erase(it);
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
                       const HeadlessWorld* world)
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
        sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
        ++totalPacketsOut;
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
        sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
        ++totalPacketsOut;
        printf("%s [SERVER JOIN REJECT] reason=empty-token\n", serverTimestamp());
        return;
    }

    // If registered with coordinator, validate the join token
    if (!gServerCoordinatorCode.empty())
    {
        printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        if (!coordinatorValidateJoin(gServerCoordinatorCode, joinTokenStr))
        {
            printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s valid=0\n",
                   gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
            // Fallback: check against stored join token from registration
            if (joinTokenStr == gServerCoordinatorJoinToken)
            {
                printf("[ROOM TOKEN VALIDATE] fallback local token match code=%s\n",
                       gServerCoordinatorCode.c_str());
            }
            else
            {
                JoinRejectPacket reject{};
                reject.header.type = PACKET_JOIN_REJECT;
                reject.header.tick = tick;
                reject.reason = 2; // bad token
                sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
                ++totalPacketsOut;
                printf("%s [SERVER JOIN REJECT] reason=coordinator-rejected-token\n", serverTimestamp());
                return;
            }
        }
        printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s valid=1\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        printf("%s [SERVER JOIN] coordinator validated token for %s\n",
               serverTimestamp(), join->name);
    }
    else
    {
        printf("%s [SERVER JOIN] no coordinator — accepting %s without validation\n",
               serverTimestamp(), join->name);
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
    p.addr = from;
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
    accept.tickRate = SERVER_TICK_RATE;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    std::memset(accept.mapId, 0, sizeof(accept.mapId));
    std::strncpy(accept.mapId, gServerMapId.c_str(), sizeof(accept.mapId) - 1);
    sendto(sock, (const char*)&accept, sizeof(accept), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;
}

void handleReconnectRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ReconnectRequestPacket))
        return;
    const ReconnectRequestPacket* req = reinterpret_cast<const ReconnectRequestPacket*>(buffer);

    // Find player with matching reconnect token
    uint32_t foundId = 0;
    for (auto& kv : players)
    {
        if (kv.second.reconnectToken == req->reconnectToken)
        {
            foundId = kv.first;
            break;
        }
    }

    if (foundId == 0)
    {
        printf("%s [SERVER RECONNECT] rejected token=%s not-found\n",
               serverTimestamp(), req->reconnectToken);
        return;
    }

    ServerPlayer& p = players[foundId];
    p.addr = from;
    p.lastHeardMs = nowMs();
    p.reconnectToken = generateReconnectToken(); // rotate token
    beginAuthoritativeTransform(p, p.pos, p.vel, p.yaw, "reconnect");

    ReconnectAcceptPacket accept{};
    accept.header.type = PACKET_RECONNECT_ACCEPT;
    accept.header.tick = tick;
    accept.header.playerId = foundId;
    accept.assignedPlayerId = foundId;
    accept.tickRate = SERVER_TICK_RATE;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    accept.restoredHealth = p.health;
    accept.restoredKills = p.kills;
    accept.restoredDeaths = p.deaths;
    accept.restorePx = p.pos.x;
    accept.restorePy = p.pos.y;
    accept.restorePz = p.pos.z;
    sendto(sock, (const char*)&accept, sizeof(accept), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;

    printf("%s [SERVER RECONNECT] accepted id=%u name=\"%s\" health=%d\n",
           serverTimestamp(), foundId, p.name.c_str(), p.health);
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
    SnapshotPacket snapshot{};
    snapshot.header.type = PACKET_SNAPSHOT;
    snapshot.header.tick = tick;
    uint32_t index = 0;
    for (const auto& kv : players)
    {
        if (index >= MAX_SNAPSHOT_ENTITIES)
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
        snapshot.entities[index++] = makePlayerEntity(kv.second);
        ++snapshot.playerCount;
    }
    for (const auto& kv : npcs)
    {
        if (index >= MAX_SNAPSHOT_ENTITIES)
            break;
        snapshot.entities[index++] = makeNpcEntity(kv.second);
        ++snapshot.npcCount;
    }
    snapshot.entityCount = index;

    if (tick % 60 == 0)
    {
        printf("%s [SERVER SNAPSHOT BUILD] tick=%u playersIncluded=%u npcsIncluded=%u entitiesIncluded=%u\n",
               serverTimestamp(), tick, snapshot.playerCount, snapshot.npcCount, snapshot.entityCount);
        for (uint32_t i = 0; i < snapshot.entityCount; ++i)
            logSnapshotEntity(snapshot.entities[i]);
    }

    for (const auto& kv : players)
    {
        bool sent = false;
        if (kv.second.transport)
        {
            sent = kv.second.transport->send(&snapshot, sizeof(snapshot));
        }
        else
        {
            int bytesSent = sendto(
                sock, (const char*)&snapshot, sizeof(snapshot), 0,
                (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            sent = (bytesSent != SOCKET_ERROR);
            if (!sent)
                printf("%s [NET TX ERROR] sendto failed id=%u error=%d\n",
                       serverTimestamp(), kv.first, WSAGetLastError());
        }
        ++totalPacketsOut;
        if (tick % 60 == 0)
            printf("%s [SERVER SNAPSHOT SEND] toClientId=%u sent=%d bytes=%zu\n",
                   serverTimestamp(), kv.first, (int)sent, sizeof(snapshot));
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
        int bytesSent = sendto(
            sock, (const char*)&packet, sizeof(packet), 0,
            (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        if (bytesSent == SOCKET_ERROR)
            printf("%s [NET TX ERROR] sendDisagreementToAll failed id=%u error=%d\n",
                   serverTimestamp(), kv.first, WSAGetLastError());
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
            sendto(sock, (const char*)&pd.packet, sizeof(pd.packet), 0,
                   (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            ++totalPacketsOut;
        }

        --pd.retransmitsLeft;
        if (pd.retransmitsLeft <= 0)
            pd.active = false;
    }
}

} // namespace MimitaNet
