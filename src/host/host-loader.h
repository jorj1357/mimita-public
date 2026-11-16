// 2026-11-12
// Purpose: resident EXE-side loader for one whole-game game.dll generation.
//
// Load flow:
//   loadCandidate(path, services)  -> LoadLibrary + validate ABI/size/hash +
//                                     initialize; on any failure it runs the
//                                     candidate's shutdown (partial-init cleanup)
//                                     and frees the library.
//   barrier.finishQuiesce() == Ok  -> caller guarantees no old-generation call,
//                                     callback, job, or return address remains.
//   commit(candidate)              -> make the candidate active; retire the old
//                                     module until process exit (deferred unload).
//   reject(candidate)              -> shutdown + FreeLibrary the candidate.
//
// This is header-only scaffolding. It is not called by the EXE yet, so it adds
// no translation unit or link input.

#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/game-server-abi.h"

namespace mimita::host {

struct LoadedGeneration {
    HMODULE module = nullptr;
    const GameModuleV1* api = nullptr;
    const GameServerV1* serverApi = nullptr;
    const GameServerPolicyV1* serverPolicy = nullptr;
    const GameExportV1* exportApi = nullptr;
    const GameModuleHostQueryV1* moduleHostQuery = nullptr;
    std::uint64_t generation = 0;
    std::string codeHash;
    std::wstring path;

    bool valid() const { return module != nullptr && api != nullptr; }

    void reset() {
        module = nullptr;
        api = nullptr;
        serverApi = nullptr;
        serverPolicy = nullptr;
        exportApi = nullptr;
        moduleHostQuery = nullptr;
        generation = 0;
        codeHash.clear();
        path.clear();
    }
};

class HostLoader {
public:
    HostLoader() = default;
    ~HostLoader() { unloadActive(); }

    HostLoader(const HostLoader&) = delete;
    HostLoader& operator=(const HostLoader&) = delete;

    const GameModuleV1* active() const { return active_.api; }
    std::uint64_t activeGeneration() const { return active_.generation; }
    const LoadedGeneration& activeGenerationInfo() const { return active_; }

