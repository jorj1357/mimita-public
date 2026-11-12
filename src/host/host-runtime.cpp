// 2026-11-12
// Resident host runtime implementation. Disabled unless MIMITA_GAME_PROBE=1.

#include "host/host-runtime.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "debug/structured-log.h"
#include "host/abi/log-capability.h"
#include "host/abi/audio-capability.h"
#include "host/abi/effects-capability.h"
#include "host/abi/module-capability.h"
#include "host/abi/transport-capability.h"
#include "audio/audio.h"
#include "audio/music-manager.h"
#include "effects/effect-part.h"
#include "hot-reload/module-host.h"
#include "host/transport-adapter.h"

namespace mimita::host {
namespace {

std::wstring widenUtf8(const char* text) {
    if (!text || !*text) {
        return std::wstring();
    }
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
    if (length > 1) {
        ::MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), length);
    }
    return result;
}

void hostLogText(void*, std::int32_t category, std::int32_t level, const char* message) {
    StructuredLogger::instance().writeEvent(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level),
        "log.text", "", message ? message : "", 0,
        nlohmann::json{{"message", message ? message : ""}},
        __FILE__, __LINE__, __FUNCTION__);
}

void hostLogEvent(void*, std::int32_t category, std::int32_t level,
                  const char* eventName, const char* correlationId,
                  const char* reason, std::uint32_t tick, const char* fieldsJson,
                  const char* sourceFile, std::int32_t sourceLine,
                  const char* functionName) {
    nlohmann::json fields = nlohmann::json::object();
    if (fieldsJson && *fieldsJson) {
        try {
            fields = nlohmann::json::parse(fieldsJson);
        } catch (...) {
            fields = nlohmann::json{{"raw", fieldsJson}};
        }
    }
    StructuredLogger::instance().writeEvent(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level),
        eventName ? eventName : "", correlationId ? correlationId : "",
        reason ? reason : "", tick, fields,
        sourceFile, sourceLine, functionName);
}

std::int32_t hostShouldLog(void*, std::int32_t category, std::int32_t level) {
    return StructuredLogger::instance().shouldLog(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level)) ? 1 : 0;
}

