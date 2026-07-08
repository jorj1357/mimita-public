#include "spawn-override.h"
#include "debug/debug-log.h"
#include <cstdio>

static SpawnOverride gSpawnOverride;

SpawnOverride& getSpawnOverride()
{
    return gSpawnOverride;
}

bool tryGetSpawnOverride(glm::vec3& outPos)
{
    if (gSpawnOverride.enabled) {
        outPos = gSpawnOverride.position;
        return true;
    }
    return false;
}

void setSpawnOverridePosition(const glm::vec3& pos)
{
    gSpawnOverride.position = pos;
    Debug::log(Debug::Category::General, "[SPAWN OVERRIDE] position set to (%.1f %.1f %.1f)\n",
               pos.x, pos.y, pos.z);
}

void setSpawnOverrideEnabled(bool enabled)
{
    gSpawnOverride.enabled = enabled;
    if (enabled)
        Debug::log(Debug::Category::General, "[SPAWN OVERRIDE] enabled=1 position=(%.1f %.1f %.1f)\n",
                   gSpawnOverride.position.x, gSpawnOverride.position.y, gSpawnOverride.position.z);
    else
        Debug::log(Debug::Category::General, "[SPAWN OVERRIDE] disabled\n");
}
