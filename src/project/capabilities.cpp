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

void CapabilityRegistry::provideId(const std::string& subject, std::uint64_t capabilityId)
{
    providedIds_[subject].insert(capabilityId);
}

void CapabilityRegistry::requestId(const std::string& subject, std::uint64_t capabilityId)
{
    requestedIds_[subject].insert(capabilityId);
}

bool CapabilityRegistry::providesId(const std::string& subject, std::uint64_t capabilityId) const
{
    auto it = providedIds_.find(subject);
    return it != providedIds_.end() && it->second.count(capabilityId) != 0;
}

bool CapabilityRegistry::allowsId(const std::string& subject, std::uint64_t capabilityId) const
{
    return providesId(subject, capabilityId) || providesId("*", capabilityId);
}

std::size_t CapabilityRegistry::providedIdCount(const std::string& subject) const
{
    auto it = providedIds_.find(subject);
    return it == providedIds_.end() ? 0 : it->second.size();
}

} // namespace Project
