#pragma once

#include <glm/glm.hpp>
#include "npc/npc.h"

float clamp01(float v);
float difficulty01(float difficulty);
float random01(unsigned int& state);
glm::vec3 randomPlanarDirection(unsigned int& state);
