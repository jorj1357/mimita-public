// 2026-11-07
/* purpose
* first live map-editor module generation
* owns the property-panel position decision while the host owns the map
* document, undo history, collision refresh, and persistence
* this file DOES NOT own map data, GLFW, camera, or renderer state
*/
// Behavior-preserving source-save probe for generation activation.
// Second probe: verifies host selection/undo state survives module reload.
#include "modules/api/map-editor-api.h"

#include <cstdio>
#include <cstring>

#include <module-identity.h>

namespace {

using mimita_map_editor::HostServicesV1;

const HostServicesV1* gServices = nullptr;

float distanceSquared(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

void logPosition(const char* id, const float before[3], const float after[3])
{
    if (!gServices || !gServices->log)
        return;
    char fields[384];
    std::snprintf(fields, sizeof(fields),
                  "{\"object\":\"%s\",\"field\":\"position\","
                  "\"before\":[%.4f,%.4f,%.4f],"
                  "\"after\":[%.4f,%.4f,%.4f],"
                  "\"source\":\"property_panel\",\"handled_by_module\":true,"
                  "\"module_generation\":%u}",
                  id,
                  before[0], before[1], before[2],
                  after[0], after[1], after[2],
                  static_cast<unsigned>(MIMITA_MODULE_GENERATION));
    gServices->log(
        mimita_module::LOG_CATEGORY_WORLD,
        mimita_module::LOG_LEVEL_IMPORTANT,
        "map-editor.module.position",
        id,
        "position edit handled by map-editor module",
        0,
        fields,
        __FILE__,
        __LINE__,
        __FUNCTION__);
}

int initialize(const HostServicesV1* services)
{
    if (!services || services->version != mimita_map_editor::ABI_VERSION ||
        services->structSize != sizeof(HostServicesV1) ||
        !services->getPrimarySelection || !services->applyPrimaryPosition)
        return 0;
    gServices = services;
    return 1;
}

void shutdown()
{
    gServices = nullptr;
}

int setSelectedPosition(const float position[3])
{
    if (!gServices || !position)
        return 0;

    char id[mimita_module::MAX_OBJECT_ID] = {};
    int kind = 0;
    float current[3] = {};
    if (!gServices->getPrimarySelection(
            id, static_cast<std::uint32_t>(sizeof(id)), &kind, current))
        return 0;

    if (distanceSquared(current, position) <= 1e-8f)
        return 1;

    if (!gServices->applyPrimaryPosition(position))
        return 0;

    logPosition(id, current, position);
    return 1;
}

std::uint32_t exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_map_editor::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION,
     sizeof(mimita_map_editor::ModuleV1),
     "map-editor",
     MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize,
    shutdown,
    setSelectedPosition,
    exportState,
    importState,
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    if (abiVersion != mimita_module::ABI_VERSION)
        return nullptr;
    return &gModule.base;
}
