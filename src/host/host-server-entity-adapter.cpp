// 2026-11-12
// Resident entity adapter implementation for the dedicated server.

#include "host/host-server-entity-adapter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mimita::host {
namespace {

float safe_size_scale(float sizeScale) {
    if (!std::isfinite(sizeScale) || sizeScale <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(sizeScale, 0.1f, 10.0f);
}

void copyName(char* out, std::size_t capacity, const std::string& name) {
    if (!out || capacity == 0) {
        return;
    }
    const std::size_t count = name.size() < capacity - 1 ? name.size() : capacity - 1;
    std::memcpy(out, name.data(), count);
    out[count] = 0;
}

void copyString(char* out, std::size_t capacity, const std::string& value) {
    copyName(out, capacity, value);
}

}  // namespace

HostServerEntityAdapter& HostServerEntityAdapter::instance() {
    static HostServerEntityAdapter adapter;
    return adapter;
}

HostServerEntityAdapter::HostServerEntityAdapter() {
    adapter_.version = kEntityCapabilityVersion;
    adapter_.structSize = static_cast<std::uint32_t>(sizeof(EntityAdapterV1));
    adapter_.owner = this;
    adapter_.read = &HostServerEntityAdapter::readThunk;
    adapter_.write = &HostServerEntityAdapter::writeThunk;
    adapter_.list = &HostServerEntityAdapter::listThunk;
    adapter_.create = &HostServerEntityAdapter::createThunk;
    adapter_.reset_transform = &HostServerEntityAdapter::resetTransformThunk;
    adapter_.remove = &HostServerEntityAdapter::removeThunk;
    adapter_.find_connection = &HostServerEntityAdapter::findConnectionThunk;
    adapter_.find_reconnect_token =
        &HostServerEntityAdapter::findReconnectTokenThunk;
    adapter_.find_join_token = &HostServerEntityAdapter::findJoinTokenThunk;
}

void HostServerEntityAdapter::bindPlayers(
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players) {
    players_ = players;
}

void HostServerEntityAdapter::bindNpcs(
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>* npcs) {
    npcs_ = npcs;
}

void HostServerEntityAdapter::bindNextIds(std::uint32_t* nextPlayerId,
                                          std::uint32_t* nextNpcId) {
    nextPlayerId_ = nextPlayerId;
    nextNpcId_ = nextNpcId;
}

void HostServerEntityAdapter::clear() {
    players_ = nullptr;
    npcs_ = nullptr;
    nextPlayerId_ = nullptr;
    nextNpcId_ = nullptr;
}

HostResult HostServerEntityAdapter::readThunk(void* owner, EntityHandle handle,
                                               EntitySnapshotV1* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !out) {
        return HostResult::InvalidArgument;
    }
    out->structSize = static_cast<std::uint32_t>(sizeof(EntitySnapshotV1));
    out->schemaVersion = 4;
    out->handle = handle;
    out->kind = static_cast<std::int32_t>(entityKind(handle));
    out->team = -1;
    out->health = 0.0f;
    out->radius = 0.0f;
    out->flags = 0;
    out->playerFlags = 0;
    out->maxHealth = 0;
    out->displayName[0] = 0;
    out->yaw = 0.0f;
    out->onGround = 0;
    out->transformEpoch = 0;
    out->accountId = 0;
    out->pingMs = 0;
    out->difficulty = 0.0f;
    out->avatarName[0] = 0;
    out->joinToken[0] = 0;
    out->reconnectToken[0] = 0;
    out->previousReconnectToken[0] = 0;
    out->previousReconnectTokenValidUntilMs = 0;
    out->spawnGeneration = 0;
    out->kills = 0;
    out->deaths = 0;
    out->reliableEventSessionId = 0;
    out->joinTokenValidated = 0;
    out->spawned = 0;
    out->lastHeardMs = 0;
    out->chatTokens = 0.0f;
    out->chatLastUpdateTick = 0;
    out->lastChatRequestId = 0;
    out->hasCachedChatEvent = 0;
    out->cachedChatEventId = 0;
    out->cachedChatRequestId = 0;
    out->cachedChatMessageId = 0;
    out->cachedChatServerTick = 0;
    out->cachedChatUtcMs = 0;
    out->cachedChatSenderAccountId = 0;
    out->cachedChatText[0] = 0;
    out->cachedChatSenderName[0] = 0;
    out->vipAccountId = 0;
    out->accountRole[0] = 0;
    out->vipTier = 0;
    out->vipStyleKind = 0;
    out->vipColorR = 0;
    out->vipColorG = 0;
    out->vipColorB = 0;
    out->vipFlags = 0;
    out->vipStyleEpoch = 0;
    out->vipDetailStyleKind = 0;
    out->vipDetailAnimation = 0;
    out->vipDetailDirection = 0;
    out->vipRainbowSpeed = 1.0f;
    out->vipSolidColor[0] = 0.62f;
    out->vipSolidColor[1] = 0.62f;
    out->vipSolidColor[2] = 0.62f;
    out->vipSolidColor[3] = 1.0f;
    out->vipColorCount = 0;
    out->progressionTicket[0] = 0;

