// 2026-11-05
#include "gamemode/map-edit-applier.h"

#include <algorithm>

#include "gamemode/map-config.h"

namespace {

using MimitaWorldProject::WorldEditOpKind;
using MimitaWorldProject::WorldEditOperation;
using MimitaWorldProject::WorldEditTargetKind;
using MimitaWorldProject::WorldEditValue;
using MimitaWorldProject::WorldEditValueType;

bool fail(std::string* reason, const std::string& message)
{
    if (reason) *reason = message;
    return false;
}

bool requireType(const WorldEditValue& value, WorldEditValueType type,
                 std::string* reason)
{
    if (value.type != type)
        return fail(reason, "property-value-type-mismatch");
    return true;
}

template <typename T>
T* findById(std::vector<T>& items, const std::string& id)
{
    for (T& item : items)
        if (item.id == id) return &item;
    return nullptr;
}

bool applyVec3(glm::vec3& out, const WorldEditValue& value, std::string* reason)
{
    if (!requireType(value, WorldEditValueType::Vec3, reason)) return false;
    out = glm::vec3(static_cast<float>(value.vec3Value.x),
                    static_cast<float>(value.vec3Value.y),
                    static_cast<float>(value.vec3Value.z));
    return true;
}

} // namespace

bool readWorldEditProperty(const WorldEditOperation& operation,
                           WorldEditValue& out, bool& exists)
{
    exists = false;
    MapConfigRegistry& registry = MapConfigRegistry::instance();
    const auto property = static_cast<MapEditPropertyId>(operation.propertyId);

    auto readVec = [&](const glm::vec3& value) {
        out = WorldEditValue::vector(value.x, value.y, value.z);
        exists = true;
        return true;
    };

    switch (operation.target) {
    case WorldEditTargetKind::Entity: {
        const MapEntity* entity = registry.findEntity(operation.objectId);
        if (!entity) return false;
        switch (property) {
        case MapEditPropertyId::Position: return readVec(entity->position);
        case MapEditPropertyId::Size: return readVec(entity->size);
        case MapEditPropertyId::Yaw: out = WorldEditValue::number(entity->yaw); break;
        case MapEditPropertyId::Radius: out = WorldEditValue::number(entity->radius); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(entity->enabled); break;
        case MapEditPropertyId::Visible: out = WorldEditValue::boolean(entity->visible); break;
        case MapEditPropertyId::Name: out = WorldEditValue::text(entity->presetId); break;
        case MapEditPropertyId::RewardGold: out = WorldEditValue::integer(entity->rewardGold); break;
        case MapEditPropertyId::RewardXp: out = WorldEditValue::integer(entity->rewardXp); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::Block: {
        const AuthoredBlock* block = registry.findBlock(operation.objectId);
        if (!block) return false;
        switch (property) {
        case MapEditPropertyId::Position: return readVec(block->position);
        case MapEditPropertyId::Size: return readVec(block->size);
        case MapEditPropertyId::Yaw: out = WorldEditValue::number(block->rotationEuler.z); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(block->enabled); break;
        case MapEditPropertyId::Name: out = WorldEditValue::text(block->material); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::Vehicle: {
        const MapVehicleSpawn* vehicle = registry.findVehicle(operation.objectId);
        if (!vehicle) return false;
        switch (property) {
        case MapEditPropertyId::Position: return readVec(vehicle->position);
        case MapEditPropertyId::Yaw: out = WorldEditValue::number(vehicle->yaw); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(vehicle->enabled); break;
        case MapEditPropertyId::Template: out = WorldEditValue::text(vehicle->templateId); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::BombSite: {
        const BombSite* site = registry.findSite(operation.objectId);
        if (!site) return false;
        switch (property) {
        case MapEditPropertyId::Position: return readVec(site->position);
        case MapEditPropertyId::Radius: out = WorldEditValue::number(site->radius); break;
        case MapEditPropertyId::Visible: out = WorldEditValue::boolean(site->visibleDebug); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::NpcPreset: {
        const NpcPreset* preset = nullptr;
        for (const auto& item : registry.npcPresets())
            if (item.id == operation.objectId) { preset = &item; break; }
        if (!preset) return false;
        switch (property) {
        case MapEditPropertyId::Name: out = WorldEditValue::text(preset->roleId); break;
        case MapEditPropertyId::Template: out = WorldEditValue::text(preset->weaponSet); break;
        case MapEditPropertyId::Prompt: out = WorldEditValue::text(preset->movementProfile); break;
        case MapEditPropertyId::RuleSet: out = WorldEditValue::text(preset->appearance); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(preset->enabled); break;
        case MapEditPropertyId::RewardGold: out = WorldEditValue::integer(preset->rewardGold); break;
        case MapEditPropertyId::RewardXp: out = WorldEditValue::integer(preset->rewardXp); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::GenerationZone: {
        const GenerationZone* zone = nullptr;
        for (const auto& item : registry.generationZones())
            if (item.id == operation.objectId) { zone = &item; break; }
        if (!zone) return false;
        switch (property) {
        case MapEditPropertyId::Seed: out = WorldEditValue::integer(zone->seed); break;
        case MapEditPropertyId::GeneratorVersion: out = WorldEditValue::text(zone->generatorVersion); break;
        case MapEditPropertyId::RuleSet: out = WorldEditValue::text(zone->ruleSet); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(zone->enabled); break;
        default: return false;
        }
        break;
    }
    case WorldEditTargetKind::Interaction: {
        const AuthoredInteraction* interaction = nullptr;
        for (const auto& item : registry.interactions())
            if (item.id == operation.objectId) { interaction = &item; break; }
        if (!interaction) return false;
        switch (property) {
        case MapEditPropertyId::Name: out = WorldEditValue::text(interaction->actionId); break;
        case MapEditPropertyId::Prompt: out = WorldEditValue::text(interaction->prompt); break;
        case MapEditPropertyId::InputMode: out = WorldEditValue::text(interaction->inputMode); break;
        case MapEditPropertyId::HoldTicks: out = WorldEditValue::integer(interaction->holdTicks); break;
        case MapEditPropertyId::Radius: out = WorldEditValue::number(interaction->range); break;
        case MapEditPropertyId::Enabled: out = WorldEditValue::boolean(interaction->enabled); break;
        default: return false;
        }
        break;
    }
    default:
        return false;
    }
    exists = true;
    return true;
}

bool applyWorldEditOperation(const WorldEditOperation& operation,
                             std::string* reason)
{
    if (operation.kind != WorldEditOpKind::Set)
        return fail(reason, "operation-kind-unsupported");

    MapConfigRegistry& registry = MapConfigRegistry::instance();
    const auto& value = operation.value;

    switch (operation.target) {
    case WorldEditTargetKind::Entity: {
        MapEntity* entity = registry.findEntityMutable(operation.objectId);
        if (!entity) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Position: return applyVec3(entity->position, value, reason);
        case MapEditPropertyId::Size: return applyVec3(entity->size, value, reason);
        case MapEditPropertyId::Yaw:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            entity->yaw = static_cast<float>(value.floatValue);
            break;
        case MapEditPropertyId::Radius:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            entity->radius = std::max(0.1f, static_cast<float>(value.floatValue));
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            entity->enabled = value.boolValue;
            break;
        case MapEditPropertyId::Visible:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            entity->visible = value.boolValue;
            break;
        case MapEditPropertyId::Name: {
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            entity->presetId = value.stringValue;
            break;
        }
        case MapEditPropertyId::RewardGold:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            entity->rewardGold = std::max<int64_t>(0, value.intValue);
            break;
        case MapEditPropertyId::RewardXp:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            entity->rewardXp = std::max<int64_t>(0, value.intValue);
            break;
        default: return fail(reason, "entity-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::Block: {
        AuthoredBlock* block = registry.findBlockMutable(operation.objectId);
        if (!block) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Position: return applyVec3(block->position, value, reason);
        case MapEditPropertyId::Size: return applyVec3(block->size, value, reason);
        case MapEditPropertyId::Yaw:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            block->rotationEuler.z = static_cast<float>(value.floatValue);
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            block->enabled = value.boolValue;
            break;
        case MapEditPropertyId::Name:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            block->material = value.stringValue;
            break;
        default: return fail(reason, "block-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::Vehicle: {
        MapVehicleSpawn* vehicle = registry.findVehicleMutable(operation.objectId);
        if (!vehicle) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Position: return applyVec3(vehicle->position, value, reason);
        case MapEditPropertyId::Yaw:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            vehicle->yaw = static_cast<float>(value.floatValue);
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            vehicle->enabled = value.boolValue;
            break;
        case MapEditPropertyId::Template:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            vehicle->templateId = value.stringValue;
            break;
        default: return fail(reason, "vehicle-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::BombSite: {
        BombSite* site = nullptr;
        for (auto& candidate : registry.bombSitesMutable())
            if (candidate.id == operation.objectId) { site = &candidate; break; }
        if (!site) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Position:
            if (!applyVec3(site->position, value, reason)) return false;
            site->hasPosition = true;
            break;
        case MapEditPropertyId::Radius:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            site->radius = std::max(0.1f, static_cast<float>(value.floatValue));
            break;
        case MapEditPropertyId::Visible:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            site->visibleDebug = value.boolValue;
            break;
        default: return fail(reason, "bombsite-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::NpcPreset: {
        NpcPreset* preset = findById(registry.npcPresetsMutable(),
                                     operation.objectId);
        if (!preset) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Name:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            preset->roleId = value.stringValue;
            break;
        case MapEditPropertyId::Template:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            preset->weaponSet = value.stringValue;
            break;
        case MapEditPropertyId::Prompt:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            preset->movementProfile = value.stringValue;
            break;
        case MapEditPropertyId::RuleSet:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            preset->appearance = value.stringValue;
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            preset->enabled = value.boolValue;
            break;
        case MapEditPropertyId::RewardGold:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            preset->rewardGold = static_cast<int>(std::max<int64_t>(0, value.intValue));
            break;
        case MapEditPropertyId::RewardXp:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            preset->rewardXp = static_cast<int>(std::max<int64_t>(0, value.intValue));
            break;
        default: return fail(reason, "npc-preset-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::GenerationZone: {
        GenerationZone* zone = findById(registry.generationZonesMutable(),
                                        operation.objectId);
        if (!zone) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Seed:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            zone->seed = static_cast<uint32_t>(std::max<int64_t>(0, value.intValue));
            break;
        case MapEditPropertyId::GeneratorVersion:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            zone->generatorVersion = value.stringValue;
            break;
        case MapEditPropertyId::RuleSet:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            zone->ruleSet = value.stringValue;
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            zone->enabled = value.boolValue;
            break;
        default: return fail(reason, "generation-zone-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::Interaction: {
        AuthoredInteraction* interaction = findById(registry.interactionsMutable(),
                                                   operation.objectId);
        if (!interaction) return fail(reason, "target-missing");
        switch (static_cast<MapEditPropertyId>(operation.propertyId)) {
        case MapEditPropertyId::Name:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            interaction->actionId = value.stringValue;
            break;
        case MapEditPropertyId::Prompt:
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            interaction->prompt = value.stringValue;
            break;
        case MapEditPropertyId::InputMode: {
            if (!requireType(value, WorldEditValueType::String, reason)) return false;
            interaction->inputMode = value.stringValue == "hold" ? "hold" : "press";
            break;
        }
        case MapEditPropertyId::HoldTicks:
            if (!requireType(value, WorldEditValueType::Int, reason)) return false;
            interaction->holdTicks = static_cast<int>(std::max<int64_t>(0, value.intValue));
            break;
        case MapEditPropertyId::Radius:
            if (!requireType(value, WorldEditValueType::Float, reason)) return false;
            interaction->range = std::max(0.25f, static_cast<float>(value.floatValue));
            break;
        case MapEditPropertyId::Enabled:
            if (!requireType(value, WorldEditValueType::Bool, reason)) return false;
            interaction->enabled = value.boolValue;
            break;
        default: return fail(reason, "interaction-property-unsupported");
        }
        break;
    }
    case WorldEditTargetKind::World:
        return fail(reason, "world-property-unsupported");
    default:
        return fail(reason, "target-kind-unsupported");
    }

    registry.markChanged();
    return true;
}