std::int32_t moduleInvoke(void*, ModuleOp op, const void* input,
                          std::uint32_t inputSize, void* output,
                          std::uint32_t outputSize) {
    auto& host = ModuleHost::instance();
    const auto writeInt = [&](std::int32_t value) {
        if (output && outputSize >= sizeof(std::int32_t)) {
            *static_cast<std::int32_t*>(output) = value;
        }
    };
    const auto moduleName = [&](const char*& name) {
        if (!input || inputSize == 0) {
            return false;
        }
        name = static_cast<const char*>(input);
        return name[0] != '\0';
    };
    switch (op) {
        case ModuleOp::ActiveGeneration: {
            const char* name = nullptr;
            if (!moduleName(name)) return 0;
            if (output && outputSize >= sizeof(std::uint32_t)) {
                *static_cast<std::uint32_t*>(output) = host.activeGeneration(name);
            }
            return 1;
        }
        case ModuleOp::ModuleLoaded: {
            const char* name = nullptr;
            if (!moduleName(name)) return 0;
            writeInt(host.moduleLoaded(name) ? 1 : 0);
            return 1;
        }
        case ModuleOp::MapEditorLoaded:
            writeInt(host.mapEditorLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::NpcPerceptionLoaded:
            writeInt(host.npcPerceptionLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::NpcBehaviorLoaded:
            writeInt(host.npcBehaviorLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::BeginFixedTick:
            host.beginFixedTick();
            return 1;
        case ModuleOp::EndFixedTick:
            host.endFixedTick();
            return 1;
        case ModuleOp::Startup:
            host.startup();
            return 1;
        case ModuleOp::Poll:
            host.poll();
            return 1;
    }
    return 0;
}

std::int32_t audioInvoke(void*, AudioOp op, const void* input,
                         std::uint32_t inputSize, void* output,
                         std::uint32_t outputSize) {
    auto& audio = ::AudioManager::instance();
    auto& music = ::MusicManager::instance();
    const auto readFloat = [&](float& value) {
        if (!input || inputSize < sizeof(float)) return false;
        value = *static_cast<const float*>(input);
        return true;
    };
    const auto readInt = [&](std::int32_t& value) {
        if (!input || inputSize < sizeof(std::int32_t)) return false;
        value = *static_cast<const std::int32_t*>(input);
        return true;
    };
    const auto readU32 = [&](std::uint32_t& value) {
        if (!input || inputSize < sizeof(std::uint32_t)) return false;
        value = *static_cast<const std::uint32_t*>(input);
        return true;
    };
    const auto writeInt = [&](std::int32_t value) {
        if (output && outputSize >= sizeof(std::int32_t)) {
            *static_cast<std::int32_t*>(output) = value;
        }
    };
    const auto writeFloat = [&](float value) {
        if (output && outputSize >= sizeof(float)) {
            *static_cast<float*>(output) = value;
        }
    };
    const auto readString = [&]() -> std::string {
        return input ? std::string(static_cast<const char*>(input)) : std::string();
    };
    switch (op) {
        case AudioOp::AudioPlay: {
            const auto* p = static_cast<const AudioEventPayload*>(input);
            if (!p || inputSize < sizeof(AudioEventPayload)) return 0;
            ::AudioEvent event;
            event.name = p->name;
            event.category = static_cast<::AudioCategory>(p->category);
            event.world = p->world != 0;
            event.position = glm::vec3(p->position[0], p->position[1], p->position[2]);
            event.volume = p->volume;
            event.pitch = p->pitch;
            event.maxDistance = p->maxDistance;
            event.ownerId = p->ownerId;
            event.startSeconds = p->startSeconds;
            event.endSeconds = p->endSeconds;
            event.retrigger = p->retrigger != 0;
            audio.play(event);
            return 1;
        }
        case AudioOp::AudioStopOwner: {
            std::uint32_t owner = 0;
            if (!readU32(owner)) return 0;
            audio.stopOwner(owner);
            return 1;
        }
        case AudioOp::AudioStopAll:
            audio.stopAll();
            return 1;
        case AudioOp::AudioSetDebug: {
            std::int32_t enabled = 0;
            if (!readInt(enabled)) return 0;
            audio.setDebug(enabled != 0);
            return 1;
        }
        case AudioOp::AudioDebug:
            writeInt(audio.debug() ? 1 : 0);
            return 1;
        case AudioOp::MusicUpdate: {
            float dt = 0.0f;
            if (!readFloat(dt)) return 0;
            music.update(dt);
            return 1;
        }
        case AudioOp::MusicEnterMenuMode:
            music.enterMenuMode();
            return 1;
        case AudioOp::MusicEnterGameMode:
            music.enterGameMode();
            return 1;
        case AudioOp::MusicPlayModeTrack:
            writeInt(music.playModeTrack(readString()) ? 1 : 0);
            return 1;
        case AudioOp::MusicStopModeTrack:
            music.stopModeTrack();
            return 1;
        case AudioOp::MusicPause:
            music.pause();
            return 1;
        case AudioOp::MusicResume:
            music.resume();
            return 1;
        case AudioOp::MusicSkip:
            music.skip();
            return 1;
        case AudioOp::MusicPrevious:
            music.previous();
            return 1;
        case AudioOp::MusicSetPlaylistMode: {
            std::int32_t mode = 0;
            if (!readInt(mode)) return 0;
            music.setPlaylistMode(static_cast<::MusicManager::PlaylistMode>(mode));
            return 1;
        }
        case AudioOp::MusicSetPersonalMusicOverride: {
            std::int32_t enabled = 0;
            if (!readInt(enabled)) return 0;
            music.setPersonalMusicOverride(enabled != 0);
            return 1;
        }
        case AudioOp::MusicStop:
            music.stop();
            return 1;
        case AudioOp::MusicReload:
            music.reload();
            return 1;
        case AudioOp::MusicSetVolume: {
            float volume = 1.0f;
            if (!readFloat(volume)) return 0;
            music.setVolume(volume);
            return 1;
        }
        case AudioOp::MusicVolume:
            writeFloat(music.volume());
            return 1;
        case AudioOp::MusicSetPlaybackSpeed: {
            float speed = 1.0f;
            if (!readFloat(speed)) return 0;
            music.setPlaybackSpeed(speed);
            return 1;
        }
        case AudioOp::MusicPlaybackSpeed:
            writeFloat(music.playbackSpeed());
            return 1;
        case AudioOp::MusicSetMuted: {
            std::int32_t muted = 0;
            if (!readInt(muted)) return 0;
            music.setMuted(muted != 0);
            return 1;
        }
        case AudioOp::MusicMuted:
            writeInt(music.muted() ? 1 : 0);
            return 1;
        case AudioOp::MusicIsPlaying:
            writeInt(music.isPlaying() ? 1 : 0);
            return 1;
        case AudioOp::MusicCurrentTrackInfo: {
            const std::string info = music.currentTrackInfo();
            if (output && outputSize > 0) {
                const std::size_t count =
                    info.size() < outputSize - 1 ? info.size() : outputSize - 1;
                std::memcpy(output, info.data(), count);
                static_cast<char*>(output)[count] = 0;
            }
            return 1;
        }
        case AudioOp::PlayWorldSound: {
            const auto* p = static_cast<const WorldSoundPayload*>(input);
            if (!p || inputSize < sizeof(WorldSoundPayload)) return 0;
            ::playWorldSound(p->name,
                             glm::vec3(p->position[0], p->position[1], p->position[2]),
                             p->volume, p->pitch, p->maxDistance,
                             static_cast<::AudioCategory>(p->category));
            return 1;
        }
        case AudioOp::PlayWorldSoundOwned: {
            const auto* p = static_cast<const WorldSoundPayload*>(input);
            if (!p || inputSize < sizeof(WorldSoundPayload)) return 0;
            ::playWorldSoundOwned(p->name,
                                  glm::vec3(p->position[0], p->position[1], p->position[2]),
                                  p->volume, p->pitch, p->maxDistance, p->ownerId,
                                  p->retrigger != 0,
                                  static_cast<::AudioCategory>(p->category));
            return 1;
        }
        case AudioOp::AudioListenerPosition: {
            const glm::vec3 position = ::audioListenerPosition();
            if (output && outputSize >= 3 * sizeof(float)) {
                auto* out = static_cast<float*>(output);
                out[0] = position.x;
                out[1] = position.y;
                out[2] = position.z;
            }
            return 1;
        }
        case AudioOp::ComputeImpactAudio: {
            const auto* p = static_cast<const ImpactAudioPayload*>(input);
            if (!p || inputSize < sizeof(ImpactAudioPayload)) return 0;
            float volume = 0.0f;
            float pitch = 0.0f;
            ::computeImpactAudio(p->baseVolume, p->distance, p->severity, volume, pitch);
            if (output && outputSize >= 2 * sizeof(float)) {
                auto* out = static_cast<float*>(output);
                out[0] = volume;
                out[1] = pitch;
            }
            return 1;
        }
    }
    return 0;
}

std::int32_t effectsInvoke(void*, EffectsOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize) {
    auto& effects = ::EffectPartSystem::instance();
    const auto writePointer = [&](::EffectPart* value) {
        if (output && outputSize >= sizeof(std::uintptr_t)) {
            *static_cast<std::uintptr_t*>(output) =
                reinterpret_cast<std::uintptr_t>(value);
        }
    };
    const auto payload = [&]() -> const EffectsSpawnPayload* {
        if (!input || inputSize < sizeof(EffectsSpawnPayload)) return nullptr;
        return static_cast<const EffectsSpawnPayload*>(input);
    };
    const auto vec = [](const EffectsVec3& value) {
        return glm::vec3(value.x, value.y, value.z);
    };
    switch (op) {
        case EffectsOp::Spawn: {
            const auto* effect = static_cast<const ::EffectPart*>(input);
            if (!effect || inputSize < sizeof(::EffectPart)) return 0;
            writePointer(effects.spawn(*effect));
            return 1;
        }
        case EffectsOp::SpawnFootstep: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFootstep(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnDash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDash(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnFreeze: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFreeze(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnDamageImpactSphere: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDamageImpactSphere(vec(p->a), vec(p->b), p->s0));
            return 1;
        }
        case EffectsOp::SpawnMuzzleFlash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnMuzzleFlash(vec(p->a), p->s0, p->f0, p->s1,
                                                   p->b0 != 0, p->b1 != 0));
            return 1;
        }
        case EffectsOp::SpawnTracer: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnTracer(vec(p->a), vec(p->b), p->s0, p->f0, p->s1));
            return 1;
        }
        case EffectsOp::SpawnBulletImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnBulletImpact(vec(p->a), vec(p->b), p->f0, p->f1,
                                                   p->f2));
            return 1;
        }
        case EffectsOp::SpawnDeathEllipsoid: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDeathEllipsoid(vec(p->a), vec(p->b), p->f0, p->f1,
                                                     p->f2, p->f3));
            return 1;
        }
        case EffectsOp::SpawnImpactSphereTickCfg: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnImpactSphereTickCfg(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnImpactSphereTick: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnImpactSphereTick(vec(p->a), vec(p->b), p->f0));
            return 1;
        }
        case EffectsOp::SpawnFreezeTrail: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFreezeTrail(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnDownDash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDownDash(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnBodyContactSpark: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnBodyContactSpark(vec(p->a), vec(p->b), vec(p->c),
                                                       p->f0));
            return 1;
        }
        case EffectsOp::SpawnWorldCracks: {
            const auto* p = payload();
            if (!p) return 0;
            effects.spawnWorldCracks(vec(p->a), vec(p->b), vec(p->c), p->f0, p->f1,
                                     p->f2);
            return 1;
        }
        case EffectsOp::SpawnExplosionMark: {
            const auto* p = payload();
            if (!p) return 0;
            effects.spawnExplosionMark(vec(p->a), vec(p->b));
            return 1;
        }
        case EffectsOp::QueueWorldHit: {
            const auto* p = payload();
            if (!p) return 0;
            effects.queueWorldHit(vec(p->a), vec(p->b), vec(p->c), p->f0, p->s0,
                                  p->s1);
            return 1;
        }
        case EffectsOp::DrainPendingWorldHits: {
            if (!input || inputSize < sizeof(std::int32_t)) return 0;
            effects.drainPendingWorldHits(*static_cast<const std::int32_t*>(input));
            return 1;
        }
        case EffectsOp::DestroyOwner: {
            if (!input || inputSize < sizeof(std::uint32_t)) return 0;
            effects.destroyOwner(*static_cast<const std::uint32_t*>(input));
            return 1;
        }
        case EffectsOp::PushSurfaceDecal:
            // Private resident operation; game code cannot call it directly.
            return 0;
        case EffectsOp::SpawnEntityImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnEntityImpact(vec(p->a), vec(p->b), p->s0, p->s1,
                                                    p->f0));
            return 1;
        }
        case EffectsOp::SpawnWorldImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnWorldImpact(vec(p->a), vec(p->b), p->f0, vec(p->c),
                                                   p->f1));
            return 1;
        }
        case EffectsOp::CollectAlive: {
            if (!input || inputSize < sizeof(EffectsCollectAlivePayload)) return 0;
            const auto* p = static_cast<const EffectsCollectAlivePayload*>(input);
            const int count = effects.collectAlive(
                reinterpret_cast<::EffectPartSystem::PartSnapshot*>(p->out),
                p->maxCount, p->minAlpha);
            if (output && outputSize >= sizeof(std::int32_t)) {
                *static_cast<std::int32_t*>(output) = count;
            }
            return 1;
        }
    }
    return 0;
}

