// 2026-11-20
/* purpose
* define the shared proof ledger and completion gate for agent work
* require runtime evidence or an explicit, reasoned exemption for behavior changes
* make unproven/stale changes visible to every agent before completion
* keep the game architecture unchanged while maximizing agent development capability
* this file DOES NOT make build success equal runtime proof
* this file DOES NOT grant production, compiler, or deployment authority
*/

# Agent proof and verification

## Intent

Maximize useful agent capability while making proof of behavior the default
completion path.

A change is not complete because it compiles. A behavior change is complete
only when a pinned executable and game generation produce the required event in
the exact run journal, or when the owner records an explicit proof exemption
with a reason.

The agent development system is allowed to evolve. The game architecture is
not the subject of this specification. Gameplay, networking, collision, packet,
and server behavior remain owned by their existing specifications.

## Current boundaries

- One shared checkout and one source tree.
- The root dev-loop is the only builder and run authority.
- Agents may request builds and isolated test runs through the root dev-loop.
- Direct compilers, `build.py`, module workers, and second loops are forbidden.
- One EXE build at a time.
- Candidate game-DLL builds are isolated and bounded by PC resources.
- Test runs pin one EXE artifact and one game-DLL generation/path.
- Source worktrees/overlays are deferred until a separate policy change.
- No production or deployment authority is granted by this document.

## Definitions

**Change** — a source, configuration, or documentation edit associated with a
task, issue, or explicit path set.

**Proof record** — a durable link between a change and the evidence that it
works, or a reasoned exemption.

**Runtime proof** — the exact pinned executable and game generation produced
the required event, field value, and absence of forbidden behavior in the run's
`events.jsonl`.

**Pinned** — the run metadata names the immutable EXE artifact, executable
SHA-256, game-DLL generation/path, run ID, and each process journal.

**Stale proof** — proof existed, but a linked path changed after verification.

**Exemption** — a proof record with a non-empty reason explaining why runtime
proof is impossible or not applicable. Exemptions are visible, not silent.

## Proof states

| State | Meaning |
|---|---|
| `unproven` | changed; no proof or exemption exists |
| `build_only` | the source compiled; runtime behavior is unproven |
| `runtime_proven` | the required journal evidence was observed and verified |
| `stale` | proof existed, but a linked path changed afterward |
| `exempt` | no runtime proof is required; a reason and owner are recorded |
| `human_accepted` | a human accepted visible behavior; separate from runtime proof |

`human_accepted` does not replace `runtime_proven`. A run may be
`runtime_proven` and still await human visual acceptance.

## Completion gate

The proof gate applies to task completion and issue closure.

A task or issue is complete only when at least one of these is true:

1. a task proof has `result=proven`, or an issue proof has
   `proof_kind=integration|unit` and `result=proven`, and its linked paths are
   not stale; or
2. an `exempt` proof/record is linked with a non-empty reason and responsible
   agent.

For behavior work, `exempt` is not a shortcut. Acceptable reasons include:

- the change is documentation-only;
- the change is tooling-only with a focused tooling test instead of a runtime
  event;
- the behavior cannot be exercised in the current environment, and the
  limitation is recorded with the next required test;
- a human explicitly accepted the limitation.

A gate failure must say exactly what is missing and which command can record it.
The gate does not silently downgrade `runtime_proven` to a build result.

The gate is implemented in:

```text
agent-coord task status <task-id> done
agent-coord issue close <issue-id> ...
```

Task completion checks the verified task proof or task exemption. Issue closure
checks proven issue evidence (`integration` or `unit`), or an explicit issue
exemption with a reason.

Administrative issue states (`deferred`, `needs_clarification`, `in_review`)
are not completion. `done`/`cancelled` are.

## Proof record fields

A proof record contains:

- `proof_id`, task ID, issue ID, agent;
- changed paths and a source fingerprint;
- evidence class: `runtime`, `tooling`, `docs`, `config`, or `human`;
- `build_id`, executable SHA-256, game generation/path, run ID;
- journal path(s), required event, field predicate, forbidden event;
- observed timestamp, intent timestamp, edit timestamp, evidence latency;
- status (`unproven`, `build_only`, `runtime_proven`, `stale`, `exempt`,
  `human_accepted`);
- verification state and verifier;
- exemption reason when applicable;
- bounded notes and review links.

A proof is reproducible only when its artifact/generation/run identities and
journal path are present. A journal from a different artifact or unpinned
executable cannot verify the change.

## Commands

```text
agent-coord proof task-add <task> --paths <paths> \
  --build-id <id> --generation <n> --run-id <id> \
  --journal <events.jsonl> --event <event> --field "key=value"

agent-coord proof task-verify <proof-id>
agent-coord proof exempt <task> --paths <paths> \
  --reason "<why runtime proof is not applicable>"
agent-coord issue exempt <issue> --paths <paths> \
  --reason "<why issue-level runtime proof is not applicable>"
agent-coord proof status

agent-coord proof record <issue> --kind integration \
  --generation <n> --runtime <runtime-id> --run-id <run-id> \
  --server-journal <events.jsonl> --client-journal <events.jsonl> \
  --event <event> --dll <dll-path>
agent-coord proof verify <evidence-id>
agent-coord proof list --issue <issue>
```

