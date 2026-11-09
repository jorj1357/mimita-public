// 2026-11-08
/* purpose
* plain-data boundary for authored-map geometry signature policy
* the host canonicalizes the authored document and the module hashes it
* this API does not expose World, GL, Recast, or document ownership
*/
#pragma once

#include <cstdint>

namespace mimita_map_geometry {

constexpr std::uint32_t ABI_VERSION = 1;
constexpr std::uint32_t SIGNATURE_VERSION = 1;

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