LogCapabilityV1 gLogCapability{
    kLogCapabilityVersion,
    static_cast<std::uint32_t>(sizeof(LogCapabilityV1)),
    &hostLogText,
    &hostLogEvent,
    &hostShouldLog,
};


std::chrono::steady_clock::time_point gStartTime = std::chrono::steady_clock::now();

void emitProbeEvent(const char* eventName, const char* reason, std::uint32_t tick,
                    const nlohmann::json& fields) {
    if (!StructuredLogger::instance().shouldLog(StructuredCategory::Executable,
                                                StructuredLevel::Important)) {
        return;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Executable, StructuredLevel::Important, eventName, "", reason,
        tick, fields, __FILE__, __LINE__, __FUNCTION__);
}

}  // namespace

HostRuntime& HostRuntime::instance() {
    static HostRuntime runtime;
    return runtime;
}

HostRuntime::HostRuntime() : barrier_(hostBarrier()) {}

bool HostRuntime::startFromEnvironment() {
    if (started_) {
        return probeEnabled_.load(std::memory_order_acquire);
    }

    const char* enabled = std::getenv("MIMITA_GAME_PROBE");
    if (!enabled || std::strcmp(enabled, "1") != 0) {
        return false;
    }

    const char* readyPath = std::getenv("MIMITA_GAME_READY");
    readyPath_ = (readyPath && *readyPath) ? readyPath : "build/game/ready.json";

    const char* dllPath = std::getenv("MIMITA_GAME_DLL");
    if (!dllPath || !*dllPath) {
        emitProbeEvent("game.host.probe.disabled", "missing MIMITA_GAME_DLL", 0,
                       nlohmann::json{{"env", "MIMITA_GAME_DLL"}});
        return false;
    }

    services_ = HostServicesV1{};
    services_.version = kHostServicesVersion;
    services_.structSize = static_cast<std::uint32_t>(sizeof(HostServicesV1));
    services_.hostUserData = this;
    services_.log = &HostRuntime::logThunk;
    services_.entity_read = &HostRuntime::entityReadThunk;
    services_.entity_write = &HostRuntime::entityWriteThunk;
    services_.tick = &HostRuntime::tickThunk;
    services_.time_seconds = &HostRuntime::timeThunk;
    services_.reserve = &HostRuntime::reserveThunk;
    services_.release = &HostRuntime::releaseThunk;
    services_.get_capability = &HostRuntime::getCapabilityThunk;

    const std::wstring widePath = widenUtf8(dllPath);
    if (widePath.empty()) {
        emitProbeEvent("game.host.probe.failed", "invalid dll path", 0,
                       nlohmann::json{{"path", dllPath}});
        return false;
    }

    LoadedGeneration candidate;
    const HostResult loaded = loader_.loadCandidate(widePath, &services_, &candidate);
    if (loaded != HostResult::Ok) {
        emitProbeEvent("game.host.probe.failed", "candidate rejected", 0,
                       nlohmann::json{{"path", dllPath},
                                      {"result", static_cast<int>(loaded)}});
        return false;
    }

    const std::uint64_t generation = candidate.generation != 0 ? candidate.generation : 1;
    barrier_.beginGeneration(generation);
    loader_.commit(std::move(candidate));
    probeEnabled_.store(true, std::memory_order_release);
    started_ = true;

    emitProbeEvent("game.host.probe.started", "generation loaded", 0,
                   nlohmann::json{{"generation", generation},
                                  {"dll", dllPath},
                                  {"abi", kGameHostAbiVersion}});
    return true;
}