    const std::uint32_t id = entityId(handle);
    switch (entityKind(handle)) {
        case EntityKind::Player: {
            if (!adapter->players_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->players_->find(id);
            if (it == adapter->players_->end()) {
                return HostResult::NotFound;
            }
            const MimitaNet::ServerPlayer& player = it->second;
            out->team = player.matchTeam;
            out->position[0] = player.pos.x;
            out->position[1] = player.pos.y;
            out->position[2] = player.pos.z;
            out->velocity[0] = player.vel.x;
            out->velocity[1] = player.vel.y;
            out->velocity[2] = player.vel.z;
            out->health = static_cast<float>(player.health);
            out->radius = 0.35f;
            out->maxHealth = static_cast<std::uint32_t>(player.maxHealth);
            if (player.isHost) {
                out->playerFlags |= static_cast<std::uint32_t>(PlayerFlag::Host);
            }
            if (player.adminCommandsEnabled) {
                out->playerFlags |=
                    static_cast<std::uint32_t>(PlayerFlag::AdminEnabled);
            }
            if (player.adminCommandsGranted) {
                out->playerFlags |=
                    static_cast<std::uint32_t>(PlayerFlag::AdminGranted);
            }
            if (player.connectionStale) {
                out->playerFlags |= static_cast<std::uint32_t>(PlayerFlag::Stale);
            }
            if (player.dead) {
                out->playerFlags |= static_cast<std::uint32_t>(PlayerFlag::Dead);
            }
            if (player.downed) {
                out->playerFlags |= static_cast<std::uint32_t>(PlayerFlag::Downed);
            }
            copyName(out->displayName, sizeof(out->displayName), player.name);
            out->yaw = player.yaw;
            out->onGround = player.onGround ? 1u : 0u;
            out->transformEpoch = player.transformEpoch;
            out->accountId = player.accountId;
            out->pingMs = player.pingMs > 0
                ? static_cast<std::uint32_t>(player.pingMs) : 0u;
            copyName(out->avatarName, sizeof(out->avatarName),
                     player.avatarName);
            copyString(out->joinToken, sizeof(out->joinToken),
                       player.joinToken);
            copyString(out->reconnectToken, sizeof(out->reconnectToken),
                       player.reconnectToken);
            copyString(out->previousReconnectToken,
                       sizeof(out->previousReconnectToken),
                       player.previousReconnectToken);
            out->previousReconnectTokenValidUntilMs =
                player.previousReconnectTokenValidUntilMs;
            out->spawnGeneration = player.spawnGeneration;
            out->kills = player.kills;
            out->deaths = player.deaths;
            out->reliableEventSessionId = player.reliableEventSessionId;
            out->joinTokenValidated = player.joinTokenValidated ? 1u : 0u;
            out->spawned = player.spawned ? 1u : 0u;
            out->lastHeardMs = player.lastHeardMs;
            out->chatTokens = static_cast<float>(player.chatRateTokens);
            out->chatLastUpdateTick = player.chatRateLastUpdateTick;
            out->lastChatRequestId = player.lastChatRequestId;
            out->hasCachedChatEvent = player.hasCachedChatEvent ? 1u : 0u;
            out->cachedChatEventId = player.cachedChatEvent.eventId;
            out->cachedChatRequestId = player.cachedChatEvent.requestId;
            out->cachedChatMessageId = player.cachedChatEvent.messageId;
            out->cachedChatServerTick = player.cachedChatEvent.serverTick;
            out->cachedChatUtcMs = player.cachedChatEvent.utcUnixMilliseconds;
            out->cachedChatSenderAccountId =
                player.cachedChatEvent.senderAccountId;
            std::snprintf(out->cachedChatText, sizeof(out->cachedChatText),
                          "%s", player.cachedChatEvent.utf8Message);
            std::snprintf(out->cachedChatSenderName,
                          sizeof(out->cachedChatSenderName), "%s",
                          player.cachedChatEvent.senderName);
            out->vipAccountId = player.vipAccountId;
            std::snprintf(out->accountRole, sizeof(out->accountRole), "%s",
                          player.accountRole.c_str());
            out->vipTier = player.vipAppearance.tier;
            out->vipStyleKind = player.vipAppearance.styleKind;
            out->vipColorR = player.vipAppearance.colorR;
            out->vipColorG = player.vipAppearance.colorG;
            out->vipColorB = player.vipAppearance.colorB;
            out->vipFlags = player.vipAppearance.flags;
            out->vipStyleEpoch = player.vipStyleEpoch;
            out->vipDetailStyleKind = player.vipStyleDetail.styleKind;
            out->vipDetailAnimation = player.vipStyleDetail.animation;
            out->vipDetailDirection = player.vipStyleDetail.direction;
            out->vipRainbowSpeed = player.vipStyleDetail.rainbowSpeed;
            out->vipSolidColor[0] = player.vipStyleDetail.solidColor.r;
            out->vipSolidColor[1] = player.vipStyleDetail.solidColor.g;
            out->vipSolidColor[2] = player.vipStyleDetail.solidColor.b;
            out->vipSolidColor[3] = player.vipStyleDetail.solidColor.a;
            const std::uint32_t colorCount = static_cast<std::uint32_t>(
                std::min<std::size_t>(player.vipStyleDetail.colors.size(), 32));
            out->vipColorCount = colorCount;
            for (std::uint32_t i = 0; i < colorCount; ++i) {
                out->vipColors[i][0] = player.vipStyleDetail.colors[i].r;
                out->vipColors[i][1] = player.vipStyleDetail.colors[i].g;
                out->vipColors[i][2] = player.vipStyleDetail.colors[i].b;
                out->vipColors[i][3] = player.vipStyleDetail.colors[i].a;
            }
            std::snprintf(out->progressionTicket,
                          sizeof(out->progressionTicket), "%s",
                          player.progressionTicket.c_str());
            return HostResult::Ok;
        }
        case EntityKind::Npc: {
            if (!adapter->npcs_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->npcs_->find(id);
            if (it == adapter->npcs_->end()) {
                return HostResult::NotFound;
            }
            const MimitaNet::ServerNpc& npc = it->second;
            out->team = npc.matchTeam;
            out->position[0] = npc.pos.x;
            out->position[1] = npc.pos.y;
            out->position[2] = npc.pos.z;
            out->velocity[0] = npc.vel.x;
            out->velocity[1] = npc.vel.y;
            out->velocity[2] = npc.vel.z;
            out->health = static_cast<float>(npc.health);
            out->radius = 0.5f;
            out->maxHealth = static_cast<std::uint32_t>(npc.health);
            copyName(out->displayName, sizeof(out->displayName), npc.name);
            out->yaw = npc.yaw;
            out->onGround = npc.onGround ? 1u : 0u;
            out->transformEpoch = npc.transformEpoch;
            out->difficulty = npc.difficulty;
            return HostResult::Ok;
        }
        default:
            return HostResult::InvalidArgument;
    }
}

HostResult HostServerEntityAdapter::writeThunk(void* owner, EntityHandle handle,
                                                const EntitySnapshotV1* in) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !in || in->structSize < sizeof(EntitySnapshotV1)) {
        return HostResult::InvalidArgument;
    }
    const std::uint32_t id = entityId(handle);
    switch (entityKind(handle)) {
        case EntityKind::Player: {
            if (!adapter->players_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->players_->find(id);
            if (it == adapter->players_->end()) {
                return HostResult::NotFound;
            }
            MimitaNet::ServerPlayer& player = it->second;
            player.pos = glm::vec3(in->position[0], in->position[1], in->position[2]);
            player.vel = glm::vec3(in->velocity[0], in->velocity[1], in->velocity[2]);
            player.health = static_cast<int>(in->health);
            player.matchTeam = in->team;
            if (in->maxHealth > 0) {
                player.maxHealth = static_cast<int>(in->maxHealth);
            }
            player.isHost =
                (in->playerFlags & static_cast<std::uint32_t>(PlayerFlag::Host)) != 0;
            player.adminCommandsEnabled =
                (in->playerFlags &
                 static_cast<std::uint32_t>(PlayerFlag::AdminEnabled)) != 0;
            player.adminCommandsGranted =
                (in->playerFlags &
                 static_cast<std::uint32_t>(PlayerFlag::AdminGranted)) != 0;
            player.dead =
                (in->playerFlags & static_cast<std::uint32_t>(PlayerFlag::Dead)) != 0;
            player.downed =
                (in->playerFlags & static_cast<std::uint32_t>(PlayerFlag::Downed)) != 0;
            player.yaw = in->yaw;
            player.onGround = in->onGround != 0;
            if (in->transformEpoch != 0) {
                player.transformEpoch = static_cast<std::uint16_t>(in->transformEpoch);
            }
            if (in->displayName[0] != '\0') {
                player.name = in->displayName;
            }
            if (in->avatarName[0] != '\0') {
                player.avatarName = in->avatarName;
            }
            if (in->joinToken[0] != '\0') {
                player.joinToken = in->joinToken;
            }
            if (in->reconnectToken[0] != '\0') {
                player.reconnectToken = in->reconnectToken;
            }
            if (in->previousReconnectToken[0] != '\0') {
                player.previousReconnectToken = in->previousReconnectToken;
            }
            player.previousReconnectTokenValidUntilMs =
                in->previousReconnectTokenValidUntilMs;
            player.spawnGeneration = in->spawnGeneration;
            player.kills = in->kills;
            player.deaths = in->deaths;
            player.reliableEventSessionId = in->reliableEventSessionId;
            player.joinTokenValidated = in->joinTokenValidated != 0;
            player.spawned = in->spawned != 0;
            player.lastHeardMs = in->lastHeardMs;
            player.chatRateTokens = in->chatTokens;
            player.chatRateLastUpdateTick = in->chatLastUpdateTick;
            player.lastChatRequestId = in->lastChatRequestId;
            player.hasCachedChatEvent = in->hasCachedChatEvent != 0;
            if (in->hasCachedChatEvent != 0) {
                player.cachedChatEvent.eventId = in->cachedChatEventId;
                player.cachedChatEvent.requestId = in->cachedChatRequestId;
                player.cachedChatEvent.messageId = in->cachedChatMessageId;
                player.cachedChatEvent.serverTick = in->cachedChatServerTick;
                player.cachedChatEvent.utcUnixMilliseconds =
                    in->cachedChatUtcMs;
                player.cachedChatEvent.senderAccountId =
                    in->cachedChatSenderAccountId;
                std::snprintf(player.cachedChatEvent.utf8Message,
                              sizeof(player.cachedChatEvent.utf8Message), "%s",
                              in->cachedChatText);
                std::snprintf(player.cachedChatEvent.senderName,
                              sizeof(player.cachedChatEvent.senderName), "%s",
                              in->cachedChatSenderName);
            }
            return HostResult::Ok;
        }
        case EntityKind::Npc: {
            if (!adapter->npcs_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->npcs_->find(id);
            if (it == adapter->npcs_->end()) {
                return HostResult::NotFound;
            }
            MimitaNet::ServerNpc& npc = it->second;
            npc.pos = glm::vec3(in->position[0], in->position[1], in->position[2]);
            npc.vel = glm::vec3(in->velocity[0], in->velocity[1], in->velocity[2]);
            npc.health = static_cast<int>(in->health);
            npc.matchTeam = in->team;
            npc.yaw = in->yaw;
            npc.onGround = in->onGround != 0;
            npc.difficulty = in->difficulty;
            if (in->transformEpoch != 0) {
                npc.transformEpoch = static_cast<std::uint16_t>(in->transformEpoch);
            }
            if (in->displayName[0] != '\0') {
                npc.name = in->displayName;
            }
            return HostResult::Ok;
        }
        default:
            return HostResult::InvalidArgument;
    }
}

HostResult HostServerEntityAdapter::resetTransformThunk(
    void* owner, EntityHandle handle, const float position[3],
    const float velocity[3], float yaw, std::uint32_t onGround,
    std::uint32_t invulnerabilityTicks, std::uint32_t maxAirJumps,
    std::uint32_t awaitAck) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !adapter->players_ || !position || !velocity) {
        return HostResult::InvalidArgument;
    }
    if (entityKind(handle) != EntityKind::Player) {
        return HostResult::InvalidArgument;
    }
    auto it = adapter->players_->find(entityId(handle));
    if (it == adapter->players_->end()) {
        return HostResult::NotFound;
    }
    MimitaNet::ServerPlayer& player = it->second;
    const glm::vec3 nextPosition(position[0], position[1], position[2]);
    const glm::vec3 nextVelocity(velocity[0], velocity[1], velocity[2]);

