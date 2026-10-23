# Juggernaut startup freeze and connection report

- Time: 2026-10-22T20:08:38Z
- Branch / HEAD: `20261011zt` / `cbfa4b0df4e56b6f217c45dbe986b49df599b50a`
- Result: `PASS` for issue-inbox documentation; no gameplay fix or crash cause established.
- Updated the existing Juggernaut startup issue in `docs/specs/open-gameplay-issues.md` with the new player report: approximately 27 seconds frozen during NPC loading, weak-connection packet age changing from about 3 to 30 seconds, then a reported `mimita.exe` crash; RAM about 6,600 MB and CPU about 7% are estimates without a matching build/journal/dump.
- Source review: `mpNextConnectionHealth` transitions based on age since the client last heard a packet; the displayed age is computed against `lastHeardServerMs`. A client frame-loop stall could delay processing/rendering and make the displayed age jump when it resumes, but this is a hypothesis, not a confirmed explanation of the report.
- Prior build-2845 journal has severe frame stalls and working-set growth but does not record this reported crash. No current matching run journal, process exit code, or Windows crash evidence was supplied.
- Added trace requirements to correlate packet receive and connection transitions, client frame-loop progress, server tick, process memory, exit code, and crash data. No source code, build, test, or live process was changed or run.
- Pre-existing edits in `config/analytics.json`, `src/engine/engine-tick.cpp`, and other changelog files were preserved.
