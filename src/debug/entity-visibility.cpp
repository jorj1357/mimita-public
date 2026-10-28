// 2026-10-28
/* purpose
* Implement the local-only entity visibility master and category switches.
* See docs/specs/entity-editor/entity-editor.md.
* Does NOT render, send packets, or write configuration.
*/
#include "debug/entity-visibility.h"

#include <array>
#include <cstddef>

namespace EntityVisibility {
namespace {

bool gMaster = false;
std::array<bool, static_cast<std::size_t>(Category::Count)> gCategories{{
    true, true, true, true
}};

std::size_t indexOf(Category category)
{
    const auto index = static_cast<std::size_t>(category);
    return index < gCategories.size() ? index : 0;
}

} // namespace

bool master() { return gMaster; }
void setMaster(bool enabled) { gMaster = enabled; }

bool category(Category category) { return gCategories[indexOf(category)]; }
void setCategory(Category category, bool enabled) { gCategories[indexOf(category)] = enabled; }

bool draw(Category cat) { return gMaster && category(cat); }

const char* categoryName(Category category)
{
    switch (category) {
        case Category::ActorCapsules: return "actor_capsules";
        case Category::WeaponHitboxes: return "weapon_hitboxes";
        case Category::NpcPositions: return "npc_positions";
        case Category::MapObjects: return "map_objects";
        default: return "unknown";
    }
}

const char* categoryCommand(Category category)
{
    switch (category) {
        case Category::ActorCapsules: return "entity_visibility_actor_capsules";
        case Category::WeaponHitboxes: return "entity_visibility_weapon_hitboxes";
        case Category::NpcPositions: return "entity_visibility_npc_positions";
        case Category::MapObjects: return "entity_visibility_map_objects";
        default: return "entity_visibility";
    }
}

void reset()
{
    gMaster = false;
    gCategories.fill(true);
}

} // namespace EntityVisibility
