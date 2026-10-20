#pragma once

#include <vector>
#include <glm/glm.hpp>

class Camera;

// Minimal client-side rain presentation for Zombie Tower. The drops are local
// presentation only; they never affect collision, movement, or gameplay.
class RainSystem
{
public:
    static RainSystem& instance();

    void setEnabled(bool enabled);
    bool enabled() const { return mEnabled; }
    void updateAndRender(const Camera& camera, float dt);

private:
    RainSystem() = default;

    struct Drop {
        glm::vec3 position{0.0f};
        float speed = 20.0f;
    };

    bool mEnabled = false;
    bool mSeeded = false;
    std::vector<Drop> mDrops;
};