Task proofs use `result=proven`, `result=unproven`, `result=stale`, or
`result=exempt`. A task exemption creates an exempt proof record and stores the
reason on the task. Issue evidence uses `proof_kind=integration|unit` and
`result=proven|unproven`; issue exemption stores the reason on the issue.
`proof status` reports proven, stale, and exempt counts, and the board includes
unproven/stale proof rows. `runtime_proven` in this document is the user-facing
meaning of task `result=proven` with a runtime evidence class.

`proof task-verify`/`proof verify` reads the journal and checks the required
event. A missing file, missing event, wrong run/artifact identity, or mismatched
path fingerprint leaves the proof unverified or stale.

`agent-coord board` and `--status` show:

- open unproven and stale proofs;
- oldest unproven age;
- owner and last build/run;
- linked task/issue.

The dev-loop status and dashboard show the same summary.

## Staleness

Each proof stores a fingerprint of its linked paths. When a path changes after
verification:

- a runtime proof becomes `stale`;
- the task/issue returns to unproven completion status;
- the board marks it for re-test;
- the previous proof remains in history; it is not deleted or rewritten.

A later run appends a new proof record. Corrections are append-only.

## Evidence classes

| Change | Required proof |
|---|---|
| Gameplay, networking, collision, server behavior | pinned runtime journal with the required event/fields and forbidden-event check |
| Configuration or hot-reload behavior | config apply/load event plus the owning runtime event |
| Build/test/dev-loop tooling | focused unit/tooling test and one real invocation of the changed path |
| Documentation | diff/doc check and links; no fabricated gameplay event |
| Human-visible feel/visual | runtime proof plus separate human acceptance |

A build-only result can be recorded as `build_only`; it cannot satisfy the
completion gate unless the task is explicitly exempted with a reason.

## Agent build/test capability

The long-term direction is maximum safe agent capability:

- any registered agent may request an EXE build, candidate game-DLL build,
  cold-link trial, or isolated pinned test run;
- requests record agent, task, issue, permit, build, run, and wait reason;
- candidate DLL builds are accepted as often as needed and run concurrently
  only within configured CPU/memory limits;
- candidate output is isolated and never overwrites the normal published
  `build/game/current.json` or `ready.json`;
- pinned test runs can start while another build is pending;
- the normal automatic build path remains unchanged;
- direct compiler access and a second dev-loop remain forbidden.

### Feature flags

New high-risk paths are implemented behind flags until live-proven:

```text
agent_candidate_builds       off -> on after two isolated candidates are proven
agent_warm_runtime           off -> on after one hot candidate activation is proven
build_friction_auto_issue    notify -> create after the signal is tuned
proof_gate                   warn -> block after the proof path is stable
```

The gate in this document is the target behavior. If a temporary
`proof_gate=warn` rollout is used, the unproven board remains mandatory and the
reason for warning must be recorded. The default after rollout is block.

Once proven, candidate builds and warm runtime are enabled by default so agents
have maximum capability. Human approval remains required for changes to shared
authority, production, or the game specification.

## Build-friction and improvement

Every wait, cold/EXE-required change, slow phase, or long change-to-evidence
path is a potential `build-friction` issue.

- Record every queue wait and wait reason.
- Auto-create a `needs_triage` issue when a wait is avoidable or above the
  measured budget.
- Agents may also file any wait they consider unnecessary.
- Agents review, reproduce, and gather evidence for or against fixes.
- Humans make the final decision on shared-authority changes.
- Agents may propose capability and coordination improvements without waiting
  for the user to ask.

## Change-to-evidence metric

Measure from intent/idea and from edit-save to the observed event:

```text
intent -> edit saved -> build requested -> queue wait -> worker start
       -> compiled/linked -> published/activated -> event observed -> stopped
```

Record per class, agent, build, game generation, and run. Report median, p95,
and outliers. Use measured per-class budgets. Tighten budgets as baselines
improve. No zero-second promise. A stale or unproven change is a first-class
metric and issue.

## Acceptance

- A behavior task cannot be marked done without verified runtime proof or a
  reasoned exemption.
- The unproven board shows every changed behavior path without current proof.
- A source change after proof marks the proof stale.
- A proof names the exact artifact, SHA-256, game generation, run ID, journal,
  event, and time.
- A build-only result cannot masquerade as runtime proof.
- Two agents can build and test independently; one EXE build serializes and
  candidate builds remain isolated and bounded.
- A pinned run can start while another agent's build is pending.
- The normal shared runtime is not replaced by an agent test.
- The game architecture boundary and source-isolation deferral remain explicit.
- A human can inspect the spec and the board and understand what is proven,
  stale, exempt, or still unproven.
