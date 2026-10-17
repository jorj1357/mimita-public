2026 10 13 1933 jorj:
"explain  to me the 

cle problem the spec described. I’m stopping only those identified dev-loop daemons through their own control sockets, then I’ll start one fresh daemon so the scenario and journal have an unambiguous execu

explain to m e why this happens and how we coudl make it stop ahppenign  
C:\mimita-v9\docs\specs\dev-loop\dev-loop.md like u can prob just  put direct in to here like a super specific specification to ensure that the script , if its running, its just running, it doenst get closed, mimtia exe can launch but the script doenst just  disappear or close like how it does right n ow , its just open, only 1 daemon and then it just sticks to being open and it doesnt die so quickly so the dev loop is just running consatntly, so AI can use that to  send commands into the gamae. like mimtia.exe can open with no dev loop script but  if tahts not open then its not good, idk, need a way to  ensure that the sript gets auto launched so that if AI is working on teh game then it uses that script for like sending commands  and confirming we're on the right build and agree on time etc "

also this 2026 10 13 1937: "The delay field did not appear in the live journal because the already-running daemon had the old Python code loaded. This is exactly the stale-live-loop class of failure the logging spec warns about. I’m restarting the existing dev-loop through its control path, then I’ll rerun against a new journal so the queue timing and build identity are unambiguous."

from AI, so , we need a way to edit the script, rebuild it, and it does not disappear, it just relaunches itself , bc thats how we buidl faster we dont use  the build scripts every time we just let this run and it builds automatically, as well as l etting AI run commands to test the game 

2026 10 13 1945 also the dev loop "d into that actual movement owner now, then I’ll rebuild through the persistent dev loop and verify movement plus walking animation in the live journal." - i dont seee the dev loop in the windows task bar its nto there iw ant to be able to see it and watch AI running moevment commands there too 

## Open problem notes from 2026-10-13

The unresolved user problem is not “add more command names.” It is to define
one reliable path through which an AI can send a command to one already-running
MiMITA client, prove that the client received it, prove that the normal gameplay
owner applied it, and keep the dev-loop daemon available long enough to collect
the result.

The desired user-facing shape is one script, not a second gameplay controller:

```text
python devscripts/dev-loop.py --command "agent.move 1 0 120"
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
```

Those command-line forms are target contracts for the implementation plan
below. They are not currently implemented merely because the game recognizes
`agent.move` or because a launch mode contains `--startup-command`.

<!-- Original investigation notes retained below. -->
2026 10 13 1915 jorj todo - 
how come the dev loop closes? we need to ensure that it starts and it si running like all the time when doing AI tests

also, how does teh agent.move  and agent.aim etc work?
how does AI send the inputs  to the game and then how can i verify and watchthat happen? in tests recently i watched it and i saw the cmds but it didnt run the commands so idk 

need to make it so AI has a clear path, prob thru the dev-loop.py thing, like
but not sure? bc its like kinda heavy of a script and is logging things all the time so idk 
maybe one python script for AI to use the commands with? that works with dev loop py?
id like to just keep it all in the 1 script tho , so AI can just  do like 
dev-loop.py --command "agent.move" 
stuff like that
or maybe like putting all the  moves u want to do in a .json smoewhere, so its just running that json so ai doesnt ahve to make it up as it goes

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

### 5.1 Lower-priority update-speed and script-structure investigation

The user reports that updates can feel slow, but says the dev loop currently
works and this is not a high-priority problem. Treat this as a future
investigation, not an immediate rewrite or a reason to risk the always-running
loop. The question of splitting `dev-loop.py` into smaller files is open; do
not split it just because the file is large.

Current source has several distinct waits and costs:

- `POLL_SECONDS = 0.50`: the watcher checks for file changes twice per second.
- `snapshot_inputs()` recursively walks the watched source trees for each
  watcher pass. The script comment estimates about 60 ms for a snapshot on
  this checkout; that is a source comment, not a fresh measurement.
- `DEBOUNCE_SECONDS = 0.30`: after a change is queued, the loop waits before
  starting the build.
- `run_build()` invokes `build.py build-only`; actual C++ compile/link time
  depends on changed files, compiler cache hits, and build state.
- `publish_build()` validates and copies the executable and runtime DLLs into
  a numbered build directory after compilation.
- process-identity/liveness helpers (`powershell Get-CimInstance`) are invoked
  from the status render path. When the server is adopted (daemon restart while
  a persistent server keeps running), this once spawned a helper process per
  render, several per second (a visible PowerShell window flood; see 10.1). A
  single `state()` must query server liveness once and reuse it, and
  `windows_process_identity()` must cache its result for a short TTL. The
  steady-state target is no helper process at all (10.1).

These stages mean the code intentionally trades some edit-to-build delay for
lower background polling during gameplay, safe build completion, artifact
validation, and build identity. Do not remove the delay, cache checks, copy
validation, or identity guards by guessing.

If the user asks to prioritize speed later, first record timestamps for edit
observed, debounce complete, build start/end, publish complete, and any second
build queued. Measure a no-op edit, a small C++ edit, and a broad/header edit.
Then tune only the measured bottleneck while preserving one build at a time,
correct source-generation identity, a running client's stability, and the
persistent daemon. Consider splitting the script only if measurements reveal a
specific maintenance or timing boundary that benefits from separate modules;
the single `dev-loop.py` entry point and one-daemon lifecycle must remain.

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

A loopback-only bind such as `127.0.0.1` may be used only by a clearly named
isolated local test profile. It is not acceptable for `ss`, a server-preset
start, or any scenario claimed to prove an online-capable client/server path.
Those flows must start the dedicated server using the same registered room
setup as the GUI, then join by room code through the coordinator/ICE flow; the
server must use a supported non-loopback bind for the intended network scope.
A direct client `127.0.0.1` address is not an acceptable substitute.

The dev-loop's own control socket may bind to `127.0.0.1`: that socket carries
AI commands to the Python dev-loop on this same computer. It is local process
control, not the game client's connection to the dedicated server. Keep these
addresses and purposes distinct in status and logs.

A non-loopback server bind is rejected unless the profile explicitly sets
`allow_external_bind`; that exception is an intentional network test and can
never claim `FIREWALL_CLEAR` merely because the process started. Before or
during a bounded launch monitor it must record:

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

### 10.1 Helper subprocess and console hygiene

The dev loop spawns short-lived helper processes (process enumeration/identity,
`taskkill`, `--versioninfo`, and the incremental build). When the daemon runs
detached it has no console, so a console helper started with a plain
`subprocess` call allocates a new console window. Because process identity was
queried from the status render path, this produced a visible flood of PowerShell
windows (several per second) during daemon start/restart.

Contract:

- Every short-lived helper subprocess must be created with no visible console:
  `creationflags=CREATE_NO_WINDOW` and a `STARTUPINFO` with
  `STARTF_USESHOWWINDOW`/`SW_HIDE` (plus `close_fds=True`).
- A helper process must never allocate a visible window, steal focus, or become
  the foreground window. This holds for headless, visible, and detached daemon
  modes alike.
- The console policy in this section applies to the loop's own helpers as well
  as the game server/client; "child console hidden" is not limited to the game.
- Process identity/liveness must not be a subprocess-per-status-render; see the
  polling-cost rule in section 5.1.

Target implementation (removes PowerShell entirely). The current helper uses
`powershell -Command Get-CimInstance Win32_Process`. The intended steady-state
implementation uses Win32 APIs directly through `ctypes` and spawns nothing:

- liveness: `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE)` and
  `GetExitCodeProcess == STILL_ACTIVE` (the watcher already does this);
- executable path: `QueryFullProcessImageNameW`;
- enumeration for the checkout lock: `CreateToolhelp32Snapshot` +
  `Process32FirstW`/`Process32NextW` filtered by the python executable;
- command line (only needed to confirm a `--server` launch): read it once from
  the persisted `server-owner.json` marker, or via
  `NtQueryInformationProcess(ProcessCommandLineInformation)`. Do not re-query it
  at render frequency.

Until that migration, helpers must at minimum be spawned hidden and cached
(section 5.1). The migration is complete only when no `powershell`, `wmic`, or
`tasklist` process is created at steady state.

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
- [1] queues a build when the selected artifact is missing or stale; when a
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

## 17. Deterministic in-game command scenarios

The dev loop is not itself the gameplay owner and must not invent a second
input implementation. The running client owns the command registry and the
shared action surface. The dev loop starts or reuses one client, submits a
scenario through the existing terminal command entry point, and reads the
result from the same canonical `events.jsonl` journal.

### What can be automated

Every user-visible action that already has a shared action owner must expose a
deterministic command adapter with the same semantics as the corresponding
keyboard, mouse, or GUI action:

