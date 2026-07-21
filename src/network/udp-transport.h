// 07 21 2026, 18 32
/* purpose
* Implements the UDP-backed IGameTransport used by client networking paths.
* Sends datagrams to the selected server endpoint and drains local receive data.
* Keeps transport polling small and independent from packet interpretation.
* Does NOT own packet schemas, server authority, coordinator signaling, or ICE.
* Does NOT validate movement, simulate gameplay, or own reconnect policy.
* Does NOT allocate persistent packet queues outside the caller-provided vector.
*/

#pragma once

#include "network/game-transport.h"
#include "network/net_common.h"

#include <winsock2.h>
#include <cstdint>
#include <vector>
#include <cstdio>

namespace MimitaNet {

class UdpTransport : public IGameTransport
{
public:
    UdpTransport(SOCKET sock, const sockaddr_in& serverAddr)
        : mSock(sock), mServerAddr(serverAddr) {}

    bool send(const void* data, size_t size) override
    {
        int sent = sendto(mSock, (const char*)data, (int)size, 0,
                          (sockaddr*)&mServerAddr, sizeof(mServerAddr));
        return sent != SOCKET_ERROR;
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        out.clear();
        char buffer[2048];
        sockaddr_in from{};

        for (;;)
        {
            int fromLen = sizeof(from);
            int bytes = recvfrom(mSock, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
                break;

            ReceivedPacket pkt;
            pkt.bytes.assign(buffer, buffer + bytes);
            pkt.receivedAtMs = nowMs();
            out.push_back(std::move(pkt));
        }
    }

    bool connected() const override
    {
        return mSock != INVALID_SOCKET;
    }

    void close() override
    {
        if (mSock != INVALID_SOCKET)
        {
            closesocket(mSock);
            mSock = INVALID_SOCKET;
        }
    }

    SOCKET socket() const { return mSock; }

private:
    SOCKET mSock;
    sockaddr_in mServerAddr;
};

} // namespace MimitaNet
