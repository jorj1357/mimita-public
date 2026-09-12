// 09 12 2026
/* purpose
* Implements active-GameAPI module lookup by name and ABI size.
* Does NOT call the returned functions or manage generations.
*/
#include "live-code/live-modules.h"

#include "hot-reload/game-api.h"
#include "hot-reload/hot-reload-system.h"

#include <cstring>

namespace LiveModules {

const void* findFunctions(const char* name, std::uint32_t expectedStructSize)
{
    if (!name)
        return nullptr;
    const GameAPI* api = HotReloadSystem::instance().gameAPI();
    if (!api)
        return nullptr;
    const std::uint32_t count = api->moduleCount < MIMITA_GAME_MAX_MODULES
        ? api->moduleCount : MIMITA_GAME_MAX_MODULES;
    for (std::uint32_t i = 0; i < count; ++i) {
        const GameModuleDescriptor& module = api->modules[i];
        if (module.name && std::strcmp(module.name, name) == 0 &&
            module.functions && module.structSize == expectedStructSize)
            return module.functions;
    }
    return nullptr;
}

} // namespace LiveModules
