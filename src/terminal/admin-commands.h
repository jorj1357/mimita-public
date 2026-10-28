// 2026-10-28
/* purpose
* Register terminal commands for server-session admin access.
* Commands are requests; the server owns validation and session state.
* Does NOT apply host commands locally or bypass server authority.
*/
#pragma once

void registerAdminCommands();
