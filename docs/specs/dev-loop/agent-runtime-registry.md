// 2026-11-20
/* purpose
* specify the central registry for isolated per-agent test runtimes
* link runtime -> issue -> build -> DLL -> journal -> proof
* keep N parallel tests coordinated without mixing builds or killing unrelated processes
* this file DOES NOT grant process-kill or deployment authority
* this file DOES NOT replace the root dev-loop run authority
*/

# Agent runtime registry

## Purpose

Agents need many isolated live tests at once:

```text
map-editor test   -> integration DLL generation 400
melee test        -> preview DLL generation 401
juggernaut test   -> integration DLL generation 400
```

Each runtime must be identifiable, pinned to one DLL, and linked to its issue,
build request, journals, and proof.

## Registry record

```text
runtime_id
agent
cohort
issue_id
build_request_id
generation
dll_path
dll_sha256
exe_path
exe_sha256
server_pid
client_pids
bind/port
room_code
run_id
server_journal
client_journals
crash_dir
state: starting | running | stalled | stopping | stopped | failed
started_utc
last_event_utc
last_health_utc
stop_reason
proof_ids
```

The registry is a view over the existing run manifest and runtime state, not a
second process owner.

## Process ownership

- The dev-loop or the explicit isolated runtime launcher owns the processes.
- The registry never kills a process it did not launch or explicitly adopt.
- Stop/restart validates checkout, run ID, PID, executable identity, and
  start time before acting.
- Unrelated or user-owned processes are never killed.
- A runtime whose identity cannot be verified is reported `NEEDS_USER_ACTION`.

## DLL identity

- Every runtime records the exact EXE and game DLL paths and SHA-256 values.
- Server and client in one runtime must use the same EXE and DLL identity.
- A client on DLL A must never join a server on DLL B.
- A pinned runtime uses `MIMITA_GAME_DLL` and verifies:
  - generation
  - path
  - SHA-256
  - build request/issue when present
- Publishing a newer global generation does not change a running runtime.
- Reloading a runtime is an explicit action; it is not implicit on build publish.

## Health

Health checks are bounded and read-only:

- process alive with matching identity;
- journal exists and grows during active play;
- last event age;
- build/DLL identity still matches the run manifest;
- crash artifacts;
- required readiness events;
- fatal error events.

A missing process or stale journal is reported, not silently relaunched.
A run that needs relaunch preserves its prior journal and evidence.

## Concurrency and resources

- Cap concurrent runtimes by available memory, CPU, ports, and disk.
- Each runtime gets unique ports, room file, run directory, and journals.
- The scheduler may queue runtime starts.
- Builds and tests share the machine; reserve resources for active tests.
- Runtime count and resource pressure appear in dev-loop status and the board.

## Warm runtimes

Warm runtimes may be kept for fast proof:

- prelaunch server/client;
- pinned to a known DLL;
- reset to a defined state before a proof run;
- record the reset action and starting tick;
- never treat a dirty warm session as proof.

A warm runtime must be discarded or reset when the pinned DLL changes.

## Linking evidence

When a runtime produces proof:

```text
runtime_id
issue_id
build_generation
dll_sha256
run_id
server_journal
client_journal
matched events
scenario.finished
captured_by
captured_utc
```

The proof verifier rejects evidence when:

- runtime/build identity does not match;
- journal paths do not belong to the run;
- required events are missing;
- the scenario failed;
- evidence predates the build or issue change.

## Commands

Suggested:

```text
agent-coord runtime list
agent-coord runtime show <runtime-id>
agent-coord runtime start --agent <id> --issue <id> --game-generation N
agent-coord runtime stop <runtime-id>
agent-coord runtime health
```

The commands call the existing dev-loop/runtime launcher; they do not bypass
process ownership or start a second loop.

## Idle cleanup

- Runtimes record an idle TTL.
- Expired runtimes stop only after identity verification.
- Journals and crash artifacts are preserved.
- Stop reason and final state are recorded.
- A runtime referenced by an open proof or explicit pin is not auto-stopped.

## Acceptance

- Two runtimes can use different DLLs with separate ports and journals.
- Server/client in one runtime always share one DLL identity.
- A runtime pinned to generation N remains on N after N+1 publishes.
- Runtime health is visible without polling loops.
- Stopping validates ownership and preserves evidence.
- Proof verifier rejects wrong-build or wrong-runtime journals.
- Idle cleanup never kills unrelated processes.
- Resource caps prevent test starvation.

## Lease v2: prove why it is alive

Every runtime carries a lease that names its purpose and its release event.
The full contract is
[`process-lifecycle-and-release-events.md`](process-lifecycle-and-release-events.md).

```text
lease_id, pid, role, run_id
owner_agent, owner_session
purpose, issue_id, proof_target
keep_until: issue.proven | task.ended | owner.session.ended | run.superseded
window_policy: hidden | taskbar | visible
audio_policy: muted | on
journal, executable, build_id, started_utc
```

Rules:

- A process without a lease is a defect.
- `keep_until` is an event expression, never a duration.
- When the release event fires, the owner stops the run and verifies exit.
- If the owner session ended, `process-guard` may stop the run after identity
  verification.
- A stale journal or old start time raises `NEEDS_REVIEW`; it does not by
  itself authorize a kill.
- Agent runs default to `taskbar` window policy and muted audio.
- A hidden or taskbar run MUST NOT initialize an audio device unless its lease
  says `audio_policy: on`.

## Open review questions

- Should warm runtimes be the default, or explicitly requested?
- Should a runtime be allowed to auto-relaunch after a crash, or always preserve
  and require an explicit retry?
- What is the maximum concurrent runtime count on the reference laptop?
- Which proof targets are strong enough to fire `issue.proven` automatically?
