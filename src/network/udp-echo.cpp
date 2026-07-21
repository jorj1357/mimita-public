// 07 21 2026, 18 32
/* purpose
* Implements a bounded raw UDP echo server for live transport diagnostics.
* Logs requested and actual bind endpoints, receive sources, and echo results.
* Gives automated harnesses a minimal socket path independent from gameplay.
* Does NOT own gameplay packet dispatch, coordinator registration, or ICE signaling.
* Does NOT alter movement, weapon, projectile, damage, or snapshot behavior.
* Does NOT run indefinitely when a timeout is omitted by a test harness.
*/

#include "network/udp-echo.h"
#include "network/net_common.h"
#include "network/packets.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include <windows.h>

namespace MimitaNet {
namespace {

std::string processPath()
{
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, path, (DWORD)sizeof(path));
    if (length == 0 || length >= sizeof(path))
        return "(unknown)";
    return std::string(path, path + length);
}

std::string currentDirectory()
{
    std::error_code error;
    std::filesystem::path path = std::filesystem::current_path(error);
    if (error)
        return "(unknown)";
    return path.string();
}

sockaddr_in actualSocketAddress(SOCKET sock, const sockaddr_in& fallback)
{
    sockaddr_in actual = fallback;
    int actualLen = sizeof(actual);
    if (getsockname(sock, (sockaddr*)&actual, &actualLen) != 0)
        actual = fallback;
    return actual;
}

} // namespace

int runUdpEchoServer(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (!netStartup())
    {
        printf("[UDP ECHO FATAL] stage=startup error=WSAStartup\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("[UDP ECHO FATAL] stage=socket error=%d\n", WSAGetLastError());
        netShutdown();
        return 1;
    }

    int reuseAddr = 1;
    const bool reuseOk = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
        (const char*)&reuseAddr, sizeof(reuseAddr)) != SOCKET_ERROR;
    const bool nonBlockingOk = setNonBlocking(sock);

    const std::string requested = options.bindExplicit ? options.bind : "127.0.0.1:0";
    sockaddr_in bindAddr{};
    if (!parseAddress(requested, bindAddr, true))
    {
        printf("[UDP ECHO FATAL] stage=parse-bind requested=%s family=AF_INET\n",
               requested.c_str());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    const uint32_t timeoutSecs = options.timeoutSecs > 0 ? options.timeoutSecs : 10;
    printf("[UDP ECHO START] protocol=%u role=udp-echo-server pid=%lu exe=\"%s\" cwd=\"%s\" "
           "requested=%s family=AF_INET reuse=%d nonblocking=%d timeout=%u\n",
           PROTOCOL_VERSION, (unsigned long)GetCurrentProcessId(),
           processPath().c_str(), currentDirectory().c_str(), requested.c_str(),
           (int)reuseOk, (int)nonBlockingOk, timeoutSecs);

    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        printf("[UDP ECHO FATAL] stage=bind requested=%s error=%d\n",
               requested.c_str(), WSAGetLastError());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    sockaddr_in actual = actualSocketAddress(sock, bindAddr);
    printf("[UDP ECHO READY] requested=%s actual=%s\n",
           requested.c_str(), addressToString(actual).c_str());

    uint64_t recvAttempts = 0;
    uint64_t recvWouldBlock = 0;
    uint64_t recvErrors = 0;
    uint64_t packetsReceived = 0;
    uint64_t packetsSent = 0;
    uint64_t bytesReceived = 0;
    uint64_t bytesSent = 0;
    uint64_t sendErrors = 0;
    const uint64_t started = nowMs();

    while (nowMs() - started < (uint64_t)timeoutSecs * 1000)
    {
        char buffer[2048];
        sockaddr_in from{};
        int fromLen = sizeof(from);
        ++recvAttempts;
        int bytes = recvfrom(sock, buffer, sizeof(buffer), 0,
                             (sockaddr*)&from, &fromLen);
        if (bytes <= 0)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                ++recvWouldBlock;
            else
            {
                ++recvErrors;
                printf("[UDP ECHO RX ERROR] error=%d local=%s\n",
                       err, addressToString(actual).c_str());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        ++packetsReceived;
        bytesReceived += (uint64_t)bytes;
        const std::string source = addressToString(from);
        printf("[UDP ECHO RX] source=%s bytes=%d\n", source.c_str(), bytes);

        int sent = sendto(sock, buffer, bytes, 0, (sockaddr*)&from, sizeof(from));
        if (sent == SOCKET_ERROR)
        {
            ++sendErrors;
            printf("[UDP ECHO TX ERROR] dest=%s bytes=%d error=%d\n",
                   source.c_str(), bytes, WSAGetLastError());
            continue;
        }

        ++packetsSent;
        bytesSent += (uint64_t)sent;
        printf("[UDP ECHO TX] dest=%s bytes=%d sent=%d\n",
               source.c_str(), bytes, sent);
    }

    printf("[UDP ECHO SUMMARY] packetsReceived=%llu bytesReceived=%llu packetsSent=%llu "
           "bytesSent=%llu recvAttempts=%llu recvWouldBlock=%llu recvErrors=%llu sendErrors=%llu\n",
           (unsigned long long)packetsReceived,
           (unsigned long long)bytesReceived,
           (unsigned long long)packetsSent,
           (unsigned long long)bytesSent,
           (unsigned long long)recvAttempts,
           (unsigned long long)recvWouldBlock,
           (unsigned long long)recvErrors,
           (unsigned long long)sendErrors);

    closesocket(sock);
    netShutdown();
    return packetsReceived > 0 && packetsSent > 0 && sendErrors == 0 ? 0 : 2;
}

} // namespace MimitaNet
