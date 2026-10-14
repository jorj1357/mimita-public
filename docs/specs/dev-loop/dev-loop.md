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
owner applied it, and keep the dev-loop supervisor alive long enough to collect
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

These names are the v1 command contract. The current implementation provides
`agent.move`, `agent.aim`, `agent.action`, `agent.shoot`, and
`agent.snapshot`; the scenario queue/file and assertion runner are the next
slice and must not be described as implemented until their events are present
in a live journal. Existing commands remain valid. The adapters must call the
same action/input functions used by normal gameplay;
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

`--startup-command` remains useful for setup such as `teleport`, avatar
selection, and `equipslot`. A bounded scenario begins only after the client is
connected and the world is loaded. The scenario state machine then owns a
single queue of timestamped steps and emits `scenario.started`,
`scenario.step`, `scenario.assert`, and `scenario.finished` records. A second
client or competing dev loop is not required for a local transform test.

### Scenario file

The planned scenario runner will accept a repository-local JSON scenario file
through the terminal command `agent.scenario_file <path>`. The current
repository scenario document is
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

For the current slice, submit the individual commands directly through the
client terminal after the startup readiness gate. Arguments are parsed and
validated by the client command owner. The future scenario file is not a
second gameplay configuration system and is never loaded by maps, servers, or
ordinary players. It is a bounded development/test input.

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

Python may launch/reuse the dev loop, submit a bounded command sequence through
the documented terminal/control path, wait for `scenario.finished`, and parse
`events.jsonl`. Python must not manipulate the game window to synthesize
keyboard/mouse events for this contract, and must not declare success from
process liveness or build status alone.

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

The loop has two different lifetimes that must not be confused:

1. The Python daemon owns the watcher, control socket, status file, build
   worker, and dev-loop-launched client.
2. The dedicated server has an independent lifetime and may remain after the
   client or Python daemon stops.

The current implementation enters its shutdown path when the daemon receives
`Q`, Ctrl-C/console termination, an unrecovered exception, or an external
process close. Its `finally` block stops daemon-owned clients, closes the
control socket, writes a stopped status, and leaves the persistent server.
`--auto-restart` can relaunch a dead client only while the daemon itself is
still alive; it cannot revive a closed Python process. `--headless` hides the
game client and consoles; it does not make the Python daemon persistent.

The required fix is a supervisor contract, not an unconditional infinite
loop:

- The daemon writes a heartbeat and last-exit reason to the status snapshot.
- Every uncaught exception is written to the run journal before shutdown.
- A client exit records exit code, signal/termination reason, and whether the
  daemon attempted recovery.
- The status command distinguishes `daemon_alive`, `client_alive`, and
  `server_alive`.
- A control request can explicitly stop the client, server, or whole daemon;
  normal client exit must not silently stop the server.
- AI tests launch the daemon in a host-managed process/session whose stdout
  and stderr are redirected to a durable log. Closing a visible terminal
  window must not be the mechanism used to keep the test alive.
- The loop refuses to start a competing daemon and reports the existing PID,
  status path, control port, journal path, and recovery instructions.

The implementation must not claim “running all the time” if the daemon is
gone. The status path and heartbeat are the authority, and a fresh daemon
must be started only through the controlled lifecycle path when the old one is
actually absent.

### 19.1 The exact process topology

The implementation and every future investigation must treat these as three
different processes with three different lifetimes:

```text
host supervisor (future required layer)
  -> Python dev-loop daemon
       -> dev-loop-launched MiMITA client
       -> dev-loop-launched dedicated server
```

The responsibilities are deliberately separated:

| Process | Owns | May restart | Must not silently restart or kill |
|---|---|---|---|
| host supervisor | keeping the Python daemon available, durable stdout/stderr, daemon PID and restart policy | the Python daemon after a verified unexpected exit | unrelated Python processes, unrelated game/server processes |
| Python dev-loop daemon | file watcher, build worker, control socket, status, command queue, owned client lifecycle | its owned client when `--auto-restart` is enabled; a client after an explicit launch request | the daemon itself after it has already exited; an unverified server |
| MiMITA client | gameplay, terminal registry, fixed-tick input, animation, snapshots | only through the daemon's controlled launch/recovery path | the Python daemon or dedicated server |
| dedicated server | authoritative server state and server tick | only through explicit server replacement/stop policy | merely because the client or daemon exits |

An implementation must never use “the EXE is open” as proof that the dev loop
is available. The minimum availability check is:

