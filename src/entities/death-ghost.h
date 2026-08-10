// 08 10 2026, 14 35
/* purpose
* Declares DeathGhostSystem: spawns a standalone fall-over death animation clone.
* Each ghost is a full copy of the dying player body rendered at the death spot,
* so the real player body is never pinned, frozen, or hidden by the death anim.
* Does NOT own weapon authority, network packets, or server validation.
* Does NOT simulate physics; ghosts only play the scripted fall-over and vanish.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "entities/player.h"

class Camera;

class DeathGhostSystem
{
public:
    static DeathGhostSystem& instance();

    void spawnFromPlayer(const Player& victim,
                         const glm::vec3& direction,
                         const std::string& actorId,
                         uint32_t ownerId = 0);
    void update(float dt);
    void render(const Camera& camera) const;
    void removeForOwner(uint32_t ownerId);
    void clear();

    std::size_t activeCount() const { return mGhosts.size(); }

private:
    DeathGhostSystem() = default;

    struct DeathGhost
    {
        Player body;
        uint32_t ownerId = 0;
    };

    std::vector<DeathGhost> mGhosts;
};
