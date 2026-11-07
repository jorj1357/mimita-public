// 2026-11-07
/* purpose
* live Infinite World generation policy
* owns connector compatibility, placement, quarter-turn alignment, chunk bounds,
* deterministic selection, and grid-cell placement
* the host owns documents, streamed chunk state, GPU resources, and authority
* this file DOES NOT include MapConfigRegistry, World, Player, or STL types
*/
#include "modules/api/worldgen-api.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <module-identity.h>

namespace {

using mimita_module::HostServicesV1;
using mimita_module::ModuleEventV1;
using mimita_worldgen::ChunkV1;
using mimita_worldgen::ConnectorV1;
using mimita_worldgen::ResolveV1;
using mimita_worldgen::TablesV1;
using mimita_worldgen::ZoneV1;

const HostServicesV1* gHost = nullptr;

enum Side
{
    SIDE_NONE = 0,
    SIDE_EAST,
    SIDE_WEST,
    SIDE_NORTH,
    SIDE_SOUTH,
    SIDE_UP,
    SIDE_DOWN,
};

Side sideFromName(const char* name)
{
    if (!name)
        return SIDE_NONE;
    if (std::strcmp(name, "east") == 0) return SIDE_EAST;
    if (std::strcmp(name, "west") == 0) return SIDE_WEST;
    if (std::strcmp(name, "north") == 0) return SIDE_NORTH;
    if (std::strcmp(name, "south") == 0) return SIDE_SOUTH;
    if (std::strcmp(name, "up") == 0) return SIDE_UP;
    if (std::strcmp(name, "down") == 0) return SIDE_DOWN;
    return SIDE_NONE;
}

Side oppositeSide(Side side)
{
    switch (side) {
    case SIDE_EAST: return SIDE_WEST;
    case SIDE_WEST: return SIDE_EAST;
    case SIDE_NORTH: return SIDE_SOUTH;
    case SIDE_SOUTH: return SIDE_NORTH;
    case SIDE_UP: return SIDE_DOWN;
    case SIDE_DOWN: return SIDE_UP;
    default: return SIDE_NONE;
    }
}

void sideDirection(Side side, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    switch (side) {
    case SIDE_EAST: out[0] = 1.0f; break;
    case SIDE_WEST: out[0] = -1.0f; break;
    case SIDE_NORTH: out[1] = 1.0f; break;
    case SIDE_SOUTH: out[1] = -1.0f; break;
    case SIDE_UP: out[2] = 1.0f; break;
    case SIDE_DOWN: out[2] = -1.0f; break;
    default: break;
    }
}

int axisForSide(Side side)
{
    switch (side) {
    case SIDE_EAST:
    case SIDE_WEST: return 0;
    case SIDE_NORTH:
    case SIDE_SOUTH: return 1;
    case SIDE_UP:
    case SIDE_DOWN: return 2;
    default: return -1;
    }
}

bool tokenListContains(const char* list, const char* token)
{
    if (!list || !token || !*token)
        return false;
    const char* cursor = list;
    while (*cursor) {
        while (*cursor == '|' || *cursor == ',')
            ++cursor;
        const char* end = cursor;
        while (*end && *end != '|' && *end != ',')
            ++end;
        const size_t length = static_cast<size_t>(end - cursor);
        if (length == std::strlen(token) &&
            std::strncmp(cursor, token, length) == 0)
            return true;
        cursor = end;
    }
    return false;
}

bool tagsCompatible(const char* a, const char* b)
{
    if (!a || !b || !*a || !*b)
        return true;
    const char* cursor = a;
    while (*cursor) {
        while (*cursor == '|' || *cursor == ',')
            ++cursor;
        const char* end = cursor;
        while (*end && *end != '|' && *end != ',')
            ++end;
        const size_t length = static_cast<size_t>(end - cursor);
        if (length > 0) {
            char token[128];
            const size_t copy = length < sizeof(token) - 1 ? length : sizeof(token) - 1;
            std::memcpy(token, cursor, copy);
            token[copy] = '\0';
            if (tokenListContains(b, token))
                return true;
        }
        cursor = end;
    }
    return false;
}

bool rolesCompatible(const char* a, const char* b)
{
    if (!a || !b)
        return false;
    if (std::strcmp(a, "none") == 0 || std::strcmp(b, "none") == 0)
        return false;
    if (std::strcmp(a, "bidirectional") == 0)
        return std::strcmp(b, "entry") == 0 ||
               std::strcmp(b, "exit") == 0 ||
               std::strcmp(b, "bidirectional") == 0;
    if (std::strcmp(b, "bidirectional") == 0)
        return std::strcmp(a, "entry") == 0 ||
               std::strcmp(a, "exit") == 0;
    return (std::strcmp(a, "entry") == 0 && std::strcmp(b, "exit") == 0) ||
           (std::strcmp(a, "exit") == 0 && std::strcmp(b, "entry") == 0);
}

bool connectorsCompatible(const ConnectorV1& a, const ConnectorV1& b)
{
    return tagsCompatible(a.tags, b.tags) && rolesCompatible(a.role, b.role);
}

bool linkMatches(const char* link, const char* chunkId, const char* connectorId)
{
    if (!link || !*link)
        return false;
    const char* separator = std::strchr(link, '/');
    if (!separator)
        separator = std::strchr(link, ':');
    if (!separator)
        return std::strcmp(link, chunkId) == 0;
    const size_t chunkLength = static_cast<size_t>(separator - link);
    if (std::strlen(chunkId) != chunkLength ||
        std::strncmp(link, chunkId, chunkLength) != 0)
        return false;
    return std::strcmp(separator + 1, connectorId) == 0;
}

bool linksAllow(const ConnectorV1& connector, const char* chunkId,
                const char* connectorId)
{
    if (!connector.links[0])
        return true;
    const char* cursor = connector.links;
    while (*cursor) {
        while (*cursor == '|' || *cursor == ',')
            ++cursor;
        const char* end = cursor;
        while (*end && *end != '|' && *end != ',')
            ++end;
        const size_t length = static_cast<size_t>(end - cursor);
        if (length > 0) {
            char link[128];
            const size_t copy = length < sizeof(link) - 1 ? length : sizeof(link) - 1;
            std::memcpy(link, cursor, copy);
            link[copy] = '\0';
            if (linkMatches(link, chunkId, connectorId))
                return true;
        }
        cursor = end;
    }
    return false;
}

bool hasExplicitLink(const ConnectorV1& connector, const char* chunkId,
                     const char* connectorId)
{
    return connector.links[0] && linksAllow(connector, chunkId, connectorId);
}

const ConnectorV1* connectorOnSide(const ChunkV1& chunk, Side side,
                                   const TablesV1& tables)
{
    for (std::uint32_t i = 0; i < chunk.connectorCount; ++i) {
        const ConnectorV1& connector = tables.connectors[chunk.connectorOffset + i];
        if (sideFromName(connector.side) == side)
            return &connector;
    }
    return nullptr;
}

void transformPoint(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2] + m[12];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2] + m[13];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2] + m[14];
}