    player.pos = nextPosition;
    player.vel = nextVelocity;
    player.yaw = yaw;
    player.actorCollisionPreviousPos = nextPosition;
    player.hasActorCollisionPreviousPos = true;
    player.actorCollisionChanged = false;
    ++player.transformEpoch;
    player.awaitingAuthoritativeTransformAck = awaitAck != 0;
    player.authoritativeTransformPosition = nextPosition;
    player.authoritativeTransformEpoch = player.transformEpoch;
    player.authoritativeTransformAssignedMs = MimitaNet::nowMs();
    player.clientStateUpdated = false;
    player.lastAcceptedClientPosition = nextPosition;
    player.lastAcceptedClientVelocity = nextVelocity;
    player.lastAcceptedClientTransformMs = MimitaNet::nowMs();
    player.hasAcceptedClientTransform = true;
    player.onGround = onGround != 0;

    MovementState state;
    state.lifecycle = MovementLifecycleIdentity{
        player.spawnGeneration, static_cast<std::uint32_t>(player.transformEpoch)};
    state.movementEnabled =
        !player.dead && player.spawnState == MimitaNet::ServerPlayer::Active;
    state.position = nextPosition;
    state.baseVelocity = nextVelocity;
    state.externalImpulse = glm::vec3(0.0f);
    state.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    state.yaw = yaw;
    state.sizeScale = safe_size_scale(player.sizeScale);
    state.ground.onGround = player.onGround;
    state.ground.stableOnGround = player.onGround;
    state.ground.hasWorldContact = player.onGround;
    state.jump.airJumpsLeft = static_cast<int>(maxAirJumps);
    state.jump.airJumpArmed = true;
    state.jump.airJumpLocked = false;
    state.dash.dashAvailable = true;
    state.dash.frictionOverride = 1.0f;
    state.downDash.available = true;
    state.freeze.available = true;
    state.groundReturn.available = true;
    state.downDash = MovementDownDashState{};
    state.freeze = MovementFreezeState{};
    state.groundReturn = MovementGroundReturnState{};
    state.dashMomentumProtection = MovementDashMomentumProtectionState{};
    state.contactHistory.resetForLifecycle(state.lifecycle);
    player.movement = state;

