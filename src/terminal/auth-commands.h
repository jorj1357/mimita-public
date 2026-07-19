// 07 19 2026, 12 00
/* purpose
* Declare terminal account auth commands for the Mimita exe.
* Let players sign in and sign out without using GUI login screens.
* Keep account command registration owned by the terminal auth module.
* DOES NOT store passwords or implement backend password checks.
* DOES NOT render account GUI.
* DOES NOT define website database schema.
*/

#pragma once

void registerAuthCommands();