void transformVector(const float m[16], const float in[3], float out[3])
{
    out[0] = m[0] * in[0] + m[4] * in[1] + m[8] * in[2];
    out[1] = m[1] * in[0] + m[5] * in[1] + m[9] * in[2];
    out[2] = m[2] * in[0] + m[6] * in[1] + m[10] * in[2];
}

void multiply(const float a[16], const float b[16], float out[16])
{
    float result[16] = {};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += a[k * 4 + row] * b[column * 4 + k];
            result[column * 4 + row] = value;
        }
    }
    std::memcpy(out, result, sizeof(result));
}

void translateMatrix(const float t[3], float out[16])
{
    std::memset(out, 0, sizeof(float) * 16);
    out[0] = out[5] = out[10] = out[15] = 1.0f;
    out[12] = t[0];
    out[13] = t[1];
    out[14] = t[2];
}

void rotateZQuarterMatrix(int quarterTurns, float out[16])
{
    std::memset(out, 0, sizeof(float) * 16);
    out[10] = out[15] = 1.0f;
    switch ((quarterTurns % 4 + 4) % 4) {
    case 0:
        out[0] = 1.0f; out[5] = 1.0f;
        break;
    case 1:
        out[0] = 0.0f; out[1] = 1.0f; out[4] = -1.0f; out[5] = 0.0f;
        break;
    case 2:
        out[0] = -1.0f; out[5] = -1.0f;
        break;
    default:
        out[0] = 0.0f; out[1] = -1.0f; out[4] = 1.0f; out[5] = 0.0f;
        break;
    }
}

float dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void normalize3(float v[3])
{
    const float length = std::sqrt(dot3(v, v));
    if (length <= 0.000001f) {
        v[0] = v[1] = v[2] = 0.0f;
        return;
    }
    v[0] /= length;
    v[1] /= length;
    v[2] /= length;
}

