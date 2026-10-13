// 2026-10-13T20:00:00Z
/* purpose
* define the exact behavior of the MiMITA development orchestration loop
* give humans and AI contributors one contract for build, publish, launch,
  process lifetime, diagnostics, and runtime evidence
* make script/specification mismatches explicit and testable
* this file DOES NOT define gameplay, physics, networking, or rendering behavior
* this file DOES NOT replace the canonical build, logging, terminal-command,
  runtime-validation, or task-completion documents
* this file DOES NOT authorize killing arbitrary user-owned game/server processes
*/

# MiMITA development loop

## 1. Purpose and outcome

The development loop reduces the time between a source/configuration change and
an observed result in the real MiMITA executable. It is an orchestration tool,
not a second game implementation.

The loop owns this sequence:

```text
observe source change
 -> debounce and queue a build
 -> invoke the canonical build system
 -> publish an immutable, numbered build artifact
 -> report build identity and result
 -> on explicit launch request, select the newest valid artifact
 -> reconcile only identifiable dev-loop processes
 -> launch the matching server/client pair
 -> capture version, process, journal, and readiness evidence
 -> expose status and controlled lifecycle actions
```

The desired developer experience is:

1. A source edit is detected without requiring manual build commands.
2. The current runtime is not interrupted merely because a file changed.
3. A failed build is visible and prevents launch of an unverified artifact.
4. A successful build receives a unique build number and identity.
5. An explicit launch request uses only that verified newest build.
6. A server and client never silently use different build identities.
7. Stale dev-loop servers cannot remain the authoritative server for a newer
   client.
8. The exact executable and shared `events.jsonl` path are known before runtime
   claims are made.
9. Headless operation hides windows and consoles but does not pretend to solve
   Windows network permission policy.

## 2. Authority and terminology

The canonical entry point is:

```text
python devscripts/dev-loop.py
```

The loop delegates compilation and linking to `build.py build-only`. It does
not duplicate dependency tracking, compilation, linking, gameplay, or server
authority.

Definitions:

| Term | Required meaning |
|---|---|
| source generation | Monotonically increasing loop-local count of detected input changes. |
| build number | Monotonically increasing numeric identifier for a published artifact directory. |
| published build | `build/.dev/builds/<number>/mimita.exe` plus its copied runtime files and identity metadata. |
| valid build | Published artifact with a real runnable PE executable and complete identity information. |
| current build | Valid build produced from the loop's latest source generation. |
| stale build | Previously published build that has not been proven against the current source generation. |
| build identity | At minimum build number, executable path, SHA-256, source generation, and source snapshot metadata. |
| dev-loop server | A server launched by this loop and marked with the dev-loop process/environment identity. |
| unrelated process | A process not provably launched by this loop for this checkout and run. |
| launch request | Explicit `[1]`, control request `launch`, or an equivalent documented user action. |
| runtime evidence | Fresh output from the selected executable and its live shared `events.jsonl`; build output alone is not runtime evidence. |

The following are separate claims and must never be conflated:

- build compiled and linked;
- artifact was published;
- server/client started;
- server/client used the same build;
- the expected journal path was used;
- the scenario reached the expected state;
- a human accepted visual, gameplay, audio, or multiplayer behavior.

## 3. Does not and safety invariants

The loop must not:

- kill every process named `mimita.exe`, `mimita*.exe`, or `python.exe`;
- terminate an unrelated or user-owned server because its executable is old;
- replace a healthy external server merely because the client exited;
- launch a stale artifact after a source change;
- launch a client against a server with a different build identity;
- automatically approve, dismiss, or interact with a Windows Firewall consent dialog;
- treat a hidden console or hidden game window as proof that firewall permission exists;
- create a competing dev-loop daemon for the same checkout;
- launch a second server on a conflicting bind/port without recording the conflict;
- call a build, unit test, or `--versioninfo` result gameplay acceptance;
- create a second unmanaged diagnostic log file.

The loop may terminate a process only when all of the following are true:

1. the loop launched or explicitly adopted the process through a documented
   control action;
2. the process has a matching checkout/run ownership marker or an equivalent
   verified identity;
3. the reason is a controlled lifecycle operation, build identity replacement,
   launch-configuration replacement, or explicit server-stop request;
4. the action, target PID, executable, build identity, reason, and result are
   recorded.

If ownership cannot be proven, the loop must stop and report
`NEEDS_USER_ACTION`.

## 4. Inputs and configuration

The loop reads its profile and launch-mode configuration through the existing
dev-loop configuration path. A profile may define the map, game mode, server
bind, NPC count, client name, server name, visibility policy, and selected
launch mode.

