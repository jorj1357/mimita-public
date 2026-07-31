// 07 31 2026, 15 30
/* purpose
* Declares registration for the badconn terminal commands.
* Registers 'badconn list', 'badconn <N>', 'badconn 0', and 'badconn reload'.
* Does NOT own packet hooks, config parsing, or the simulator state.
*/

#pragma once

void registerBadConnCommands();
