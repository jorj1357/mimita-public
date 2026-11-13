// 2026-11-13
// Game-DLL side of resident stack diagnostics.

#include <cstdint>
#include <cstddef>

#include "debug/crash-handler.h"
#include "host/abi/diagnostics-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostDiagnostics(const mimita::host::HostServicesV1* services);
namespace {
const mimita::host::DiagnosticsCapabilityV1* gDiagnostics = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostDiagnostics(const mimita::host::HostServicesV1* services) {
    gDiagnostics = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::DiagnosticsCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Diagnostics),
            mimita::host::kDiagnosticsCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kDiagnosticsCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::DiagnosticsCapabilityV1)) {
        gDiagnostics = capability;
    }
}
}  // namespace mimita::game

bool captureThreadStackTrace(std::uint32_t threadId, char* out, std::size_t outSize) {
    if (!mimita::game::gDiagnostics || !mimita::game::gDiagnostics->capture_thread_stack ||
        !out || outSize == 0) {
        return false;
    }
    return mimita::game::gDiagnostics->capture_thread_stack(
               mimita::game::gHostData, threadId, out,
               static_cast<std::uint32_t>(outSize)) != 0;
}
