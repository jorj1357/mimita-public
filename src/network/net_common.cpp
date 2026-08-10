#include "network/net_common.h"

#include <chrono>
#include <cstdio>
#ifdef _WIN32
#else
#include <fcntl.h>
#include <errno.h>
#endif
namespace MimitaNet {

bool netStartup()
{
    #ifdef _WIN32
        WSADATA data{};
        int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0)
        {
            printf("[NET] WSAStartup failed error=%d\n", result);
            return false;
        }
        return true;
    #else
        return true;
    #endif
}

void netShutdown()
{
    #ifdef _WIN32
        WSACleanup();
    #endif
}
bool setNonBlocking(Socket socketHandle)
{
#ifdef _WIN32
    u_long mode = 1;
    int result = ioctlsocket(socketHandle, FIONBIO, &mode);

    if (result != 0)
        printf("[NET] ioctlsocket nonblocking failed error=%d\n", WSAGetLastError());

    return result == 0;
#else
    int flags = fcntl(socketHandle, F_GETFL, 0);

    if (flags == -1)
    {
        printf("[NET] fcntl get flags failed error=%d\n", errno);
        return false;
    }

    int result = fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK);

    if (result == -1)
        printf("[NET] fcntl nonblocking failed error=%d\n", errno);

    return result == 0;
#endif
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

} // namespace MimitaNet
