// 2026-11-08
/* purpose
* plain-data boundary for authored-map geometry signature policy
* the host canonicalizes the authored document and the module hashes it
* this API does not expose World, GL, Recast, or document ownership
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_map_geometry {

constexpr std::uint32_t ABI_VERSION = 1;
constexpr std::uint32_t SIGNATURE_VERSION = 1;
constexpr std::uint32_t NODE_POLICY_VERSION = 1;
constexpr std::uint32_t OBJECT_PLAN_VERSION = 1;

// Node inclusion is gameplay/tool policy, not parsing. The loader reports the
// node; the module decides whether it is world geometry, sky-only, or skipped.
struct NodePolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::int32_t nodeIndex;
    std::uint32_t hasMesh;
    char name[mimita_module::MAX_TAG_TEXT];
    std::uint32_t outInclude; // 0 skip, 1 world, 2 sky
    std::uint32_t outReason;  // 0 default, 1 sky-name
    int result;
};

// Identity and override matching. The host owns the GLB object and the saved
// override; the module decides whether the override applies to this object,
// including a name/source alias when a node index moved after a GLB update.
struct ObjectPlanV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    char objectId[mimita_module::MAX_OBJECT_ID];
    char objectName[mimita_module::MAX_TAG_TEXT];
    std::int32_t nodeIndex;
    std::uint32_t hasOverride;
    char overrideId[mimita_module::MAX_OBJECT_ID];
    char overrideName[mimita_module::MAX_TAG_TEXT];
    char overrideSource[mimita_module::MAX_OBJECT_ID];
    std::uint32_t outMatch;
    std::uint32_t outMatchKind; // 0 none, 1 id, 2 name alias, 3 source alias
    int result;
};

struct SignatureV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    const char* bytes;
    std::uint32_t byteCount;
    std::uint32_t signatureVersion;
    std::uint64_t outSignature;
    int result;
};

} // namespace mimita_map_geometry
