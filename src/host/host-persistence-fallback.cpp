// Resident persistence defaults used by the host persistence worker.
//
// The game generation owns the strong definition. This weak copy keeps the
// strict cold EXE linkable while retaining the documented default rewards.

#include "persistence/persistence-rewards.h"

__attribute__((weak)) const PersistenceRewards& getDefaultRewards() {
    static const PersistenceRewards rewards;
    return rewards;
}