| User behavior | Scenario command | Required result |
|---|---|---|
| walk forward/back/left/right | `agent.move <forward> <side> <duration_ticks>` | fixed-tick input is held for the requested ticks, then released |
| jump/dash | `agent.action <jump\|dash>` | the normal action edge is delivered once |
| aim at a direction | `agent.aim <x> <y> <z>` | camera/player aim target is set through the shared aim owner |
| equip a weapon | existing `equipslot <n>` | normal inventory/equip path runs |
| shoot once | `agent.shoot` | normal weapon input path runs and records the shot result |
| inspect state | `agent.snapshot <label>` | one structured state sample is written |
| run a bounded scenario | `agent.scenario <id>` | registered steps execute in order with step IDs and assertions |

These names are the v1 command contract. The current source provides
`agent.move`, `agent.aim`, `agent.action`, `agent.shoot`, and
`agent.snapshot`. `dev-loop.py --scenario-file` also reads a repository-local
JSON (or fenced-JSON Markdown) file and queues its `setup` and `steps` commands
in the existing ready client's command queue. The command queue executes
steps, but it is not yet a complete scenario runner: assertions are not
evaluated, `scenario.finished` explicitly records
`assertions_evaluated=false`, and the control call returns a queued response
rather than a final pass/fail result. Existing commands remain valid. The
adapters must call the same action/input functions used by normal gameplay;
they must not teleport by editing `Player::pos`, fire by spawning a projectile
directly, or rotate a limb by bypassing the animation owner.

### One running executable

The supported flow is:

```text
dev-loop.py --launch-mode <mode>
  -> one visible client and its existing server
  -> startup command or terminal command dispatch
  -> agent scenario state machine on fixed simulation ticks
  -> normal movement/aim/weapon/avatar owners
  -> StructuredLogger
  -> events.jsonl
```

`--startup-command` remains useful for setup such as avatar selection and
`equipslot`. Today, `--scenario-file` requires the dev-loop daemon and a
logger-ready client; it does not launch the server/client or wait for network
join itself. It queues ordered commands with optional `delay_ms_after`; the
client emits delivery and `scenario.step`, followed by `scenario.finished`
when the last command returns. A second client or competing dev loop is not
required for a local transform test.

The complete target workflow is one scenario-file request from any supported
dev-loop state: ensure exactly one daemon, build/launch the selected run if
needed, wait for server readiness/room registration and client connection/map
readiness, then execute scenario steps and assertions. Lifecycle actions use
the existing dev-loop and game owners; the runner must not start a second
server, fake a join, or bypass normal gameplay commands.

#### End-to-end scenario file contract

The file should be versioned JSON with these top-level fields:

```json
{
  "schema_version": 1,
  "id": "weapon_respawn_identity_v1",
  "launch": {
    "profile": "networking-regression",
    "launch_mode": 1,
    "visibility": "visible",
    "reuse_matching_run": true
  },
  "setup": [],
  "steps": [],
  "assertions": [],
  "timeout_seconds": 180
}
```

`launch` asks the dev-loop to ensure one daemon and a matching client/server
run; it never authorizes stopping an unrelated or ambiguously owned process.
If a run is started, it uses the selected mode's normal server registration
and client join flow. If a healthy matching run is reused, the runner verifies
its build, mode, server, room/session, map, and journal identity before acting.
The invocation returns the scenario ID/revision, request ID, run/build
identity, client/server PIDs, and exact journal path.

Each ordered step has a stable ID and one operation: `command` routes through
the normal client terminal, `wait_for_event` waits for a correlated structured
event, `wait_for_state` waits for a named authoritative/presented state from an
existing snapshot owner, `snapshot` captures state without mutation, and
`assert` compares expected and actual values with explicit tolerance. Waits
have bounded tick/time limits and report the last event/state observed on
timeout. Wall-clock delay is optional pacing only; it must not substitute for
waiting on readiness or game state. Bounded repeats may continue a normal
action until an event/state condition (for example NPC death) or a maximum
count/deadline; unbounded loops are invalid.

The runner emits `scenario.started` before setup, `scenario.step` on each
operation, `scenario.assert` for every assertion, and exactly one terminal
`scenario.finished` with `passed`, `failure_kind`, first failed step/assertion,
last successful step, expected/actual values, and whether the run was
intervened. The CLI waits for that record by request ID and returns nonzero
unless `passed=true`. Missing logs, timeouts, stale build/run identity,
unmatched server/client identity, early client/server exit, or incomplete
assertions fail the scenario rather than producing a queued-as-success result.

### Scenario file

The current runner accepts a repository-local JSON scenario file through
`python devscripts/dev-loop.py --scenario-file <path>`; it does not require a
game-terminal `agent.scenario_file` command. The current repository scenario
document is
`C:\mimita-v9\devscripts\scenarios\readmewhat.md`. The file is
declarative:

```json
{
  "id": "avatar_transform_walk_aim_shoot_v1",
  "steps": [
    {"command": "agent.snapshot", "args": ["start"]},
    {"command": "agent.move", "args": [1, 0, 120]},
    {"command": "agent.snapshot", "args": ["after_walk"]},
    {"command": "agent.aim", "args": [1, 0, 0.2]},
    {"command": "agent.snapshot", "args": ["after_aim"]},
    {"command": "equipslot", "args": ["5"]},
    {"command": "agent.shoot", "args": [1]},
    {"command": "agent.snapshot", "args": ["after_shoot"]}
  ],
  "assertions": [
    {"type": "position_delta_nonzero", "from": "start", "to": "after_walk"},
    {"type": "camera_forward_matches", "step": "after_aim", "value": [1, 0, 0.2], "tolerance": 0.01},
    {"type": "avatar_model_ready", "step": "after_shoot"},
    {"type": "avatar_arm_pivot_stable", "step": "after_shoot", "part": "rightArm", "tolerance": 0.01}
  ]
}
```

Currently, the control-side parser queues each `setup` and `steps` command in
order, with wall-clock delays from `delay_ms_after`. It does not evaluate the
example `assertions` array, wait for state/event predicates between commands,
or return a scenario verdict. It also does not launch a server/client or join a
room. Client arguments are parsed by each command owner. The scenario file is
a bounded development input, not a gameplay configuration system loaded by
ordinary players.

### Avatar transform scenario

The canonical avatar scenario runs the same steps for Jason/default, a
previous custom avatar with no `body_frame_policy` field, Boomer v21 as the
known working custom model, Stabber v6 as the previously failing model, a
current monster model, and one unrelated weapon or map GLB as a non-player
control.

Each run records raw and normalized node transforms, the normalized model
signature, fixed-tick movement, camera aim, right-arm world frame, weapon grip,
muzzle, and the first failed assertion. The expected result is that old
avatars remain authored/legacy-compatible, opted-in player models use the
canonical frame, and non-player GLBs produce no player-normalization records.

### Python responsibilities

Today Python can submit a command or queue a scenario into an already-running
logger-ready client and return the queue acknowledgement. The AI caller must
then inspect the exact journal; the current `scenario.finished` is execution
completion only, not a pass. The target Python entry point must be able to
ensure daemon/run readiness, wait for the matching final verdict, and return
that verdict and journal path. It must not manipulate the game window to
synthesize keyboard/mouse events, and must not declare success from process
liveness or build status alone.

### Completion evidence

A scenario is successful only when all of the following are present in the
fresh journal for the exact executable:

- `startup.command.dispatch` or scenario dispatch proves the requested action
  entered the client;
- `scenario.step` proves each step executed on the expected simulation ticks;
- movement/aim/weapon owner events prove the action was applied;
- `avatar.model.transform-source`, `avatar.model.raw-node`,
  `avatar.model.normalized-node`, and `avatar.pose.sample` prove the model
  frame and pose values;
- `avatar.command.assert` records pass with explicit tolerances;
- `scenario.finished` reports `passed=true`;
- visual/human review is still reported separately when the claim includes
  appearance, material, or camera framing.

Build success and a running window are necessary setup evidence, not feature
acceptance.

### Agent command proof before scenario automation

Do not treat command registration, queue acknowledgement, or a
`startup.command.dispatch` event as proof that an agent action worked. Prove
each adapter through its full owner path in a fresh identified run before
building a larger scenario around it:

| Adapter | Current owner path to trace | Minimum live-journal proof |
|---|---|---|
| `agent.move` | `src/main-systems.cpp` registration -> scheduled input -> `src/input/input-poll.cpp` -> fixed-tick application in `src/engine/engine-tick-net.cpp` -> movement state | request/dispatch, accepted axes and tick window, applied axes/tick, before/after position or velocity, then release after requested ticks |
| `agent.aim` | command registration -> shared aim/camera owner -> player aim state | requested world target, accepted target, resulting direction/state and tick |
| `agent.action` | command registration -> shared action-edge/input owner -> fixed-tick action handler | action name, accepted edge, consuming owner and result/state at the corresponding tick |
| `agent.shoot` | command registration -> normal weapon input -> attack request -> authoritative server validation/result | equipped stable identity plus generation/revision, attempt, server accept/reject reason, ammo/cooldown/result; command acceptance alone is insufficient |
| `agent.snapshot` | command registration -> state sampler -> StructuredLogger | labeled sample with tick and the fields needed by the scenario; it is observation, not proof that prior actions succeeded |

