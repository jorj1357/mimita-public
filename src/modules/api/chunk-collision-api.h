// 2026-11-07
/* purpose
* define the versioned plain-data boundary for generated chunk collision policy
* the module owns collidable-part selection and transform composition; the host
* owns static-solid union, physical-entity registration, and the broadphase
* this file DOES NOT expose PhysicalEntitySystem, World, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_chunk_collision {

constexpr std::uint32_t ABI_VERSION = 2;
constexpr std::uint32_t MAX_ID = 64;
constexpr std::uint32_t TRIANGLES_PER_BOX = 12;
constexpr std::uint32_t MAX_PARTS = 256;
constexpr std::uint32_t MAX_OUTPUT_TRIANGLES = 4096;

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    mimita_module::LogFn log;
};

struct CollisionPartV1
{
    char id[MAX_ID];
    float position[3];
    float size[3];
    float rotation[3];
    int collision;
};

struct TriangleV1
{
    float a[3];
    float b[3];
    float c[3];
    float normal[3];
};

struct BuildChunkV1
{
    char chunkId[MAX_ID];
    float chunkTransform[16];
    const CollisionPartV1* parts;
    std::uint32_t partCount;
    // World-space triangles after chunk * part transforms.
    TriangleV1* outTriangles;
    // Parallel solid-group id per triangle; equal ids may be unioned.
    std::uint32_t* outSolidIds;
    std::uint32_t capacity;
    std::uint32_t outCount;
    int result;
};

} // namespace mimita_chunk_collision

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
