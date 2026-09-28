// 09 28 2026, 00 00
/* purpose
* Register terminal commands for the procedural-world (Infinite Dungeon Slayer)
* feature.
* Commands forward intent to the authoritative server via the existing
* PACKET_SERVER_COMMAND path; the terminal never mutates server state directly.
* Does NOT own room generation, NPC spawning, physics, or rendering.
*/

#pragma once

void registerProceduralWorldCommands();
