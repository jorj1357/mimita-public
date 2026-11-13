// 2026-11-13
// Game-DLL side of the resident render-device lifecycle.

#include "game/render-device-bridge-module.h"

#include <cstdint>

#include "host/abi/render-device-capability.h"
#include "renderer/renderer.h"

namespace mimita::game {
namespace {
const mimita::host::RenderDeviceCapabilityV1* gRenderDevice = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostRenderDevice(const mimita::host::HostServicesV1* services) {
    gRenderDevice = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::RenderDeviceCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::RenderDevice),
            mimita::host::kRenderDeviceCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kRenderDeviceCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::RenderDeviceCapabilityV1)) {
        gRenderDevice = capability;
    }
}

Renderer* acquireHostRenderer(int width, int height, const char* title) {
    if (!gRenderDevice || !gRenderDevice->acquire) return nullptr;
    return static_cast<Renderer*>(
        gRenderDevice->acquire(gHostData, width, height, title));
}

void setHostCursorVisible(bool visible) {
    if (gRenderDevice && gRenderDevice->set_cursor_visible)
        gRenderDevice->set_cursor_visible(gHostData, visible ? 1 : 0);
}
}  // namespace mimita::game

float Renderer::beginFrame() {
    return mimita::game::gRenderDevice
        ? mimita::game::gRenderDevice->begin_frame(mimita::game::gHostData)
        : 0.0f;
}

void Renderer::endFrame() {
    if (mimita::game::gRenderDevice)
        mimita::game::gRenderDevice->end_frame(mimita::game::gHostData);
}

bool Renderer::shouldClose() {
    return mimita::game::gRenderDevice &&
           mimita::game::gRenderDevice->should_close(mimita::game::gHostData) != 0;
}

void Renderer::shutdown() {
    if (mimita::game::gRenderDevice)
        mimita::game::gRenderDevice->shutdown(mimita::game::gHostData);
}

FullscreenMode Renderer::applyVideoMode(int gameWidth, int gameHeight,
                                        FullscreenMode mode) {
    if (!mimita::game::gRenderDevice) return mode;
    return static_cast<FullscreenMode>(
        mimita::game::gRenderDevice->apply_video_mode(
            mimita::game::gHostData, gameWidth, gameHeight,
            static_cast<std::int32_t>(mode)));
}

void Renderer::setVSync(bool on) {
    if (mimita::game::gRenderDevice)
        mimita::game::gRenderDevice->set_vsync(
            mimita::game::gHostData, on ? 1 : 0);
}

bool Renderer::reloadWorldShaderIfChanged() {
    return mimita::game::gRenderDevice &&
           mimita::game::gRenderDevice->reload_world_shader_if_changed(
               mimita::game::gHostData);
}
