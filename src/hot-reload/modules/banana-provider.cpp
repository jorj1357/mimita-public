// 09 14 2026
/* purpose
* Falsification provider: registers a capability id the EXE does not know
* (`banana.launch`) as an ordinary hot package provider. Used to prove that a
* brand-new CapabilityId can be provided by a hot source with no EXE relink.
* Editable/replaceable/deletable live; the consumer stays in banana-consumer.cpp.
* Does NOT own gameplay state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

std::uint32_t gBananaLaunches = 0;

std::uint32_t MIMITA_GAME_CALL bananaLaunch(void* /*host*/, std::uint32_t input)
{
    ++gBananaLaunches;
    return input + gBananaLaunches;
}

const MimitaHotPackage::CapabilityRegistrar s_bananaProvider{
    {gameHash("banana.launch"), gameHash("sig.banana.launch.v1"), 0,
     reinterpret_cast<void*>(&bananaLaunch), "banana.provider"}};

} // namespace

#endif
