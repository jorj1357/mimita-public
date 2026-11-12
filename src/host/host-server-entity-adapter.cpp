// 2026-11-12
// Resident entity adapter implementation for the dedicated server.

#include "host/host-server-entity-adapter.h"

#include <cstring>

namespace mimita::host {
namespace {

void copyName(char* out, std::size_t capacity, const std::string& name) {
    if (!out || capacity == 0) {
        return;
    }
    const std::size_t count = name.size() < capacity - 1 ? name.size() : capacity - 1;
    std::memcpy(out, name.data(), count);
    out[count] = 0;
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
}

void HostServerEntityAdapter::bindPlayers(
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players) {
    players_ = players;
}

void HostServerEntityAdapter::bindNpcs(
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>* npcs) {
    npcs_ = npcs;
}

void HostServerEntityAdapter::clear() {
    players_ = nullptr;
    npcs_ = nullptr;
}

HostResult HostServerEntityAdapter::readThunk(void* owner, EntityHandle handle,
                                               EntitySnapshotV1* out) {
    auto* adapter = static_cast<HostServerEntityAdapter*>(owner);
    if (!adapter || !out) {
        return HostResult::InvalidArgument;
    }
    out->structSize = static_cast<std::uint32_t>(sizeof(EntitySnapshotV1));
    out->schemaVersion = 1;
    out->handle = handle;
    out->kind = static_cast<std::int32_t>(entityKind(handle));
    out->team = -1;
    out->health = 0.0f;
    out->radius = 0.0f;
    out->flags = 0;
    out->playerFlags = 0;
    out->maxHealth = 0;
    out->displayName[0] = 0;

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
            return HostResult::Ok;
        }
        default:
            return HostResult::InvalidArgument;
    }
}

}  // namespace mimita::host
