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

constexpr std::uint32_t ABI_VERSION = 2;
constexpr std::uint32_t MAX_MODULE_NAME = 64;
constexpr std::uint32_t MAX_OBJECT_ID = 128;
constexpr std::uint32_t MAX_TAG_TEXT = 256;

// Numeric mirrors of StructuredCategory/StructuredLevel so a module does not
// need to include the host logger's C++ headers.
constexpr std::uint32_t LOG_CATEGORY_GENERAL = 0;
constexpr std::uint32_t LOG_CATEGORY_WORLD = 16;
constexpr std::uint32_t LOG_CATEGORY_RENDERING = 21;
constexpr std::uint32_t LOG_CATEGORY_NPC_MOVEMENT = 9;
constexpr std::uint32_t LOG_LEVEL_ERRORS = 1;
constexpr std::uint32_t LOG_LEVEL_IMPORTANT = 2;
constexpr std::uint32_t LOG_LEVEL_VERBOSE = 3;

// Capability tables fetched through HostServicesV1::getCapability. Each table
// begins with version/structSize so a module can reject an incompatible host.
constexpr std::uint32_t CAPABILITY_EDITOR = 1;
constexpr std::uint32_t CAPABILITY_WORLDGEN = 2;
constexpr std::uint32_t CAPABILITY_MESH = 3;
constexpr std::uint32_t CAPABILITY_COLLISION = 4;

// Generic event ids. A module only receives events that its catalog entry
// declares; the host never dispatches by hard-coded module name.
constexpr std::uint32_t EVENT_EDITOR_SET_POSITION = 100;
constexpr std::uint32_t EVENT_EDITOR_DRAG_DELTA = 101;
constexpr std::uint32_t EVENT_EDITOR_GIZMO_PICK = 102;
constexpr std::uint32_t EVENT_EDITOR_COMMAND = 103;
constexpr std::uint32_t EVENT_EDITOR_ADD_CHUNK = 104;
constexpr std::uint32_t EVENT_EDITOR_ADD_CHUNK_PART = 105;
constexpr std::uint32_t EVENT_EDITOR_ADD_CONNECTOR = 106;
constexpr std::uint32_t EVENT_EDITOR_PREVIEW_CONNECTIONS = 107;
constexpr std::uint32_t EVENT_EDITOR_INPUT = 108;
constexpr std::uint32_t EVENT_EDITOR_CLICK = 109;
constexpr std::uint32_t EVENT_RENDER_ANALYZE_FACES = 200;
constexpr std::uint32_t EVENT_RENDER_BUILD_BOX = 201;
constexpr std::uint32_t EVENT_WORLDGEN_RESOLVE = 300;
constexpr std::uint32_t EVENT_WORLDGEN_PREVIEW_PAIR = 301;
constexpr std::uint32_t EVENT_COLLISION_BUILD_CHUNK = 400;
constexpr std::uint32_t EVENT_COLLISION_CONTACT_POLICY = 401;
constexpr std::uint32_t EVENT_COLLISION_SURFACE_POLICY = 402;
constexpr std::uint32_t EVENT_NPC_BEHAVIOR_DECISION = 500;

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

using GetCapabilityFn = const void* (MIMITA_MODULE_CALL *)(
    std::uint32_t capability, std::uint32_t version);

struct BaseV1
{
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    const char* name;
    std::uint32_t generation;
    const char* codeHash;
    std::uint32_t stateSchema;
};

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    LogFn log;
    GetCapabilityFn getCapability;
    std::uint64_t (MIMITA_MODULE_CALL *revision)();
};

struct ModuleEventV1
{
    std::uint32_t typeId;
    std::uint32_t structSize;
    std::uint64_t tick;
    void* payload;
    std::uint32_t payloadSize;
    int handled;
};

struct ModuleV1
{
    BaseV1 base;
    int (MIMITA_MODULE_CALL *initialize)(const HostServicesV1* services);
    void (MIMITA_MODULE_CALL *shutdown)();
    // Return 1 when the event was handled. The host leaves `handled` set when a
    // later module must not also act on the same event.
    int (MIMITA_MODULE_CALL *onEvent)(ModuleEventV1* event);
    std::uint32_t (MIMITA_MODULE_CALL *exportState)(
        std::uint8_t* buffer, std::uint32_t capacity);
    int (MIMITA_MODULE_CALL *importState)(
        const std::uint8_t* buffer, std::uint32_t size);
};

using GetModuleAPIFn = const BaseV1* (MIMITA_MODULE_CALL *)(std::uint32_t abiVersion);

constexpr const char* GET_MODULE_API_SYMBOL = "GetMimitaModuleAPI";

} // namespace mimita_module
