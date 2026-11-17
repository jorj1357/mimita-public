// 2026-11-13
// Whole-game GameModuleV1 entry point.
//
// Module entry sources are compiled into this DLL with their folded exports;
// resident ModuleHost resolves those exports from the active game module.
// This is the real whole-game export, not the probe export. It initializes the
// resident capability bridges and exposes the inbound command dispatch. Full
// engine tick/render orchestration is the next migration step; this entry keeps
// the DLL loadable and event-capable while the link is completed.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "engine/whole-game-entry.h"
#include "gui/gui-main.h"
#include "game/game-state.h"
#include "game/resident-state-module.h"
#include "debug/structured-log.h"
#include "game/server-command-dispatch.h"
#include "devtools/terminal.h"
#include "game/server-runtime.h"
#include "game/game-input-module.h"
#include "profile/local-profile-system.h"
#include "auth/auth-system.h"
#include "host/abi/game-host-abi.h"
#include "host/abi/game-server-abi.h"
#include "host/abi/transport-capability.h"

extern bool gUnlockCursorOnLaunch;

namespace mimita::game {
void initializeHostLog(const mimita::host::HostServicesV1* services);
void initializeHostTransport(const mimita::host::HostServicesV1* services);
void initializeHostModules(const mimita::host::HostServicesV1* services);
void initializeHostEffects(const mimita::host::HostServicesV1* services);
void initializeHostPersistence(const mimita::host::HostServicesV1* services);
void initializeHostJobsWatch(const mimita::host::HostServicesV1* services);
void initializeHostEntity(const mimita::host::HostServicesV1* services);
void initializeHostEntityDomain(const mimita::host::HostServicesV1* services);
void initializeHostWorld(const mimita::host::HostServicesV1* services);
void initializeHostSessionIdentity(const mimita::host::HostServicesV1* services);
void initializeHostSessionEvents(const mimita::host::HostServicesV1* services);
void initializeHostLifetime(const mimita::host::HostServicesV1* services);
void initializeHostRender(const mimita::host::HostServicesV1* services);
void initializeHostReplayWorker(const mimita::host::HostServicesV1* services);
void initializeHostRenderDevice(const mimita::host::HostServicesV1* services);
void initializeHostWindow(const mimita::host::HostServicesV1* services);
void initializeHostDiagnostics(const mimita::host::HostServicesV1* services);
void initializeHostIce(const mimita::host::HostServicesV1* services);
void initializeHostTransportSession(const mimita::host::HostServicesV1* services);
void initializeHostJobs(const mimita::host::HostServicesV1* services);
void initializeHostFiles(const mimita::host::HostServicesV1* services);
void initializeHostSocket(const mimita::host::HostServicesV1* services);
void initializeHostGlResources(const mimita::host::HostServicesV1* services);
void initializeHostState(const mimita::host::HostServicesV1* services);
void initializeResidentState(const mimita::host::HostServicesV1* services);
bool handleMapEditorHostCall(mimita::host::MapEditorHostCallV1& call);
bool handleEffectsHostCall(mimita::host::EffectsHostCallV1& call);
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
    mimita::game::initializeHostEffects(services);
    mimita::game::initializeHostPersistence(services);
    mimita::game::initializeHostJobsWatch(services);
    mimita::game::initializeHostEntity(services);
    mimita::game::initializeHostEntityDomain(services);
    mimita::game::initializeHostWorld(services);
    mimita::game::initializeHostSessionIdentity(services);
    mimita::game::initializeHostSessionEvents(services);
    mimita::game::initializeHostLifetime(services);
    mimita::game::initializeHostRender(services);
    mimita::game::initializeHostReplayWorker(services);
    mimita::game::initializeHostRenderDevice(services);
    mimita::game::initializeHostWindow(services);
    mimita::game::initializeHostDiagnostics(services);
    mimita::game::initializeHostIce(services);
    mimita::game::initializeHostTransportSession(services);
    mimita::game::initializeHostJobs(services);
    mimita::game::initializeHostFiles(services);
    mimita::game::initializeHostSocket(services);
    mimita::game::initializeHostGlResources(services);
    mimita::game::initializeHostState(services);
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

namespace {
void ensureStartupIdentity(const char* sessionToken) {
    LocalProfileSystem::instance().init();
    AuthSystem::instance().init(sessionToken ? sessionToken : "");
}
}  // namespace

HostResult gameBootstrap(const GameStartupV1* startup) {
    if (!startup || startup->structSize < sizeof(GameStartupV1)) {
        return HostResult::InvalidArgument;
    }
    ensureStartupIdentity(startup->sessionToken);
    gUnlockCursorOnLaunch = startup->unlockCursor != 0;
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
        ensureStartupIdentity(nullptr);
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
    if (eventId == kEventGameInput) {
        if (!payload || payloadSize < sizeof(GameInputEventV1)) {
            return HostResult::InvalidArgument;
        }
        auto* input = static_cast<GameInputEventV1*>(payload);
        if (input->version != 2 || input->structSize < sizeof(GameInputEventV1)) {
            return HostResult::InvalidArgument;
        }
        mimita::game::handleGameInputEvent(*input);
        if (handled) *handled = 1;
        return HostResult::Ok;
    }
    if (eventId == kEventStartupConnect) {
        if (!payload || payloadSize < sizeof(StartupConnectV1)) {
            return HostResult::InvalidArgument;
        }
        auto* request = static_cast<StartupConnectV1*>(payload);
        if (request->version != kStartupConnectVersion ||
            request->structSize < sizeof(StartupConnectV1)) {
            return HostResult::InvalidArgument;
        }
        MultiplayerConnectInfo info;
        info.shouldConnect = request->shouldConnect != 0;
        info.directAddress = request->directAddress;
        info.roomCode = request->roomCode;
        info.playerName = request->playerName;
        if (info.playerName.empty()) {
            info.playerName = AuthSystem::instance().displayName();
        }
        info.mapName = request->mapName;
        info.password = request->password;
        info.serverName = request->serverName;
        info.passwordProtected = request->passwordProtected != 0;
        if (auto* state = mimita::game::gameStatePtr()) {
            *state = GAME_PLAYING;
        }
        setPendingMultiplayerConnect(info);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "game.startup.connect", "startup-connect",
            "launcher connect request applied to game generation", 0,
            {{"should_connect", info.shouldConnect},
             {"direct_address", info.directAddress},
             {"room_code", info.roomCode},
             {"player_name", info.playerName},
             {"map_name", info.mapName}}, __FILE__, __LINE__, __func__);
        const std::uint32_t commandCount =
            request->startupCommandCount < 4 ? request->startupCommandCount : 4;
        for (std::uint32_t index = 0; index < commandCount; ++index) {
            if (request->startupCommands[index][0] != '\0') {
                mimita::game::queueStartupCommand(request->startupCommands[index]);
            }
        }
        if (handled) *handled = 1;
        return HostResult::Ok;
    }
    if (eventId == kEventAgentCommand) {
        if (!payload || payloadSize < sizeof(AgentCommandV1)) {
            return HostResult::InvalidArgument;
        }
        auto* command = static_cast<AgentCommandV1*>(payload);
        if (command->command[0] == '\0') return HostResult::InvalidArgument;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "dev-loop.command.delivered", command->requestId,
            "game generation consumed host-owned agent command", 0,
            {{"command", command->command}, {"scenario_id", command->scenarioId},
             {"step_index", command->stepIndex}, {"step_count", command->stepCount}},
            __FILE__, __LINE__, __func__);
        Terminal::instance().executeWithRequestId(command->command,
                                                  command->requestId);
        if (handled) *handled = 1;
        return HostResult::Ok;
    }
    if (eventId == kEventMapEditorHost) {
        if (!payload || payloadSize < sizeof(MapEditorHostCallV1)) {
            return HostResult::InvalidArgument;
        }
        auto* call = static_cast<MapEditorHostCallV1*>(payload);
        if (call->version != kMapEditorHostVersion ||
            call->structSize != sizeof(MapEditorHostCallV1)) {
            return HostResult::InvalidArgument;
        }
        const bool ok = mimita::game::handleMapEditorHostCall(*call);
        if (handled) *handled = ok ? 1 : 0;
        return ok ? HostResult::Ok : HostResult::NotFound;
    }
    if (eventId == kEventEffectsHost) {
        if (!payload || payloadSize < sizeof(EffectsHostCallV1)) {
            return HostResult::InvalidArgument;
        }
        auto* call = static_cast<EffectsHostCallV1*>(payload);
        if (call->version != kEffectsHostVersion ||
            call->structSize != sizeof(EffectsHostCallV1)) {
            return HostResult::InvalidArgument;
        }
        const bool ok = mimita::game::handleEffectsHostCall(*call);
        if (handled) *handled = ok ? 1 : 0;
        return ok ? HostResult::Ok : HostResult::NotFound;
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
            mimita::game::dispatchInboundMessage(message, gServices);
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
    api.frameCapable = 1;
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

extern "C" __declspec(dllexport) const mimita::host::GameServerV1*
GetMimitaGameServerV1(std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameServerAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameServerV1 api = [] {
        mimita::host::GameServerV1 server{};
        server.abiVersion = mimita::host::kGameServerAbiVersion;
        server.structSize = static_cast<std::uint32_t>(sizeof(server));
        server.runtimeRevision = 2;
        server.process_packet = &mimita::game::serverRuntimeProcessPacket;
        server.simulate_players = &mimita::game::serverRuntimeSimulatePlayers;
        server.simulate_world = &mimita::game::serverRuntimeSimulateWorld;
        server.publish = &mimita::game::serverRuntimePublish;
        server.gamemode = &mimita::game::serverRuntimeGamemode;
        return server;
    }();
    return &api;
}
