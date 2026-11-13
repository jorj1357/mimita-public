// 2026-11-13
// Whole-game GameModuleV1 entry point.
//
// This is the real whole-game export, not the probe export. It initializes the
// resident capability bridges and exposes the inbound command dispatch. Full
// engine tick/render orchestration is the next migration step; this entry keeps
// the DLL loadable and event-capable while the link is completed.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "engine/whole-game-entry.h"
#include "game/server-command-dispatch.h"
#include "host/abi/game-host-abi.h"
#include "host/abi/transport-capability.h"

namespace mimita::game {
void initializeHostLog(const mimita::host::HostServicesV1* services);
void initializeHostTransport(const mimita::host::HostServicesV1* services);
void initializeHostModules(const mimita::host::HostServicesV1* services);
void initializeHostAudio(const mimita::host::HostServicesV1* services);
void initializeHostEffects(const mimita::host::HostServicesV1* services);
void initializeHostDuel(const mimita::host::HostServicesV1* services);
void initializeHostPersistence(const mimita::host::HostServicesV1* services);
void initializeHostJobsWatch(const mimita::host::HostServicesV1* services);
void initializeHostEntity(const mimita::host::HostServicesV1* services);
void initializeHostLifetime(const mimita::host::HostServicesV1* services);
void initializeHostRender(const mimita::host::HostServicesV1* services);
void initializeHostReplayWorker(const mimita::host::HostServicesV1* services);
void initializeHostRenderDevice(const mimita::host::HostServicesV1* services);
void initializeHostDiagnostics(const mimita::host::HostServicesV1* services);
void initializeHostIce(const mimita::host::HostServicesV1* services);
void initializeHostTransportSession(const mimita::host::HostServicesV1* services);
void initializeHostAuthoredRebuild(const mimita::host::HostServicesV1* services);
void initializeHostJobs(const mimita::host::HostServicesV1* services);
void initializeHostFiles(const mimita::host::HostServicesV1* services);
void initializeHostSocket(const mimita::host::HostServicesV1* services);
void initializeHostGlResources(const mimita::host::HostServicesV1* services);
void initializeResidentState(const mimita::host::HostServicesV1* services);
}  // namespace mimita::game

namespace mimita::host {
namespace {

const HostServicesV1* gServices = nullptr;

HostResult gameInitialize(const HostServicesV1* services) {
    if (!services || services->version != kHostServicesVersion ||
        services->structSize < sizeof(HostServicesV1) ||
        !services->log || !services->get_capability) {
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
    mimita::game::initializeHostReplayWorker(services);
    mimita::game::initializeHostRenderDevice(services);
    mimita::game::initializeHostDiagnostics(services);
    mimita::game::initializeHostIce(services);
    mimita::game::initializeHostTransportSession(services);
    mimita::game::initializeHostAuthoredRebuild(services);
    mimita::game::initializeHostJobs(services);
    mimita::game::initializeHostFiles(services);
    mimita::game::initializeHostSocket(services);
    mimita::game::initializeHostGlResources(services);
    mimita::game::initializeResidentState(services);
    return HostResult::Ok;
}

HostResult gameQuiesce(QuiesceMode) {
    return HostResult::Ok;
}

HostResult gameShutdown() {
    mimita::game::shutdownWholeGameEntry();
    gServices = nullptr;
    return HostResult::Ok;
}

HostResult gameBootstrap(const GameStartupV1* startup) {
    if (!startup || startup->structSize < sizeof(GameStartupV1)) {
        return HostResult::InvalidArgument;
    }
    return mimita::game::initializeWholeGameEntry(
               startup->width > 0 ? startup->width : 1024,
               startup->height > 0 ? startup->height : 768,
               startup->title ? startup->title : "MiMITA Game")
        ? HostResult::Ok : HostResult::InternalError;
}

std::int32_t gameShouldClose() {
    return mimita::game::wholeGameShouldClose() ? 1 : 0;
}

HostResult gameTick(double deltaSeconds) {
    static bool frameInitialized = false;
    if (!frameInitialized) {
        if (!mimita::game::initializeWholeGameEntry(1024, 768, "MiMITA Game")) {
            return HostResult::InternalError;
        }
        frameInitialized = true;
    }
    mimita::game::tickWholeGame(deltaSeconds);
    return HostResult::Ok;
}

HostResult gameRender(void* renderContext) {
    mimita::game::renderWholeGame(renderContext);
    return HostResult::Ok;
}

HostResult gameEvent(std::uint32_t eventId, void* payload, std::size_t payloadSize,
                     std::int32_t* handled) {
    if (handled) {
        *handled = 0;
    }
    if (eventId == kEventTransportMessage) {
        if (!payload || payloadSize < sizeof(TransportMessageV1)) {
            return HostResult::InvalidArgument;
        }
        auto* message = static_cast<TransportMessageV1*>(payload);
        if (message->version != kTransportCapabilityVersion ||
            message->structSize != sizeof(TransportMessageV1)) {
            return HostResult::InvalidArgument;
        }
        const bool handledMessage =
            mimita::game::dispatchPingMessage(message, gServices);
        if (handled) {
            *handled = handledMessage ? 1 : 0;
        }
        return handledMessage ? HostResult::Ok : HostResult::NotFound;
    }
    if (eventId != kEventTransportDispatch) {
        return HostResult::NotFound;
    }
    if (!payload || payloadSize < sizeof(TransportDispatchV1)) {
        return HostResult::InvalidArgument;
    }
    auto* dispatch = static_cast<TransportDispatchV1*>(payload);
    if (dispatch->version != kTransportCapabilityVersion ||
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

std::uint32_t gameExportState(void*, std::uint32_t) {
    return 0;
}

HostResult gameImportState(const void*, std::uint32_t) {
    return HostResult::Ok;
}

}  // namespace

GameModuleV1 makeWholeGameApi() {
    GameModuleV1 api{};
    api.abiVersion = kGameHostAbiVersion;
    api.structSize = static_cast<std::uint32_t>(sizeof(GameModuleV1));
    api.version = 2;
    api.stateSchema = 1;
    api.generation = 1;
    const char* hash = "whole-game";
    std::memcpy(api.codeHash, hash, std::strlen(hash));
    const char* name = "game";
    std::memcpy(api.name, name, std::strlen(name));
    api.initialize = &gameInitialize;
    api.quiesce = &gameQuiesce;
    api.shutdown = &gameShutdown;
    api.tick = &gameTick;
    api.render = &gameRender;
    api.on_event = &gameEvent;
    api.export_state = &gameExportState;
    api.import_state = &gameImportState;
    api.frameCapable = 0;  // full DLL bootstrap is not yet safe; see hot-reload spec §2.3.1
    api.bootstrap = &gameBootstrap;
    api.should_close = &gameShouldClose;
    return api;
}

}  // namespace mimita::host

extern "C" __declspec(dllexport) const mimita::host::GameModuleV1*
GetMimitaGameModuleV1(std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameHostAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameModuleV1 api =
        mimita::host::makeWholeGameApi();
    return &api;
}
