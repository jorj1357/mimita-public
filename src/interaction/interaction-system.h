// 2026-10-27
/* purpose
* One general world-interaction owner: register objects that can be used with
* F, select the best target, activate it, and draw a large camera-facing prompt
* anchored to the object's origin.
* Seats, doors, pickups, and future machines register the same target shape.
* Does NOT own vehicle, door, or pickup behavior; actions are callbacks.
*/
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct GLFWwindow;
class Camera;

struct InteractionContext {
    const Camera* camera = nullptr;
};

using InteractionAction = std::function<bool(const InteractionContext&)>;

struct InteractionTarget {
    uint32_t id = 0;
    std::string owner;
    std::string label;
    // World point where the billboard is drawn (usually the object origin plus
    // a readable offset). The action is range-checked around interactOrigin.
    glm::vec3 promptOrigin{0.0f};
    glm::vec3 interactOrigin{0.0f};
    float radius = 1.0f;
    int priority = 0;
    bool enabled = true;
    InteractionAction activate;
};

class InteractionSystem {
public:
    static InteractionSystem& instance();

    uint32_t add(InteractionTarget target);
    void remove(uint32_t id);
    void clearOwner(const std::string& owner);
    void clear();

    // Called by the owner each fixed tick to keep the prompt anchored while the
    // object moves.
    void updateTarget(uint32_t id, const glm::vec3& promptOrigin,
                      const glm::vec3& interactOrigin,
                      const std::string& label, float radius, bool enabled);

    // While occupied (seated), keep the occupant's target selected regardless
    // of the moving camera/seat distance. 0 clears the sticky target.
    void setStickyTarget(uint32_t id) { mStickyTarget = id; }

    // F edge. A valid target consumes F and wins over revive.
    bool handleKey(GLFWwindow* window, const Camera& camera);
    // Activate the currently selected target without a hardware key. Used by
    // terminal/scenario validation so the same action is tested.
    bool activateSelected(const Camera& camera);
    void draw(const Camera& camera);

    const InteractionTarget* selected(const Camera& camera) const;
    const std::vector<InteractionTarget>& targets() const { return mTargets; }
    size_t count() const { return mTargets.size(); }

private:
    InteractionSystem() = default;

    std::vector<InteractionTarget> mTargets;
    uint32_t mNextId = 1;
    bool mPreviousKeyHeld = false;
    uint32_t mLastPromptTarget = 0;
    uint32_t mStickyTarget = 0;
    int mMissingFrames = 0;
};
