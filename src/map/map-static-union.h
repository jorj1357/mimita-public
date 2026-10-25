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
    double elapsedMs = 0.0;
    bool fallback = false;
    std::string fallbackReason;
};

bool removeFullyCoveredStaticTriangles(
    std::vector<CollisionTriangle>& triangles,
    std::vector<uint32_t>& solidIds,
    StaticUnionStats* outStats = nullptr);

bool isClosedSolidTriangleRange(
    const std::vector<CollisionTriangle>& triangles,
    size_t first,
    size_t last);

bool staticFaceRemovalSelfTest(std::string& report);
