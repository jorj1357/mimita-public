// 2026-11-13
// Transitional EXE-side implementation of game long-lived file handles.
//
// The hot game DLL uses the resident CAP_FILES capability. Until ownership
// cutover, ordinary game sources still run in the EXE and need this fallback.

#include <fstream>
#include <unordered_map>

#include "game/file-bridge-module.h"

namespace mimita::game {
namespace {
std::unordered_map<std::uint64_t, std::ofstream> gFiles;
std::uint64_t gNextHandle = 1;
}  // namespace

std::uint64_t openFileWrite(const char* path) {
    if (!path || !*path) return 0;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return 0;
    const std::uint64_t handle = gNextHandle++;
    gFiles.emplace(handle, std::move(stream));
    return handle;
}

bool writeFileHandle(std::uint64_t handle, const void* data, std::uint32_t size) {
    auto it = gFiles.find(handle);
    if (it == gFiles.end() || (!data && size)) return false;
    if (size) it->second.write(static_cast<const char*>(data), size);
    return it->second.good();
}

void closeFileHandle(std::uint64_t handle) {
    auto it = gFiles.find(handle);
    if (it == gFiles.end()) return;
    it->second.flush();
    it->second.close();
    gFiles.erase(it);
}
}  // namespace mimita::game
