#pragma once

#include <deque>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#include "replay-scene.h"

struct ReplayHeader;
struct ReplaySceneFrame;

nlohmann::json vec3Json(const glm::vec3& value);
nlohmann::json vec4Json(const glm::vec4& value);
glm::vec3 jsonVec3(const nlohmann::json& value, const glm::vec3& fallback = {});
ReplayEffectEvent parseEffect(const nlohmann::json& value);
ReplayActorState parseActor(const nlohmann::json& value);
nlohmann::json actorJson(const ReplayActorState& actor);
nlohmann::json materialJson(const ReplayMaterialReference& material);
nlohmann::json effectJson(const ReplayEffectEvent& effect);
nlohmann::json buildValidationJson(
    const std::string& replayPath,
    const ReplayHeader& header,
    const std::vector<ReplaySceneFrame>& sceneFrames);
