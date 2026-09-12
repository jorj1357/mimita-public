// 09 12 2026
/* purpose
* Implements the EXE-side bridge to the hot presentation module.
* Owns ABI-safe module invocation for damage numbers and rocket trails.
*/
#include "live-code/live-presentation.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"

namespace {

const GamePresentationModuleV1* presentationModule()
{
    return static_cast<const GamePresentationModuleV1*>(
        LiveModules::findFunctions("presentation", sizeof(GamePresentationModuleV1)));
}

} // namespace

namespace LivePresentation {

bool formatDamage(const DamageNumberStyleV1& base, int damage, std::uint32_t flags,
                  DamageNumberStyleV1& out)
{
    const GamePresentationModuleV1* module = presentationModule();
    if (!module || !module->formatDamageNumber)
        return false;
    out = base;
    return module->formatDamageNumber(
        &base, damage, flags, &out, &HotReloadSystem::instance().gameMemory());
}

bool rocketTrail(const RocketTrailStyleV1& base, RocketTrailStyleV1& out)
{
    const GamePresentationModuleV1* module = presentationModule();
    if (!module || !module->rocketTrail)
        return false;
    out = base;
    return module->rocketTrail(
        &base, &out, &HotReloadSystem::instance().gameMemory());
}

} // namespace LivePresentation
