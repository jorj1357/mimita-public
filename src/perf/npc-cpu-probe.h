#pragma once

#include <cstdint>

// Bounded, opt-in sampling for a small set of fixed hot paths. Reported
// per-site CPU milliseconds and shares are estimates derived from sampled
// thread cycles, not exact per-function hardware-counter measurements.
enum class PerfCpuProbeSite : uint8_t {
    NpcUpdate,
    CollisionDispatch,
    EngineSimulate,
    Networking,
    EngineRender,
    Count
};

class PerfNpcCpuProbe {
public:
    PerfNpcCpuProbe(PerfCpuProbeSite site, const char* sourceFile,
                    int sourceLine, const char* functionName) noexcept;
    ~PerfNpcCpuProbe() noexcept;

    PerfNpcCpuProbe(const PerfNpcCpuProbe&) = delete;
    PerfNpcCpuProbe& operator=(const PerfNpcCpuProbe&) = delete;

private:
    PerfCpuProbeSite site_;
    const char* sourceFile_ = nullptr;
    const char* functionName_ = nullptr;
    int sourceLine_ = 0;
    uint64_t startCycles_ = 0;
    bool sampled_ = false;
};

void perfStartNpcCpuProbe(uint32_t durationSeconds = 15);
void perfStopNpcCpuProbe();
