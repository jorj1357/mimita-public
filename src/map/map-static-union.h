#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "physics/physics-types.h"

struct StaticUnionStats
{
    uint32_t inputTriangles = 0;
    uint32_t outputTriangles = 0;
    uint32_t planeGroups = 0;
    uint32_t solidGroups = 0;
    uint32_t candidatePairs = 0;
    uint32_t removedTriangles = 0;
    uint32_t unionSolids = 0;
    uint32_t unionInputTriangles = 0;
    uint32_t unionOutputTriangles = 0;
    uint32_t unionRawTriangles = 0;
    double unionMs = 0.0;
    bool unionFallback = false;
    std::string unionFallbackReason;
    double elapsedMs = 0.0;
    bool fallback = false;
    std::string fallbackReason;
};

bool unionStaticSolidGroups(
    std::vector<CollisionTriangle>& triangles,
    std::vector<uint32_t>& solidIds,
    uint32_t maxOutputTriangles,
    StaticUnionStats* outStats = nullptr,
    uint32_t maxSolids = 0);

bool removeFullyCoveredStaticTriangles(
    std::vector<CollisionTriangle>& triangles,
    std::vector<uint32_t>& solidIds,
    StaticUnionStats* outStats = nullptr);

bool isClosedSolidTriangleRange(
    const std::vector<CollisionTriangle>& triangles,
    size_t first,
    size_t last);

bool staticFaceRemovalSelfTest(std::string& report);
bool staticSolidUnionSelfTest(std::string& report);