    player.hasMovementSequence = false;
    player.lastMovementSequence = 0;
    player.hasAcceptedClientTransform = false;
    player.movementValidation.lastAcceptedSequence = 0;
    player.movementValidation.lastAcceptedClientTick = 0;
    player.lastInputCommandSequence = 0;
    player.lastProcessedInputCommandSequence = 0;
    for (auto& entry : player.inputCommandBuffer) {
        entry.valid = false;
    }

    player.broadcastPosition = nextPosition;
    player.broadcastVelocity = nextVelocity;
    player.hasBroadcastTransform = true;
    player.hasInterpSegment = false;
    player.interpFromPos = nextPosition;
    player.interpToPos = nextPosition;
    player.interpToTick = 0;
    player.interpDurationTicks = 2;
    player.interpSegmentStartTick = 0;

    if (invulnerabilityTicks > 0) {
        player.teleportInvulnerabilityTicks =
            static_cast<std::uint16_t>(invulnerabilityTicks);
        player.dead = false;
        player.health = player.maxHealth > 0 ? player.maxHealth : 100;
        player.movement.movementEnabled = true;
    }
    return HostResult::Ok;
}

HostResult HostServerEntityAdapter::removeThunk(void* owner,
                                                EntityHandle handle) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter) {
        return HostResult::InvalidArgument;
    }
    const std::uint32_t id = entityId(handle);
    switch (entityKind(handle)) {
        case EntityKind::Player: {
            if (!adapter->players_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->players_->find(id);
            if (it == adapter->players_->end()) {
                return HostResult::NotFound;
            }
            if (it->second.transport) {
                it->second.transport->close();
            }
            adapter->players_->erase(it);
            return HostResult::Ok;
        }
        case EntityKind::Npc: {
            if (!adapter->npcs_) {
                return HostResult::Unsupported;
            }
            auto it = adapter->npcs_->find(id);
            if (it == adapter->npcs_->end()) {
                return HostResult::NotFound;
            }
            adapter->npcs_->erase(it);
            return HostResult::Ok;
        }
        default:
            return HostResult::InvalidArgument;
    }
}

