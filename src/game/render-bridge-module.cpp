// 2026-11-12
// Game-DLL render phone for CAP_RENDER.
//
// The generation submits bounded draw commands; the resident host owns the
// device and flushes the stream.

#include "host/abi/game-host-abi.h"
#include "host/abi/render-capability.h"

namespace mimita::game {

void initializeHostRender(const mimita::host::HostServicesV1* services);
mimita::host::HostResult submitRenderCommands(
    const mimita::host::RenderCommandV1* commands, std::uint32_t count);
mimita::host::HostResult flushRender();
std::uint32_t pendingRenderCommands();

namespace {
const mimita::host::RenderCapabilityV1* gRender = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostRender(const mimita::host::HostServicesV1* services) {
    gRender = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::RenderCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Render),
            mimita::host::kRenderCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kRenderCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::RenderCapabilityV1)) {
        gRender = capability;
    }
}

mimita::host::HostResult submitRenderCommands(
    const mimita::host::RenderCommandV1* commands, std::uint32_t count) {
    if (!gRender || !gRender->submit || !commands || count == 0) {
        return mimita::host::HostResult::Unsupported;
    }
    return gRender->submit(gHostData, commands, count);
}

mimita::host::HostResult flushRender() {
    if (!gRender || !gRender->flush) {
        return mimita::host::HostResult::Unsupported;
    }
    return gRender->flush(gHostData);
}

std::uint32_t pendingRenderCommands() {
    return gRender && gRender->pending ? gRender->pending(gHostData) : 0;
}

}  // namespace mimita::game
