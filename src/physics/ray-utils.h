#pragma once
#include <glm/glm.hpp>
#include "physics/physics-types.h"

struct World;

bool rayTriangle(glm::vec3 origin, glm::vec3 direction,
                 const CollisionTriangle& tri, float& distance);

glm::vec3 castWorldRay(const World& world, glm::vec3 origin, glm::vec3 direction);

int selectWorldTriangle(const World& world, glm::vec3 origin, glm::vec3 direction);