void HostRuntime::shutdown() {
    if (!started_) {
        return;
    }
    probeEnabled_.store(false, std::memory_order_release);
    barrier_.beginQuiesce(QuiesceMode::Shutdown);
    barrier_.finishQuiesce(2000);
    loader_.unloadActive();
    barrier_.shutdown();
    clearEntityAdapter();
    clearTransportAdapter();
    renderPending_.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        for (auto& entry : resources_) {
            std::free(entry.second);
        }
        resources_.clear();
    }
    started_ = false;
    emitProbeEvent("game.host.probe.stopped", "generation unloaded", 0,
                   nlohmann::json::object());
}

bool HostRuntime::deliverInbound(const TransportMessageV1& message) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !message.payload ||
        message.payloadSize == 0) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    TransportMessageV1 mutableMessage = message;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventTransportMessage, &mutableMessage, sizeof(TransportMessageV1),
        &handled);
    barrier_.leave();
    return result == HostResult::Ok && handled != 0;
}

bool HostRuntime::dispatchInbound(TransportDispatchV1* dispatch) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !dispatch ||
        !dispatch->message.payload || dispatch->message.payloadSize == 0) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    dispatch->response.structSize =
        static_cast<std::uint32_t>(sizeof(TransportResponseV1));
    dispatch->response.version = kTransportCapabilityVersion;
    dispatch->response.typeId = 0;
    dispatch->response.flags = 0;
    dispatch->response.payloadSize = 0;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventTransportDispatch, dispatch, sizeof(TransportDispatchV1), &handled);
    barrier_.leave();
    if (result != HostResult::Ok || handled == 0) {
        return false;
    }
    if (dispatch->response.payloadSize > kTransportMaxResponseBytes) {
        dispatch->response.payloadSize = 0;
        return false;
    }
    return dispatch->response.payloadSize != 0;
}

