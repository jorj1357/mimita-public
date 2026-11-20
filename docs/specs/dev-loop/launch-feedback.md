// 2026-11-20
/* purpose
* define launch-stage timing, failure taxonomy, diagnostic reports, and the one-button launch contract
* make every failed launch investigatable by an AI without asking the human for logs
* this file DOES NOT grant process-kill, firewall, build, or deployment authority
* this file DOES NOT replace the dev-loop or scenario validation specs
*/

# Launch feedback and diagnosis

## Successful-launch contract

A launch is successful only when:

- the exact artifact and build identity are validated;
- the server and client use the same build identity;
- the server is ready/reused;
- the client is created;
- the client connects;
- the map loads;
- the mode starts;
- the first frame renders;
- required runtime events are present.

A live process alone is not a successful launch.

## Launch timeline

Persist one record per launch:

```text
launch_id, request_id, press_utc,
accepted_utc, artifact_selected_utc, artifact_validated_utc,
server_reused_or_started_utc, server_logger_ready_utc,
server_gameplay_ready_utc, room_code_ready_utc,
client_spawned_utc, client_logger_ready_utc, client_connected_utc,
map_loaded_utc, mode_started_utc, first_frame_utc,
runtime_ready_utc, complete_utc,
result, failure_code, failure_stage, failure_detail,
build_id, generation, dll_sha256, server_pid, client_pids,
server_journal, client_journals
```

Each stage records monotonic elapsed, UTC time, result, reason, and identity.

Report directory:

```text
.dev/launch-reports/<launch-id>/
  launch.json
  stages.jsonl
  report.md
  probes.json
  server-tail.jsonl
  client-tail.jsonl
  processes.json
  build-identity.json
```

## Failure taxonomy

Every failure has one code:

```text
no_artifact
artifact_invalid
artifact_stale
launch_config_invalid
firewall_blocked
server_spawn_failed
server_logger_timeout
server_gameplay_timeout
room_code_timeout
client_spawn_failed
client_logger_timeout
client_connect_timeout
map_load_timeout
mode_start_timeout
identity_mismatch
server_exit
client_exit
render_init_failed
first_frame_timeout
crash
stale_lock
resource_exhausted
timeout
unknown
launch_exception
```

Reports include the first failed stage, exact reason, and next safe probes.
An uncaught exception during launch is finalized as `launch_exception`, with
its exception type/message, last completed stage, terminal failed report, and
the normal automatic issue link. It must never leave that attempt looking
`running` after control has returned with failure.

## One-button launch

When a build is pending, launch the last known good artifact immediately and
show that newer source/build is pending. Do not silently wait for a build.

If no valid artifact exists, show that result and start the explicit build path.
Build, launch, and test stages remain separate.

## Launch doctor

A rotating launch-doctor role may:

- inspect the report and journals;
- run safe read-only probes;
- retry a known-transient launch once;
- relaunch dev-loop-owned processes when identity still matches;
- clear a stale dev-loop-owned lock;
- select the last known good artifact;
- request a rebuild through the dev-loop;
- create/append one issue per failure signature.

It must not auto-approve firewall dialogs, kill unrelated processes, change
policy, lower readiness/proof gates, or hide failures.

## Feedback to agents

`dev-loop --launch-report <id>`, `--last-launch-report`, and
`--launch-doctor <id>` expose bounded JSON/text reports. `observe`/status/board
include launch result, failure stage/code, report path, build/queue/runtime
state, and next probe.

## Acceptance

- Every launch has a complete stage timeline.
- Every failure has a code and first failed stage.
- An AI can diagnose without human-provided logs.
- A pending build does not block the last stable launch.
- A failed launch creates/updates one issue per signature.
- Safe transient failures may retry once and record the attempt.
- No generic “failed” status is accepted when a stage/code is available.
- A successful launch includes connection, map, mode, first frame, and events.
