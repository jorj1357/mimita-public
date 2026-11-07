// 2026-11-07
/* purpose
* define the versioned plain-data boundary for Infinite World generation policy
* host owns map documents and streamed chunk state; the module owns placement,
* connector compatibility, quarter-turn solving, and streaming-distance policy
* this file DOES NOT expose MapConfigRegistry, World, Player, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_worldgen {

constexpr std::uint32_t ABI_VERSION = 2;
constexpr std::uint32_t MAX_ID = 64;
constexpr std::uint32_t MAX_TEXT = 256;

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    mimita_module::LogFn log;
};

struct ZoneV1
{
    char id[MAX_ID];
    char plane[32];
    char spacingMode[32];
    char generatorVersion[32];
    char ruleSet[32];
    char dependencies[MAX_TEXT];
    std::uint32_t seed;
    float origin[3];
    float cellSize[3];
    float chunkSize[3];
    int repeatAxes[3];
    // Connector mode: meters per axis. Grid mode: cells per axis.
    int streamRadius[3];
    float connectorGap;
    int maxBranchDepth;
    int allowCycles;
    int allowCrossings;
};

struct ConnectorV1
{
    char id[MAX_ID];
    char side[16];
    char orientation[32];
    char role[32];
    char tags[MAX_TEXT];
    char links[MAX_TEXT];
    float position[3];
    float radius;
};

struct PartV1
{
    char id[MAX_ID];
    char kind[32];
    float position[3];
    float size[3];
    float rotation[3];
    int collision;
    int visible;
};

struct ChunkV1
{
    char id[MAX_ID];
    float boundsMin[3];
    float boundsMax[3];
    std::uint32_t partOffset;
    std::uint32_t partCount;
    std::uint32_t connectorOffset;
    std::uint32_t connectorCount;
    int enabled;
};

struct TablesV1
{
    const ChunkV1* chunks;
    std::uint32_t chunkCount;
    const PartV1* parts;
    std::uint32_t partCount;
    const ConnectorV1* connectors;
    std::uint32_t connectorCount;
};

// requestKind: 0 = grid cell, 1 = connector neighbor, 2 = explicit pair preview.
struct ResolveV1
{
    ZoneV1 zone;
    const TablesV1* tables;
    std::int32_t requestKind;
    std::int32_t cell[3];
    std::int32_t anchor[3];
    std::int32_t parentChunk;
    std::int32_t parentConnector;
    std::int32_t childChunk;
    std::int32_t childConnector;
    float parentTransform[16];

    std::int32_t outChunk;
    std::int32_t outValid;
    std::int32_t outQuarterTurns;
    float outTransform[16];
    float outBoundsMin[3];
    float outBoundsMax[3];
    float outDistanceMeters;
    char reason[128];
    int result;
};

} // namespace mimita_worldgen

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
