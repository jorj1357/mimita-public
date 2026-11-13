// 2026-11-12
// Game-DLL render phone for CAP_RENDER.
//
// The generation submits bounded draw commands; the resident host owns the
// device and flushes the stream.

#include "host/abi/game-host-abi.h"
#include "game/render-bridge-module.h"
#include "host/abi/render-capability.h"

namespace mimita::game {

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

mimita::host::HostResult bufferData(std::uint32_t target, std::uint64_t buffer,
                                    const void* data, std::uint32_t size,
                                    std::uint32_t usage) {
    if (!gRender || !gRender->buffer_data) return mimita::host::HostResult::Unsupported;
    return gRender->buffer_data(gHostData, target, buffer, data, size, usage);
}

mimita::host::HostResult texImage2D(std::uint64_t texture, std::int32_t level,
                                    std::int32_t internalFormat, std::int32_t width,
                                    std::int32_t height, std::int32_t border,
                                    std::uint32_t format, std::uint32_t type,
                                    const void* data, std::uint32_t size) {
    if (!gRender || !gRender->tex_image_2d) return mimita::host::HostResult::Unsupported;
    return gRender->tex_image_2d(gHostData, texture, level, internalFormat,
                                 width, height, border, format, type, data, size);
}

mimita::host::HostResult texParameteri(std::uint32_t target, std::uint32_t pname,
                                       std::int32_t value) {
    if (!gRender || !gRender->tex_parameteri) return mimita::host::HostResult::Unsupported;
    return gRender->tex_parameteri(gHostData, target, pname, value);
}

mimita::host::HostResult vertexAttribPointer(std::uint32_t index, std::int32_t size,
                                             std::uint32_t type,
                                             std::uint32_t normalized,
                                             std::int32_t stride,
                                             std::uint32_t offset) {
    if (!gRender || !gRender->vertex_attrib_pointer) return mimita::host::HostResult::Unsupported;
    return gRender->vertex_attrib_pointer(gHostData, index, size, type,
                                          normalized, stride, offset);
}

mimita::host::HostResult enableVertexAttrib(std::uint32_t index, bool enabled) {
    if (!gRender || !gRender->enable_vertex_attrib) return mimita::host::HostResult::Unsupported;
    return gRender->enable_vertex_attrib(gHostData, index, enabled ? 1u : 0u);
}

mimita::host::HostResult uniform1i(std::int32_t location, std::int32_t value) {
    if (!gRender || !gRender->uniform1i) return mimita::host::HostResult::Unsupported;
    return gRender->uniform1i(gHostData, location, value);
}
mimita::host::HostResult uniform1f(std::int32_t location, float value) {
    if (!gRender || !gRender->uniform1f) return mimita::host::HostResult::Unsupported;
    return gRender->uniform1f(gHostData, location, value);
}
mimita::host::HostResult uniform3f(std::int32_t location, float x, float y, float z) {
    if (!gRender || !gRender->uniform3f) return mimita::host::HostResult::Unsupported;
    return gRender->uniform3f(gHostData, location, x, y, z);
}
mimita::host::HostResult uniform4f(std::int32_t location, float x, float y, float z, float w) {
    if (!gRender || !gRender->uniform4f) return mimita::host::HostResult::Unsupported;
    return gRender->uniform4f(gHostData, location, x, y, z, w);
}
mimita::host::HostResult uniformMatrix4fv(std::int32_t location,
                                          std::uint32_t count, const float* values) {
    if (!gRender || !gRender->uniform_matrix4fv) return mimita::host::HostResult::Unsupported;
    return gRender->uniform_matrix4fv(gHostData, location, count, values);
}

}  // namespace mimita::game
