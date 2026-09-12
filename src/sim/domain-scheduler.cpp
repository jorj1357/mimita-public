// 09 12 2026
/* purpose
* Implements the multi-domain scheduler singleton.
* Does NOT own entity state or behaviors.
*/
#include "sim/domain-scheduler.h"

namespace Sim {

DomainScheduler& DomainScheduler::instance()
{
    static DomainScheduler scheduler;
    return scheduler;
}

SimulationDomain& DomainScheduler::add(const std::string& name, double tickRateHz)
{
    if (SimulationDomain* existing = find(name)) {
        existing->tickRateHz = tickRateHz;
        return *existing;
    }
    SimulationDomain domain;
    domain.name = name;
    domain.tickRateHz = tickRateHz;
    domains_.push_back(domain);
    return domains_.back();
}

SimulationDomain* DomainScheduler::find(const std::string& name)
{
    for (auto& domain : domains_)
        if (domain.name == name)
            return &domain;
    return nullptr;
}

const SimulationDomain* DomainScheduler::find(const std::string& name) const
{
    for (const auto& domain : domains_)
        if (domain.name == name)
            return &domain;
    return nullptr;
}

} // namespace Sim
