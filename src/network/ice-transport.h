// 07 21 2026, 18 32
/* purpose
* Adapts IceAgent datagrams to the generic game transport interface.
* Drains queued ICE receive events into ReceivedPacket records for server/client code.
* Keeps ICE transport ownership tied to its live agent lifetime.
* Does NOT own ICE signaling, packet schemas, UDP sockets, or gameplay simulation.
* Does NOT validate movement, authorize joins, or contact the coordinator.
* Does NOT buffer packets beyond one caller poll.
*/

#pragma once

#include "network/game-transport.h"
#include "network/ice/ice-agent.h"

#include <memory>
#include <vector>
#include <cstdint>

class IceTransport : public IGameTransport
{
public:
    IceTransport(std::unique_ptr<IceAgent> agent)
        : mAgent(std::move(agent)) {}

    IceTransport(IceTransport&&) = default;
    IceTransport& operator=(IceTransport&&) = default;

    bool send(const void* data, size_t size) override
    {
        return mAgent && mAgent->send(data, size);
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        out.clear();
        if (!mAgent) return;
        mAgent->tick();

        std::vector<IceEvent> events;
        mAgent->pollEvents(events);

        uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

        for (auto& ev : events)
        {
            if (ev.type == IceEventType::Recv)
            {
                ReceivedPacket pkt;
                pkt.bytes.assign(ev.data.begin(), ev.data.end());
                pkt.receivedAtMs = now;
                out.push_back(std::move(pkt));
            }
        }
    }

    bool connected() const override
    {
        return mAgent && (mAgent->state() == IceAgentState::Connected ||
                          mAgent->state() == IceAgentState::Completed);
    }

    void close() override
    {
        if (mAgent)
        {
            mAgent->shutdown();
            // Brief drain window — juice_destroy() closes the socket and
            // joins the background thread, but a few final OS-queued
            // packets may still fire callbacks.  A 50ms yield is enough
            // for those to flush through, after which it's safe to free.
            Sleep(50);
            mAgent.reset();
        }
    }

    IceAgent* agent() { return mAgent.get(); }

private:
    std::unique_ptr<IceAgent> mAgent;
};