void HostRuntime::poll() {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastPoll_ < std::chrono::milliseconds(250)) {
        return;
    }
    lastPoll_ = now;

    std::lock_guard<std::mutex> swapLock(swapMutex_);
    std::ifstream input(readyPath_);
    if (!input.is_open()) {
        return;
    }
    nlohmann::json ready;
    try {
        input >> ready;
    } catch (const std::exception& error) {
        emitProbeEvent("game.generation.ready-invalid", error.what(), 0,
                       nlohmann::json{{"path", readyPath_}});
        return;
    }
    if (ready.value("status", std::string()) != "ready") {
        return;
    }
    const std::uint64_t generation = ready.value("generation", std::uint64_t{0});
    const std::string dll = ready.value("dll", std::string());
    if (generation == 0 || dll.empty() ||
        generation == loader_.activeGeneration()) {
        return;
    }
    if (!std::filesystem::exists(dll)) {
        emitProbeEvent("game.generation.activate-failed", "candidate missing", 0,
                       nlohmann::json{{"generation", generation}, {"dll", dll}});
        return;
    }

    const std::uint64_t oldGeneration = loader_.activeGeneration();
    barrier_.beginQuiesce(QuiesceMode::Swap);
    const HostResult quiesced = barrier_.finishQuiesce(2000);
    if (quiesced != HostResult::Ok) {
        emitProbeEvent("game.generation.swap-refused", "generation busy", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(quiesced)}});
        barrier_.beginGeneration(oldGeneration);
        return;
    }

    LoadedGeneration candidate;
    const HostResult loaded = loader_.loadCandidate(
        widenUtf8(dll.c_str()), &services_, &candidate);
    if (loaded != HostResult::Ok) {
        emitProbeEvent("game.generation.activate-failed", "candidate rejected", 0,
                       nlohmann::json{{"generation", generation},
                                      {"dll", dll},
                                      {"result", static_cast<int>(loaded)}});
        barrier_.beginGeneration(oldGeneration);
        return;
    }

    const GameModuleV1* oldApi = loader_.active();
    if (oldApi && oldApi->quiesce) {
        oldApi->quiesce(QuiesceMode::Swap);
    }
    if (oldApi && oldApi->shutdown) {
        oldApi->shutdown();
    }
    // The ready manifest is the generation identity for the swap; a module may
    // hardcode a local version field that is not the build generation.
    candidate.generation = generation;
    loader_.commit(std::move(candidate));
    lastReadyGeneration_ = generation;
    barrier_.beginGeneration(generation);
    emitProbeEvent("game.generation.activated", "candidate committed", 0,
                   nlohmann::json{{"old_generation", oldGeneration},
                                  {"generation", generation},
                                  {"dll", dll}});
}

