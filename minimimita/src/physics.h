#pragma once
#include "types.h"

bool capsuleTriangleCollision(glm::vec3 capA, glm::vec3 capB, float radius,
                              const Triangle& tri, Contact& contact);
void computeWishDir(InputState& input, const Camera& camera);
void updatePlayer(Player& player, const InputState& input,
                  const std::vector<Triangle>& triangles, float dt);