The command-proof slice should first run bounded isolated checks through the
real command entry point and exact executable journal. It must show one
successful example and one deliberate invalid-argument/blocked-state example
per adapter, with request/step correlation and rate-limited owner diagnostics.
If an adapter has no owner event or cannot distinguish accepted from applied,
specify and add that diagnostic at its existing owner before writing a
high-level pass assertion. Do not add a parallel logger or bypass normal input,
movement, weapon, or server owners. Record the executable identity and exact
`EVENTS_JSONL_PATH`; distinguish source review, build, live action evidence,
and visual review.

After those proofs, implement the end-to-end scenario lifecycle and verdict
contract above. Then author the priority 1 spawn scenario and priority 2
weapon lifecycle scenario as separate files. Priority 1 should be proven first
because a valid map and in-bounds spawn are prerequisites for trustworthy
combat testing. Priority 2 can be traced in parallel, but its final integrated
scenario should run only on a known-good map/session. The broader weapon
component migration is not a prerequisite for fixing a reproduced equip or
respawn lifecycle defect; promote it into the bug fix only if the trace proves
the current identity/slot model prevents a safe, bounded correction.

### Live scenario inspection and steering

An agent must be able to inspect the matching journal while a scenario runs
and issue a normal `--command` to investigate an unexpected state. Commands
submitted during a scenario must be serialized at a declared step boundary
and tagged with the parent scenario ID, intervention ID, reason, and current
step. An intervention that changes gameplay marks the original run as
`intervened`; its original assertions cannot silently remain a clean pass.
The agent can then continue from a recorded checkpoint, or abort and submit a
revised scenario file with a new content hash/revision. Edits to a scenario
file after it was queued must not change the already-running request.

Current limitation: the dev-loop accepts new command requests and appends
them to the same client queue, but it does not provide scenario pause,
step-boundary arbitration, intervention correlation, or assertion invalidation.

## 18. What must be solved before AI commands are reliable

### 18.1 Separate the five command stages

Every AI command must have an observable record for each stage:

```text
Python request
 -> dev-loop control socket accepts request
 -> client receives request on its main/game thread
 -> Terminal resolves the command name
 -> command handler calls the normal gameplay owner
 -> fixed-tick owner applies the action
 -> events.jsonl records result and optional assertion
```

The current `startup.command.dispatch` event proves only that `main.cpp`
called `Terminal::instance().execute(command)` after connection and world-load
gates. It does not prove that the command name was registered, that its
handler ran, or that movement/aim/shoot was applied. A successful test must not
stop at that event.

### 18.2 Explain the failed `agent.move` observation

The 2026-10-13 journal showed startup command dispatch for `agent.move`,
`agent.aim`, and `agent.snapshot`, but did not show the handler events
`avatar.command.dispatch`, `avatar.pose.sample`, or `avatar.command.assert`.
That narrows the first missing stage to the client terminal/handler boundary,
unless the launched executable was stale or the command was never actually
registered in that executable.

The implementation must therefore add bounded terminal evidence:

| Event | Meaning | Required fields |
|---|---|---|
| `dev-loop.command.request` | the Python/control side accepted a request | request ID, command text, run ID, target client PID |
| `terminal.command.received` | the client received text and entered `Terminal::execute` | request ID, command text, build number, client tick |
| `terminal.command.resolved` | command lookup found a registered handler | request ID, command name, argument list |
| `terminal.command.unknown` | lookup failed | request ID, command name, build number |
| `terminal.command.completed` | handler returned | request ID, command name, success/failure, client tick |
| `avatar.command.dispatch` | the agent adapter accepted validated arguments | request ID, normalized arguments |
| `avatar.command.assert` | the requested gameplay state was checked | request ID, assertion, expected, actual, tolerance, passed |

All records must use the same request ID. The first absent event is the first
diagnostic boundary. A test must report “unknown command,” “stale executable,”
“request not delivered,” or “handler ran but gameplay owner did not apply”
instead of saying that the command simply did nothing.

### 18.3 Use one control path for direct commands and scenarios

`dev-loop.py` should remain the only AI-facing Python entry point. It should
gain two bounded control requests:

```text
python devscripts/dev-loop.py --command "agent.move 1 0 120"
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
```

These invocations must contact the already-running daemon through its existing
loopback control socket. They must not launch a second daemon, inject keyboard
events, type into a window, or edit gameplay state directly.

The daemon must reject the request unless all of the following are true:

- exactly one matching daemon owns the checkout lock;
- the selected client is alive and its executable/build identity is known;
- the client logger is ready;
- the client is connected and the world-load gate is true;
- no earlier bounded scenario is still running, unless the request explicitly
  asks to queue behind it.

The control response must return `accepted`, `queued`, `request_id`, the
client PID, build number, run ID, and journal path. The caller then waits for
the matching terminal/result event, not for a fixed sleep.

### 18.4 Make the command implementation use the real game owners

The handler contract remains:

- `agent.move` schedules normal input for fixed simulation ticks;
- `agent.aim` uses the normal camera/aim owner;
- `agent.action` produces the normal one-frame action edge;
- `agent.shoot` enters the normal weapon input path;
- `agent.snapshot` reads state without changing it.

No adapter may teleport by writing `Player::pos`, spawn a projectile directly,
rotate an avatar limb directly, or bypass the fixed-tick gameplay owner. The
adapter may only translate deterministic arguments into the same input/action
surface used by ordinary play.

### 18.5 Make scenario JSON deterministic

The scenario file is a development input, not a new gameplay configuration
system. It must contain a stable ID, ordered steps, explicit tick durations,
request IDs generated by the runner, and assertions with tolerances. The
runner must emit `scenario.started`, `scenario.step`, `scenario.assert`, and
`scenario.finished` in the shared journal. It must stop at the first failed
assertion and include the last successful step plus the first missing event.

For avatar rotation, the first scenario should be:

```text
snapshot start
aim at fixed world target
snapshot after_aim
move forward for 120 ticks
snapshot after_walk
shoot once
snapshot after_shoot
```

Assertions should compare camera forward, player position, each physical-body
part world frame, weapon grip/muzzle, and the model normalization source.
Vertex-level evidence is recorded by the model loader; pose-level evidence is
recorded after animation/aim application. Visual review remains a separate
acceptance claim.

## 19. Why the dev-loop sometimes closes or disappears

The loop has several different lifetimes that must not be confused:

1. The visible dashboard is a reconnectable view; closing it does not stop the
   dev-loop daemon.
2. The Python dev-loop daemon owns the source watcher, control socket, status
   file, build worker, and dev-loop-launched client.
3. `devscripts/dev-loop-watch.py` is the process watchdog that restarts a dead
   or safely replaceable daemon; it is not a second build/game controller.
4. The dedicated server has an independent lifetime and may remain after the
   client, dashboard, or Python daemon stops.

The current implementation enters its shutdown path when the daemon receives
`Q`, Ctrl-C/console termination, an unrecovered exception, or an external
process close. Its `finally` block stops daemon-owned clients, closes the
control socket, writes a stopped status, and leaves the persistent server.
`--auto-restart` can relaunch a dead client only while the daemon itself is
still alive; it cannot revive a closed Python process. `--headless` hides the
game client and consoles; it does not make the Python daemon persistent.

The required design is one self-starting user/AI entry point, not a second
supervisor application that the user must manage. The existing
`devscripts/dev-loop-watch.py` is the watchdog helper in this design. An AI call such as
`python devscripts/dev-loop.py --command "agent.move 1 0 120"` is both the
command interface and recovery entry point; opening the desktop shortcut is the
visible dashboard entry point:

- The entry point ensures the one watchdog and one healthy daemon for this
  checkout, checking PID, authenticated control socket, heartbeat, run, and
  source identity. The desktop entry also opens or reconnects the visible
  dashboard; an AI command does not open a dashboard.
- If the daemon is healthy, the entry point sends the request to it and exits
  with the request result.
- If the daemon is absent, stale, or has a different script identity, the
  entry point starts exactly one background instance of the same
  `dev-loop.py`, waits for it to publish ready status, then sends the request
  once. It must not start duplicate daemons or duplicate a command during
  recovery.
- The background instance stays alive between requests so its watcher,
  builder, status, and command socket remain ready. It does not exit just
  because one AI command finished or the game client closed.
- The daemon writes a heartbeat and last-exit reason to the status snapshot.
- Every uncaught exception is written to the run journal before shutdown.
- A client exit records exit code, signal/termination reason, and whether the
  daemon attempted recovery.
