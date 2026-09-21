// 09 20 2026
// Hot diagnostic command. It reads one kernel-owned runtime snapshot and
// prints exactly which process, generation, server view, and events.jsonl are
// active. The command registration is part of the hot package.
#if defined(MIMITA_GAME_DLL)
#include "hot-reload/hot-package.h"
#include "hot-reload/game-api.h"

#include <cstdio>
#include <cstdarg>

namespace {

void writeLine(GameplayContextV1* ctx, const char* format, ...)
{
    char line[768] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (ctx && ctx->resolveCapability) {
        auto fn = reinterpret_cast<GameTerminalOutputFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_TERMINAL_OUTPUT));
        if (fn) {
            fn(ctx->host, line);
            return;
        }
    }
    std::printf("%s\n", line);
}

void MIMITA_GAME_CALL versionInfo(void* host, const char*)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability) {
        writeLine(nullptr, "[VERSIONINFO] runtime context unavailable");
        return;
    }
    auto fn = reinterpret_cast<GameRuntimeInfoFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RUNTIME_INFO));
    GameRuntimeInfoV1 info{};
    if (!fn || !fn(ctx->host, &info)) {
        writeLine(ctx, "[VERSIONINFO] runtime.info unavailable");
        return;
    }
    writeLine(ctx, "[VERSIONINFO] process=%s pid=%u session=%llu uptime_ms=%llu",
        info.process, info.pid, (unsigned long long)info.sessionId,
        (unsigned long long)info.uptimeMs);
    writeLine(ctx, "[VERSIONINFO] exe=%s", info.exePath);
    writeLine(ctx, "[VERSIONINFO] events_jsonl=%s", info.eventsPath);
    writeLine(ctx, "[VERSIONINFO] client_generation=%u active_hash=%016llx client_tick=%u",
        info.activeGeneration, (unsigned long long)info.activeHash, info.clientTick);
    writeLine(ctx, "[VERSIONINFO] server_generation=%u server_hash=%016llx server_tick=%u phase=%u",
        info.serverGeneration, (unsigned long long)info.serverHash,
        info.serverTick, info.serverPhase);
    writeLine(ctx, "[VERSIONINFO] server_logical_hash=%016llx platform_hash=%016llx abi=%u",
        (unsigned long long)info.serverLogicalHash,
        (unsigned long long)info.serverPlatformHash, info.hotAbiVersion);
    writeLine(ctx, "[VERSIONINFO] room=%s server=%s last_error=%s",
        info.roomCode[0] ? info.roomCode : "(none)",
        info.serverName[0] ? info.serverName : "(none)",
        info.lastError[0] ? info.lastError : "(none)");

    auto logFn = reinterpret_cast<GameLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (logFn) {
        GameLogEventV1 event{};
        event.level = 2;
        event.simulationTick = info.clientTick;
        event.serverTick = info.serverTick;
        std::snprintf(event.category, sizeof(event.category), "LIVE_CODE");
        std::snprintf(event.name, sizeof(event.name), "versioninfo.executed");
        std::snprintf(event.message, sizeof(event.message),
                      "client_generation=%u server_generation=%u events_jsonl=%s",
                      info.activeGeneration, info.serverGeneration, info.eventsPath);
        logFn(ctx->host, &event);
    }
}

const MimitaHotPackage::CommandRegistrar s_versionInfo{
    {"versioninfo", "Print live process, generation, server, and JSONL identity",
     0, versionInfo}};

} // namespace
#endif
