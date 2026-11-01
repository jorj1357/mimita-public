#pragma once

#include <glm/glm.hpp>

#include "config/spatial-audio-config.h"

struct World;

struct AcousticEstimate {
    // 0 = fully open, 1 = fully blocked / tightly enclosed.
    float obstruction = 0.0f;
    float enclosure = 0.0f;
};

AcousticEstimate estimateWorldAcoustics(const World* world,
                                        const SpatialProfile& profile,
                                        const glm::vec3& source,
                                        const glm::vec3& listener);
