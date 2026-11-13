// 2026-11-13
#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostGlResources(const mimita::host::HostServicesV1* services);
std::uint32_t genTexture();
void deleteTexture(std::uint32_t id);
std::uint32_t genBuffer();
void deleteBuffer(std::uint32_t id);
std::uint32_t genVertexArray();
void deleteVertexArray(std::uint32_t id);
std::uint32_t genFramebuffer();
void deleteFramebuffer(std::uint32_t id);
std::uint32_t genRenderbuffer();
void deleteRenderbuffer(std::uint32_t id);
std::uint32_t genQuery();
void deleteQuery(std::uint32_t id);
void deleteShader(std::uint32_t id);
void deleteProgram(std::uint32_t id);
void generateMipmap(std::uint32_t target);
}  // namespace mimita::game
