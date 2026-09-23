// 09 23 2026
/* purpose
* Hot server movement-report validation policy module. Registers the generic
* `net.movement.validate` capability and serves the shared policy from
* `hot-reload/hot-movement-validation.h`. Editing that header (or this file)
* changes the server's accept/correct/reject decision live: the cold bridge in
* `network/movement-validation.cpp` prefers this provider over its compiled
* fallback, with no EXE call site.
* Does NOT own the world, transport, or authoritative movement state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-movement-validation.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL validateMovementReport(void* /*host*/,
                                             GameMovementValidateV1* request)
{
    HotMovementValidationImpl::validate(request);
}

const GameCapabilityDescriptorV1 kMovementValidateProvider{
    GAME_CAP_MOVEMENT_VALIDATE, GAME_SIG_MOVEMENT_VALIDATE, 0,
    reinterpret_cast<void*>(&validateMovementReport), "net.movement.validate"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_movementValidateProviderRegistrar{
    kMovementValidateProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_movementValidateRequirement{
    GAME_CAP_MOVEMENT_VALIDATE, GAME_SIG_MOVEMENT_VALIDATE, 0};

#endif // MIMITA_GAME_DLL
