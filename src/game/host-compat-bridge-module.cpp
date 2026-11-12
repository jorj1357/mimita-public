// 2026-11-12
// Purpose: small game-DLL compatibility bridges for resident utilities that are
// pure or already covered by CAP_LOG.
//
// - MimitaNet::nowMs is a pure steady-clock read; the DLL can compute it.
// - recordCrashBreadcrumb forwards to the CAP_LOG text path; the resident
//   crash handler (which owns the crash file/breadcrumb ring) is not linked here.
//
// Name ends in "-module.cpp" so the canonical EXE source list excludes it.

#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "debug/crash-handler.h"
#include "debug/structured-log.h"
#include "network/net_common.h"

namespace MimitaNet {

uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string normalizeMapId(const std::string& mapId) {
    std::string result = mapId;
    const std::size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        result = result.substr(lastSlash + 1);
    }
    if (result.size() > 4 && result.substr(result.size() - 4) == ".glb") {
        result = result.substr(0, result.size() - 4);
    }
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

bool mapIdsReferToSameMap(const std::string& a, const std::string& b) {
    return normalizeMapId(a) == normalizeMapId(b);
}

bool parseAddress(const std::string& text, sockaddr_in& out, bool allowPortZero) {
    if (text.empty() || text.front() == '[') {
        return false;
    }
    std::string host = text;
    uint16_t port = DEFAULT_PORT;
    const std::size_t colon = text.rfind(':');
    if (colon != std::string::npos) {
        host = text.substr(0, colon);
        const std::string portText = text.substr(colon + 1);
        if (portText.empty()) {
            return false;
        }
        for (char c : portText) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        const unsigned long parsed = std::strtoul(portText.c_str(), nullptr, 10);
        if (parsed > 65535 || (parsed == 0 && !allowPortZero)) {
            return false;
        }
        port = static_cast<uint16_t>(parsed);
    }
    if (host == "localhost") {
        host = "127.0.0.1";
    }
    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    return inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1;
}

std::string addressToString(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, const_cast<void*>(reinterpret_cast<const void*>(&addr.sin_addr)),
              ip, sizeof(ip));
    char out[64];
    std::snprintf(out, sizeof(out), "%s:%u", ip,
                  static_cast<unsigned>(ntohs(addr.sin_port)));
    return std::string(out);
}

}  // namespace MimitaNet

void recordCrashBreadcrumb(const char* subsystem, const char* format, ...) {
    char message[2048];
    message[0] = '\0';
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
    }
    StructuredLogger::instance().writeFormatted(
        StructuredCategory::General, StructuredLevel::Important, __FILE__, __LINE__,
        __FUNCTION__, "crash-breadcrumb %s: %s", subsystem ? subsystem : "(unknown)",
        message);
}