void computeBounds(const ChunkV1& chunk, const float transform[16],
                   float outMin[3], float outMax[3])
{
    outMin[0] = outMin[1] = outMin[2] = 1.0e30f;
    outMax[0] = outMax[1] = outMax[2] = -1.0e30f;
    for (int corner = 0; corner < 8; ++corner) {
        float local[3] = {
            (corner & 1) ? chunk.boundsMax[0] : chunk.boundsMin[0],
            (corner & 2) ? chunk.boundsMax[1] : chunk.boundsMin[1],
            (corner & 4) ? chunk.boundsMax[2] : chunk.boundsMin[2],
        };
        float world[3];
        transformPoint(transform, local, world);
        for (int axis = 0; axis < 3; ++axis) {
            if (world[axis] < outMin[axis]) outMin[axis] = world[axis];
            if (world[axis] > outMax[axis]) outMax[axis] = world[axis];
        }
    }
}

std::uint64_t hash64(const void* data, size_t size, std::uint64_t seed)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    std::uint64_t hash = seed ^ 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ull;
    hash ^= hash >> 27;
    return hash;
}

std::uint64_t optionScore(const ZoneV1& zone, const float desired[3],
                          const ChunkV1& chunk, const ConnectorV1& connector)
{
    std::uint64_t hash = 1469598103934665603ull;
    hash = hash64(&zone.seed, sizeof(zone.seed), hash);
    hash = hash64(zone.generatorVersion, std::strlen(zone.generatorVersion), hash);
    hash = hash64(zone.dependencies, std::strlen(zone.dependencies), hash);
    const float quantized[3] = {
        std::floor(desired[0] * 100.0f + 0.5f),
        std::floor(desired[1] * 100.0f + 0.5f),
        std::floor(desired[2] * 100.0f + 0.5f),
    };
    hash = hash64(quantized, sizeof(quantized), hash);
    hash = hash64(chunk.id, std::strlen(chunk.id), hash);
    hash = hash64(connector.id, std::strlen(connector.id), hash);
    return hash;
}

bool repeatAllows(const ZoneV1& zone, Side side)
{
    const int axis = axisForSide(side);
    if (axis < 0)
        return false;
    return zone.repeatAxes[axis] != 0;
}

void fillZoneFromRequest(const ZoneV1& zone, const ChunkV1& chunk,
                         int chunkIndex, const float transform[16],
                         ResolveV1* out)
{
    out->outChunk = chunkIndex;
    out->outValid = 1;
    std::memcpy(out->outTransform, transform, sizeof(float) * 16);
    computeBounds(chunk, transform, out->outBoundsMin, out->outBoundsMax);
    out->result = 1;
}

int resolveGridCell(ResolveV1* request)
{
    if (!request->tables || request->tables->chunkCount == 0)
        return 0;
    const ZoneV1& zone = request->zone;
    float origin[3] = {
        zone.origin[0] + static_cast<float>(request->cell[0]) * zone.cellSize[0],
        zone.origin[1] + static_cast<float>(request->cell[1]) * zone.cellSize[1],
        zone.origin[2] + static_cast<float>(request->cell[2]) * zone.cellSize[2],
    };
    std::uint64_t hash = hash64(&zone.seed, sizeof(zone.seed),
                                1469598103934665603ull);
    hash = hash64(zone.generatorVersion, std::strlen(zone.generatorVersion), hash);
    hash = hash64(zone.dependencies, std::strlen(zone.dependencies), hash);
    hash = hash64(request->cell, sizeof(request->cell), hash);
    std::uint32_t index = static_cast<std::uint32_t>(
        hash % request->tables->chunkCount);
    for (std::uint32_t attempt = 0; attempt < request->tables->chunkCount;
         ++attempt) {
        const std::uint32_t candidate =
            (index + attempt) % request->tables->chunkCount;
        const ChunkV1& chunk = request->tables->chunks[candidate];
        if (!chunk.enabled || chunk.partCount == 0)
            continue;
        float transform[16];
        translateMatrix(origin, transform);
        fillZoneFromRequest(zone, chunk, static_cast<int>(candidate), transform,
                            request);
        request->outQuarterTurns = 0;
        return 1;
    }
    return 0;
}