- The status command distinguishes `daemon_alive`, `client_alive`, and
  `server_alive`.
- A control request can explicitly stop the client, server, or whole daemon;
  normal client exit must not silently stop the server.
- The entry point starts the daemon in a durable process session and ensures
  the watchdog is available. Daemon stdout/stderr go to a durable log. Closing
  the short-lived AI command window or the dashboard does not close the daemon.
- The loop refuses to start a competing daemon and reports the existing PID,
  status path, control port, journal path, and recovery instructions.

This design keeps the loop available during AI work without asking the user to
keep the dashboard open as a lifetime anchor or manage a second supervisor
application. The watchdog keeps the daemon alive during the Windows session. If
Windows, a user, or an error still closes the watchdog or daemon, the next
`--command`, `--scenario-file`, or `--ensure-running` call must restore the
watchdog/daemon before sending work. No software can promise that a process is
literally impossible to close; the guarantee is that the normal AI entry point
detects missing processes, explains the last known reason, and ensures exactly
one healthy replacement before proceeding.

### 19.1 The exact process topology

The implementation and every future investigation must distinguish the
dashboard, one-shot command entry point, watchdog, daemon, game client, and
dedicated server. The dashboard is presentation only; the watchdog owns daemon
recovery; the daemon remains the only build/game/control authority:

```text
Desktop shortcut -> dev-loop.py dashboard entry
  -> ensure dev-loop-watch.py watchdog
  -> ensure one dev-loop.py daemon
  -> attach visible terminal dashboard to daemon status

AI/task runner -> short-lived dev-loop.py command entry point
  -> ensure watchdog/daemon if absent; otherwise reuse them
  -> send one request to the same Python dev-loop daemon
       -> dev-loop-launched MiMITA client
       -> dev-loop-launched dedicated server

dev-loop-watch.py -> monitor/restart only the identified dev-loop daemon
```

The responsibilities are deliberately separated:

| Process/role | Owns | May restart | Must not silently restart or kill |
|---|---|---|---|
| dev-loop.py dashboard entry | ensure watchdog/daemon and attach one visible view | the dashboard view only | unrelated Python processes, game/server processes |
| dev-loop.py command entry | ensure one matching watchdog/daemon and forward one command/scenario | the same script in background daemon mode | unrelated Python processes, unrelated game/server processes |
| dev-loop-watch.py | watchdog lock, daemon liveness/source identity, bounded daemon recovery | the identified dev-loop daemon only | a second daemon, unverified server, or healthy client/server pair |
| Python dev-loop daemon | file watcher, build worker, control socket, status, command queue, owned client lifecycle | its owned client when `--auto-restart` is enabled; itself only through controlled replacement | an unverified server or duplicate command |
| MiMITA client | gameplay, terminal registry, fixed-tick input, animation, snapshots | only through the daemon's controlled launch/recovery path | the Python daemon or dedicated server |
| dedicated server | authoritative server state and server tick | only through explicit server replacement/stop policy | merely because the client, dashboard, watchdog, or daemon exits |

An implementation must never use “the EXE is open” as proof that the dev loop
is available. The minimum availability check is:

1. the recorded daemon PID is alive;
2. the recorded control socket accepts the current token;
3. the heartbeat is newer than the configured heartbeat timeout;
4. the status file identifies the current run and journal;
5. the client and server identities are separately reported.

If the daemon is absent, `--command`, `--scenario-file`, and
`--ensure-running` must start one daemon, wait until its authenticated control
socket and heartbeat are ready, then continue. If recovery fails, they return
`DAEMON_START_FAILED` with the exit reason and log location. `--status` stays
read-only and reports `NO_RUNNING_DAEMON`; it does not start anything.

### 19.2 Why the current Python daemon becomes stale

`dev-loop.py` is ordinary long-running Python code. It imports and executes its
functions when the process starts. The source watcher notices source changes
for the game build, but it does not reload the already-running Python module.
Consequently:

```text
edit dev-loop.py
  -> the file on disk is newer
  -> the existing Python interpreter still has old functions in memory
  -> old command/status/queue behavior continues
```

This is why a new field such as `delay_ms_after` can exist in the file and be
missing from a live journal. The journal belongs to the old daemon, not to the
new source on disk. This is a daemon-activation mismatch, not evidence that
the new source branch failed.

The status record must therefore identify both:

- the daemon source identity: script path, script hash, daemon start time,
  daemon PID, and implementation/status schema version;
- the game identity: build number, executable path, executable hash, source
  generation, client PID, server PID, run ID, and journal path.

Before interpreting a runtime result, compare the requested source identity
with the daemon identity and compare the selected executable identity with the
live client/server identities. If either comparison fails, report
`DAEMON_SOURCE_MISMATCH` or `BUILD_IDENTITY_MISMATCH` before debugging
gameplay.

### 19.3 Required daemon self-update/restart behavior

The Python daemon is not required to hot-reload arbitrary Python functions in
place. That would leave old threads, queues, sockets, and object state mixed
with new code. The required behavior is a controlled daemon replacement:

1. The watcher detects that `devscripts/dev-loop.py` or a directly imported
   dev-loop module changed.
2. The daemon finishes or cancels only the current build operation according
   to the documented cancellation policy.
3. The daemon writes `dev-loop.daemon.restart_requested` with the old script
   hash, new script hash, PID, run ID, reason, and whether a scenario is
   active.
4. The daemon stops accepting new commands and returns
   `DAEMON_RESTARTING` to new control requests.
5. The daemon drains or explicitly marks the command queue. It must never
   duplicate a command after restart. Every queued request is marked
   `preserved`, `cancelled`, or `completed`.
6. The daemon closes its control socket and writes a final handoff record.
7. The next CLI entry point starts exactly one replacement daemon using the
   new script identity. The same script is used as the bootstrap and daemon;
   no separate supervisor application is required.
8. The replacement daemon obtains the checkout lock, creates a new control
   token, opens or adopts the documented current run, and writes a new
   heartbeat.
9. The replacement daemon reports whether the existing client/server pair was
   preserved, revalidated, or intentionally relaunched. A Python-script
   restart must not automatically replace a healthy game pair unless the
   launch policy explicitly requires it.

The CLI entry point may detect the stale script hash and perform this
controlled replacement. The important contract is that the old interpreter
cannot continue presenting itself as the current daemon after its source
identity changed.

### 19.4 Visible terminal dashboard, daemon, and watchdog

The intended desktop experience is: the user opens the existing shortcut to
`devscripts/dev-loop.py`, sees a real terminal window with a live rainbow status
display, and can leave that window open to watch builds and game runs. The
display must show whether a build is running, the latest and active build IDs,
whether the game client and server are alive, the selected mode, and the exact
`events.jsonl` path. It must be a readable terminal, not a transparent or blank
window. This is a separate window from the MiMITA game.

The system has three distinct roles. Keep `devscripts/dev-loop.py` as the
canonical entry point and the single owner of build/run/control state. Keep
`devscripts/dev-loop-watch.py` as the detached watchdog that ensures the
dev-loop daemon is running. Add a dashboard view that attaches to the daemon
status; it may be a mode of the existing script or a small presentation helper,
but it must not become a second build/game owner. Do not add a second watchdog
or competing daemon architecture.

```text
Desktop shortcut -> dev-loop.py entry point -> ensure watchdog + one daemon
                 -> open/attach one visible terminal dashboard

dev-loop-watch.py (background watchdog) -> restart one failed/stale daemon

AI calls dev-loop.py --command / --scenario-file
  -> connect to that same daemon; do not open another dashboard
  -> daemon runs normal build/game/scenario owners
  -> events.jsonl records correlated evidence
  -> dashboard shows current status and recent output
```

The desktop shortcut must not rely on Windows' `.py` file association or
inherited stdout to decide whether a terminal appears. Its normal launch must
explicitly open or attach to a supported Windows terminal/console host and
connect the dashboard to the one daemon. Prefer Windows Terminal with a
dedicated opaque profile; use a standard Windows console if that host is
unavailable or cannot guarantee readable output. Do not use `pythonw.exe` or a
transparent terminal profile for the visible dashboard. Enable
virtual-terminal color/control support when available. If rainbow rendering is
unsupported, show the same complete status in plain text. Refresh the status in
place instead of printing the same status block on each poll. Give the window a
stable title containing the checkout name and run ID.

Dashboard and game visibility are independent:

```text
dashboard_visibility = visible | hidden
client_visibility    = visible | headless
```

The normal desktop shortcut defaults to `dashboard_visibility=visible`.
`--headless` and the existing game visibility toggle affect only the game
window. Hiding the game must leave the dashboard visible. AI command and
scenario calls use the control socket and do not create a dashboard for each
request.

