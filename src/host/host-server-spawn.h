// 2026-11-13
// Resident safe-spawn selection for the authoritative server.

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace MimitaNet {
struct HeadlessWorld;
}

namespace mimita::host {

struct SpawnSelection {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    int index = -1;
    bool validated = false;
};

bool pickServerSpawn(const MimitaNet::HeadlessWorld* world,
                     std::uint32_t playerId, bool overrideEnabled,
                     const glm::vec3& overridePosition, SpawnSelection* out);

}  // namespace mimita::host
