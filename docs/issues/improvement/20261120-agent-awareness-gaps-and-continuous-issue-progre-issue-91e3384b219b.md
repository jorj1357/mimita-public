# issue-91e3384b219b: Agent awareness gaps and continuous issue progress

## Human report (verbatim, immutable)

> make it so ai looks for gaps in its awareness of what is going on in teh game or what i intend, and it marks those as issues and we try to fixthose over time


## Task linked

### 2026-11-20T05:43:03.472Z — runtime-60hz

Task: define-awareness-issues (step 0)

## Task linked

### 2026-11-20T05:43:05.372Z — runtime-60hz

Task: review-while-waiting (step 0)

## Task linked

### 2026-11-20T05:43:06.654Z — runtime-60hz

Task: stale-issue-heartbeat (step 0)

## Context

### 2026-11-20T06:25:33.589Z — work-metrics

reproduction: Compare user priorities, board state, issue context, and agent activity before and after a wait; inspect sync and board.
expected_behavior: Agents surface blocked work, claim actionable issues, and propose improvements while waiting without duplicate status chatter.
acceptance: Board shows awareness-gap proposals and issue links; no stale work without blocker/next step; repeated status reports are rate-limited.
paths: ['docs/specs/dev-loop/agent-coordination.md', 'docs/specs/dev-loop/agent-self-improvement.md', 'devscripts/agent_coord.py']
evidence: {'spec': 'docs/specs/dev-loop/agent-self-improvement.md', 'observed': 'agents idle during build waits without claiming work'}

## User quote

### 2026-11-20T06:31:02.385Z — juggernaut-npc-stuck-shooting

Via juggernaut-npc-stuck-shooting; supports issue-91e3384b219b:

> ensure AI can doit full autonomosuly and if not, then suggest gaps we need to fix liek where ai doesnt know about somethign that it would benefit from knowing about

## Agent suggestion

### 2026-11-20T06:31:02.730Z — juggernaut-npc-stuck-shooting

For this set of investigations, audit which completion steps the agent can perform from repository specs, coordination, dev-loop status, and per-run journals; list anything blocked by unavailable mode/map details, missing logs or runtime controls, capacity/permissions, or unobservable state. For each gap propose the smallest tool/context/automation addition and a proof that it enables autonomous diagnosis without falsely claiming runtime acceptance.

## Triage

### 2026-11-20T06:32:36.610Z — juggernaut-npc-stuck-shooting

Summary: Audit end-to-end AI autonomy on the current NPC, connection-crash, and capsule-collision investigations. Identify which repository, dev-loop, journal, runtime, context, or workflow capabilities are unavailable and propose the smallest changes with proof.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent suggestion

### 2026-11-20T06:45:16.702Z — juggernaut-npc-stuck-shooting

Confirmed autonomy gaps in this investigation: issue-82cdc1277704 references logs/2026-11-20/20261120_040807, but that run directory is absent; the only nearby GUI launch run is a different mode with no client; and current mode selection changes whether NPCs exist. Smallest helpful improvements: validate issue evidence paths on intake, associate the user's GUI action/request ID and selected mode/map with the task, and expose one bounded bundle of run.json plus each process journal for the exact run. Preserve isolated pinned runs and never replace another agent's active shared runtime.

## Agent suggestion

### 2026-11-20T06:51:23.116Z — juggernaut-npc-stuck-shooting

Autonomy review found useful context is split across mode JSON/spec, dev-loop launch reports, per-process journals, and issue records: base juggernaut disables NPCs; available 064201 journal is TDM with stuck/recovery but not Juggernaut; recent 063206 GUI request made a server but no client before readiness timeout and had duplicate --mode args, while 063641/064201 clients did connect. Ensure issue intake links dashboard request_id to run_id, exact gamemode/map, build/generation and process journals; agents should recognize unrelated runs as non-proof and report the precise missing run rather than infer a code cause.

## Agent suggestion

### 2026-11-20T06:53:40.803Z — juggernaut-npc-stuck-shooting

Direct review confirms an AI context/telemetry gap: .dev/launch-reports/*/launch.json currently preserves launch/request/build IDs and a summary failure but omits run_id, actual mode/map, process PIDs, per-process journal paths, and child exit/crash artifacts. The 06:38 report says client_connect_timeout for PID 22852, yet run.json and client journal are separate; the 06:51 launch says reconciliation failed because PID 22852 remained alive. Correlate dashboard request_id → dev-loop event run_id → run.json instances → exact journals/crash artifacts; launch-feedback.md already specifies most of these fields. AI can inspect each source manually, but cannot autonomously produce a reliable self-contained diagnosis from the launch report alone.
