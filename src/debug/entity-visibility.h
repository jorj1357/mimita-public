// 2026-10-28
/* purpose
* Own the local viewer's entity-visibility master switch and category toggles.
* Presentation only: this state never changes gameplay, collision, or packets.
* Defaults are master off with every category available when the master is on.
* Does NOT persist across restarts and does NOT own map authoring data.
*/
#pragma once

namespace EntityVisibility {

enum class Category
{
    ActorCapsules = 0,
    WeaponHitboxes,
    NpcPositions,
    MapObjects,
    Count
};

bool master();
void setMaster(bool enabled);
bool category(Category category);
void setCategory(Category category, bool enabled);
bool draw(Category category);
const char* categoryName(Category category);
const char* categoryCommand(Category category);
void reset();

} // namespace EntityVisibility