int resolvePair(ResolveV1* request)
{
    const TablesV1* tables = request->tables;
    if (!tables || request->parentChunk < 0 || request->childChunk < 0 ||
        static_cast<std::uint32_t>(request->parentChunk) >= tables->chunkCount ||
        static_cast<std::uint32_t>(request->childChunk) >= tables->chunkCount)
        return 0;
    const ChunkV1& parentChunk = tables->chunks[request->parentChunk];
    const ChunkV1& childChunk = tables->chunks[request->childChunk];
    if (request->parentConnector < 0 || request->childConnector < 0 ||
        static_cast<std::uint32_t>(request->parentConnector) >=
            tables->connectorCount ||
        static_cast<std::uint32_t>(request->childConnector) >=
            tables->connectorCount)
        return 0;
    const ConnectorV1& parentConnector = tables->connectors[request->parentConnector];
    const ConnectorV1& childConnector = tables->connectors[request->childConnector];
    const Side parentSide = sideFromName(parentConnector.side);
    const Side childSide = sideFromName(childConnector.side);
    if (parentSide == SIDE_NONE || childSide == SIDE_NONE ||
        childSide != oppositeSide(parentSide)) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "connector sides do not oppose");
        return 0;
    }
    if (!connectorsCompatible(parentConnector, childConnector)) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "connector role or tags incompatible");
        return 0;
    }
    if (parentConnector.links[0] &&
        !linksAllow(parentConnector, childChunk.id, childConnector.id)) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "parent explicit links reject child");
        return 0;
    }
    if (childConnector.links[0] &&
        !linksAllow(childConnector, parentChunk.id, parentConnector.id)) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "child explicit links reject parent");
        return 0;
    }

    float parentWorld[3];
    transformPoint(request->parentTransform, parentConnector.position, parentWorld);
    float parentNormal[3];
    float localParentNormal[3];
    sideDirection(parentSide, localParentNormal);
    transformVector(request->parentTransform, localParentNormal, parentNormal);
    normalize3(parentNormal);

    float childLocalNormal[3];
    sideDirection(childSide, childLocalNormal);
    int quarterTurns = -1;
    for (int turn = 0; turn < 4; ++turn) {
        float rotate[16];
        rotateZQuarterMatrix(turn, rotate);
        float rotated[3];
        transformVector(rotate, childLocalNormal, rotated);
        if (dot3(rotated, parentNormal) < -0.999f) {
            quarterTurns = turn;
            break;
        }
    }
    if (quarterTurns < 0) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "no quarter-turn aligns connectors");
        return 0;
    }

    float desired[3] = {
        parentWorld[0] + parentNormal[0] * request->zone.connectorGap,
        parentWorld[1] + parentNormal[1] * request->zone.connectorGap,
        parentWorld[2] + parentNormal[2] * request->zone.connectorGap,
    };
    float rotation[16];
    rotateZQuarterMatrix(quarterTurns, rotation);
    float rotatedConnector[3];
    transformPoint(rotation, childConnector.position, rotatedConnector);
    float translation[3] = {
        desired[0] - rotatedConnector[0],
        desired[1] - rotatedConnector[1],
        desired[2] - rotatedConnector[2],
    };
    float translate[16];
    translateMatrix(translation, translate);
    float transform[16];
    multiply(translate, rotation, transform);

    fillZoneFromRequest(request->zone, childChunk, request->childChunk,
                        transform, request);
    request->outQuarterTurns = quarterTurns;
    return 1;
}

