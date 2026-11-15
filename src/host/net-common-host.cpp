// 2026-11-14
// Resident EXE-side WinSock ownership and socket option lifetime. The hot
// generation uses CAP_SOCKET and the game-side bridge instead.
#include "network/net_common.h"

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

bool disableUdpConnReset(SOCKET socketHandle)
{
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    BOOL disabled = FALSE;
    DWORD bytesReturned = 0;
    int result = WSAIoctl(socketHandle, SIO_UDP_CONNRESET,
                          &disabled, sizeof(disabled),
                          nullptr, 0, &bytesReturned,
                          nullptr, nullptr);
    if (result != 0)
    {
        int err = WSAGetLastError();
        if (err != WSAEINVAL && err != WSAEOPNOTSUPP)
            printf("[NET] WSAIoctl SIO_UDP_CONNRESET failed error=%d\n", err);
        return false;
    }
    return true;
}
}  // namespace MimitaNet
