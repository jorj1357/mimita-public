#pragma once

// Installs the process-wide crash diagnostics: an unhandled-exception filter,
// a std::terminate handler for uncaught C++ exceptions, and a small
// allocation-free breadcrumb ring printed into every crash report.
void installCrashHandler();
void setCrashHandlerTestMode(bool enabled);
void setCrashHandlerForceTempFallback(bool enabled);

// Records one recent operation into a fixed-size, allocation-free ring that the
// crash report dumps ("last subsystem"). Safe to call from hot paths and from
// the crash handler itself. The subsystem is a short stable tag such as
// "boolean", "fracture", "entity-add", "render-upload", "cut-apply". For
// important, searchable events use the normal Debug::/structured logger, which
// already writes to the events.jsonl stream.
void recordCrashBreadcrumb(const char* subsystem, const char* format, ...);
