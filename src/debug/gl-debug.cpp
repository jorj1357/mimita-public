#include "debug/gl-debug.h"

#include <cstdio>
#include <cstdlib>

#include "render/render-command.h"

namespace {
constexpr std::uint32_t kInvalidEnum = 0x0500u;
constexpr std::uint32_t kInvalidValue = 0x0501u;
constexpr std::uint32_t kInvalidOperation = 0x0502u;
constexpr std::uint32_t kOutOfMemory = 0x0505u;
constexpr std::uint32_t kInvalidFramebufferOperation = 0x0506u;

bool traceVerbose()
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = std::getenv("MIMITA_GL_TRACE_VERBOSE") ? 1 : 0;
    return enabled != 0;
}

const char* errorName(std::uint32_t err)
{
    switch (err)
    {
        case kInvalidEnum: return "GL_INVALID_ENUM";
        case kInvalidValue: return "GL_INVALID_VALUE";
        case kInvalidOperation: return "GL_INVALID_OPERATION";
        case kOutOfMemory: return "GL_OUT_OF_MEMORY";
        case kInvalidFramebufferOperation: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default: return "UNKNOWN";
    }
}
} // namespace

namespace GLDebug {

void clearErrors(const char* stage)
{
    std::uint32_t err = mimita::game::render::getError();
    bool hadError = false;
    while (err != 0u)
    {
        hadError = true;
        printf("[GL TRACE] cleared stale error before %s err=0x%X (%s)\n",
               stage ? stage : "unknown", err, errorName(err));
        err = mimita::game::render::getError();
    }
    if (traceVerbose() && !hadError)
        printf("[GL TRACE] clear before %s err=0x0\n", stage ? stage : "unknown");
}

bool logErrors(const char* label, const char* file, int line)
{
    std::uint32_t err = mimita::game::render::getError();
    if (err == 0u)
    {
        if (traceVerbose())
            printf("[GL TRACE] after %s err=0x0 (%s:%d)\n",
                   label ? label : "unknown", file ? file : "?", line);
        return false;
    }
    bool hadError = false;
    while (err != 0u)
    {
        hadError = true;
        printf("[GL TRACE] after %s err=0x%X (%s) at %s:%d\n",
               label ? label : "unknown", err, errorName(err),
               file ? file : "?", line);
        err = mimita::game::render::getError();
    }
    return hadError;
}

void traceBefore(const char* label, const char* file, int line)
{
    if (traceVerbose())
        printf("[GL TRACE] before %s (%s:%d)\n",
               label ? label : "unknown", file ? file : "?", line);
}
} // namespace GLDebug
