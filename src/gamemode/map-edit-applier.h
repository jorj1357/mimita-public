// 2026-11-05
/* purpose
* Apply one host-accepted typed WorldEditOperation to MapConfigRegistry.
* The live session owns ordering/validation; this applier owns the map-data
* property mapping and marks the authored revision changed.
*/
#pragma once

#include <cstdint>
#include <string>

#include "worldproject/world-edit-session.h"

enum class MapEditPropertyId : uint16_t
{
    Existence = 0,
    Position = 1,
    Size = 2,
    Yaw = 3,
    Radius = 4,
    Enabled = 5,
    Visible = 6,
    Name = 7,
    Template = 8,
    Prompt = 9,
    InputMode = 10,
    HoldTicks = 11,
    Seed = 12,
    GeneratorVersion = 13,
    RuleSet = 14,
    RewardGold = 15,
    RewardXp = 16,
    Value = 17,
};

// Returns false with a stable reason when the target or property is unsupported.
bool applyWorldEditOperation(
    const MimitaWorldProject::WorldEditOperation& operation,
    std::string* reason = nullptr);

// Read the current authored value before a host accepts an edit so the first
// edit of a property is still undoable. `exists` is false for unsupported or
// missing values; callers may submit the operation without a prior value.
bool readWorldEditProperty(
    const MimitaWorldProject::WorldEditOperation& operation,
    MimitaWorldProject::WorldEditValue& out,
    bool& exists);
