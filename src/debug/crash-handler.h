#pragma once

#include <cstddef>
#include <cstdint>

// Installs the process-wide crash diagnostics: an unhandled-exception filter,
// a std::terminate handler for uncaught C++ exceptions, and a small
// allocation-free breadcrumb ring printed into every crash report.
void installCrashHandler();
void setCrashHandlerShowDialog(bool showDialog);
void setCrashHandlerTestMode(bool enabled);
void setCrashHandlerForceTempFallback(bool enabled);

// Best-effort stack capture for a suspended thread in this process. The caller
// resumes the target internally before returning. Addresses are included when
// MinGW debug symbols cannot be read by DbgHelp.
bool captureThreadStackTrace(uint32_t threadId, char* out, size_t outSize);

// Records one recent operation into a fixed-size, allocation-free ring that the
// crash report dumps ("last subsystem"). Safe to call from hot paths and from
// the crash handler itself. The subsystem is a short stable tag such as
// "boolean", "fracture", "entity-add", "render-upload", "cut-apply". For
// important, searchable events use the normal Debug::/structured logger, which
// already writes to the events.jsonl stream.
void recordCrashBreadcrumb(const char* subsystem, const char* format, ...);
