// 09 12 2026
/* purpose
* Declares registration for the live-code terminal commands.
* Registers 'hotreload status' and 'hotreload rollback'.
* Does NOT own the pipeline, build worker, or activation state machine.
*/
#pragma once

void registerLiveCodeCommands();