    // Load and validate a candidate but do not make it active. The caller owns
    // the returned generation and must commit() or reject() it.
    HostResult loadCandidate(const std::wstring& candidatePath,
                             const HostServicesV1* services,
                             LoadedGeneration* out) {
        if (!services || !out) {
            return HostResult::InvalidArgument;
        }
        *out = LoadedGeneration{};

        HMODULE module = ::LoadLibraryW(candidatePath.c_str());
        if (!module) {
            return HostResult::NotFound;
        }

        const auto resolve = reinterpret_cast<GetGameModuleFn>(
            ::GetProcAddress(module, kGetGameModuleSymbol));
        if (!resolve) {
            ::FreeLibrary(module);
            return HostResult::Unsupported;
        }

        const GameModuleV1* api = resolve(kGameHostAbiVersion);
        const HostResult validation = validate(api, services);
        if (validation != HostResult::Ok) {
            ::FreeLibrary(module);
            return validation;
        }

        const HostResult initialized = api->initialize(services);
        if (initialized != HostResult::Ok) {
            // Partial initialization must still release what it acquired.
            if (api->shutdown) {
                api->shutdown();
            }
            ::FreeLibrary(module);
            return initialized;
        }

        const auto resolveServer = reinterpret_cast<GetGameServerFn>(
            ::GetProcAddress(module, kGetGameServerSymbol));
        const GameServerV1* serverApi =
            resolveServer ? resolveServer(kGameServerAbiVersion) : nullptr;
        if (serverApi &&
            (serverApi->abiVersion != kGameServerAbiVersion ||
             serverApi->structSize != sizeof(GameServerV1) ||
             !serverApi->process_packet || !serverApi->simulate_players ||
             !serverApi->simulate_world || !serverApi->publish ||
             !serverApi->gamemode)) {
            serverApi = nullptr;
        }

        const auto resolveServerPolicy = reinterpret_cast<GetGameServerPolicyFn>(
            ::GetProcAddress(module, kGetGameServerPolicySymbol));
        const GameServerPolicyV1* serverPolicy =
            resolveServerPolicy
                ? resolveServerPolicy(kGameServerPolicyAbiVersion)
                : nullptr;
        if (serverPolicy &&
            (serverPolicy->abiVersion != kGameServerPolicyAbiVersion ||
             serverPolicy->structSize != sizeof(GameServerPolicyV1) ||
             !serverPolicy->startup || !serverPolicy->world_load ||
             !serverPolicy->configure ||              !serverPolicy->create_player ||
             !serverPolicy->destroy_player ||
             !serverPolicy->make_udp_connection_id ||
             !serverPolicy->allocate_ice_connection_id ||
             !serverPolicy->legacy_endpoint_for_connection ||
             !serverPolicy->clear_actor_preset ||
             !serverPolicy->poll_reload ||
             !serverPolicy->auth_identity ||
             !serverPolicy->projectile_perf ||
             !serverPolicy->clear_runtime_objects ||
             !serverPolicy->coordinator_ice_done)) {
            serverPolicy = nullptr;
        }

        const auto resolveExport = reinterpret_cast<GetGameExportFn>(
            ::GetProcAddress(module, kGetGameExportSymbol));
        const GameExportV1* exportApi =
            resolveExport ? resolveExport(kGameExportAbiVersion) : nullptr;
        if (exportApi &&
            (exportApi->abiVersion != kGameExportAbiVersion ||
             exportApi->structSize != sizeof(GameExportV1) ||
             !exportApi->run_export)) {
            exportApi = nullptr;
        }

        const auto resolveModuleHostQuery =
            reinterpret_cast<GetGameModuleHostQueryFn>(
                ::GetProcAddress(module, kGetGameModuleHostQuerySymbol));
        const GameModuleHostQueryV1* moduleHostQuery =
            resolveModuleHostQuery
                ? resolveModuleHostQuery(kGameModuleHostQueryAbiVersion)
                : nullptr;
        if (moduleHostQuery &&
            (moduleHostQuery->abiVersion != kGameModuleHostQueryAbiVersion ||
             moduleHostQuery->structSize != sizeof(GameModuleHostQueryV1) ||
             !moduleHostQuery->map_revision ||
             !moduleHostQuery->map_objective_config ||
             !moduleHostQuery->profile_footprint ||
             !moduleHostQuery->npc_line_of_sight_blocked ||
             !moduleHostQuery->npc_raycast_blocked ||
             !moduleHostQuery->set_npc_world)) {
            moduleHostQuery = nullptr;
        }

        out->module = module;
        out->api = api;
        out->serverApi = serverApi;
        out->serverPolicy = serverPolicy;
        out->exportApi = exportApi;
        out->moduleHostQuery = moduleHostQuery;
        out->generation = api->generation;
        std::size_t hashLength = 0;
        while (hashLength < kCodeHashHexLength && api->codeHash[hashLength] != '\0') {
            ++hashLength;
        }
        out->codeHash.assign(api->codeHash, hashLength);
        out->path = candidatePath;
        return HostResult::Ok;
    }

    // The barrier must already have confirmed the active generation is quiesced.
    void commit(LoadedGeneration&& candidate) {
        if (!candidate.valid()) {
            return;
        }
        if (active_.module) {
            // Deferred unload: legacy generations still own static C++ objects
            // (workers/threads) whose DLL destructors can block FreeLibrary.
            // The host callbacks are resident now, so the old module is inert.
            retiredModules_.push_back(active_.module);
        }
        active_ = std::move(candidate);
        candidate.reset();
    }

    void reject(LoadedGeneration&& candidate) {
        if (candidate.api && candidate.api->shutdown) {
            candidate.api->shutdown();
        }
        if (candidate.module) {
            ::FreeLibrary(candidate.module);
        }
        candidate.reset();
    }

    // The barrier must already have confirmed the active generation is quiesced.
    void unloadActive() {
        if (active_.api && active_.api->shutdown) {
            active_.api->shutdown();
        }
        if (active_.module) {
            retiredModules_.push_back(active_.module);
        }
        active_.reset();
    }

private:
    std::vector<HMODULE> retiredModules_;

    static HostResult validate(const GameModuleV1* api, const HostServicesV1* services) {
        if (!api) {
            return HostResult::Unsupported;
        }
        if (api->abiVersion != kGameHostAbiVersion ||
            api->structSize != static_cast<std::uint32_t>(sizeof(GameModuleV1))) {
            return HostResult::Unsupported;
        }
        if (api->name[0] == '\0') {
            return HostResult::Unsupported;
        }
        if (!api->initialize || !api->quiesce || !api->shutdown ||
            !api->tick || !api->on_event) {
            return HostResult::Unsupported;
        }
        if (!services->log || !services->entity_read || !services->entity_write ||
            !services->tick || !services->time_seconds) {
            return HostResult::InvalidArgument;
        }
        return HostResult::Ok;
    }

    LoadedGeneration active_;
};

}  // namespace mimita::host
