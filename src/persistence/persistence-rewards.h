// 09 01 2026, 00 00
/* purpose
* Define configurable XP and gold reward amounts for gameplay events.
* Single source of truth for reward values on the server side.
* Does NOT implement persistence transport or database logic.
* Does NOT define level calculation.
*/

#pragma once

struct PersistenceRewards
{
    int playerKillXp = 100;
    int playerKillGold = 50;
    int npcKillXp = 10;
    int npcKillGold = 0;
};

const PersistenceRewards& getDefaultRewards();