int resolveNeighbor(ResolveV1* request)
{
    const TablesV1* tables = request->tables;
    if (!tables || tables->chunkCount == 0 ||
        request->parentChunk < 0 ||
        static_cast<std::uint32_t>(request->parentChunk) >= tables->chunkCount ||
        request->parentConnector < 0 ||
        static_cast<std::uint32_t>(request->parentConnector) >=
            tables->connectorCount)
        return 0;
    const ChunkV1& parentChunk = tables->chunks[request->parentChunk];
    const ConnectorV1& parentConnector = tables->connectors[request->parentConnector];
    const Side parentSide = sideFromName(parentConnector.side);
    const Side childSide = oppositeSide(parentSide);
    if (parentSide == SIDE_NONE || childSide == SIDE_NONE)
        return 0;
    if (!repeatAllows(request->zone, parentSide)) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "connector direction is not repeated");
        return 0;
    }

    float parentWorld[3];
    transformPoint(request->parentTransform, parentConnector.position, parentWorld);
    float localParentNormal[3];
    sideDirection(parentSide, localParentNormal);
    float parentNormal[3];
    transformVector(request->parentTransform, localParentNormal, parentNormal);
    normalize3(parentNormal);
    const float desired[3] = {
        parentWorld[0] + parentNormal[0] * request->zone.connectorGap,
        parentWorld[1] + parentNormal[1] * request->zone.connectorGap,
        parentWorld[2] + parentNormal[2] * request->zone.connectorGap,
    };

    std::uint64_t bestScore = 0;
    bool haveBest = false;
    int bestChunk = -1;
    int bestConnector = -1;
    int bestTurns = 0;
    float bestTransform[16] = {};

    for (std::uint32_t chunkIndex = 0; chunkIndex < tables->chunkCount;
         ++chunkIndex) {
        const ChunkV1& childChunk = tables->chunks[chunkIndex];
        if (!childChunk.enabled || childChunk.partCount == 0)
            continue;
        for (std::uint32_t connectorIndex = 0;
             connectorIndex < childChunk.connectorCount; ++connectorIndex) {
            const ConnectorV1& childConnector =
                tables->connectors[childChunk.connectorOffset + connectorIndex];
            if (sideFromName(childConnector.side) != childSide)
                continue;
            if (!connectorsCompatible(parentConnector, childConnector))
                continue;
            if (parentConnector.links[0] &&
                !linksAllow(parentConnector, childChunk.id, childConnector.id))
                continue;
            if (childConnector.links[0] &&
                !linksAllow(childConnector, parentChunk.id, parentConnector.id))
                continue;

            float childLocalNormal[3];
            sideDirection(childSide, childLocalNormal);
            int quarterTurns = -1;
            for (int turn = 0; turn < 4; ++turn) {
                float rotate[16];
                rotateZQuarterMatrix(turn, rotate);
                float rotated[3];
                transformVector(rotate, childLocalNormal, rotated);
                if (dot3(rotated, parentNormal) < -0.999f) {
                    quarterTurns = turn;
                    break;
                }
            }
            if (quarterTurns < 0)
                continue;

            float rotation[16];
            rotateZQuarterMatrix(quarterTurns, rotation);
            float rotatedConnector[3];
            transformPoint(rotation, childConnector.position, rotatedConnector);
            float translation[3] = {
                desired[0] - rotatedConnector[0],
                desired[1] - rotatedConnector[1],
                desired[2] - rotatedConnector[2],
            };
            float translate[16];
            translateMatrix(translation, translate);
            float transform[16];
            multiply(translate, rotation, transform);

            const std::uint64_t score = optionScore(
                request->zone, desired, childChunk, childConnector);
            if (!haveBest || score > bestScore) {
                haveBest = true;
                bestScore = score;
                bestChunk = static_cast<int>(chunkIndex);
                bestConnector = static_cast<int>(
                    childChunk.connectorOffset + connectorIndex);
                bestTurns = quarterTurns;
                std::memcpy(bestTransform, transform, sizeof(bestTransform));
            }
        }
    }
    if (!haveBest) {
        std::snprintf(request->reason, sizeof(request->reason),
                      "no compatible connector");
        return 0;
    }
    const ChunkV1& childChunk = tables->chunks[bestChunk];
    fillZoneFromRequest(request->zone, childChunk, bestChunk, bestTransform,
                        request);
    request->outQuarterTurns = bestTurns;
    return 1;
}

int onEvent(ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId != mimita_module::EVENT_WORLDGEN_RESOLVE)
        return 0;
    ResolveV1* request = static_cast<ResolveV1*>(event->payload);
    request->outValid = 0;
    request->result = 0;
    request->reason[0] = '\0';
    int handled = 0;
    if (request->requestKind == 0)
        handled = resolveGridCell(request);
    else if (request->requestKind == 1)
        handled = resolveNeighbor(request);
    else if (request->requestKind == 2)
        handled = resolvePair(request);
    if (!handled && request->reason[0] == '\0')
        std::snprintf(request->reason, sizeof(request->reason),
                      "generation request rejected");
    return handled;
}

int initialize(const HostServicesV1* services)
{
    if (!services || services->version != mimita_worldgen::ABI_VERSION ||
        services->structSize != sizeof(HostServicesV1))
        return 0;
    gHost = services;
    return 1;
}

void shutdown()
{
    gHost = nullptr;
}

std::uint32_t exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION,
     sizeof(mimita_module::ModuleV1),
     "infinite-worldgen",
     MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize,
    shutdown,
    onEvent,
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
