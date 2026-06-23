#pragma once
#include "types.h"

glm::vec3 closestPtPointTriangle(glm::vec3 p, const Triangle& tri);
glm::vec3 closestPtPointSegment(glm::vec3 p, glm::vec3 a, glm::vec3 b);
bool capsuleTriangleCollision(glm::vec3 capA, glm::vec3 capB, float radius,
                              const Triangle& tri, Contact& contact);
void collectContacts(const Player& player, const std::vector<Triangle>& triangles,
                     ContactState& state);
void resolveContacts(Player& player, const ContactState& state);
void updatePlayer(Player& player, const InputState& input,
                  const std::vector<Triangle>& triangles, float dt);
