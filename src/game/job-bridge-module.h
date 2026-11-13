// 2026-11-13
#pragma once

#include <cstdint>
#include <functional>

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void initializeHostJobs(const mimita::host::HostServicesV1* services);
bool runDetached(std::function<void()> work, const char* tag);

class HostWorker {
public:
    HostWorker() = default;
    ~HostWorker();
    HostWorker(const HostWorker&) = delete;
    HostWorker& operator=(const HostWorker&) = delete;

    bool start(std::function<void()> work, const char* tag);
    void wait();
    void release();
    bool active() const { return handle_ != 0; }

private:
    std::uint64_t handle_ = 0;
};
}  // namespace mimita::game
