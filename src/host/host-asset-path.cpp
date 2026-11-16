// Resident asset-path resolution for host-owned device and UI sources.
//
// Names are host-prefixed so the migration EXE can link path_utils.cpp too.

#include "host/host-asset-path.h"

#include <fstream>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

std::string hostExecutableDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
        std::string exePath(path);
        const std::size_t slash = exePath.find_last_of("\\/");
        if (slash != std::string::npos) {
            return exePath.substr(0, slash + 1);
        }
    }
#endif
    return "./";
}

std::string hostResolveAssetPath(std::string relativePath) {
    const std::string fullPath = hostExecutableDirectory() + relativePath;
    std::ifstream candidate(fullPath);
    if (candidate.good()) {
        return fullPath;
    }
    return relativePath;
}

// Legacy name used by resident audio/renderer sources. In the migration EXE
// the game-owned path_utils.cpp provides the strong definition.
__attribute__((weak)) std::string resolveAssetPath(std::string relativePath) {
    return hostResolveAssetPath(std::move(relativePath));
}
