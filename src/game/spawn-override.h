#pragma once
#include <glm/glm.hpp>

struct SpawnOverride {
    bool enabled = false;
    glm::vec3 position{0.0f};
};

SpawnOverride& getSpawnOverride();
bool tryGetSpawnOverride(glm::vec3& outPos);
void setSpawnOverridePosition(const glm::vec3& pos);
void setSpawnOverrideEnabled(bool enabled);
