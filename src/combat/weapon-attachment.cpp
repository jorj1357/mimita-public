#include "combat/weapon-attachment.h"
#include "combat/weapon-types.h"
#include "combat/weapon-config.h"
#include "combat/weapon-execution.h"
#include "entities/player.h"
#include "config/size-scaling-config.h"
#include "physics/movement/actor-collision-mesh.h"
#include <algorithm>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
struct ModelBounds { glm::vec3 min{0}, max{0}; bool valid = false; };
thread_local std::unordered_map<std::string, ModelBounds> boundsCache;
glm::mat4 rotateDegrees(glm::mat4 matrix, const glm::vec3& degrees) {
    matrix = glm::rotate(matrix, glm::radians(degrees.x), glm::vec3(1,0,0));
    matrix = glm::rotate(matrix, glm::radians(degrees.y), glm::vec3(0,1,0));
    return glm::rotate(matrix, glm::radians(degrees.z), glm::vec3(0,0,1));
}
}

bool resolveWeaponAttachment(const WeaponDefinition& def,
    const PhysicalBodyPart& arm, float actorScale, WeaponAttachmentPose& out,
    const glm::vec3& actionRotationDegrees, const glm::vec3& actionOffset)
{
    const auto* config = WeaponConfig::instance().get(def.id);
    const std::string& path = config && !config->modelPath.empty() ? config->modelPath : def.modelPath;
    auto it = boundsCache.find(path);
    if (it == boundsCache.end()) {
        ModelBounds bounds;
        std::vector<CollisionTriangle> triangles;
        if (loadActorWeaponTriangles(path.c_str(), triangles) && !triangles.empty()) {
            bounds.min = bounds.max = triangles.front().a;
            for (const auto& tri : triangles) for (const auto& point : {tri.a, tri.b, tri.c}) {
                bounds.min = glm::min(bounds.min, point);
                bounds.max = glm::max(bounds.max, point);
            }
            bounds.valid = true;
        }
        it = boundsCache.emplace(path, bounds).first;
    }
    if (!it->second.valid) return false;
    const auto& bounds = it->second;
    const glm::vec3 size = bounds.max - bounds.min;
    int axis = size.y > size.x ? 1 : 0;
    if (size.z > size[axis]) axis = 2;
    glm::vec3 base = (bounds.min + bounds.max) * 0.5f, tip = base;
    base[axis] = bounds.min[axis];
    tip[axis] = bounds.max[axis];
    if (glm::length(tip - base) < 0.001f) return false;

    const glm::vec3 armSize = arm.collider.localMax - arm.collider.localMin;
    int handAxis = armSize.y > armSize.x ? 1 : 0;
    if (armSize.z > armSize[handAxis]) handAxis = 2;
    glm::vec3 hand = (arm.collider.localMin + arm.collider.localMax) * 0.5f;
    hand[handAxis] = std::fabs(arm.collider.localMax[handAxis]) >= std::fabs(arm.collider.localMin[handAxis])
        ? arm.collider.localMax[handAxis] : arm.collider.localMin[handAxis];
    glm::vec3 handDirection(0);
    handDirection[handAxis] = hand[handAxis] >= 0 ? 1.0f : -1.0f;
    out.localToArm = glm::translate(glm::mat4(1), hand) *
        glm::mat4_cast(glm::rotation(glm::normalize(tip - base), handDirection)) *
        rotateDegrees(glm::mat4(1), def.attachmentRotation) *
        glm::translate(glm::mat4(1), def.attachmentOffset) *
        rotateDegrees(glm::mat4(1), actionRotationDegrees) *
        glm::translate(glm::mat4(1), actionOffset) *
        glm::translate(glm::mat4(1), -base);
    if (config && config->enabled) {
        const auto& scaling = SizeScalingConfig::instance().data();
        out.localToArm *= glm::scale(rotateDegrees(
            glm::translate(glm::mat4(1), config->positionOffset), config->rotationDegrees),
            config->scale * scaling.scale(1.0f, scaling.weaponSizeExponent, std::max(actorScale, 0.001f)));
    }
    out.world = arm.worldTransform * out.localToArm;
    out.bladeBase = glm::vec3(out.world * glm::vec4(base, 1));
    out.bladeTip = glm::vec3(out.world * glm::vec4(tip, 1));
    const float scale = std::max({glm::length(glm::vec3(out.world[0])),
        glm::length(glm::vec3(out.world[1])), glm::length(glm::vec3(out.world[2]))});
    out.bladeRadius = std::max(0.001f, WeaponExecution::paramOr(def, "contactBladeRadius", 0.08f) * scale);
    return true;
}
