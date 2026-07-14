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

    bool send(const void* data, size_t size) override
    {
        return mAgent && mAgent->send(data, size);
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        out.clear();
        if (!mAgent) return;

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
            mAgent.reset();
        }
    }

    IceAgent* agent() { return mAgent.get(); }

private:
    std::unique_ptr<IceAgent> mAgent;
};
