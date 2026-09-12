// 09 12 2026
/* purpose
* Declares the live-project terminal commands for humans.
* Registers 'project ...' status/history/record/undo/redo/checkpoint/restore
* plus 'project serve|stop' for the agent control socket.
* Does NOT own project logic; it forwards to ProjectControl.
*/
#pragma once

void registerProjectCommands();
