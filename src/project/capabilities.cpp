// 09 12 2026
/* purpose
* Implements capability grants and requests.
* Does NOT enforce capabilities at call sites yet; callers check `allows`.
*/
#include "project/capabilities.h"

namespace Project {

CapabilityRegistry& CapabilityRegistry::instance()
{
    static CapabilityRegistry registry;
    return registry;
}

void CapabilityRegistry::grant(const std::string& subject, CapabilityMask mask)
{
    grants_[subject] |= mask;
}

void CapabilityRegistry::setRequested(const std::string& subject, CapabilityMask mask)
{
    requests_[subject] = mask;
}

CapabilityMask CapabilityRegistry::granted(const std::string& subject) const
{
    auto it = grants_.find(subject);
    return it == grants_.end() ? 0 : it->second;
}

CapabilityMask CapabilityRegistry::requested(const std::string& subject) const
{
    auto it = requests_.find(subject);
    return it == requests_.end() ? 0 : it->second;
}

CapabilityMask CapabilityRegistry::denied(const std::string& subject) const
{
    return requested(subject) & ~granted(subject);
}

bool CapabilityRegistry::allows(const std::string& subject, Capability capability) const
{
    return hasCapability(granted(subject), capability);
}

} // namespace Project