The daemon must remain available while the Windows user session and host are
running, even if the dashboard is closed or restarted. Closing the dashboard
detaches the view; it does not stop the daemon, client, or server. The existing
watchdog is responsible for detecting a dead daemon, recording its exit reason,
and restarting exactly one daemon with bounded recovery. It must preserve a
healthy identified game/server pair and must not create duplicates. A dashboard
reopen reconnects to the current daemon and fetches current state. The next
shortcut or AI invocation must be able to start the watchdog/daemon if absent
and adopt only a run whose identity is verified. Do not promise persistence
across Windows shutdown, logoff, or power loss.

Show at minimum: watchdog and daemon health/heartbeat, their role labels and
PIDs, build phase and latest-versus-running build identity, client/server
status and PIDs, game uptime, mode, game-window visibility, run ID, last
error/recovery action, and journal path. For each Python process, show its
script path as well as the PID so the user can match it to Task Manager. Use
stable human-readable role names: `MiMITA Dev Loop Watchdog`, `MiMITA Dev Loop
Daemon`, and `MiMITA Dev Loop Dashboard`.

The status display is a fixed, non-scrolling dashboard. Keep each status field
on a stable row and update that row in place; never append the full status block
again just because uptime or another value changed. A compact layout should
make it easy to select and copy the text. Provide a dashboard-only pause/freeze
control so the user can select or copy a stable snapshot while the watchdog,
daemon, game, and scenario continue running. Resuming the display must refresh
from current state and mark when the view was refreshed; it must not present a
frozen `RUNNING` value as current. Put meaningful new activity in a separate
bounded output area, rather than mixing repeated status refreshes into the
scrollback.

On Windows, a Python process normally remains `python.exe` in Task Manager's
Image Name column. Do not pretend that a script can rename that executable
image. Pass a stable role marker in each managed process command line (for
example, `--dev-loop-role=watchdog`, `--dev-loop-role=daemon`, or
`--dev-loop-role=dashboard`). The dashboard must show the role label, PID,
interpreter image name, role marker, and exact script path. In Task Manager, the
Image Name may still be `python.exe`, but its Command Line column must
unambiguously show the role marker and script. Keep the initial implementation
Python-first and cross-platform; do not add a compiled per-role wrapper solely
to change the Task Manager Image Name. If a distinct native process image later
becomes a hard requirement, document and review that platform-specific tradeoff
separately.

If the daemon is unavailable, show that state and retry or provide an actionable
error; never leave stale `RUNNING` status displayed as current. Keep the status
file and durable logs usable when the dashboard is hidden or disconnected.

#### Implementation status (2026-10-17)

Implemented:

- `devscripts/dev-loop.py` is one entry point with roles selected by flags:
  `--daemon` runs the daemon, the default (and `--dashboard`) opens the visible
  dashboard, and `--command`/`--scenario-file`/`--request launch`/`--ensure-running`
  are one-shot control clients.
- The entry point ensures exactly one `devscripts/dev-loop-watch.py` watchdog
  (started detached with `--dev-loop-role=watchdog`) and one daemon (started by
  the watchdog with `--daemon --dev-loop-role=daemon`), waits for the
  authenticated control endpoint, and never starts duplicates (locks prevent
  them). `--status` remains read-only.
- `--command`/`--cmd` sends one command, waits for the correlated
  `dev-loop.command.delivered`/`terminal.command.completed` (or a reasoned
  rejection) from the exact live journal, prints the result, and opens no
  dashboard.
- The dashboard renders a fixed, non-scrolling status area refreshed in place,
  shows watchdog/daemon/dashboard roles, PIDs, interpreter image name, script
  paths, build phase and latest/running build, server/client state and uptime,
  mode, run id, game-window visibility, last error/recovery message, the exact
  `EVENTS_JSONL_PATH`, and a bounded recent-activity area. `P` freezes, `R`
  resumes/refreshes, and `Q` quits only the view. The title names the checkout.
- `devscripts/dev_loop_terminal.py` owns the platform terminal work: it enables
  Windows virtual-terminal processing, sets the window title, and prefers to
  relaunch the dashboard in an opaque Windows Terminal window, falling back to
  the current Windows console when Windows Terminal is unavailable.
- Dashboard attach/detach and watchdog/daemon start/recovery are recorded as
  categorized `dev-loop.dashboard.*` / `dev-loop.watcher.*` events in the
  canonical journal; no parallel gameplay log is created.

Not yet exercised in this environment: the Windows Terminal relaunch path
(`wt.exe` was not installed on the validating machine, so the standard-console
fallback was used), and the human visual reading of the live window.

#### Acceptance

1. Opening the canonical shortcut produces one real visible terminal with live
   status; it never substitutes a blank/transparent window. Rainbow colors are
   used where supported, with complete plain-text fallback.
2. The dashboard shows labeled watchdog/daemon/dashboard process identities and
   PIDs, build-in-progress and build success/failure, running/latest build
   identity, game status and uptime, server state, mode, and journal path. The
   Task Manager Command Line column clearly distinguishes watchdog and daemon
   roles even if both have `python.exe` as their Image Name.
3. Status refresh changes fixed rows in place; it does not add repeated status
   blocks or force old text to scroll away. The user can pause/freeze the view,
   select/copy it, and resume without pausing game or scenario execution.
4. Hiding the game leaves the dashboard visible; dashboard visibility does not
   silently change game visibility.
5. Closing/reopening the dashboard leaves the watchdog and daemon available,
   reconnects to the one current daemon, and displays current rather than stale
   status.
6. Repeated shortcut launches and AI `--command`/`--scenario-file` calls reuse
   exactly one watchdog and daemon and do not duplicate builds, clients,
   servers, or scenario requests.
7. A daemon crash produces a recorded exit/recovery chain. Watchdog recovery
   restores the control endpoint or reports a clear failure. An explicit stop
   remains stopped until a later explicit start/ensure request.
8. Runtime acceptance names the exact executable/build, watchdog and daemon
   identities, client/server PIDs, and `EVENTS_JSONL_PATH`. A visible dashboard
   alone is not proof that a command reached the gameplay owner.

### 19.4a AI restart authority for the dev loop (always allowed)

An AI agent is always allowed to restart the dev-loop daemon and any
dev-loop-owned game process whenever it wants or needs to, including to pick up
edited `devscripts/dev-loop.py` code, to clear a stale daemon source identity
(`DAEMON_SOURCE_MISMATCH`), to recover a wedged control socket, or before a live
scenario. This is an explicit, standing authorization and does not require
asking first.

The two hard conditions on that authorization are:

1. **Restart it after.** The agent must start a working dev-loop daemon before
   finishing the step, and must never leave the dev loop dead or absent.
2. **Keep it running.** After the restart, verify one healthy daemon owns the
   checkout lock and reports a live control endpoint/status. If a restart
   fails, keep trying or report the failure loudly; do not silently continue
   with no loop.

Do not restart processes that are not owned by this checkout. Preserve
unrelated running processes, and preserve a healthy matching game pair unless
the change or scenario explicitly requires relaunching it.

**Always-on watcher (implemented 2026-10-17).** The supervisor is
`devscripts/dev-loop-watch.py`. It:

- detects the exit of the checkout-owned dev-loop daemon and promptly restarts
  it from the current `devscripts/dev-loop.py`;
- detects a daemon whose recorded `daemon_script_hash` (in
  `.dev/current-run.json`) differs from the on-disk hash and performs the
  controlled replacement in `19.3`; while a live run is present it defers the
  replacement by default (`dev-loop.watcher.source-change-deferred`) so a
  healthy game pair is not replaced, unless `--force-source-change` is set;
- keeps exactly one daemon alive per checkout (its own lock plus the daemon
  lock), never spawns a competing/duplicate loop, and clears a stale daemon
  lock only after confirming its PID is dead;
- records `dev-loop.watcher.started` / `dev-loop.watcher.restarted` with old/new
  daemon PID and script hash in the active journal, and logs a visible error if
  it cannot keep a daemon alive.

Run it detached; it adopts the last-used profile/launch-mode/visibility from
`.dev/current-run.json` unless overridden, and supports `--once` for a single
check. The daemon now records `daemon_pid`, `daemon_script_hash`, and
`daemon_started_utc` in its status/current-run record so a stale daemon is
detectable (`DAEMON_SOURCE_MISMATCH`). The AI restart authority above remains the
fallback if the watcher itself is not running.

### 19.5 Why a command can be visible but have no gameplay effect

Every command must be diagnosed through this exact chain:

```text
1. request accepted by Python control client
2. request accepted by the daemon control socket
3. command record appended to the client queue
4. client consumes the record after readiness/world-load gates
5. Terminal receives the command
6. Terminal resolves the command name
7. handler validates and dispatches to the normal owner
8. fixed-tick owner applies movement/aim/action/shoot
9. snapshot or assertion observes the changed state
```

