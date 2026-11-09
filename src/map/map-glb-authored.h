// 2026-11-08
/* purpose
* narrow sidecar for authored GLB v2 state and pristine per-object bases
* keeps map_common.h stable so GLB edits do not rebuild 276 translation units
*/
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

struct GLBObjectAuthoredV2
{
    glm::mat4 baseTransform{1.0f};
    glm::vec3 basePosition{0.0f};
    std::vector<glm::vec3> basePositions;
    std::vector<glm::vec3> baseNormals;

    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    std::string material;
    bool visible = true;
    bool collision = true;
    bool deleted = false;
    bool clone = false;
    bool authoredHidden = false;
    bool authoredCollisionHidden = false;
};

class MapGLBAuthoredState
{
public:
    static MapGLBAuthoredState& instance();

    GLBObjectAuthoredV2& ensure(const void* owner, const std::string& id,
                                const glm::mat4& baseTransform,
                                const glm::vec3& basePosition);
    const GLBObjectAuthoredV2* find(const void* owner,
                                    const std::string& id) const;
    void clear(const void* owner);

private:
    MapGLBAuthoredState() = default;
    std::unordered_map<const void*,
        std::unordered_map<std::string, GLBObjectAuthoredV2>> mState;
};
