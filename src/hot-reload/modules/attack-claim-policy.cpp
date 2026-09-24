// 09 23 2026
/* purpose
* Hot client hit-claim policy module. Registers the generic `net.attack-claim`
* capability and serves the shared eligibility/tolerance policy from
* `hot-reload/hot-attack-claim.h`. Editing that header (or this file) changes
* "shoot what I saw" acceptance live: the cold path in
* `network/server-attack.cpp` prefers this provider over its compiled fallback,
* with no EXE call site.
* Does NOT own hit geometry, poses, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-attack-claim.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateAttackClaim(void* /*host*/, GameAttackClaimV1* request)
{
    HotAttackClaimImpl::evaluate(*request);
}

void MIMITA_GAME_CALL resolveClaimPart(void* /*host*/, GameClaimPartV1* request)
{
    HotAttackClaimImpl::resolvePart(*request);
}

const GameCapabilityDescriptorV1 kAttackClaimProvider{
    GAME_CAP_ATTACK_CLAIM, GAME_SIG_ATTACK_CLAIM, 0,
    reinterpret_cast<void*>(&evaluateAttackClaim), "net.attack-claim"};
const GameCapabilityDescriptorV1 kClaimPartProvider{
    GAME_CAP_CLAIM_PART, GAME_SIG_CLAIM_PART, 0,
    reinterpret_cast<void*>(&resolveClaimPart), "net.claim-part"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_attackClaimProviderRegistrar{
    kAttackClaimProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_attackClaimRequirement{
    GAME_CAP_ATTACK_CLAIM, GAME_SIG_ATTACK_CLAIM, 0};
const MimitaHotPackage::CapabilityRegistrar s_claimPartProviderRegistrar{
    kClaimPartProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_claimPartRequirement{
    GAME_CAP_CLAIM_PART, GAME_SIG_CLAIM_PART, 0};

#endif // MIMITA_GAME_DLL