HostResult HostServerEntityAdapter::findConnectionThunk(void* owner,
                                                        std::uint64_t peer,
                                                        EntityHandle* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !adapter->players_ || !out || peer == 0) {
        return HostResult::InvalidArgument;
    }
    for (const auto& entry : *adapter->players_) {
        if (entry.second.hasConnectionId &&
            entry.second.connectionId.value == peer) {
            *out = makeEntityHandle(EntityKind::Player, entry.first);
            return HostResult::Ok;
        }
    }
    return HostResult::NotFound;
}

HostResult HostServerEntityAdapter::findReconnectTokenThunk(
    void* owner, const char* token, std::uint32_t includePrevious,
    EntityHandle* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !adapter->players_ || !out || !token || token[0] == '\0') {
        return HostResult::InvalidArgument;
    }
    const std::uint64_t now = MimitaNet::nowMs();
    const std::string requested(token);
    for (const auto& entry : *adapter->players_) {
        const MimitaNet::ServerPlayer& player = entry.second;
        if (player.reconnectToken == requested) {
            *out = makeEntityHandle(EntityKind::Player, entry.first);
            return HostResult::Ok;
        }
        if (includePrevious != 0 && !player.previousReconnectToken.empty() &&
            player.previousReconnectToken == requested &&
            now <= player.previousReconnectTokenValidUntilMs) {
            *out = makeEntityHandle(EntityKind::Player, entry.first);
            return HostResult::Ok;
        }
    }
    return HostResult::NotFound;
}

