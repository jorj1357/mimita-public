// 2026-11-07
/* purpose
* define the generic versioned ABI for MiMITA reloadable modules
* keep module tables plain-data and free of STL/engine ownership
* this file DOES NOT define gameplay behavior or individual module APIs
*/
#pragma once

#include <cstdint>

#if defined(_WIN32)
#define MIMITA_MODULE_CALL __cdecl
#if defined(MIMITA_MODULE_BUILD)
#define MIMITA_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#define MIMITA_MODULE_EXPORT extern "C"
#endif
#else
#define MIMITA_MODULE_CALL
#define MIMITA_MODULE_EXPORT extern "C"
#endif

namespace mimita_module {

constexpr std::uint32_t ABI_VERSION = 1;
constexpr std::uint32_t MAX_MODULE_NAME = 64;
constexpr std::uint32_t MAX_OBJECT_ID = 128;

// Numeric mirrors of StructuredCategory/StructuredLevel so a module does not
// need to include the host logger's C++ headers.
constexpr std::uint32_t LOG_CATEGORY_GENERAL = 0;
constexpr std::uint32_t LOG_CATEGORY_WORLD = 16;
constexpr std::uint32_t LOG_CATEGORY_RENDERING = 21;
constexpr std::uint32_t LOG_LEVEL_ERRORS = 1;
constexpr std::uint32_t LOG_LEVEL_IMPORTANT = 2;
constexpr std::uint32_t LOG_LEVEL_VERBOSE = 3;

using LogFn = void (MIMITA_MODULE_CALL *)(
    std::uint32_t category,
    std::uint32_t level,
    const char* eventName,
    const char* correlationId,
    const char* reason,
    std::uint32_t tick,
    const char* jsonFields,
    const char* sourceFile,
    int sourceLine,
    const char* functionName);

struct BaseV1
{
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    const char* name;
    std::uint32_t generation;
    const char* codeHash;
    std::uint32_t stateSchema;
};

using GetModuleAPIFn = const BaseV1* (MIMITA_MODULE_CALL *)(std::uint32_t abiVersion);

constexpr const char* GET_MODULE_API_SYMBOL = "GetMimitaModuleAPI";

} // namespace mimita_module