Launch-mode configuration may add server/client arguments, but it must not
silently override the loop's identity, journal, ownership, or safety
arguments. Effective arguments must be retained in status and launch events.

Configuration changes follow the same source-generation/build rules as code
changes when they affect the executable or launch behavior. A changed launch
mode or effective server configuration invalidates an existing incompatible
server even if its executable build number is current.

## 5. Source watching and build state machine

The loop watches the repository inputs defined by the implementation's watch
inventory. On a change it must:

1. increment the source generation;
2. mark the current published build stale;
3. preserve the running client/server unless an explicit launch request says
   otherwise;
4. debounce edits;
5. queue exactly one build, with another build queued if inputs change during
   the active build.

Required states:

```text
STARTING
  -> BUILD_PENDING
  -> BUILDING
  -> PUBLISHED_CURRENT
  -> BUILD_FAILED

PUBLISHED_CURRENT + launch request
  -> RECONCILING_PROCESSES
  -> LAUNCHING_SERVER
  -> SERVER_READY
  -> LAUNCHING_CLIENT
  -> RUNNING

RUNNING + source change
  -> BUILD_PENDING
  (existing runtime remains alive until explicit replacement)
```

The loop must expose enough status to distinguish at least:

- no artifact;
- stale artifact;
- build pending;
- building;
- build failed;
- published current build;
- server starting/ready/failed;
- client starting/ready/exited;
- build mismatch;
- firewall prompt detected;
- ownership or port conflict;
- stopped with persistent server retained.

## 6. Build and publish contract

The loop invokes the canonical build-only command and captures its output and
exit code. A build is successful only when the canonical build system reports
success or an explicitly supported no-change result and the expected artifact
can be validated.

After success the loop must publish a new numbered artifact directory. The
published artifact must include:

- `mimita.exe`;
- required runtime DLLs/assets according to the build procedure;
- build number;
- executable path;
- SHA-256 or equivalent content identity;
- source generation and source snapshot identity;
- build result and timestamp.

The loop must not select an older valid artifact merely because it is easier to
launch. If the current build fails, it must report the failure and keep any
existing runtime alive; it may not claim that the old runtime contains the new
source.

Retention cleanup must not delete files needed by a live process. Before
deleting an old published directory, the loop must prove that no owned server
or client uses that executable. If proof is unavailable, retain the artifact
and record the retention reason.

## 7. Meaning of `[1]` and launch selection

`[1]` is the explicit “build/launch newest verified build” action.

If no build exists or the latest build is stale, `[1]` queues a build. When
that build succeeds, the loop may continue to launch it because the user made
an explicit launch request.

If a current verified build already exists, `[1]` must not launch an older
numbered build. It must:

1. select the highest-numbered valid current build;
2. compute its exact identity;
3. reconcile owned old processes and configuration;
4. launch/reuse only a matching server;
5. launch the client with the same identity;
6. verify readiness and identity before reporting `RUNNING`.

Automatic watcher builds publish only. They do not replace the running client
or server unless an explicit auto-restart policy is enabled and the policy is
shown in status.

## 8. Process and server lifecycle

### 8.1 Ownership discovery

At startup and before launch, the loop must inspect its own persisted state and
the Windows process table sufficiently to identify previously launched dev-loop
processes. The server ownership record is persisted at
`.dev/server-owner.json` and includes checkout root, role, PID, executable,
arguments, run ID, build ID, journal path, and readiness. PID alone is not
ownership proof because PIDs can be reused. The live process executable and
command line must still match the record before adoption or termination.

The loop may adopt a prior server only when its executable, arguments, marker,
build identity, map, mode, bind, and journal contract all match the current
requested configuration.

### 8.2 Replacing an old server

The required behavior for build `2137` is:

```text
server 2135 + client 2135
source changes
build 2137 succeeds
[1] pressed
 -> identify server 2135 as owned and stale
 -> stop server 2135 and verify exit
 -> launch server 2137
 -> verify server 2137 readiness and identity
 -> launch client 2137
 -> verify client/server identity match
```

The old server should be stopped immediately before replacement launch, not
blindly at the beginning of compilation. This prevents an unsuccessful build
from destroying a working runtime.

If an old process is not provably owned, the loop must not kill it. It must
report the PID, executable, bind/port, observed identity, and the user action
needed to resolve the conflict.

### 8.3 Client lifetime and loop shutdown

Stopping or replacing the dev-loop client must not implicitly stop a healthy
external server. An explicit `stop-server` action may stop the owned server.
The status and journal must state whether the server was retained or stopped.

When a client exits unexpectedly, the loop must check server liveness and
record the server exit code if it also died. It must not attribute the failure
to networking, build identity, or client cleanup without evidence.