HostResult HostServerEntityAdapter::findJoinTokenThunk(void* owner,
                                                       const char* token,
                                                       EntityHandle* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !adapter->players_ || !out || !token || token[0] == '\0') {
        return HostResult::InvalidArgument;
    }
    const std::string requested(token);
    for (const auto& entry : *adapter->players_) {
        if (!entry.second.dead &&
            entry.second.joinToken == requested) {
            *out = makeEntityHandle(EntityKind::Player, entry.first);
            return HostResult::Ok;
        }
    }
    return HostResult::NotFound;
}

HostResult HostServerEntityAdapter::createThunk(void* owner, std::int32_t kind,
                                                EntityHandle* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !out) {
        return HostResult::InvalidArgument;
    }
    const EntityKind entityKindValue = static_cast<EntityKind>(kind);
    if (entityKindValue == EntityKind::Npc) {
        if (!adapter->npcs_ || !adapter->nextNpcId_) {
            return HostResult::Unsupported;
        }
        std::uint32_t id = (*adapter->nextNpcId_)++;
        if (id == 0) {
            id = (*adapter->nextNpcId_)++;
        }
        if (id == 0) {
            return HostResult::InternalError;
        }
        MimitaNet::ServerNpc& npc = (*adapter->npcs_)[id];
        npc.entityId = id;
        npc.name = "NPC " + std::to_string(id);
        *out = makeEntityHandle(EntityKind::Npc, id);
        return HostResult::Ok;
    }
    if (entityKindValue == EntityKind::Player) {
        if (!adapter->players_ || !adapter->nextPlayerId_) {
            return HostResult::Unsupported;
        }
        std::uint32_t id = (*adapter->nextPlayerId_)++;
        if (id == 0) {
            id = (*adapter->nextPlayerId_)++;
        }
        if (id == 0) {
            return HostResult::InternalError;
        }
        MimitaNet::ServerPlayer& player = (*adapter->players_)[id];
        player.id = id;
        *out = makeEntityHandle(EntityKind::Player, id);
        return HostResult::Ok;
    }
    return HostResult::InvalidArgument;
}

std::uint32_t HostServerEntityAdapter::listThunk(void* owner, std::int32_t kind,
                                                 EntityHandle* out,
                                                 std::uint32_t capacity) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !out || capacity == 0) {
        return 0;
    }
    const EntityKind entityKindValue = static_cast<EntityKind>(kind);
    if (entityKindValue == EntityKind::Player) {
        if (!adapter->players_) {
            return 0;
        }
        std::uint32_t written = 0;
        for (const auto& entry : *adapter->players_) {
            if (written >= capacity) {
                break;
            }
            out[written++] = makeEntityHandle(EntityKind::Player, entry.first);
        }
        return static_cast<std::uint32_t>(adapter->players_->size());
    }
    if (entityKindValue == EntityKind::Npc) {
        if (!adapter->npcs_) {
            return 0;
        }
        std::uint32_t written = 0;
        for (const auto& entry : *adapter->npcs_) {
            if (written >= capacity) {
                break;
            }
            out[written++] = makeEntityHandle(EntityKind::Npc, entry.first);
        }
        return static_cast<std::uint32_t>(adapter->npcs_->size());
    }
    return 0;
}

}  // namespace mimita::host
