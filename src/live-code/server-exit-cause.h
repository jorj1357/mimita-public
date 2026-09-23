// 09 23 2026
/* purpose
* Classify how a server process ended so a silent restart is never used before
* the original cause is observable. The parent/client records the child PID and
* exit code; the server itself records a clean shutdown if execution reaches its
* logger. Does NOT restart anything.
*/
#pragma once

#include <cstdint>

namespace LiveCode {

enum class ServerExitCause : std::uint32_t {
    Unknown = 0,
    CleanShutdown,        // server ran its shutdown path and returned 0
    NonZeroExit,          // process exited with a non-zero code
    TransportFailure,     // socket/transport error path
    HotReloadFailure,      // hot reload left the server unusable
    ExternalTermination,  // another process called TerminateProcess / killed it
};

inline const char* serverExitCauseName(ServerExitCause cause)
{
    switch (cause) {
    case ServerExitCause::Unknown: return "unknown";
    case ServerExitCause::CleanShutdown: return "clean_shutdown";
    case ServerExitCause::NonZeroExit: return "non_zero_exit";
    case ServerExitCause::TransportFailure: return "transport_failure";
    case ServerExitCause::HotReloadFailure: return "hot_reload_failure";
    case ServerExitCause::ExternalTermination: return "external_termination";
    }
    return "unknown";
}

// Classify from the facts the PARENT can observe: whether it externally
// terminated the child, and the child's exit code. The server's own journal is
// the authority on clean vs crash (it writes server.shutdown.completed only when
// it reaches its shutdown path); the parent's classification is deliberately
// coarse and never assumes clean. A zero exit the parent did not force is
// reported as CleanShutdown only when `childReportedClean` is known true.
inline ServerExitCause classifyServerExit(bool childReportedClean, std::uint32_t exitCode,
                                          bool externallyTerminated)
{
    if (externallyTerminated)
        return ServerExitCause::ExternalTermination;
    if (childReportedClean)
        return ServerExitCause::CleanShutdown;
    if (exitCode != 0)
        return ServerExitCause::NonZeroExit;
    // Zero exit without the child's clean-shutdown record: the process ended
    // without running its own shutdown path. Not clean.
    return ServerExitCause::Unknown;
}

} // namespace LiveCode
