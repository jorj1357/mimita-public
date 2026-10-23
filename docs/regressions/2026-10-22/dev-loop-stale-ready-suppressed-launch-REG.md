# Cached client-ready flags suppressed a new launch

Time created: 2026-10-22T21:49:48Z
Time last updated: 2026-10-22T21:49:48Z

Status: ATTEMPTED FIX (1)

## Occurrence 1 — 2026-10-22

- Expected behavior: a Juggernaut launch request starts a client when no client process is alive.
- Actual behavior: the loop reported a successful/reusable launch while `client_pids` was empty; the run ID and journal did not advance.
- Wrong code: the early reuse gate trusted cached `client_ready` and `builds_match` flags without checking whether any client child process was alive. Client stop also left those flags set.
- Corrected code: the reuse gate now requires a live client process; stop/process-maintenance paths clear readiness and build-match flags after the last client exits.
- Cause: readiness described the previous run, not the current process lifetime.
- Evidence: after reloading the loop, a fresh Juggernaut launch reported client PID 21608 and a new run/journal. Later release runs also had matching live client/server build identities.
- Remaining review: a human should confirm the visible launch in the game window. Keep this attempted status until then.

