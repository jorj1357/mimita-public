// 07 21 2026, 18 32
/* purpose
* Runs a focused UDP socket transport smoke test for MiMITA networking helpers.
* Verifies IPv4 endpoint parsing, localhost normalization, nonblocking receive, and echo.
* Confirms protocol 25 packet helpers still report the expected wire identity.
* Does NOT launch mimita.exe, contact the coordinator, or run gameplay simulation.
* Does NOT test ICE, movement formulas, weapons, projectiles, or rendering.
* Does NOT rely on stale server processes or fixed UDP ports.
*/

#include "network/net_common.h"
#include "network/packets.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

bool recvWithDeadline(SOCKET sock, char* buffer, int bufferBytes,
                      sockaddr_in* from, int* outBytes, uint64_t timeoutMs)
{
    const uint64_t deadline = MimitaNet::nowMs() + timeoutMs;
    while (MimitaNet::nowMs() < deadline)
    {
        int fromLen = sizeof(*from);
        int bytes = recvfrom(sock, buffer, bufferBytes, 0,
                             (sockaddr*)from, &fromLen);
        if (bytes > 0)
        {
            *outBytes = bytes;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    *outBytes = 0;
    return false;
}

bool bindEphemeral(SOCKET sock, const char* addressText, sockaddr_in& actual)
{
    sockaddr_in bindAddr{};
    if (!MimitaNet::parseAddress(addressText, bindAddr, true))
        return false;
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
        return false;
    actual = bindAddr;
    int actualLen = sizeof(actual);
    return getsockname(sock, (sockaddr*)&actual, &actualLen) == 0;
}

} // namespace

int main()
{
    if (!MimitaNet::netStartup())
        return 1;

    sockaddr_in parsedLoopback{};
    sockaddr_in parsedAny{};
    sockaddr_in parsedLocalhost{};
    sockaddr_in parsedIpv6{};
    const bool parse127 = MimitaNet::parseAddress("127.0.0.1:1357", parsedLoopback);
    const bool parseAny = MimitaNet::parseAddress("0.0.0.0:0", parsedAny, true);
    const bool parseLocalhost = MimitaNet::parseAddress("localhost:1357", parsedLocalhost);
    const bool ipv6Rejected = !MimitaNet::parseAddress("[::1]:1357", parsedIpv6);

    MimitaNet::InputPacket input{};
    input.header.type = MimitaNet::PACKET_INPUT;
    const bool headerOk = MimitaNet::validHeader(input.header, MimitaNet::PACKET_INPUT);

    SOCKET serverSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET clientSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSock == INVALID_SOCKET || clientSock == INVALID_SOCKET)
        return 2;

    bool socketsOk = MimitaNet::setNonBlocking(serverSock) &&
                     MimitaNet::setNonBlocking(clientSock);

    sockaddr_in serverActual{};
    sockaddr_in clientActual{};
    socketsOk = socketsOk &&
                bindEphemeral(serverSock, "127.0.0.1:0", serverActual) &&
                bindEphemeral(clientSock, "127.0.0.1:0", clientActual);

    char buffer[256] = {};
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int noDataBytes = recvfrom(serverSock, buffer, sizeof(buffer), 0,
                               (sockaddr*)&from, &fromLen);
    const int noDataError = WSAGetLastError();
    const bool noDataWouldBlock = noDataBytes == SOCKET_ERROR &&
                                  noDataError == WSAEWOULDBLOCK;

    const char* payload = "udp-smoke-stage3b";
    int payloadBytes = (int)std::strlen(payload) + 1;
    int sent = sendto(clientSock, payload, payloadBytes, 0,
                      (sockaddr*)&serverActual, sizeof(serverActual));

    sockaddr_in source{};
    int receivedBytes = 0;
    const bool serverReceived =
        sent == payloadBytes &&
        recvWithDeadline(serverSock, buffer, sizeof(buffer), &source,
                         &receivedBytes, 1000);

    int echoedBytes = SOCKET_ERROR;
    if (serverReceived)
        echoedBytes = sendto(serverSock, buffer, receivedBytes, 0,
                             (sockaddr*)&source, sizeof(source));

    char echoBuffer[256] = {};
    sockaddr_in echoSource{};
    int replyBytes = 0;
    const bool clientReceived =
        echoedBytes == receivedBytes &&
        recvWithDeadline(clientSock, echoBuffer, sizeof(echoBuffer),
                         &echoSource, &replyBytes, 1000);
    const bool echoed = clientReceived &&
                        replyBytes == payloadBytes &&
                        std::memcmp(echoBuffer, payload, payloadBytes) == 0;

    std::printf("[UDP TRANSPORT SMOKE] protocol=%u parse127=%d parseAny=%d "
                "parseLocalhost=%d ipv6Rejected=%d headerOk=%d socketsOk=%d "
                "noDataWouldBlock=%d server=%s client=%s payloadBytes=%d echoed=%d\n",
                MimitaNet::PROTOCOL_VERSION,
                (int)parse127, (int)parseAny, (int)parseLocalhost,
                (int)ipv6Rejected, (int)headerOk, (int)socketsOk,
                (int)noDataWouldBlock,
                MimitaNet::addressToString(serverActual).c_str(),
                MimitaNet::addressToString(clientActual).c_str(),
                payloadBytes, (int)echoed);

    closesocket(serverSock);
    closesocket(clientSock);
    MimitaNet::netShutdown();

    return parse127 && parseAny && parseLocalhost && ipv6Rejected &&
           headerOk && socketsOk && noDataWouldBlock && echoed ? 0 : 3;
}
