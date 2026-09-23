// 09 23 2026
/* purpose
* Hot physical-contact damage/knockback policy module. Registers the generic
* `net.physical-contact` capability and serves the shared formulas from
* `hot-reload/hot-physical-contact.h`. Editing that header (or this file)
* changes contact weapon damage/knockback live: the cold bridge in
* `network/server-physical-contact.cpp` prefers this provider over its compiled
* fallback, with no EXE call site.
* Does NOT own contact detection, transport, or authoritative state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-physical-contact.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL contactDamage(void* /*host*/, GamePhysicalContactDamageV1* request)
{
    if (!request)
        return;
    request->outDamage = HotPhysicalContactImpl::damage(*request);
    request->result = 1u;
}

void MIMITA_GAME_CALL contactKnockback(void* /*host*/,
                                       GamePhysicalContactKnockbackV1* request)
{
    if (!request)
        return;
    HotPhysicalContactImpl::knockback(*request);
    request->result = 1u;
}

std::uint32_t MIMITA_GAME_CALL contactIntervalTicks(void* /*host*/, float seconds,
                                                    float tickRate)
{
    return HotPhysicalContactImpl::intervalTicks(seconds, tickRate);
}

std::uint32_t MIMITA_GAME_CALL contactShouldConfirm(void* /*host*/, std::uint32_t active,
                                                    std::int32_t pendingDamage,
                                                    std::uint32_t ending,
                                                    std::uint32_t samples,
                                                    std::uint32_t batchSize)
{
    return HotPhysicalContactImpl::shouldConfirm(active, pendingDamage, ending, samples,
                                                 batchSize);
}

const GamePhysicalContactPolicyV1 kPhysicalContactPolicy{
    sizeof(GamePhysicalContactPolicyV1), 1, &contactDamage, &contactKnockback,
    &contactIntervalTicks, &contactShouldConfirm, "net.physical-contact"};

const GamePhysicalContactPolicyV1* MIMITA_GAME_CALL lookupPhysicalContact(void* /*host*/)
{
    return &kPhysicalContactPolicy;
}

const GameCapabilityDescriptorV1 kPhysicalContactProvider{
    GAME_CAP_PHYSICAL_CONTACT, GAME_SIG_PHYSICAL_CONTACT, 0,
    reinterpret_cast<void*>(&lookupPhysicalContact), "net.physical-contact"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_physicalContactProviderRegistrar{
    kPhysicalContactProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_physicalContactRequirement{
    GAME_CAP_PHYSICAL_CONTACT, GAME_SIG_PHYSICAL_CONTACT, 0};

#endif // MIMITA_GAME_DLL
