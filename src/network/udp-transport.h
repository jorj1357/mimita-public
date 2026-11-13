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
#include "game/socket-bridge-module.h"
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
        const bool sent = mimita::game::sendToSocket(
            mSock, data, static_cast<std::uint32_t>(size), &mServerAddr);
        return sent;
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        size_t used = 0;
        char buffer[2048];
        sockaddr_in from{};

        for (;;)
        {
            int fromLen = sizeof(from);
            std::uint32_t bytes = 0;
            bool wouldBlock = false;
            mimita::game::recvFromSocket(mSock, buffer, sizeof(buffer), &from,
                                        &bytes, &wouldBlock);
            if (wouldBlock) break;
            if (bytes <= 0)
                break;

            if (used >= out.size())
                out.emplace_back();
            ReceivedPacket& pkt = out[used++];
            pkt.bytes.assign(buffer, buffer + bytes);
            pkt.receivedAtMs = nowMs();
        }
        out.resize(used);
    }

    bool connected() const override
    {
        return mSock != INVALID_SOCKET;
    }

    void close() override
    {
        if (mSock != INVALID_SOCKET)
        {
            mimita::game::closeSocket(mSock);
            mSock = INVALID_SOCKET;
        }
    }

    SOCKET socket() const { return mSock; }

private:
    SOCKET mSock;
    sockaddr_in mServerAddr;
};

} // namespace MimitaNet
