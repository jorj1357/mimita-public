// 2026-11-06
/* purpose
* Implements the shared actor body state and its JSON-backed defaults.
* Does NOT own network state, damage requests, or render presentation.
*/
#include "entities/actor-body.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

namespace
{
constexpr const char* kActorBodyConfigPath = "config/actor-body.json";

bool partIndex(ActorBodyPart part, int& out)
{
    const int value = static_cast<int>(part);
    if (value < 0 || value >= kActorBodyPartCount)
        return false;
    out = value;
    return true;
}
}

ActorBodyConfig& ActorBodyConfig::instance()
{
    static ActorBodyConfig config;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        std::ifstream file(kActorBodyConfigPath);
        if (file.is_open())
        {
            try
            {
                nlohmann::json root;
                file >> root;
                if (root.contains("part_max_health") && root["part_max_health"].is_number())
                    config.partMaxHealth = std::clamp(root["part_max_health"].get<int>(), 1, 1000);
                auto readFloat = [&root](const char* key, float& out, float low, float high) {
                    if (root.contains(key) && root[key].is_number())
                        out = std::clamp(root[key].get<float>(), low, high);
                };
                readFloat("meat_threshold", config.meatThreshold, 0.01f, 0.99f);
                readFloat("bone_threshold", config.boneThreshold, 0.01f, 0.99f);
                readFloat("meat_shell_scale", config.meatShellScale, 0.1f, 1.0f);
                readFloat("bone_shell_scale", config.boneShellScale, 0.1f, 1.0f);
                readFloat("destroyed_blood_scale", config.destroyedBloodScale, 0.1f, 8.0f);
            }
            catch (...)
            {
                // Keep defaults on an invalid file; runtime behavior must not
                // depend on a partially parsed body config.
            }
        }
    }
    return config;
}

void ActorBodyState::reset()
{
    partHealth.fill(static_cast<uint8_t>(
        std::clamp(ActorBodyConfig::instance().partMaxHealth, 0, 255)));
}

bool ActorBodyState::applyDamage(ActorBodyPart part, int damage)
{
    int index = 0;
    if (!partIndex(part, index) || damage <= 0)
        return false;
    uint8_t& current = partHealth[(size_t)index];
    if (current == 0)
        return false;
    const int next = std::max(0, (int)current - damage);
    current = static_cast<uint8_t>(std::min(255, next));
    return current == 0;
}

bool ActorBodyState::destroyed(ActorBodyPart part) const
{
    int index = 0;
    return partIndex(part, index) && partHealth[(size_t)index] == 0;
}

bool ActorBodyState::lethalPartDestroyed() const
{
    return destroyed(ActorBodyPart::Head) || destroyed(ActorBodyPart::Torso);
}

uint8_t ActorBodyState::health(ActorBodyPart part) const
{
    int index = 0;
    return partIndex(part, index) ? partHealth[(size_t)index] : 0;
}

ActorBodyLayer ActorBodyState::layer(ActorBodyPart part) const
{
    int index = 0;
    if (!partIndex(part, index))
        return ActorBodyLayer::Skin;
    const float maxHealth = (float)std::max(1, ActorBodyConfig::instance().partMaxHealth);
    const float healthValue = (float)partHealth[(size_t)index];
    if (healthValue <= 0.0f)
        return ActorBodyLayer::Destroyed;
    const ActorBodyConfig& config = ActorBodyConfig::instance();
    if (healthValue > maxHealth * config.meatThreshold)
        return ActorBodyLayer::Skin;
    if (healthValue > maxHealth * config.boneThreshold)
        return ActorBodyLayer::Meat;
    return ActorBodyLayer::Bone;
}

const char* actorBodyPartName(ActorBodyPart part)
{
    switch (part)
    {
    case ActorBodyPart::Head: return "head";
    case ActorBodyPart::Torso: return "torso";
    case ActorBodyPart::LeftArm: return "leftArm";
    case ActorBodyPart::RightArm: return "rightArm";
    case ActorBodyPart::LeftLeg: return "leftLeg";
    case ActorBodyPart::RightLeg: return "rightLeg";
    default: return "";
    }
}

ActorBodyPart actorBodyPartFromName(const std::string& name)
{
    if (name == "head") return ActorBodyPart::Head;
    if (name == "torso") return ActorBodyPart::Torso;
    if (name == "leftArm") return ActorBodyPart::LeftArm;
    if (name == "rightArm") return ActorBodyPart::RightArm;
    if (name == "leftLeg") return ActorBodyPart::LeftLeg;
    if (name == "rightLeg") return ActorBodyPart::RightLeg;
    return ActorBodyPart::Unknown;
}

ActorBodyPart actorBodyPartFromRagdollIndex(int index)
{
    switch (index)
    {
    case 0: return ActorBodyPart::Torso;
    case 1: return ActorBodyPart::Head;
    case 2: return ActorBodyPart::LeftArm;
    case 3: return ActorBodyPart::RightArm;
    case 4: return ActorBodyPart::LeftLeg;
    case 5: return ActorBodyPart::RightLeg;
    default: return ActorBodyPart::Unknown;
    }
}

ActorBodyPart actorBodyPartFromHitBodyPart(uint8_t hitBodyPart)
{
    // WeaponExecution::HitBodyPart: 0 torso, 1 head, 2 leg.
    if (hitBodyPart == 1) return ActorBodyPart::Head;
    if (hitBodyPart == 2) return ActorBodyPart::RightLeg;
    return ActorBodyPart::Torso;
}

ActorBodyPart actorBodyPartFromWireClaim(uint8_t wirePart)
{
    // ClaimedHitPacket bodyPart: 1 head, 2 torso, 3 leg.
    if (wirePart == 1) return ActorBodyPart::Head;
    if (wirePart == 3) return ActorBodyPart::RightLeg;
    return ActorBodyPart::Torso;
}
