// 2026-11-07
/* purpose
* define the versioned plain-data boundary between the map editor module and
* the host-owned editor document/input/render services
* this file DOES NOT declare MapEditor, MapConfigRegistry, GLFW, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_map_editor {

constexpr std::uint32_t ABI_VERSION = 1;

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    mimita_module::LogFn log;

    // Copy the primary selection ID, kind, and current world position.
    // Returns 1 when a selection exists.
    int (MIMITA_MODULE_CALL* getPrimarySelection)(
        char* id, std::uint32_t idCapacity, int* kind, float* position);

    // Apply an absolute world position for the primary selection through the
    // host document, undo, collision, and persistence services.
    int (MIMITA_MODULE_CALL* applyPrimaryPosition)(const float* position);

    void (MIMITA_MODULE_CALL* notify)(const char* message);
    std::uint64_t (MIMITA_MODULE_CALL* revision)();
};

struct ModuleV1
{
    mimita_module::BaseV1 base;
    int (MIMITA_MODULE_CALL* initialize)(const HostServicesV1* services);
    void (MIMITA_MODULE_CALL* shutdown)();
    int (MIMITA_MODULE_CALL* setSelectedPosition)(const float position[3]);
    std::uint32_t (MIMITA_MODULE_CALL* exportState)(
        std::uint8_t* buffer, std::uint32_t capacity);
    int (MIMITA_MODULE_CALL* importState)(
        const std::uint8_t* buffer, std::uint32_t size);
};

} // namespace mimita_map_editor

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
