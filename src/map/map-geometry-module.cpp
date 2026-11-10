// 2026-11-08
#include "modules/api/map-geometry-api.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "modules/api/module-abi.h"
#include <module-identity.h>

namespace {

bool containsAny(const char* text, const char* const* needles,
                 std::size_t needleCount)
{
    if (!text)
        return false;
    for (std::size_t i = 0; i < needleCount; ++i) {
        const char* needle = needles[i];
        if (!needle || !*needle)
            continue;
        const std::size_t length = std::strlen(needle);
        for (const char* at = text; *at; ++at) {
            std::size_t matched = 0;
            while (matched < length && at[matched] &&
                   std::tolower(static_cast<unsigned char>(at[matched])) ==
                       std::tolower(static_cast<unsigned char>(needle[matched])))
                ++matched;
            if (matched == length)
                return true;
        }
    }
    return false;
}

bool isSkyName(const char* name)
{
    static const char* const needles[] = {
        "sky", "skybox", "skydome", "sky_dome", "environment",
        "worldsphere"};
    return containsAny(name, needles,
                       sizeof(needles) / sizeof(needles[0]));
}

bool textEquals(const char* a, const char* b)
{
    return a && b && std::strcmp(a, b) == 0;
}

bool startsWithNodeId(const char* id)
{
    return id && std::strncmp(id, "node:", 5) == 0;
}

int nodePolicy(mimita_map_geometry::NodePolicyV1& policy)
{
    if (policy.version != mimita_map_geometry::ABI_VERSION ||
        policy.structSize != sizeof(policy))
        return 0;
    policy.outInclude = policy.hasMesh ? 1u : 0u;
    policy.outReason = 0u;
    if (policy.hasMesh && isSkyName(policy.name)) {
        policy.outInclude = 2u;
        policy.outReason = 1u;
    }
    policy.result = 1;
    return 1;
}

int objectPlan(mimita_map_geometry::ObjectPlanV1& plan)
{
    if (plan.version != mimita_map_geometry::ABI_VERSION ||
        plan.structSize != sizeof(plan))
        return 0;
    plan.outMatch = 0u;
    plan.outMatchKind = 0u;
    if (plan.hasOverride) {
        if (textEquals(plan.overrideId, plan.objectId)) {
            plan.outMatch = 1u;
            plan.outMatchKind = 1u;
        } else if (plan.overrideSource[0] &&
                   textEquals(plan.overrideSource, plan.objectId)) {
            plan.outMatch = 1u;
            plan.outMatchKind = 3u;
        } else if (startsWithNodeId(plan.overrideId) &&
                   plan.overrideName[0] &&
                   textEquals(plan.overrideName, plan.objectName)) {
            // A GLB re-export can move a node index. Match the saved display
            // name only for index-based ids so unrelated objects do not match.
            plan.outMatch = 1u;
            plan.outMatchKind = 2u;
        }
    }
    plan.result = 1;
    return 1;
}

int signature(mimita_map_geometry::SignatureV1& request)
{
    if (request.signatureVersion != mimita_map_geometry::SIGNATURE_VERSION)
        return 0;
    if (request.byteCount != 0 && !request.bytes)
        return 0;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint32_t i = 0; i < request.byteCount; ++i) {
        hash ^= static_cast<unsigned char>(request.bytes[i]);
        hash *= 1099511628211ull;
    }
    request.outSignature = hash ? hash : 1ull;
    request.result = 1;
    return 1;
}

int onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId == mimita_module::EVENT_MAP_GEOMETRY_SIGNATURE &&
        event->payloadSize == sizeof(mimita_map_geometry::SignatureV1))
        return signature(*static_cast<mimita_map_geometry::SignatureV1*>(
            event->payload));
    if (event->typeId == mimita_module::EVENT_MAP_GEOMETRY_NODE_POLICY &&
        event->payloadSize == sizeof(mimita_map_geometry::NodePolicyV1))
        return nodePolicy(*static_cast<mimita_map_geometry::NodePolicyV1*>(
            event->payload));
    if (event->typeId == mimita_module::EVENT_MAP_GEOMETRY_OBJECT_PLAN &&
        event->payloadSize == sizeof(mimita_map_geometry::ObjectPlanV1))
        return objectPlan(*static_cast<mimita_map_geometry::ObjectPlanV1*>(
            event->payload));
    return 0;
}

int initialize(const mimita_module::HostServicesV1* host)
{
    return host && host->version == mimita_module::ABI_VERSION &&
           host->structSize == sizeof(*host);
}

void shutdown() {}

std::uint32_t exportState(std::uint8_t*, std::uint32_t) { return 0; }
int importState(const std::uint8_t*, std::uint32_t size) { return size == 0; }

const mimita_module::ModuleV1 module = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "map-geometry", MIMITA_MODULE_GENERATION, MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t version)
{
    return version == mimita_module::ABI_VERSION ? &module.base : nullptr;
}
