// 2026-11-13
#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostFiles(const mimita::host::HostServicesV1* services);
std::uint64_t openFileWrite(const char* path);
bool writeFileHandle(std::uint64_t handle, const void* data, std::uint32_t size);
void closeFileHandle(std::uint64_t handle);
}  // namespace mimita::game