The meaning of each result is narrow:

| Last observed stage | What is proven | What is not proven |
|---|---|---|
| control response | daemon accepted or rejected the request | client received it |
| queue record | request was written for the client | client consumed it |
| `dev-loop.command.delivered` | client read the queue record | Terminal found a handler |
| `terminal.command.received` | Terminal entered command execution | handler changed gameplay |
| `terminal.command.resolved` | command name had a registered handler | fixed-tick state changed |
| `terminal.command.completed` | handler returned | movement/aim/shoot succeeded |
| `avatar.command.dispatch` | adapter accepted validated arguments | gameplay owner applied them |
| owner-applied event | normal gameplay owner consumed the action | visual appearance is correct |
| `agent.snapshot` / assertion | observed state at a known tick | human visual acceptance unless separately reviewed |

The first missing stage is the first useful diagnosis. A test must not report
“the command did nothing” when the evidence can say “the client was stale,”
“the command was queued behind the world-load gate,” “the name was unknown,”
or “the handler ran but the fixed-tick owner never applied the action.”

### 19.6 Required recovery and failure matrix

The following outcomes must be explicit and independently testable:

| Failure | Required result | Allowed recovery |
|---|---|---|
| daemon exits unexpectedly | heartbeat stops; status preserves last heartbeat and durable log records exit reason if possible | next command/scenario entry point starts one replacement daemon |
| client exits while daemon lives | client exit event includes PID/code/reason | daemon may relaunch only when `--auto-restart` policy allows |
| server exits while client lives | server exit event and client connection state are recorded | explicit server relaunch/replacement policy only |
| script changes while daemon lives | daemon/script identity becomes stale or controlled restart begins | next command entry point performs one daemon replacement |
| build succeeds while client lives | new artifact is published; current client remains on old verified build | explicit launch request selects new artifact |
| build fails | old artifact remains identifiable but stale for new source | no launch of the stale artifact as current |
| control socket is absent | `--status` reports `NO_RUNNING_DAEMON`; a work request waits for daemon readiness | `--command`, `--scenario-file`, or `--ensure-running` starts one daemon |
| second daemon requested | request is rejected with existing PID/status/control details | use the existing daemon |
| client and server build IDs differ | launch/rejoin is rejected or explicitly reconciled | controlled replacement of the mismatched owner |

The system must not “fix” these cases by killing every `mimita.exe`, every
`python.exe`, or every server process. Ownership must be proven from the
checkout, run ID, executable identity, command line, and recorded PID.

### 19.7 Investigation note: daemon availability and one-command room launch

The user reports that the development loop sometimes disappears while the
game executable or server remains in the background. They also report that
leaving a room can make it unavailable, and that running `ss` can report a
missing room identity or require leaving/joining more than once. These are
separate ownership failures and must be diagnosed independently.

The current source/spec comparison identifies these concrete gaps:

- `devscripts/dev-loop.py` currently runs as a foreground Python process. Its
  `finally` block closes the control socket and writes stopped status; a later
  `--command` call does not currently restart it. This is the main mismatch
  with the user's desired workflow.
- Current `state()` writes `run_alive` as `server_alive OR client_alive`. It
  does not report whether the Python daemon itself is alive. The status file
  can therefore say the run is alive because a server remains, even when the
  command/control owner is gone. This does not meet section 19.1's availability
  check, which requires a live daemon PID, authenticated control socket, and
  fresh heartbeat.
- `--auto-restart` is a client recovery option; it is not a daemon supervisor.
  Closing the terminal, Ctrl-C, console termination, or an uncaught Python
  exception can end the daemon while leaving the separately owned server.
- `config/command-aliases.json` maps `ss` to
  `server_start_preset sandbox_1`. The desired behavior in networking
  section 67.1 is one idempotent start-or-rejoin action. The helper now
  follows the room-code start path and reuses a live matching local server;
  end-to-end behavior remains unverified.
- At the time of the original investigation, the terminal helper rejected an
  already-running server and queued a localhost direct address, so it could
  neither reuse the room nor follow the GUI registration handshake. On
  2026-10-16, `launchServerPresetAndJoin()` was changed to queue the registered
  room-code path for a newly launched, reused, or in-process preset server.
  It compares an existing external server's live process and basic preset
  settings before reuse. This source change still needs a live leave-once,
  `ss`-once acceptance run.
- The historical report “room identity not found” was not found as a literal
  in the inspected network/terminal owners. The exact user-facing text and
  journal event still need capture during a fresh reproduction before
  assigning that separate failure diagnosis.
- `src/network/multiplayer-tick.cpp` intentionally rejects a WELCOME packet
  whose run ID or build ID differs from the client's expected identity. This
  guard protects against joining the wrong server and must not be removed to
  make `ss` appear to work. The launch/rejoin owner must instead resolve the
  old connection/server and start or join the intended matching instance.
- The GUI room-code path polls the server's room-file result and sets
  `gPendingConnect.roomCode`. The terminal preset helper now uses this same
  path instead of queuing a gameplay `directAddress`. `server_leave` still
  disconnects only the client (`src/terminal/network-commands.cpp`, around
  lines 516-523); the helper now reuses a live matching preset server and its
  published room code. If the connection is still active, the command says
  the player is already in a game and leaves it unchanged.
- The current `npc-navigation` profile binds its dev-loop-launched server to
  `127.0.0.1:1357` (`devscripts/dev-profiles/npc-navigation.json`), and the
  general bind preflight treats loopback as the normal local-test policy
  (`devscripts/dev-loop.py`, `build_server_args()` and
  `validate_firewall_policy()`). This is suitable only for a local-only test;
  it does not satisfy the required `ss` online-room scenario. The dev-loop
  control socket's separate loopback address is local AI-to-script IPC and is
  not the gameplay connection.
- `src/terminal/network-commands.cpp` constructs preset settings from the JSON
  preset then delegates to the helper. The intended contract is to apply those
  same map/mode/player/NPC settings using the same registered server startup
  and room join flow as the GUI buttons. Keep the command as a shortcut for
  those setup actions, not a second local-only server implementation. Player
  gameplay actions must still originate from the client and reach the
  authoritative server over its normal packets.

Read-only evidence from 2026-10-16: `.dev/state.json` reported build 2429 as
the latest current artifact, an adopted server PID 33140 still running build
2427, `builds_match=false`, and no client PID. The control port and token were
present, but this snapshot did not contain a daemon PID, daemon heartbeat, or
daemon source hash. This establishes a stale server/build mismatch and
insufficient daemon-liveness reporting for that snapshot. It does not prove
that this exact mismatch caused the user's earlier “room identity not found”
message; that message and the leave/second-leave sequence still need a fresh
runtime journal from reproduction.

### Launch incident: build 2430 did not reach the client launch step

Evidence supplied and inspected for run `20261016_074515`:

- `--versioninfo` succeeded for `.dev/builds/2430/mimita.exe`; this proves the
  selected executable could run the bounded identity probe, not that its
  server mode started successfully.
- The dev-loop journal recorded `dev-loop.launch.args` for server PID `10820`,
  build path `.dev/builds/2430/mimita.exe`, and bind `127.0.0.1:1357`.
- The journal then recorded phase `server_logger_missing`. It contains no
  matching `logger.started` event for process role `server` and PID `10820`.
- `DevLoop.launch_latest()` waits up to 20 seconds for exactly that event. If
  it does not find it, it saves `server_logger_missing`, stops that owned
  server, and returns before constructing or launching the client.
- The saved state showed `server_ready=false`, no client PID, and
  `build_status=server_logger_missing`. A later process check found PID `10820`
  no longer running. The client was therefore never launched; `[1]` did run
  its launch path, but it stopped at server readiness.

The exact reason the server did not write `logger.started` is not proven. The
journal has no server startup error or server exit code. The launcher does not
capture the child's stdout/stderr into a durable diagnostic record, so the
first startup error may have existed only in transient console output. `--versioninfo`
working does not rule out a failure specific to `--server`, logger setup, a
port conflict, or another startup dependency; these remain possibilities, not
findings.

### Launch incident: build 2431 did not reach the client launch step

Evidence supplied by the user for run `20261016_075225`:

- The selected executable's `--versioninfo` succeeded for build 2431.
- The journal recorded `dev-loop.launch.windows` before server launch,
  `dev-loop.launch.args` for the server, and then phase
  `server_logger_missing`.
- The journal had no matching server `logger.started` event. The dev loop
  stopped that server child and returned before the client creation code.
- The console later showed `RUN: NOT RUNNING`, no server/client PIDs, and no
  events path for the run. Therefore the game window was not launched by this
  attempt: the server readiness gate prevented it.
