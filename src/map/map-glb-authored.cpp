// 2026-11-08
#include "map/map-glb-authored.h"

#include <utility>

MapGLBAuthoredState& MapGLBAuthoredState::instance()
{
    static MapGLBAuthoredState state;
    return state;
}

GLBObjectAuthoredV2& MapGLBAuthoredState::ensure(
    const void* owner, const std::string& id,
    const glm::mat4& baseTransform, const glm::vec3& basePosition)
{
    auto& objects = mState[owner];
    auto found = objects.find(id);
    if (found != objects.end())
        return found->second;
    GLBObjectAuthoredV2 value;
    value.baseTransform = baseTransform;
    value.basePosition = basePosition;
    return objects.emplace(id, std::move(value)).first->second;
}

const GLBObjectAuthoredV2* MapGLBAuthoredState::find(
    const void* owner, const std::string& id) const
{
    auto ownerIt = mState.find(owner);
    if (ownerIt == mState.end())
        return nullptr;
    auto found = ownerIt->second.find(id);
    return found == ownerIt->second.end() ? nullptr : &found->second;
}

void MapGLBAuthoredState::clear(const void* owner)
{
    mState.erase(owner);
}
