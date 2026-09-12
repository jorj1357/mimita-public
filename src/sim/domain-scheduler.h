// 09 12 2026
/* purpose
* Run multiple simulation domains at independent fixed/adaptive rates.
* The 60 Hz gameplay rule becomes one domain among many; a GR/N-body or
* soft-body solver can run its own domain at its own rate.
* Does NOT own entity state or behaviors; callers supply the per-step function.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Sim {

struct SimulationDomain {
    std::string name;
    double tickRateHz = 60.0;
    double accumulator = 0.0;
    std::uint64_t tick = 0;
    std::uint64_t maxCatchup = 5;
    bool active = true;
};

class DomainScheduler {
public:
    DomainScheduler() = default;

    static DomainScheduler& instance();

    SimulationDomain& add(const std::string& name, double tickRateHz);
    SimulationDomain* find(const std::string& name);
    const SimulationDomain* find(const std::string& name) const;

    // Advances every active domain by `dt` seconds, invoking `step(domain)` once
    // per fixed step, in registration order. Returns total steps run.
    template <typename StepFn>
    int advance(double dt, StepFn&& step)
    {
        int totalSteps = 0;
        for (auto& domain : domains_) {
            if (!domain.active || domain.tickRateHz <= 0.0)
                continue;
            domain.accumulator += dt;
            const double fixedDt = 1.0 / domain.tickRateHz;
            std::uint64_t steps = 0;
            while (domain.accumulator >= fixedDt && steps < domain.maxCatchup) {
                domain.accumulator -= fixedDt;
                ++domain.tick;
                ++steps;
                ++totalSteps;
                step(domain);
            }
            if (steps >= domain.maxCatchup && domain.accumulator >= fixedDt)
                domain.accumulator = fixedDt;
        }
        return totalSteps;
    }

    std::size_t count() const { return domains_.size(); }
    const std::vector<SimulationDomain>& domains() const { return domains_; }
    void clear() { domains_.clear(); }

private:
    std::vector<SimulationDomain> domains_;
};

} // namespace Sim