- Source review confirms `StructuredLogger::init()` is the first operation in
  `runServer()` that should emit `logger.started`. The pasted journal does not
  contain the child's exit code or a startup error, so the cause before that
  logger call remains unknown. Do not label this a port conflict or logger
  failure without the next run's process exit result.

The readiness wait now checks whether the child exits while it waits. On
failure, the dev loop records `dev-loop.server.startup_failed` with the exact
PID, executable, exit code when available, journal path, and whether it exited
before the deadline or stayed alive without announcing readiness. This makes
the next reproduction distinguish an early process exit from a live startup
stall. No new live run has yet validated this diagnostic.

There is also a status-ordering gap: on this failure branch the loop writes
`state.json` before calling `stop_server()`. `stop_server()` then clears the
server PID/readiness in memory but does not write another state snapshot. The
saved state can consequently continue to say the server is alive after the
loop stopped it. Also, `run_alive` currently means server alive OR client
alive, so it does not by itself mean “the client launched and is playable.”
The next status update should be written after cleanup and should show daemon,
server, and client liveness separately.

For this failure class, diagnostics should preserve the first server startup
failure or exit result, the matching child PID/role, and the selected build
identity; cleanup should then be reflected in a fresh status snapshot. Do not
report `RUNNING` until the server is ready, the room code is registered, and
the client has launched and passed its readiness/identity checks.

The desired simple behavior is:

```text
AI calls dev-loop.py --command "ss"
  -> entry point makes sure one current dev-loop daemon is ready
  -> client receives the alias once
  -> old connection is either confirmed closed or safely reused
  -> one sandbox server is confirmed alive with the expected room identity
  -> client joins that server once
  -> command result says joined, or gives the first clear failure reason
```

In everyday terms:

| Word | Simple meaning |
|---|---|
| server | The running game process that owns the match. It can outlive the visible client. |
| room code | The short code or address used to find/join that server. |
| room identity | Proof that the server found is the intended server/run/build, not an older one on the same computer. |
| leave | Disconnect this player from the match. It does not necessarily shut down the server. |
| `ss` | Start or enter the sandbox preset in one action. It should not require menu steps or repeated leave/join commands. |

The original mismatch was that `leave` kept the server running while the old
`ss` implementation refused to reuse it. The helper now checks for a live
matching preset server and queues its room code. This is source-level
alignment only; the full leave-once, `ss`-once flow still needs runtime proof.
Target behavior for `ss`:

1. If connected to any game as host or client, do nothing and print
   `You're already in a game.` (or an equally clear message).
2. If disconnected, reuse the healthy, registered sandbox server left running
   from before. If none exists, start one and wait until it is ready and
   registered.
3. Start it through the same server setup and room-registration path as the
   GUI, apply the selected preset's settings, and join exactly once as a client
   using the room code, coordinator, and ICE path. Do not substitute a hard-
   coded `127.0.0.1` gameplay connection or loopback-only server bind. Localhost
   can be used for a clearly separate same-computer packet test, but that test
   does not prove another computer can join.
4. Leaving disconnects the player only; the server stays up and joinable.
5. Report success only once connected. If the previous disconnect is still
   finishing, wait or report that state; do not require another leave.

This rule is detailed in networking section 67.1. The same flow is safe to
repeat: while connected it reports “already in a game”; after leaving it
returns the player to the existing sandbox server, or starts one only if the
old one is no longer usable.

“Room identity” here means the identity the client uses to tell which server
it reached, including its run/build identity and room code. “Room identity not
found” is the user's report, not a confirmed event string from this session.
It could mean the server never published a room code, the room lookup was
stale, the client reached a different server, or the client still had old
connection state. Only a fresh journal can distinguish these.

Required follow-up evidence before implementation is accepted:

1. Make every AI work call (`--command`, `--scenario-file`) ensure the same
   background `dev-loop.py` daemon is ready, starting it only when absent;
   keep `--status` read-only. Record daemon PID, heartbeat, source hash, and
   last exit reason. Do not require a separate supervisor program.
2. Reproduce `ss` with a fresh matching build and trace alias expansion,
   connection state, preset launch decision, room-code publication, identity
   validation, and first successful gameplay state in the canonical journal.
3. Reproduce leave once, then immediately issue `ss` once. Record whether the
   disconnect completed, whether the room remained joinable, and the first
   room/identity lookup result. The operation must be idempotent: one command
   gets the player into one sandbox session without extra leave commands or
   GUI steps.
4. Preserve run/build identity rejection. Fix stale ownership or launch
   reconciliation at the owner that produced the mismatch; do not bypass the
   identity guard or kill an unverified process.

Status: investigation recorded; no runtime reproduction or lifecycle change
was made in this documentation update.

### 19.8 Control socket, in plain language

The dev-loop is a Python program that stays open to watch files, build the
executable, keep status, and send requested game-terminal commands. A later AI
call starts a short-lived copy of `dev-loop.py`; that copy needs a way to ask
the already-running copy to do work. The control socket is that private
same-computer message line:

```text
AI runs dev-loop.py --command "ss"
  -> short-lived Python command sender
  -> local control socket
  -> persistent Python dev-loop
  -> client command queue
  -> MiMITA client runs "ss"
  -> client joins or sends gameplay requests to the authoritative server
```

The socket currently binds to `127.0.0.1` on a local port and checks a secret
token. That address is only for Python-to-Python control on this computer; it
is not a player connection and is not sent to the game server. The server
connection must follow the networking spec's registered room-code/ICE path.

The control socket does not run the game command itself. The Python loop puts
the command in the client queue; the client executes it after its readiness
gates. The game client then uses its regular networking path for server-owned
actions. This separation lets AI request a command without typing into the
visible game window.

Implemented (2026-10-17): `--command`/`--cmd` and `--scenario-file` now first
ensure the watchdog and daemon exist, start them if absent, wait for the
authenticated control endpoint, then send the request exactly once and (for
`--command`) wait for the correlated client result. This is still one script and
one persistent daemon, not a second supervisor app. `--status` stays read-only
and never starts the daemon. The visible dashboard is the default entry for the
desktop shortcut; AI command calls never open a dashboard.

## 20. Required implementation phases

The phases below are ordered implementation gates. A later phase must not be
called complete merely because its command-line spelling exists; it is
complete only when its runtime evidence and failure behavior are verified.

1. Add terminal received/resolved/unknown/completed events and propagate a
   request ID through startup commands. First reproduce the existing failure
   with a fresh build and prove the first missing event.
2. Add `command` and `scenario-file` control requests to `dev-loop.py`, with
   readiness/build/run identity checks and a bounded response protocol.
3. Add the client-side request queue. Execute commands on the client main
   thread after the world-load gate, then emit the terminal and handler events.
4. Add heartbeat, uncaught-exception, child-exit, and explicit lifecycle
   evidence. Verify that auto-restart applies only to the client and that the
   daemon remains available after a client crash.
5. Add the scenario JSON runner and avatar assertions. Run Jason/default,
   legacy authored, Boomer canonical, Stabber canonical, and a non-player GLB
   control using fresh matching builds.
6. Add the self-starting CLI/background-daemon contract from section 19.
   Verify that repeated command calls reuse one daemon, a missing/stale daemon
   starts exactly once, script updates replace it without duplicating queued
   commands, durable logs explain exits, and a healthy game pair is preserved.

Each phase requires source evidence, build evidence, a fresh executable and
journal, and separate human visual acceptance where appearance is part of the
claim. A build or a visible command line alone is insufficient.

### 20.1 Current implementation status rule

At the time this specification was expanded, phases 1–5 had partial or
working slices in the repository: the command/control path, queue delivery,
terminal events, avatar snapshots, and the repository scenario exist. They
must still be validated per run and must not be treated as proof that phase 4
or phase 6 is complete.

The following remain distinct open requirements unless a fresh runtime record
proves them:

- the Python daemon does not hot-reload its own source (the watchdog replaces
  it only on a source-hash change, per section 19.4a);
- a visible dev-loop dashboard is not implied by client visibility; both are
  independent;
- a command response is not a gameplay assertion;
- a published build is not the same as the running client build;
- `scenario.finished` is not a pass unless assertions were actually evaluated
  and reported with expected/actual values.

Implemented and runtime-verified on 2026-10-17 (see sections 19.4, 19.8, 22.1):

- work commands (`--command`/`--cmd`) and `--scenario-file` now ensure one
  watchdog and daemon, start them if absent, wait for the authenticated control
  endpoint, and send the request exactly once;
- repeated entry-point calls reuse the same watchdog/daemon PIDs (no
  duplicates);
- a stopped daemon is recovered once by the watchdog and the next command
  returns a reasoned result instead of a stale success;
- the visible dashboard render, freeze/resume, and close-without-stopping the
  daemon were exercised. The Windows Terminal relaunch path and the human visual
  reading of the live window remain unverified in this environment.

## 21. Proposed AI test workflow

