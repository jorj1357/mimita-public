// 2026-11-05
/* purpose
* Define reusable per-mode world-generation profiles and chunk-library
* references. The map/project owns zone placement overrides; the mode owns the
* default grid, chunk set, spacing, and branching policy.
*/
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MimitaWorldGen {

struct WorldGenGrid
{
    std::string plane = "xy";
    glm::ivec3 repeatAxes{1, 1, 0};
    glm::vec3 origin{0.0f};
    glm::vec3 cellSize{10.0f, 10.0f, 10.0f};
    glm::vec3 chunkSize{10.0f, 10.0f, 10.0f};
    glm::ivec3 streamRadius{1, 1, 0};
    std::string spacingMode = "center"; // center, connector
    float connectorGap = 0.0f;
};

struct WorldGenMode
{
    std::string id;
    WorldGenGrid grid;
    std::vector<std::string> chunkFiles;
    int maxBranchDepth = 8;
    bool allowCycles = true;
    bool allowCrossings = true;
};

} // namespace MimitaWorldGen
