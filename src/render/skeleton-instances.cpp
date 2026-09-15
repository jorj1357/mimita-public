// 09 15 2026
/* purpose
* Implements the cold generic per-entity skeleton instance driven by
* skeleton.apply. Does NOT own animation policy or draw submission.
*/
#include "render/skeleton-instances.h"

#include <cmath>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ecs/entity-registry.h"

namespace SkeletonInstances {
namespace {

std::unordered_map<EntityId, Instance> g_instances;

glm::mat4 trs(const float translation[3], const float rotationEuler[3])
{
    const glm::vec3 t(translation[0], translation[1], translation[2]);
    // XYZ euler (radians) -> quaternion.
    const glm::quat q =
        glm::quat(glm::vec3(rotationEuler[0], rotationEuler[1], rotationEuler[2]));
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q);
}

BonePose* findBoneMutable(Instance& inst, std::uint64_t part)
{
    for (std::uint32_t i = 0; i < inst.boneCount; ++i)
        if (inst.bones[i].part == part)
            return &inst.bones[i];
    if (inst.boneCount >= GAME_MAX_POSE_PARTS)
        return nullptr;  // pose parts exceeding the skeleton are dropped safely
    BonePose& b = inst.bones[inst.boneCount++];
    b.part = part;
    b.local = glm::mat4(1.0f);
    b.world = glm::mat4(1.0f);
    return &b;
}

} // namespace

Instance* ensure(EntityId entity)
{
    if (entity == kInvalidEntityId)
        return nullptr;
    Instance& inst = g_instances[entity];
    if (inst.entity == kInvalidEntityId) {
        inst.entity = entity;
        inst.boneCount = 0;
        inst.version = 0;
    }
    return &inst;
}

Instance* get(EntityId entity)
{
    auto it = g_instances.find(entity);
    if (it == g_instances.end())
        return nullptr;
    if (!EntityRegistry::instance().alive(entity))
        return nullptr;
    return &it->second;
}

bool applyPose(EntityId entity, const GameSkeletonPoseV1& pose)
{
    if (entity == kInvalidEntityId)
        return false;
    Instance* inst = ensure(entity);
    if (!inst)
        return false;
    const std::uint32_t n =
        pose.count < GAME_MAX_POSE_PARTS ? pose.count : GAME_MAX_POSE_PARTS;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (pose.parts[i].part == 0)
            continue;
        BonePose* bone = findBoneMutable(*inst, pose.parts[i].part);
        if (!bone)
            continue;  // too many parts; extra parts are skipped safely
        bone->translation[0] = pose.parts[i].translation[0];
        bone->translation[1] = pose.parts[i].translation[1];
        bone->translation[2] = pose.parts[i].translation[2];
        bone->rotationEuler[0] = pose.parts[i].rotationEuler[0];
        bone->rotationEuler[1] = pose.parts[i].rotationEuler[1];
        bone->rotationEuler[2] = pose.parts[i].rotationEuler[2];
        bone->local = trs(pose.parts[i].translation, pose.parts[i].rotationEuler);
        // Flat skeleton (all parts are direct children of the root), so world ==
        // local. A hierarchical mapping belongs to skeleton resource metadata.
        bone->world = bone->local;
    }
    ++inst->version;
    return true;
}

const BonePose* findBone(const Instance* instance, std::uint64_t part)
{
    if (!instance) return nullptr;
    for (std::uint32_t i = 0; i < instance->boneCount; ++i)
        if (instance->bones[i].part == part)
            return &instance->bones[i];
    return nullptr;
}

void purgeDead()
{
    for (auto it = g_instances.begin(); it != g_instances.end();) {
        if (!EntityRegistry::instance().alive(it->first))
            it = g_instances.erase(it);
        else
            ++it;
    }
}

void clear()
{
    g_instances.clear();
}

std::size_t count()
{
    return g_instances.size();
}

} // namespace SkeletonInstances