## 9. Server/client identity contract

Every server and client launch must receive:

- build number and content hash;
- source generation;
- run ID;
- process role;
- shared `events.jsonl` path;
- effective launch arguments;
- selected map/mode/profile.

The server and client are compatible only when their build identities match
and their effective launch configuration is compatible. A status line such as
`match=False` must include the first mismatch field, not only a boolean.

The loop must run the exact selected executable with `--versioninfo` before
relying on it. The reported `EVENTS_JSONL_PATH` is authoritative for the run;
the loop must not infer a journal from an older similarly named file.

## 10. Headless and Windows Firewall contract

`headless` means:

- game window hidden according to the launch-visibility policy;
- child console hidden;
- process, server, client, and journal remain active;
- no focus or cursor theft is required.

`headless` does not mean:

- firewall permissions are granted;
- network traffic is disabled;
- a security dialog cannot appear;
- the runtime is validated without reading its journal.

Every ordinary local dev profile must bind to `127.0.0.1` (or another explicit
loopback address) and record that policy. A non-loopback bind is rejected unless
the profile explicitly sets `allow_external_bind`; that exception is an
intentional network test and can never claim `FIREWALL_CLEAR` merely because
the process started. Before or during a bounded launch monitor it must record:

- bind address and ports;
- executable and process role;
- whether a firewall prompt was detected;
- prompt window information if detected;
- whether the server/client remained alive;
- the final result: `FIREWALL_CLEAR`, `FIREWALL_PROMPT_DETECTED`,
  `FIREWALL_POLICY_BLOCKED`, `EXTERNAL_BIND_REQUIRES_FIREWALL_REVIEW`, or
  `FIREWALL_CHECK_UNAVAILABLE`.

The loop must never click an OS security prompt. If the required firewall rule
is missing, the result is an actionable diagnostic or a controlled failure,
not a false `RUNNING` success.

## 11. Structured runtime evidence

All loop diagnostics use the canonical shared `events.jsonl` path. No gameplay
local log or ad-hoc debug file is allowed.

At minimum, the loop records bounded transition events for:

- source changed;
- build started, succeeded, failed, or publish failed;
- artifact identity selected;
- process discovered, adopted, replaced, retained, or refused;
- server/client launch arguments and ownership markers;
- `versioninfo` execution and reported journal path;
- server logger readiness and room publication;
- client logger readiness;
- build identity comparison and first mismatch;
- firewall/window observation;
- process exit, stop request, stop result, and exit code;
- final run state.

Each event must include UTC time, event name, process/PID, run ID, build ID,
role, result, and reason where applicable. Events describe transitions and
bounded observations; they must not log every render frame.

## 12. AI-fix-live validation procedure

Any dev-loop bug investigation follows
`docs/specs/debug-logging/AI-fix-live.md`:

1. Read this specification and the routed build, logging, terminal, runtime,
   and task-completion documents.
2. State the failure with actor/process, action, expected state, actual state,
   exact build identity, and exact journal path.
3. Use the existing dev-loop status/control path. Do not start a competing loop.
4. Trace source change -> build -> publish -> process discovery -> launch ->
   identity -> readiness -> journal/result.
5. Add only the smallest owner-level `StructuredLogger` evidence needed to
   identify the first divergence.
6. Publish a newly identified executable through the dev loop.
7. Run that exact executable and inspect its live shared journal.
8. Implement a fix only when the first divergence and intended behavior are
   clear. Otherwise improve the evidence and repeat the real scenario.
9. Rebuild and rerun the same scenario.
10. Report source, build, runtime-journal, visible behavior, and human-review
    evidence as separate claims.

For a performance or collision scenario, an FPS value is valid only when the
journal also identifies the exact executable/build, map/mode, avatar/config,
collision state, measurement window, and relevant runtime stage. A build or
status line alone cannot prove the target behavior.

## 13. Acceptance criteria

### Specification acceptance

- A contributor can determine what `[1]`, automatic build, headless mode,
  server replacement, and shutdown mean without guessing.
- Every destructive process action has an ownership and evidence rule.
- Build identity, runtime identity, and human acceptance are separate.
- Failure states identify the first missing boundary and next action.

### Runtime acceptance

A real run is accepted only when the fresh journal proves:

- the selected executable and build identity;
- the expected profile/launch mode/map;
- server and client identity match;
- the expected server/client readiness path;
- the firewall result;
- the requested scenario's actual result.

For mode 11 collision performance validation, the scenario-specific target is
the exact mode-11 configuration in `devscripts/dev-launch-modes.json`, with
the intended avatar equipped, real collisions active, and a fresh executable
whose bounded journal demonstrates the agreed FPS target. The target must not
be claimed from a stale status line or a different avatar/run.

