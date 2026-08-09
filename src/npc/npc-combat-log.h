// 08 09 2026, 12 45
/* purpose
* Always-on structured log for NPC combat behavior, written by BOTH the client
* and server processes so their numerical views can be compared.
* Writes to logs/<MM-DD-YYYY>/NPC_log_<HHMMSS>.txt (same dated-folder format as
* the main logger). Each line is tagged with the writing process (proc=client|server).
* Does NOT own any gameplay logic; it only appends diagnostic lines.
* Does NOT gate on debug flags - it logs indefinitely, one file per game run.
*/

#pragma once

#include <cstdarg>

// Set which process this instance is (server or client) at startup.
void npcLogSetProc(const char* proc);
const char* npcLogProc();

// Append a formatted line (newline added) to this process's NPC log.
void npcLog(const char* fmt, ...);