```text
1. python devscripts/dev-loop.py --profile npc-navigation --visible
2. python devscripts/dev-loop.py --status --json
3. python devscripts/dev-loop.py --command "avatar.load monstertest1"
4. python devscripts/dev-loop.py --command "agent.aim ..."
5. python devscripts/dev-loop.py --command "agent.move 1 0 120"
6. python devscripts/dev-loop.py --command "agent.snapshot after_walk"
7. wait for request-specific completion/assertion events
8. inspect the exact journal from status, then perform visual review if needed
```

Every work call is the supported AI entry point. It must reuse one healthy
daemon or start one if absent, then send the request exactly once:

```text
python devscripts/dev-loop.py --status --json
python devscripts/dev-loop.py --command "agent.move 1 0 120"
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
```

`--status` is read-only and may report `NO_RUNNING_DAEMON`; the next work call
must recover it. The user or AI should not need to leave a separate Python
window open just to keep the command interface available.

The desired end state is that an AI can issue one command or one
repository-local scenario through `dev-loop.py`, keep one identified
executable pair alive, survive a controlled Python-script update, and receive
a bounded pass/fail result from the same `events.jsonl` file.

## 22. Command reference

### Status display and launch-time feedback

The interactive status display should update in place when the terminal
supports cursor control. In plain logs or terminals without that support, it
must not append an unchanged full status block repeatedly. Emit a new status
only when a displayed field changes, or at a bounded low-frequency heartbeat;
keep the detailed status snapshot available through `--status --json`.

The display should distinguish daemon, build, server, client, and journal
identity, and it must not combine stale uptime or visibility values with a
`NOT RUNNING` state. Launch feedback should name the current stage (build,
server readiness, client launch, map readiness, join readiness) and its elapsed
time so a slow launch can be attributed to a stage. Do not promise a launch
duration until a baseline has been measured. The 2026-10-16 screenshot shows
repeated MODE/BUILD blocks followed by `RUN: NOT RUNNING` with old uptime,
empty PIDs, and no journal; this is a report of confusing output, not proof of
which state transition caused it.

This section lists the command-line and keyboard controls currently present
in `devscripts/dev-loop.py`. It does not list every command accepted by the
game's terminal; `--command` forwards one of those game commands to a running
client. The `ss` alias is a game-terminal alias defined in
`config/command-aliases.json`, not a dev-loop option.

### 22.1 Command-line options currently in the script

| Option | What it does | Current behavior / caution |
|---|---|---|
| `--help` | Shows the script's available options. | Supplied by Python's argument parser. |
| `--profile NAME` | Selects a profile from `devscripts/dev-profiles/`. | Default is `npc-navigation`; used when starting the loop. |
| `--auto-restart` | Enables recovery of the game client after it exits. | Does not restart the Python dev-loop daemon. |
| `--launch-mode NUMBER` | Selects a numbered server/client launch configuration. | Modes are in `devscripts/dev-launch-modes.json`. |
| `--headless` | Hides the game client window on its next launch. | Affects only the game window; the visible dashboard is independent (section 19.4). |
| `--visible` | Shows the game client window on its next launch. | Does not guarantee a separate daemon window or taskbar item. |
| `--status` | Prints the last saved status. | Read-only; it never starts or wakes the daemon. |
| `--json` | Prints status or control response as JSON. | Useful with `--status`, `--request`, `--command`, or `--scenario-file`. |
| `--command "GAME COMMAND"` / `--cmd "GAME COMMAND"` | Sends one game-terminal command to the daemon, waits for the correlated result, prints it, and exits. | Ensures one watchdog/daemon first; never opens a dashboard; sends the command once. `--cmd` is an alias. |
| `--scenario-file PATH` | Runs a repository-local Markdown/JSON scenario and returns its verdict. | Ensures one watchdog/daemon first; opens no dashboard. |
| `--ensure-running` | Ensures one watchdog and one daemon exist, prints status, and exits. | Idempotent; never starts a duplicate. |
| `--request launch` | Requests a build if needed, then launches the selected mode. | May include `--launch-mode NUMBER`. |
| `--request stop-client` | Stops the dev-loop-owned client. | Leaves a healthy dedicated server running. |
| `--request stop-server` | Requests stop of the identified dev-loop server. | Refuses to stop an unverified process. |
| `--request stop` | Requests shutdown of the Python dev-loop daemon. | The server has its own lifetime and may remain running. |
| `--dashboard` (default with no control option) | Opens/attaches the visible dashboard view. | This is the desktop-shortcut path; it ensures the watchdog/daemon then renders status. |
| `--daemon` | Runs the build/game/control daemon in this process. | Used by the watchdog and the entry point; open by the shortcut only via the dashboard. |
| `--dev-loop-role {watchdog,daemon,dashboard}` | Records a stable role marker in the process command line. | Windows Task Manager still shows `python.exe` as the image name; the Command Line column carries the role. |
| `--no-terminal-relaunch` | Renders the dashboard in the current console. | Suppresses the Windows Terminal preference. |
| `--dashboard-frames N` | Renders `N` dashboard frames then exits. | Bounded self-test aid; `0` runs until `Q`. |
| `--command-timeout S` | Seconds to wait for a command result. | Default 15. |

Examples:

```text
python devscripts/dev-loop.py --status --json
python devscripts/dev-loop.py --request launch --launch-mode 1
python devscripts/dev-loop.py --command "agent.snapshot start"
python devscripts/dev-loop.py --cmd "agent.move 1 0 120"
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
python devscripts/dev-loop.py --ensure-running
python devscripts/dev-loop.py --request stop-client
```

`--command`/`--cmd` and `--scenario-file` ensure the watchdog and daemon are
alive, start one if absent, wait for the authenticated control endpoint and
readiness, then forward the request exactly once. `--status` stays read-only.

### 22.2 Interactive controls shown by the running loop

| Key | What it does |
|---|---|
| `1` | Build if the latest artifact is stale/missing; otherwise launch the latest build. |
| `2` | Open the launch-mode picker; type a mode number, press Enter, or press `Q` to cancel. |
| `V` | Toggle the game client's visible/headless setting for the next launch. |
| `A` | Toggle client auto-restart. This does not keep the Python daemon alive. |
| `Q` | Shut down the dev-loop daemon through its normal shutdown path; the dedicated server may remain alive. |

A detached daemon (started by the watchdog or the entry point) has no
interactive console, so these build/launch keys are only live when the daemon
itself owns a console. The visible dashboard is a separate view with its own
controls:

| Key | What it does |
|---|---|
| `P` | Pause/freeze the dashboard view so the text can be selected/copied; the watchdog, daemon, game, and scenarios keep running. |
| `R` | Resume the frozen view, refresh from current state, and mark when the view was refreshed. |
| `Q` | Quit the dashboard only; the watchdog, daemon, client, and server keep running. |

### 22.3 Where game-terminal commands and aliases live

The game-terminal command surface is registered by C++ command owners and
documented by the owning feature specs. `help` and `aliases` in the game show
the commands and aliases loaded by that executable. Alias definitions are
stored in `config/command-aliases.json`. For example, `ss` expands to
`server_start_preset sandbox_1`; the alias itself does not prove that the
server started or that the client joined. See networking section 67.1 for the
required start/rejoin behavior and this document's section 19.7 for the
AI/dev-loop issue and evidence plan.

### Startup scan follow-up: oversized logs can delay server readiness

A new dev-loop launch was attempted on 2026-10-16 using published build 2436,
run `20261016_075833`. The server child PID 33560 remained alive during the
20-second readiness wait, but the shared journal received no server
`logger.started` event. At the deadline, the loop stopped that child and did
not launch a client. The journal records the child launch, the readiness
failure window snapshot, and `dev-loop.process.stop_result`; it contains no
server startup record. The client was never attempted.

Source tracing found that `StructuredLogger::init()` holds the cross-process
log mutex while it reconciles the log-tree byte total, calls pruning (which
scans the tree again), and reconciles a second time. The server shares the
same events path and quota mapping as the preflight process. With the previously
measured multi-gigabyte log tree, these redundant full-tree walks are a credible
cause of the 20-second stall, but the run had no stack trace or stage timers,
so this remains a likely cause rather than a proven one.

The startup path now reconciles the tree once, then reuses that exact
mutex-protected total for pruning and updates it by the bytes pruned. It removes
the extra full-tree scans before and after pruning while still counting legacy
or externally-created files. The live quota-limit path still reconciles before
pruning when a write would cross the ceiling. Runtime proof that startup now
reaches `logger.started` quickly is still required.

Two dev-loop Python processes were active in the same checkout and alternated
writes to `.dev/state.json` while publishing builds. Their conflicting status
snapshots make build/run identity unreliable. Neither process was terminated
during this investigation. Before the next runtime acceptance, establish one
dev-loop owner; do not launch another loop to work around the conflict.
