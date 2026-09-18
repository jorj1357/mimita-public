// 09 17 2026
/* purpose
* Always-on structured log for NPC combat behavior, emitted by BOTH the client
* and server processes so their numerical views can be compared.
* Each message becomes one NPC record in the authoritative events.jsonl stream;
* the writing process is tagged via LiveIdentity instead of separate files.
* Does NOT own any gameplay logic; it only emits diagnostic events.
* Does NOT gate on debug flags - it logs indefinitely.
*/

#pragma once

#include <cstdarg>

// Set which process this instance is (server or client) at startup.
void npcLogSetProc(const char* proc);
const char* npcLogProc();

// Append a formatted line (newline added) to this process's NPC log.
void npcLog(const char* fmt, ...);
