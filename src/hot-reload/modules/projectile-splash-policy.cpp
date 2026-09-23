// 09 23 2026
/* purpose
* Hot projectile splash falloff policy module. Registers the generic
* `net.projectile-splash` capability and serves the shared curves from
* `hot-reload/hot-projectile-splash.h`. Editing that header (or this file)
* changes explosion falloff live: the cold path in
* `network/server-projectiles.cpp` prefers this provider over its compiled
* fallback, with no EXE call site.
* Does NOT own projectile state, collision, or damage application.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile-splash.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL splashDamage(void* /*host*/, GameSplashFalloffV1* request)
{
    HotProjectileSplashImpl::damage(*request);
}

void MIMITA_GAME_CALL splashKnockScale(void* /*host*/, GameSplashFalloffV1* request)
{
    HotProjectileSplashImpl::knockScale(*request);
}

const GameProjectileSplashPolicyV1 kProjectileSplashPolicy{
    sizeof(GameProjectileSplashPolicyV1), 1, &splashDamage, &splashKnockScale,
    "net.projectile-splash"};

const GameProjectileSplashPolicyV1* MIMITA_GAME_CALL lookupProjectileSplash(void* /*host*/)
{
    return &kProjectileSplashPolicy;
}

const GameCapabilityDescriptorV1 kProjectileSplashProvider{
    GAME_CAP_PROJECTILE_SPLASH, GAME_SIG_PROJECTILE_SPLASH, 0,
    reinterpret_cast<void*>(&lookupProjectileSplash), "net.projectile-splash"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_projectileSplashProviderRegistrar{
    kProjectileSplashProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_projectileSplashRequirement{
    GAME_CAP_PROJECTILE_SPLASH, GAME_SIG_PROJECTILE_SPLASH, 0};

#endif // MIMITA_GAME_DLL
