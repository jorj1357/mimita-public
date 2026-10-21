#pragma once

// Shared CPU attachment used by rendering and authoritative blade contact.
// The animation owner supplies the final arm transform; this owner fits the
// equipped model to that arm and resolves its model-space blade endpoints.
#include <glm/glm.hpp>

struct WeaponDefinition;
struct PhysicalBodyPart;

struct WeaponAttachmentPose {
    glm::mat4 world{1.0f};
    glm::mat4 localToArm{1.0f};
    glm::vec3 bladeBase{0.0f};
    glm::vec3 bladeTip{0.0f};
    float bladeRadius = 0.0f;
};

bool resolveWeaponAttachment(const WeaponDefinition& definition,
    const PhysicalBodyPart& arm, float actorScale, WeaponAttachmentPose& out);
