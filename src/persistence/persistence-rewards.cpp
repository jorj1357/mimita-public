// 09 01 2026, 00 00
/* purpose
* Provide the default XP and gold reward configuration.
* Centralizes reward constants so they are not scattered as magic numbers.
* Does NOT persist rewards or implement gameplay logic.
*/

#include "persistence/persistence-rewards.h"

const PersistenceRewards& getDefaultRewards()
{
    static const PersistenceRewards rewards;
    return rewards;
}
