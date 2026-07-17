#include "combat/pellet-pattern.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

int generatePelletDirections(
    const glm::vec3& baseDirection,
    const PelletPatternConfig& config,
    glm::vec3* outDirections,
    int outputCapacity)
{
    int count = std::min({config.pelletCount, outputCapacity, MAX_PELLETS_PER_BLAST});
    if (count <= 0 || config.spreadDegrees <= 0.0f)
    {
        if (count > 0) outDirections[0] = glm::normalize(baseDirection);
        return count > 0 ? 1 : 0;
    }

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(baseDirection, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDirection, up));
    glm::vec3 localUp = glm::normalize(glm::cross(right, baseDirection));

    float halfAngleRad = glm::radians(config.spreadDegrees * 0.5f);
    int cols = std::max(1, (int)std::ceil(std::sqrt((float)count)));
    int rows = std::max(1, (int)std::ceil((float)count / (float)cols));
    int idx = 0;
    for (int r = 0; r < rows && idx < count; ++r)
        for (int c = 0; c < cols && idx < count; ++c, ++idx)
        {
            float fx = cols > 1 ? (c / ((float)cols - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float fy = rows > 1 ? (r / ((float)rows - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float ha = halfAngleRad * fx;
            float va = halfAngleRad * fy;
            glm::quat rot = glm::angleAxis(ha, localUp) * glm::angleAxis(va, right);
            outDirections[idx] = glm::normalize(rot * baseDirection);
        }

    return count;
}
