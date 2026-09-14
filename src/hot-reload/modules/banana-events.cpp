// 09 14 2026
/* purpose
* Recreated after a live delete test: registers the brand-new event type
* banana.eaten again. Demonstrates delete/replace of a hot source file while
* the process stays running.
* Does NOT own gameplay state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdio>

namespace {

void MIMITA_GAME_CALL bananaEatenDispatch(void* /*host*/, const GameEventV1* event)
{
    std::printf("[HOT_EVENT] banana.eaten delivered (replaced) typeId=%llu tick=%llu\n",
                event ? (unsigned long long)event->typeId : 0ull,
                event ? (unsigned long long)event->tick : 0ull);
}

const MimitaHotPackage::EventRegistrar s_bananaEaten{
    {gameHash("banana.eaten"), gameHash("banana.eaten.v1"), 0,
     bananaEatenDispatch, "banana.eaten"}};

} // namespace

#endif
