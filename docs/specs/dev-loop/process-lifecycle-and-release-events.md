// 2026-11-27
/* purpose
* make every MiMITA process prove why it exists and name the event that releases it
* make agent-run windows discoverable in the taskbar and their audio policy explicit
* replace time-based cleanup with ownership, leases, and release events
* this file DOES NOT grant authority to stop another agent's or a human's process
* this file DOES NOT replace the dev-loop runtime authority
*/

# Process lifecycle, visibility, and release events

**Status:** Desired behavior contract.

## Why

A machine can accumulate MiMITA clients and servers that nobody is watching.
They consume CPU and memory, some of them make sound, and none of them show in
the taskbar. A user cannot tell what is being tested or why.

The finished system never has an unexplained process.

## The rule

Every MiMITA process MUST carry a lease that answers:

```text
who owns it
why it exists
what evidence it is producing
what event makes it safe to stop
where its journal is
```

A process without a lease is a defect. A process whose release event has fired
MUST be stopped and verified gone.

## Lease

```json
{
  "lease_id": "run-lease-...",
  "pid": 21136,
  "role": "client | server | build | worker",
  "run_id": "20261127_183456",
  "owner_agent": "anim-socket-crouch",
  "owner_session": "ses_...",
  "purpose": "verify crouch capsule on generation 801",
  "issue_id": "issue-2d4032ac8044",
  "proof_target": "crouch.capsule",
  "keep_until": "issue.proven | task.ended | owner.session.ended | run.superseded",
  "window_policy": "hidden | taskbar | visible",
  "audio_policy": "muted | on",
  "journal": "logs/.../client-01/events.jsonl",
  "executable": "...",
  "build_id": "...",
  "started_utc": "..."
}
```

`keep_until` is an event expression, never a duration. Time is a diagnostic
only.

## Release events

```text
run.stop_requested     owner or user asked to stop
issue.proven           the proof target was recorded
task.ended             the owning task reached a terminal state
owner.session.ended    the owning chat/session closed
run.superseded         a newer pin replaced this runtime
build.finished         a build worker's job is complete
build.failed           a build worker's job is complete
admission.revoked      the resource gate requires this run to stop
```

When a release event fires, the owning runtime stops its own processes and
verifies exit. If the owner is gone, `process-guard` performs the stop only
when checkout, run ID, PID, executable, and start time match.

No timer kills a run. A stale journal, a missing heartbeat, or an old start time
raises `NEEDS_REVIEW`; it does not by itself authorize a kill.

## Visibility

Every agent-started client MUST have a window policy:

| Policy | Window | Taskbar | Audio | Use |
|---|---|---|---|---|
| `hidden` | not shown | no | muted | pure headless batch work |
| `taskbar` | minimized | yes | muted by default | default for agent runs |
| `visible` | shown | yes | on | human watching or recording |

`taskbar` is the default for agent runs. It keeps the desktop usable while
making the process discoverable.

The window title MUST identify the run:

```text
MiMITA [<run_id>] [<mode>] [<owner or issue>]
```

The dev-loop dashboard MUST list every live window with:

- run ID, mode, map;
- owner agent and issue;
- build/generation and DLL SHA;
- journal path and last event age;
- window policy and audio policy;
- a Watch action that restores and focuses the window;
- a Stop action gated by the same ownership rules.

The taskbar and dashboard are projections of the registry. They are not a
second source of truth.

## Audio

- A hidden run MUST NOT initialize an audio device.
- A `taskbar` run MUST be muted unless the lease says `"audio_policy": "on"`.
- A `visible` run MAY play audio.
- Audio is per process. Four clients MUST NOT all play the same event.
- The lease records who enabled audio and why.
- A run that becomes visible does not automatically unmute; unmuting is
  explicit.

The `hidden` and muted defaults are the reason a user no longer hears an
unseen test.

## Process trees

- A build worker and all its children (`g++`, `cc1plus`, `ld.lld`) run in one
  Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.
- A test runtime's server and clients run in one Job Object per run.
- When the job closes, the tree cannot outlive it.
- `process-guard scan` flags any compiler child with no live build attempt and
  any MiMITA process with no live lease.

## Commands

```text
process-guard.py scan                 # report only
process-guard.py explain <pid>        # lease, parent, purpose, release event
process-guard.py stop --run <id>      # own or unleased, identity-checked
process-guard.py orphans              # verified orphans only
process-guard.py watch <run_id>       # restore the taskbar window
dev-loop.py --start-test-run ... --window taskbar|visible|hidden --audio on|muted
```

## Activation scan

Every agent activation runs `process-guard scan` and reviews:

- its own live leases and their release events;
- unleased MiMITA processes;
- compiler children with no build;
- stale runs whose owner session ended.

At task end the agent stops its own runs and verifies exit. Unrelated runs are
left alone and reported.

## Bun and OpenCode processes

OpenCode sessions spawn Bun workers. They are not MiMITA processes and MUST NOT
be killed blindly; killing them can terminate the agent itself.

`process-guard scan --opencode` reports parent, session, port, and age. An
OpenCode Bun process is removable only when its parent is gone and no live
session or port owns it. Reducing parallel sessions reduces the count; killing
live workers does not.

## Acceptance

- Every live MiMITA process has a lease with a purpose and a release event.
- An agent-run client appears in the taskbar or dashboard with its run, mode,
  and issue.
- A hidden or taskbar run produces no audio unless explicitly enabled.
- A finished build leaves no `cc1plus`, `g++`, or `ld.lld` child.
- Ending an owner session stops its runs, or flags them for review with the
  exact reason.
- No process is stopped on a timer alone.
- The user can identify what is running and why without asking an agent.
