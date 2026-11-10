// 2026-11-08
#include "map/map-glb-authored.h"

#include <utility>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"

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
    if (found != objects.end()) {
        // The state is keyed by mesh address + node:N. If a map replacement
        // slipped past the loader clear, a different base transform/pristine
        // vertex set proves this entry belongs to another map. Reset it instead
        // of relocating this map's geometry to the old map's coordinates.
        const bool staleBase =
            glm::length(found->second.basePosition - basePosition) > 0.001f;
        if (staleBase) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map.glb-authored-state.stale-id", id,
                "stale GLB authored state reset for a new map", 0,
                nlohmann::json{{"object", id},
                               {"old_base_position", {found->second.basePosition.x,
                                                      found->second.basePosition.y,
                                                      found->second.basePosition.z}},
                               {"new_base_position", {basePosition.x,
                                                      basePosition.y,
                                                      basePosition.z}}},
                __FILE__, __LINE__, __FUNCTION__);
            found->second = GLBObjectAuthoredV2{};
            found->second.baseTransform = baseTransform;
            found->second.basePosition = basePosition;
        }
        return found->second;
    }
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
