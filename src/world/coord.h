// C:\important\quiet\n\mimita-public\mimita-public\src\world\coord.h
// dec 18 2025
/**
 * purpose
 * single source of truth for blender to code converting
 * no more defining toYUp everywhere
 */

 #pragma once
#include <glm/glm.hpp>

// Blender Z-up -> Engine Y-up
static inline glm::vec3 toYUp(const glm::vec3& p) {
    return { p.x, p.z, -p.y };
}

// sizes: swap Y/Z, no sign flip
static inline glm::vec3 sizeToYUp(const glm::vec3& s) {
    return { s.x, s.z, s.y };
}

// basis matrix for rotation conversion (Blender -> Engine)
static inline glm::mat3 basisToYUp() {
    // maps (x,y,z) -> (x,z,-y)
    return glm::mat3(
        1,  0,  0,
        0,  0, -1,
        0,  1,  0
    );
}
