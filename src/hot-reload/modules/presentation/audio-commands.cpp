// 09 23 2026
/* purpose
* Hot runtime commands for the generic audio service: `audio status`,
* `audio resources`, `audio voices`, `audio reload <logical-id>`, and
* `audio trace <0|1>`. Each command resolves the stable `audio.play` capability
* and issues a plain-data op; no cold command switch is added and no voice
* handle crosses the boundary. Output goes through `terminal.output`.
* Does NOT own the device, mixer, or sound selection policy.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

using AudioPlayFn = void (MIMITA_GAME_CALL *)(void*,
                                              const GameAudioCommandV1*);

void writeLine(GameplayContextV1* ctx, const char* format, ...)
{
    char line[512] = {};
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

// Fill the ABI header on a command so the kernel version gate accepts it.
void stamp(GameAudioCommandV1& c)
{
    c.commandVersion = GAME_AUDIO_COMMAND_VERSION;
    c.structSize = sizeof(GameAudioCommandV1);
}

void MIMITA_GAME_CALL audioCommand(void* host, const char* args)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability) {
        writeLine(nullptr, "audio: runtime context unavailable");
        return;
    }
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio) {
        writeLine(ctx, "audio: audio.play capability unavailable");
        return;
    }

    char sub[32] = {};
    const char* rest = nullptr;
    if (args && *args) {
        int consumed = 0;
        if (std::sscanf(args, "%31s%n", sub, &consumed) == 1) {
            rest = args + consumed;
            while (*rest == ' ' || *rest == '\t')
                ++rest;
        }
    }

    if (sub[0] == '\0' || std::strcmp(sub, "status") == 0) {
        GameAudioCommandV1 q{};
        stamp(q);
        q.op = GAME_AUDIO_QUERY_STATUS;
        audio(ctx->host, &q);
        writeLine(ctx, "audio: device=%s voices=%u cached_sounds=%u trace=%u ok=%u",
                  q.ok ? "up" : "down", q.activeVoices, q.loadedResources,
                  q.reserved, q.ok);
        return;
    }
    if (std::strcmp(sub, "voices") == 0) {
        GameAudioCommandV1 q{};
        stamp(q);
        q.op = GAME_AUDIO_QUERY_STATUS;
        audio(ctx->host, &q);
        writeLine(ctx, "audio: active_voices=%u (detailed listing lands with the "
                       "resource-generation phase)",
                  q.activeVoices);
        return;
    }
    if (std::strcmp(sub, "resources") == 0) {
        GameAudioCommandV1 q{};
        stamp(q);
        q.op = GAME_AUDIO_QUERY_STATUS;
        audio(ctx->host, &q);
        writeLine(ctx, "audio: cached_sounds=%u (logical sound-resource "
                       "generations land in phase 8)",
                  q.loadedResources);
        return;
    }
    if (std::strcmp(sub, "reload") == 0) {
        if (!rest || !*rest) {
            writeLine(ctx, "usage: audio reload <logical-id>");
            return;
        }
        GameAudioCommandV1 c{};
        stamp(c);
        c.op = GAME_AUDIO_RELOAD_RESOURCE;
        std::snprintf(c.sound, sizeof(c.sound), "%s", rest);
        audio(ctx->host, &c);
        writeLine(ctx, "audio: reload '%s' ok=%u (resource generations land in "
                       "phase 8)",
                  rest, c.ok);
        return;
    }
    if (std::strcmp(sub, "trace") == 0) {
        const bool on = rest && rest[0] == '1';
        GameAudioCommandV1 c{};
        stamp(c);
        c.op = GAME_AUDIO_SET_TRACE;
        c.flags = on ? 1u : 0u;
        audio(ctx->host, &c);
        writeLine(ctx, "audio: trace=%s", on ? "on" : "off");
        return;
    }
    writeLine(ctx, "usage: audio status|resources|voices|reload <logical-id>|"
                   "trace <0|1>");
}

const MimitaHotPackage::CommandRegistrar s_audioCommand{
    {"audio", "audio status|resources|voices|reload <id>|trace <0|1>", 0,
     audioCommand}};

} // namespace

#endif