void HostRuntime::setEntityAdapter(const EntityAdapterV1* adapter, void* owner) {
    entityAdapter_ = adapter;
    entityOwner_ = owner;
}

void HostRuntime::clearEntityAdapter() {
    entityAdapter_ = nullptr;
    entityOwner_ = nullptr;
}

bool HostRuntime::probeNpcTargetSelect(NpcTargetDecisionV1* decision) {
    if (!decision || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    tick_ = decision->tick;
    decision->outTargetId = 0;
    decision->outHandled = 0;

    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventNpcTargetSelect, decision, sizeof(NpcTargetDecisionV1), &handled);
    barrier_.leave();

    if (result != HostResult::Ok || handled == 0 || decision->outHandled == 0) {
        return false;
    }
    if (decision->outTargetId == 0) {
        return true;
    }
    const std::uint32_t count = decision->candidateCount < kNpcTargetMaxCandidates
        ? decision->candidateCount : kNpcTargetMaxCandidates;
    for (std::uint32_t index = 0; index < count; ++index) {
        const NpcTargetCandidateV1& candidate = decision->candidates[index];
        if (candidate.id == decision->outTargetId) {
            return candidate.dead == 0;
        }
    }
    return false;
}

void* HostRuntime::getCapabilityThunk(void*, std::uint32_t capability,
                                            std::uint32_t version) {
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Log) &&
        version == kLogCapabilityVersion) {
        return &gLogCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Transport) &&
        version == kTransportCapabilityVersion) {
        static TransportCapabilityV1 transportCapability{
            kTransportCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(TransportCapabilityV1)),
            &HostRuntime::transportSendThunk,
            &HostRuntime::transportBroadcastThunk,
            &HostRuntime::transportSendToPlayerThunk,
            &HostRuntime::transportSessionStateThunk,
            &HostRuntime::transportPeerStateThunk,
            &HostRuntime::transportPeerCountThunk,
        };
        return &transportCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Effects) &&
        version == kEffectsCapabilityVersion) {
        static EffectsCapabilityV1 effectsCapability{
            kEffectsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(EffectsCapabilityV1)),
            &effectsInvoke,
        };
        return &effectsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Entity) &&
        version == kEntityCapabilityVersion) {
        static EntityCapabilityV1 entityCapability{
            kEntityCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(EntityCapabilityV1)),
            &HostRuntime::entityCapabilityReadThunk,
            &HostRuntime::entityCapabilityWriteThunk,
        };
        return &entityCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Render) &&
        version == kRenderCapabilityVersion) {
        static RenderCapabilityV1 renderCapability{
            kRenderCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(RenderCapabilityV1)),
            &HostRuntime::renderSubmitThunk,
            &HostRuntime::renderFlushThunk,
            &HostRuntime::renderPendingThunk,
        };
        return &renderCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Jobs) &&
        version == kJobsCapabilityVersion) {
        static JobsCapabilityV1 jobsCapability{
            kJobsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(JobsCapabilityV1)),
            &HostRuntime::jobsAcquireThunk,
            &HostRuntime::jobsReleaseThunk,
            &HostRuntime::jobsBusyThunk,
        };
        return &jobsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Callbacks) &&
        version == kCallbacksCapabilityVersion) {
        static CallbacksCapabilityV1 callbacksCapability{
            kCallbacksCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(CallbacksCapabilityV1)),
            &HostRuntime::callbackRegisterThunk,
            &HostRuntime::callbackUnregisterThunk,
            &HostRuntime::callbackBusyThunk,
        };
        return &callbacksCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Resources) &&
        version == kResourcesCapabilityVersion) {
        static ResourcesCapabilityV1 resourcesCapability{
            kResourcesCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(ResourcesCapabilityV1)),
            &HostRuntime::resourceAllocateThunk,
            &HostRuntime::resourceReleaseThunk,
            &HostRuntime::resourceBusyThunk,
        };
        return &resourcesCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Audio) &&
        version == kAudioCapabilityVersion) {
        static AudioCapabilityV1 audioCapability{
            kAudioCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(AudioCapabilityV1)),
            &audioInvoke,
        };
        return &audioCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Modules) &&
        version == kModuleCapabilityVersion) {
        static ModuleCapabilityV1 moduleCapability{
            kModuleCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(ModuleCapabilityV1)),
            &moduleInvoke,
        };
        return &moduleCapability;
    }
    return nullptr;
}

