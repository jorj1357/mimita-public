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
//   commit(candidate)              -> make the candidate active, FreeLibrary old.
//   reject(candidate)              -> shutdown + FreeLibrary the candidate.
//
// This is header-only scaffolding. It is not called by the EXE yet, so it adds
// no translation unit or link input.

#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <utility>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

struct LoadedGeneration {
    HMODULE module = nullptr;
    const GameModuleV1* api = nullptr;
    std::uint64_t generation = 0;
    std::string codeHash;
    std::wstring path;

    bool valid() const { return module != nullptr && api != nullptr; }

    void reset() {
        module = nullptr;
        api = nullptr;
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

        out->module = module;
        out->api = api;
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
            ::FreeLibrary(active_.module);
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
            ::FreeLibrary(active_.module);
        }
        active_.reset();
    }

private:
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
