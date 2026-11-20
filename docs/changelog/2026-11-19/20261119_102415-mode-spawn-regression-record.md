// 2026-11-19T15:24:15Z
/* purpose
* record the mode-spawn regression cause, correction, and prevention steps
* this file DOES NOT claim runtime validation or human acceptance
*/

# Mode Spawn Regression Record

# Task

- Task ID: session-20261119-mode-spawn-regression-record
- Session ID: session-20261119-mode-spawn-regression-record
- Owner: Codex
- Focus: append-only hot-reload regression documentation
- Summary: Added a regression record explaining the generic join-spawn bypass, the source correction, and the live checks required to prevent recurrence.
- Status: documentation complete; underlying source build/runtime validation pending
- Date, time, timezone: 2026-11-19T15:24:15Z; display time 2026-11-19 10:24:15 EST (America/New_York)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: not committed by this session

# Documentation changes

- Added `docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md` with the reported symptom, source-level cause, correction, limitations, and a five-step prevention/acceptance checklist.
- Added it as entry 2 in `docs/regressions/hotreload/README.md`.
- Kept its status open because the existing source correction has not yet been built or exercised after a same-PID DLL activation.

# Evidence and scope

- Cause evidence: both new-player handlers called `pickHostSpawn`; the active mode resolver was separate and was not called on that path. The code correction is described in the linked source-fix changelog.
- The user's exact map/mode and a live journal were not available, so the reason the observed actors physically overlapped is recorded as unconfirmed.
- This session changed documentation only. No build, test, runtime, or human visual review was performed.
- No unrelated source, configuration, or regression record was changed.

