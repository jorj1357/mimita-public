// 2026-11-12
// Purpose: small game-DLL compatibility bridges for resident utilities that are
// pure or already covered by CAP_LOG.
//
// - MimitaNet::nowMs is a pure steady-clock read; the DLL can compute it.
// - recordCrashBreadcrumb forwards to the CAP_LOG text path; the resident
//   crash handler (which owns the crash file/breadcrumb ring) is not linked here.
//
// Name ends in "-module.cpp" so the canonical EXE source list excludes it.

#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>
#include <unordered_map>

#include "debug/crash-handler.h"
#include "debug/structured-log.h"
#include "network/net_common.h"

namespace {
std::mutex gBreadcrumbMutex;
std::unordered_map<std::string, uint64_t> gLastBreadcrumbMs;
}  // namespace

void recordCrashBreadcrumb(const char* subsystem, const char* format, ...) {
    char message[2048];
    message[0] = '\0';
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
    }
    // Per-frame breadcrumbs were flooding events.jsonl (tens of MB per minute)
    // and adding main-thread I/O. Keep the last message per subsystem in the
    // journal at most once per second.
    const std::string key = subsystem ? subsystem : "(unknown)";
    const uint64_t now = MimitaNet::nowMs();
    {
        std::lock_guard<std::mutex> lock(gBreadcrumbMutex);
        auto& last = gLastBreadcrumbMs[key];
        if (now - last < 1000) {
            return;
        }
        last = now;
    }
    StructuredLogger::instance().writeFormatted(
        StructuredCategory::General, StructuredLevel::Important, __FILE__, __LINE__,
        __FUNCTION__, "crash-breadcrumb %s: %s", subsystem ? subsystem : "(unknown)",
        message);
}
