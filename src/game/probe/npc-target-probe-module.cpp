// 2026-11-12
// Temporary validation-only game generation. live activation probe generation 21.
//
// Exports the versioned GameModuleV1 lifecycle and answers one real NPC
// decision: which enemy should this NPC target. It links only the pure targeting
// policy, so it can be built without the rest of the game.
//
// The file name ends in "-module.cpp" so the canonical EXE source list excludes
// it. Only the probe worker's explicit source set and, later, the whole-game
// game.dll link it.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/npc-target-probe-abi.h"
#include "game/entity-bridge-module.h"
#include "game/server-command-dispatch.h"
#include "host/abi/transport-capability.h"
#include "network/packets.h"
#include "npc/npc-targeting.h"

namespace mimita::game {
void initializeHostLog(const mimita::host::HostServicesV1* services);
void initializeHostTransport(const mimita::host::HostServicesV1* services);
void initializeHostModules(const mimita::host::HostServicesV1* services);
void initializeHostAudio(const mimita::host::HostServicesV1* services);
void initializeHostEffects(const mimita::host::HostServicesV1* services);
void initializeHostDuel(const mimita::host::HostServicesV1* services);
void initializeHostPersistence(const mimita::host::HostServicesV1* services);
void initializeHostJobsWatch(const mimita::host::HostServicesV1* services);
void initializeHostLifetime(const mimita::host::HostServicesV1* services);
void initializeHostRender(const mimita::host::HostServicesV1* services);
}

namespace mimita::host {
namespace {

const HostServicesV1* gServices = nullptr;

HostResult probeInitialize(const HostServicesV1* services) {
    if (!services || services->version != kHostServicesVersion ||
        services->structSize < sizeof(HostServicesV1)) {
        return HostResult::InvalidArgument;
    }
    gServices = services;
    mimita::game::initializeHostLog(services);
    mimita::game::initializeHostTransport(services);
    mimita::game::initializeHostModules(services);
    mimita::game::initializeHostAudio(services);
    mimita::game::initializeHostEffects(services);
    mimita::game::initializeHostDuel(services);
    mimita::game::initializeHostPersistence(services);
    mimita::game::initializeHostJobsWatch(services);
    mimita::game::initializeHostEntity(services);
    mimita::game::initializeHostLifetime(services);
    mimita::game::initializeHostRender(services);
    return HostResult::Ok;
}

HostResult probeQuiesce(QuiesceMode) {
    return HostResult::Ok;
}

HostResult probeShutdown() {
    gServices = nullptr;
    return HostResult::Ok;
}

HostResult probeTick(double) {
    return HostResult::Ok;
}

HostResult probeRender(void*) {
    return HostResult::Ok;
}

HostResult probeEvent(std::uint32_t eventId, void* payload, std::size_t payloadSize,
                      std::int32_t* handled) {
    if (handled) {
        *handled = 0;
    }
    if (eventId == kEventTransportDispatch) {
        if (!payload || payloadSize < sizeof(TransportDispatchV1)) {
            return HostResult::InvalidArgument;
        }
        auto* dispatch = static_cast<TransportDispatchV1*>(payload);
        if (dispatch->version != mimita::host::kTransportCapabilityVersion ||
            dispatch->structSize != sizeof(TransportDispatchV1)) {
            return HostResult::InvalidArgument;
        }
        const bool handledCommand =
            mimita::game::dispatchServerCommand(dispatch, gServices);
        if (handled) {
            *handled = handledCommand ? 1 : 0;
        }
        return handledCommand ? HostResult::Ok : HostResult::NotFound;
    }
    if (eventId != kEventNpcTargetSelect) {
        return HostResult::NotFound;
    }
    if (!payload || payloadSize < sizeof(NpcTargetDecisionV1)) {
        return HostResult::InvalidArgument;
    }

    auto* decision = static_cast<NpcTargetDecisionV1*>(payload);
    if (decision->version != kNpcTargetProbeVersion ||
        decision->structSize != sizeof(NpcTargetDecisionV1)) {
        return HostResult::InvalidArgument;
    }

    NpcTargetingPolicy policy;
    policy.configured = true;
    policy.mode = static_cast<NpcTargetingMode>(decision->mode);
    policy.includePlayers = decision->includePlayers != 0;
    policy.includeNpcs = decision->includeNpcs != 0;

    const std::uint32_t count = std::min(decision->candidateCount, kNpcTargetMaxCandidates);
    std::vector<NpcTargetCandidate> candidates;
    candidates.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const NpcTargetCandidateV1& source = decision->candidates[index];
        NpcTargetCandidate candidate;
        candidate.id = source.id;
        candidate.team = source.team;
        candidate.isNpc = source.isNpc != 0;
        candidate.dead = source.dead != 0;
        candidate.pos = glm::vec3(source.relativePosition[0], source.relativePosition[1],
                                  source.relativePosition[2]);
        candidates.push_back(candidate);
    }

    decision->outTargetId = selectNpcTargetId(policy, decision->myTeam, candidates,
                                              decision->currentTargetId);
    decision->outHandled = 1;
    if (handled) {
        *handled = 1;
    }
    return HostResult::Ok;
}

std::uint32_t probeExportState(void*, std::uint32_t) {
    return 0;
}

HostResult probeImportState(const void*, std::uint32_t) {
    return HostResult::Ok;
}

}  // namespace

GameModuleV1 makeApi() {
    GameModuleV1 api{};
    api.abiVersion = kGameHostAbiVersion;
    api.structSize = static_cast<std::uint32_t>(sizeof(GameModuleV1));
    api.version = kNpcTargetProbeVersion;
    api.stateSchema = 1;
    api.generation = 1;
    const char* hash = "probe-npc-target";
    std::copy(hash, hash + sizeof("probe-npc-target") - 1, api.codeHash);
    const char* name = "game";
    std::copy(name, name + sizeof("game") - 1, api.name);
    api.initialize = &probeInitialize;
    api.quiesce = &probeQuiesce;
    api.shutdown = &probeShutdown;
    api.tick = &probeTick;
    api.render = &probeRender;
    api.on_event = &probeEvent;
    api.export_state = &probeExportState;
    api.import_state = &probeImportState;
    return api;
}

}  // namespace mimita::host

extern "C" __declspec(dllexport) const mimita::host::GameModuleV1*
GetMimitaGameModuleV1(std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameHostAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameModuleV1 api = mimita::host::makeApi();
    return &api;
}
