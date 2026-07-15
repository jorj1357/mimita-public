#include "network/net_common.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

namespace MimitaNet {

bool netStartup()
{
    WSADATA data{};
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
    {
        printf("[NET] WSAStartup failed error=%d\n", result);
        return false;
    }
    return true;
}

void netShutdown()
{
    WSACleanup();
}

bool setNonBlocking(SOCKET socketHandle)
{
    u_long mode = 1;
    int result = ioctlsocket(socketHandle, FIONBIO, &mode);
    if (result != 0)
        printf("[NET] ioctlsocket nonblocking failed error=%d\n", WSAGetLastError());
    return result == 0;
}

bool parseAddress(const std::string& text, sockaddr_in& out)
{
    std::string host = text;
    uint16_t port = DEFAULT_PORT;

    size_t colon = text.rfind(':');
    if (colon != std::string::npos)
    {
        host = text.substr(0, colon);
        int parsedPort = std::atoi(text.substr(colon + 1).c_str());
        if (parsedPort <= 0 || parsedPort > 65535)
            return false;
        port = (uint16_t)parsedPort;
    }

    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) != 1)
        return false;
    return true;
}

std::string addressToString(const sockaddr_in& addr)
{
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, (void*)&addr.sin_addr, ip, sizeof(ip));
    char out[64];
    snprintf(out, sizeof(out), "%s:%u", ip, (unsigned)ntohs(addr.sin_port));
    return std::string(out);
}

uint64_t nowMs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string normalizeMapId(const std::string& mapId)
{
    std::string result = mapId;
    // Strip directory prefix
    size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        result = result.substr(lastSlash + 1);
    // Strip .glb extension
    if (result.size() > 4 && result.substr(result.size() - 4) == ".glb")
        result = result.substr(0, result.size() - 4);
    // Lowercase
    for (char& c : result)
        c = (char)std::tolower((unsigned char)c);
    return result;
}

bool mapIdsReferToSameMap(const std::string& a, const std::string& b)
{
    return normalizeMapId(a) == normalizeMapId(b);
}

} // namespace MimitaNet
