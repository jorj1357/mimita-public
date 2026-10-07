# Restore October 6 logger profile

Date: 2026-10-07

Restored `config/debuglogger.json` to the exact version from the parent of
commit `709e0b08`, which was the October 6 logging profile used by the gold
JSONL debugging workflow.

Restored values include console output, General/Physics/Performance/Avatar/
Network Important levels, network category file output, Rendering off,
Grenade Launcher off, and Ragdoll Important. The file parses successfully and
matches `git show 709e0b08^:config/debuglogger.json` exactly.

No executable rebuild was required because this is a hot-reloadable logger
configuration change. A fresh client/server session is still required for new
`logger.started` records; the previous empty journal cannot be backfilled.