void HostRuntime::setTransportAdapter(const TransportAdapterV1* adapter, void* owner) {
    transportAdapter_ = adapter;
    transportOwner_ = owner;
}

void HostRuntime::clearTransportAdapter() {
    transportAdapter_ = nullptr;
    transportOwner_ = nullptr;
}

TransportResult HostRuntime::transportSendThunk(void* userData, PeerHandle,
                                                TransportChannel, std::uint32_t,
                                                std::uint32_t,
                                                const std::uint8_t* payload,
                                                std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->send_to_server) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->send_to_server(
               runtime->transportOwner_, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportBroadcastThunk(void* userData,
                                                     TransportChannel, std::uint32_t,
                                                     std::uint32_t,
                                                     const std::uint8_t* payload,
                                                     std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->broadcast) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->broadcast(
               runtime->transportOwner_, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportSendToPlayerThunk(void* userData,
                                                        std::uint32_t playerId,
                                                        std::uint32_t,
                                                        std::uint32_t,
                                                        const std::uint8_t* payload,
                                                        std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->send_to_player) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->send_to_player(
               runtime->transportOwner_, playerId, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportSessionStateThunk(void* userData,
                                                        SessionStateV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->session_state) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->session_state(runtime->transportOwner_, out)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportPeerStateThunk(void*, PeerHandle, PeerStateV1*) {
    return TransportResult::Unsupported;
}

std::uint32_t HostRuntime::transportPeerCountThunk(void* userData) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->peer_count) {
        return 0;
    }
    return runtime->transportAdapter_->peer_count(runtime->transportOwner_);
}

void HostRuntime::logThunk(void* userData, std::int32_t level, const char* message) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    emitProbeEvent("game.module.log", "module_log",
                   runtime ? static_cast<std::uint32_t>(runtime->tick_) : 0,
                   nlohmann::json{{"level", level},
                                  {"message", message ? message : ""}});
}

HostResult HostRuntime::entityReadThunk(void* userData, EntityHandle handle,
                                         EntitySnapshotV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ || !runtime->entityAdapter_->read ||
        !out || out->structSize < sizeof(EntitySnapshotV1)) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->read(runtime->entityOwner_, handle, out);
}

