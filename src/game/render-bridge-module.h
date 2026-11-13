// 2026-11-13
#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/render-capability.h"

namespace mimita::game {
void initializeHostRender(const mimita::host::HostServicesV1* services);
mimita::host::HostResult submitRenderCommands(
    const mimita::host::RenderCommandV1* commands, std::uint32_t count);
mimita::host::HostResult flushRender();
std::uint32_t pendingRenderCommands();
mimita::host::HostResult bufferData(std::uint32_t target, std::uint64_t buffer,
                                    const void* data, std::uint32_t size,
                                    std::uint32_t usage);
mimita::host::HostResult texImage2D(std::uint64_t texture, std::int32_t level,
                                    std::int32_t internalFormat, std::int32_t width,
                                    std::int32_t height, std::int32_t border,
                                    std::uint32_t format, std::uint32_t type,
                                    const void* data, std::uint32_t size);
mimita::host::HostResult texParameteri(std::uint32_t target, std::uint32_t pname,
                                       std::int32_t value);
mimita::host::HostResult vertexAttribPointer(std::uint32_t index, std::int32_t size,
                                             std::uint32_t type,
                                             std::uint32_t normalized,
                                             std::int32_t stride,
                                             std::uint32_t offset);
mimita::host::HostResult enableVertexAttrib(std::uint32_t index, bool enabled);
mimita::host::HostResult uniform1i(std::int32_t location, std::int32_t value);
mimita::host::HostResult uniform1f(std::int32_t location, float value);
mimita::host::HostResult uniform3f(std::int32_t location, float x, float y, float z);
mimita::host::HostResult uniform4f(std::int32_t location, float x, float y, float z, float w);
mimita::host::HostResult uniformMatrix4fv(std::int32_t location,
                                          std::uint32_t count, const float* values);
}  // namespace mimita::game