### Human acceptance

Visual quality, gameplay feel, absence of intrusive prompts, and multiplayer
behavior require direct human or explicitly observed scenario acceptance. They
are not proven by compilation, artifact publication, or a unit test.

## 14. Current implementation snapshot

This section records what `devscripts/dev-loop.py` currently implements. It is
not a substitute for runtime acceptance and must be updated when the script
changes.

### Implemented in the current source

- Source changes mark the latest published build stale and queue a background
  build after debounce.
- Background builds publish numbered artifacts but do not replace the running
  client or server unless an explicit launch request is pending.
- `[1]` queues a build when the selected artifact is missing or stale; when a
  current artifact exists, it launches the newest valid published build.
- The loop records `.dev/server-owner.json` after a newly launched server has
  written `logger.started`.
- On Windows startup, the loop may adopt that server only when the marker's
  checkout root and executable match the live process and the live command
  line contains `--server`. It then rechecks executable and command-line
  identity before terminating it.
- A current in-memory server is represented by the `Popen` child record. An
  adopted server is represented by its verified PID and is checked through the
  Windows process table.
- Before a new server launch, the loop compares executable path and effective
  server arguments. A mismatch requests replacement and emits
  `dev-loop.process.replace_requested`; a successful or failed stop emits
  `dev-loop.process.stop_result`.
- The loop reports `server_adopted`, `build_mismatch_reason`, and
  `firewall_result` in newly written status snapshots.
- Ordinary profiles are rejected before server launch when their bind is not
  loopback and `allow_external_bind` is not explicitly true.
- A loopback launch begins with `LOOPBACK_ONLY`; after the bounded window
  monitor sees no matching Firewall dialog it becomes `FIREWALL_CLEAR`. A
  detected dialog becomes `FIREWALL_PROMPT_DETECTED`.
- An explicit external bind is allowed only with `allow_external_bind` and is
  labeled `EXTERNAL_BIND_REQUIRES_FIREWALL_REVIEW`; it must not be reported as
  firewall-clear.
- The loop still retains a healthy external server when the client exits or
  when the daemon shuts down. Only explicit server-stop or replacement may
  terminate the owned server.

### Current limitations and proof boundaries

- The marker is written after server logger readiness, so a crash before that
  point has no persisted ownership record.
- Marker adoption is Windows-specific. On non-Windows hosts the persisted
  server cannot be adopted through this path.
- The current source verifies executable path and the `--server` command-line
  marker during adoption; full marker argument/build-ID comparison is completed
  by launch-configuration reconciliation, not by the initial adoption check.
- The loop observes Firewall dialog windows; it does not inspect or create a
  Windows Firewall rule. Therefore `FIREWALL_CLEAR` means “no dialog observed
  during the bounded monitor for this loopback launch,” not “the OS firewall
  configuration was exhaustively audited.”
- `FIREWALL_CHECK_UNAVAILABLE` is a reserved specification result and is not
  currently emitted by the script.
- The running daemon does not hot-reload changes to `dev-loop.py`. A daemon
  already started with an older script keeps its old behavior and status
  schema until it is safely restarted. Status fields written by that daemon
  may therefore omit the newer fields or show them as null.
- A live status snapshot is evidence only for the daemon and executable that
  produced it. It cannot prove behavior from newer source until a fresh daemon
  and fresh executable are launched.

### Current observed run boundary

The read-only status observed during the 2026-10-13 implementation session
reported profile `npc-navigation`, launch mode `10`, server build `2202`,
`builds_match=false`, and a fresh journal path. Its new fields were null,
which identifies that the active daemon was using the older script version.
This is a daemon-activation mismatch, not proof that the new source paths are
failing. It must be resolved by a controlled daemon restart before runtime
claims are made.

## 15. Remaining evidence questions to resolve by runtime

These are investigation targets, not permissions to silently weaken this
specification:

- Can the current loop identify and safely adopt or reject a server left by a
  previous loop process?
- Does it detect all owned stale servers, or only the server stored in memory?
- Can published-build retention delete an executable still used by a live
  process?
- Does the current firewall observation prove absence, or only detect a visible
  prompt after launch?
- Does `match=False` expose the first identity/configuration mismatch?
- Does a fresh mode-11 run prove the intended avatar and collision path in its
  journal?

Each answer must be supported by source inspection and, where runtime behavior
is involved, a fresh real executable and journal.

## 16. Required related documents

- `docs/ROUTER.md`
- `docs/specs/debug-logging/AI-fix-live.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