HostResult HostRuntime::entityWriteThunk(void* userData, EntityHandle handle,
                                          const EntitySnapshotV1* in) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ || !runtime->entityAdapter_->write ||
        !in || in->structSize < sizeof(EntitySnapshotV1)) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->write(runtime->entityOwner_, handle, in);
}

HostResult HostRuntime::entityCapabilityReadThunk(void* userData,
                                                   EntityHandle handle,
                                                   EntitySnapshotV1* out) {
    return entityReadThunk(userData, handle, out);
}

HostResult HostRuntime::entityCapabilityWriteThunk(void* userData,
                                                    EntityHandle handle,
                                                    const EntitySnapshotV1* in) {
    return entityWriteThunk(userData, handle, in);
}

HostResult HostRuntime::renderSubmitThunk(void* userData,
                                           const RenderCommandV1* commands,
                                           std::uint32_t count) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !commands || count == 0 ||
        count > kRenderMaxCommandsPerSubmit) {
        return HostResult::InvalidArgument;
    }
    runtime->renderPending_.fetch_add(count, std::memory_order_acq_rel);
    return HostResult::Ok;
}

HostResult HostRuntime::renderFlushThunk(void* userData) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    runtime->renderPending_.store(0, std::memory_order_release);
    return HostResult::Ok;
}

std::uint32_t HostRuntime::renderPendingThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->renderPending_.load(std::memory_order_acquire) : 0;
}

std::uint64_t HostRuntime::jobsAcquireThunk(void* userData, const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    runtime->jobsBusy_.fetch_add(1, std::memory_order_acq_rel);
    return runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
}

HostResult HostRuntime::jobsReleaseThunk(void* userData, std::uint64_t) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    std::uint32_t current = runtime->jobsBusy_.load(std::memory_order_acquire);
    while (current > 0) {
        if (runtime->jobsBusy_.compare_exchange_weak(
                current, current - 1, std::memory_order_acq_rel)) {
            break;
        }
    }
    return HostResult::Ok;
}

std::uint32_t HostRuntime::jobsBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->jobsBusy_.load(std::memory_order_acquire) : 0;
}

std::uint64_t HostRuntime::callbackRegisterThunk(void* userData, std::uint32_t,
                                                  const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    runtime->callbacksBusy_.fetch_add(1, std::memory_order_acq_rel);
    return runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
}

HostResult HostRuntime::callbackUnregisterThunk(void* userData, std::uint64_t) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    std::uint32_t current = runtime->callbacksBusy_.load(std::memory_order_acquire);
    while (current > 0) {
        if (runtime->callbacksBusy_.compare_exchange_weak(
                current, current - 1, std::memory_order_acq_rel)) {
            break;
        }
    }
    return HostResult::Ok;
}

std::uint32_t HostRuntime::callbackBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->callbacksBusy_.load(std::memory_order_acquire) : 0;
}

std::uint64_t HostRuntime::resourceAllocateThunk(void* userData,
                                                  std::uint32_t,
                                                  std::size_t bytes,
                                                  std::size_t,
                                                  const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || bytes == 0) {
        return 0;
    }
    void* pointer = std::malloc(bytes);
    if (!pointer) {
        return 0;
    }
    const std::uint64_t handle =
        runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(runtime->resourceMutex_);
        runtime->resources_[handle] = pointer;
    }
    return handle;
}

void HostRuntime::resourceReleaseThunk(void* userData, std::uint64_t resource) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || resource == 0) {
        return;
    }
    void* pointer = nullptr;
    {
        std::lock_guard<std::mutex> lock(runtime->resourceMutex_);
        auto it = runtime->resources_.find(resource);
        if (it == runtime->resources_.end()) {
            return;
        }
        pointer = it->second;
        runtime->resources_.erase(it);
    }
    std::free(pointer);
}

std::uint32_t HostRuntime::resourceBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(const_cast<HostRuntime*>(runtime)->resourceMutex_);
    return static_cast<std::uint32_t>(runtime->resources_.size());
}

std::uint64_t HostRuntime::tickThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->tick_ : 0;
}

double HostRuntime::timeThunk(void*) {
    const auto elapsed = std::chrono::steady_clock::now() - gStartTime;
    return std::chrono::duration<double>(elapsed).count();
}

void* HostRuntime::reserveThunk(void*, std::size_t bytes, std::size_t, const char*) {
    return std::malloc(bytes);
}

void HostRuntime::releaseThunk(void*, void* pointer, const char*) {
    std::free(pointer);
}

}  // namespace mimita::host
