// C:\important\go away v5\s\mimita-v5\src\map\map_common.h

#pragma once
// make this cross platform happy friendly
#include <glm/glm.hpp>
#include <vector>

#include "map/map-mesh-types.h"

// Pristine authored-GLB bases owned by the resident World. These survive hot
// generation swaps so overrides never have to be recovered by inverting an
// already-applied transform.
struct AuthoredGLBBase {
    glm::mat4 transform{1.0f};
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
};

// do NOT define struct chunk here
