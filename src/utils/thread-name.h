#pragma once

// Phase 0 CPU accounting (2026-11-28): give every worker thread an OS-visible
// name and one journal record, so per-thread CPU samples can be attributed.
//
// Header-only on purpose: the name path must be usable from both the resident
// EXE and the hot-reloaded whole-game DLL without adding a translation unit to
// either build list.
//
// Cost: one SetThreadDescription call and one journal line at thread start, plus
// one line at thread exit when a guard is used. Never called from a hot path.
//
// Collector contract (docs/specs/dev-loop/system-resource-measurement.md):
// devscripts/perf-watch.py --threads reads the OS thread name and creation time
// from outside the process, so a run's cpu-threads.csv names every worker
// without in-process cooperation. Events are ``runtime.thread.started`` and
// ``runtime.thread.stopped`` with thread_id, thread_name, thread_role.

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "debug/structured-log.h"

namespace mimita::threads {

namespace detail {

inline std::uint32_t currentThreadId() {
    return static_cast<std::uint32_t>(::GetCurrentThreadId());
}

// SetThreadDescription exists on Windows 10 1607+. Resolve it dynamically so a
// missing export can never fail process startup.
inline void setOsThreadName(const char* name) {
    if (!name || !*name) return;
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const HMODULE kernel = ::GetModuleHandleW(L"kernel32.dll");
    if (!kernel) return;
    const auto setDescription = reinterpret_cast<SetThreadDescriptionFn>(
        ::GetProcAddress(kernel, "SetThreadDescription"));
    if (!setDescription) return;
    wchar_t wide[64] = {};
    const int written = ::MultiByteToWideChar(
        CP_UTF8, 0, name, -1, wide, 63);
    if (written <= 0) return;
    setDescription(::GetCurrentThread(), wide);
}

inline void emitThreadEvent(const char* event, const char* name,
                            const char* role, const char* function) {
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        event, "thread",
        "worker thread lifecycle for CPU attribution", 0,
        nlohmann::json{{"thread_id", currentThreadId()},
                       {"thread_name", name ? name : ""},
                       {"thread_role", role ? role : "worker"}},
        __FILE__, __LINE__, function);
}

}  // namespace detail

// Name the calling thread for the OS and write one runtime.thread.started
// record. Safe to call more than once; callers use it once per thread entry.
inline void nameCurrentThread(const char* name, const char* role = "worker") {
    detail::setOsThreadName(name);
    detail::emitThreadEvent("runtime.thread.started", name, role,
                            "nameCurrentThread");
}

// RAII form for thread functions that return: names the thread on construction
// and records runtime.thread.stopped on exit (normal return or exception).
struct ThreadNameGuard {
    const char* name;
    const char* role;

    explicit ThreadNameGuard(const char* threadName,
                             const char* threadRole = "worker")
        : name(threadName), role(threadRole) {
        nameCurrentThread(name, role);
    }
    ~ThreadNameGuard() {
        detail::emitThreadEvent("runtime.thread.stopped", name, role,
                                "~ThreadNameGuard");
    }
    ThreadNameGuard(const ThreadNameGuard&) = delete;
    ThreadNameGuard& operator=(const ThreadNameGuard&) = delete;
};

}  // namespace mimita::threads