1. the recorded daemon PID is alive;
2. the recorded control socket accepts the current token;
3. the heartbeat is newer than the configured heartbeat timeout;
4. the status file identifies the current run and journal;
5. the client and server identities are separately reported.

If the daemon is absent, commands must fail clearly with
`NO_RUNNING_DAEMON`. They must not silently start a second daemon as a side
effect of `--command` or `--scenario-file`.

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
7. A host supervisor starts exactly one replacement daemon using the new
   script identity.
8. The replacement daemon obtains the checkout lock, creates a new control
   token, opens or adopts the documented current run, and writes a new
   heartbeat.
9. The replacement daemon reports whether the existing client/server pair was
   preserved, revalidated, or intentionally relaunched. A Python-script
   restart must not automatically replace a healthy game pair unless the
   launch policy explicitly requires it.

The simpler allowed implementation is for the host supervisor to stop and
restart the daemon after detecting a script change. In either implementation,
the important contract is that the old interpreter cannot continue presenting
itself as the current daemon after its source identity changed.

### 19.4 Visible supervisor versus visible game

`launch_visibility=visible` applies to the MiMITA client window. It does not
promise a separate dev-loop console window. The current client/server launch
policy also uses `CREATE_NO_WINDOW` for child consoles, so a user may see the
game while seeing no separate dev-loop item in the Windows taskbar.

If humans must watch the AI commands, the future implementation must define a
separate supervisor visibility policy:

```text
supervisor_visibility = visible | hidden
client_visibility     = visible | headless
server_console        = hidden | visible
```

For `supervisor_visibility=visible`:

- launch the daemon in its own durable Windows console/process group;
- keep stdout and stderr attached to a durable log as well as the console;
- give the console a stable title containing the checkout and run ID;
- do not use closing the console as the normal way to stop the daemon;
- show the control port, daemon PID, client PID, server PID, build identity,
  and journal path in the status display.

For `supervisor_visibility=hidden`, the same records must still exist in the
durable log and status file. Hidden operation is not allowed to reduce the
available evidence.

The game being visible proves only that the client window exists. It does not
prove that the Python command socket is alive or that the AI command reached
the gameplay owner.

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
| daemon exits unexpectedly | heartbeat stops; final/host log records exit reason if possible | host supervisor starts one replacement daemon |
| client exits while daemon lives | client exit event includes PID/code/reason | daemon may relaunch only when `--auto-restart` policy allows |
| server exits while client lives | server exit event and client connection state are recorded | explicit server relaunch/replacement policy only |
| script changes while daemon lives | daemon/script identity becomes stale or controlled restart begins | host supervisor performs one daemon replacement |
| build succeeds while client lives | new artifact is published; current client remains on old verified build | explicit launch request selects new artifact |
| build fails | old artifact remains identifiable but stale for new source | no launch of the stale artifact as current |
| control socket is absent | command exits with `NO_RUNNING_DAEMON` | start/recover daemon through lifecycle entry point |
| second daemon requested | request is rejected with existing PID/status/control details | use the existing daemon |
| client and server build IDs differ | launch/rejoin is rejected or explicitly reconciled | controlled replacement of the mismatched owner |

The system must not “fix” these cases by killing every `mimita.exe`, every
`python.exe`, or every server process. Ownership must be proven from the
checkout, run ID, executable identity, command line, and recorded PID.

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
6. Add the host supervisor and controlled daemon replacement contract from
   section 19.3. Verify script-change restart, one-daemon locking, durable
   console/logging, queue handoff, and preservation of a healthy game pair.

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

- the Python daemon does not hot-reload its own source;
- an outer host supervisor does not yet guarantee daemon resurrection;
- a separate visible supervisor console is not implied by client visibility;
- a command response is not a gameplay assertion;
- a published build is not the same as the running client build;
- `scenario.finished` is not a pass unless assertions were actually evaluated
  and reported with expected/actual values.

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

The supported AI entry point is now the existing daemon control path when the
status check proves that one matching daemon is alive:

```text
python devscripts/dev-loop.py --status --json
python devscripts/dev-loop.py --command "agent.move 1 0 120"
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
```

If status does not prove daemon liveness, the command must stop with a clear
daemon-lifecycle error. It must not create a competing daemon. Until the host
supervisor and controlled daemon replacement requirements are implemented, a
human or task runner must start the daemon through the documented lifecycle
entry point and keep that owner process alive.

The desired end state is that an AI can issue one command or one
repository-local scenario through `dev-loop.py`, keep one identified
executable pair alive, survive a controlled Python-script update, and receive
a bounded pass/fail result from the same `events.jsonl` file.
