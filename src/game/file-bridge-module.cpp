// 2026-11-13
// Game-DLL side of resident long-lived file handles.

#include "game/file-bridge-module.h"

#include "host/abi/files-capability.h"

namespace mimita::game {
namespace {
const mimita::host::FilesCapabilityV1* gFiles = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostFiles(const mimita::host::HostServicesV1* services) {
    gFiles = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::FilesCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Files),
            mimita::host::kFilesCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kFilesCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::FilesCapabilityV1)) {
        gFiles = capability;
    }
}

std::uint64_t openFileWrite(const char* path) {
    if (!gFiles || !gFiles->open_write || !path) return 0;
    return gFiles->open_write(gHostData, path);
}

bool writeFileHandle(std::uint64_t handle, const void* data, std::uint32_t size) {
    return gFiles && gFiles->write && handle &&
           gFiles->write(gHostData, handle, data, size) != 0;
}

void closeFileHandle(std::uint64_t handle) {
    if (gFiles && gFiles->close && handle) gFiles->close(gHostData, handle);
}
}  // namespace mimita::game
