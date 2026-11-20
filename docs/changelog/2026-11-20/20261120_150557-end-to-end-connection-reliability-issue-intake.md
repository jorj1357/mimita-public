// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# End-to-End Connection Reliability Issue Intake

# Task

- Task ID: session-eb7bcc326184
- Session ID: session-eb7bcc326184
- Owner: root-connection-reliability
- Focus: issue intake, live-run evidence, cross-agent coordination
- Summary: Linked the user's connection, restart, HUD status, Payload NPC visual and AI evidence request to the canonical end-to-end consistency issue; recorded run evidence and staged owned proof tasks. The reliability goal remains active pending runtime reproduction and fixes.
- Status: in-progress
- Date, time, timezone: 2026-11-20T15:05:56+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 6d92afa35b434790e1b42ebb9667be69a216cde2
- Final commit: 6d92afa35b434790e1b42ebb9667be69a216cde2

# Pre-existing changes

- Exact status output: The automatic evidence below records the shared checkout state on branch checkpoint/one-checkout-condensation-20261111 at commit 6d92afa35b434790e1b42ebb9667be69a216cde2. The checkout already contained concurrent edits across dev-loop tooling, source, physics, issue records, and changelogs. This session did not edit source, config, build, or runtime files.
- Files not created or modified by this session: All source and tool paths in the captured status belong to pre-existing or concurrent work. The umbrella issue file was created by the active consistency investigator and updated here; the separate issue-55 file is an existing report updated here. The generated docs/issues/README.md and other issue records include changes from multiple agents. Do not run generated reverse commands against unrelated files. A separate architecture-consistency changelog and duplicate issue record were created by that agent.

# Requested behavior

Preserve the current report verbatim, route it to one high-priority umbrella issue, and track the reported Payload NPC/cart visual regression, TDM same-executable connection timeout or missing join, misleading Disconnected status, and the desired start/join/leave/reconnect/close/reopen lifecycle. The issue must define concrete visual and journal evidence, record current findings without claiming a root cause, and coordinate active agents so the same tests are not duplicated.

# Requested behavior

# Specification alignment

- Current specification paths: docs/workflows/issue-intake-and-triage.md; docs/specs/networking/networking.md; docs/specs/networking/network-event-contract.md; docs/specs/gamemodes/teamdeathmatch.md; docs/specs/dev-loop/agent-proof-and-verification.md; docs/specs/dev-loop/agent-self-improvement.md; docs/specs/gui/guiv2.md.
- Exact requirements: Preserve the report verbatim; complete reproduction, expected behavior, acceptance, paths, evidence, risk, and proof expectation; correlate connection stages in the canonical journals; a build does not prove runtime behavior; visual acceptance remains separate from log evidence.
- Why the change follows the specification: The report is stored once as high-priority context ctx-a05309c9f2cd and linked primarily to issue-cf9cd77eb056, with the TDM/crash and delayed-snapshot issues retained as related work. The issue defines pinned live proof and screenshot evidence. The 99.999% target remains aspirational until supported by sufficient measurements.
- Conflicts or decisions: No gameplay specification or code changed. The exact reported TDM map spelling duest2cyberiav4 is preserved in the context; the reproduction task uses the current configured map id dust2cyberiav4. The root cause and Payload visual symptom remain unverified.

# Exact implementation changes

## File: docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md

- Lines/functions/headings: Issue report, Context, Triage, and Tasks sections; issue ID issue-cf9cd77eb056.
- Old content or behavior: The high-level end-to-end launch, reconnect, Payload grounding, dashboard, and AI evidence issue already existed from the related intake.
- New content or behavior: Appended this exact user report; linked ctx-a05309c9f2cd as the primary context; added the current run trace and explicit limitations; expanded the lifecycle smoke acceptance; triaged the issue high and added owned trace, visual-proof, and reliability-matrix tasks.
- Reason: Make this broader user goal the canonical umbrella while keeping specific connection bugs and the delayed-snapshot investigation linked.
- Why unrelated behavior is preserved: No implementation code or configuration changed; existing user quotes remain intact.

## File: docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md

- Lines/functions/headings: New quote and complete reproduction, expected behavior, acceptance, paths, evidence, risk, and proof expectation fields; issue ID issue-55bd775c8026.
- Old content or behavior: Existing mode-switch connection/crash report had three open trace tasks but missing issue context fields.
- New content or behavior: Appended the current exact report and adjacent run evidence; completed all required context fields and linked the report as related to the umbrella issue.
- Reason: Preserve the detailed TDM/new-mode connection trace without creating a duplicate umbrella.
- Why unrelated behavior is preserved: Earlier quotes and open task ownership were left unchanged.

No source implementation change was made in this session.

# Changed files and reverse steps

Only these issue and changelog records are attributable to this session. Other paths in the captured shared-checkout status are concurrent or pre-existing work.

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md | Existing umbrella issue from related intake | Current report, evidence, context, and three owned proof tasks appended | Canonical cross-domain goal | Preserve append-only user context; correct links or fields with a new coordination update |
| docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md | Existing connection/crash issue missing required context fields | Current report appended and fields completed; issue remains open | Keep the TDM/new-mode connection trace linked | Preserve prior report and tasks; append a correction if needed |
| docs/issues/README.md | Generated index from shared issue state | Regenerated view includes concurrent issue updates | Coordination tooling maintains the index | Regenerate from the issue brain; do not hand-edit |
| docs/changelog/2026-11-20/20261120_150557-end-to-end-connection-reliability-issue-intake.md | Not present | This session record | Required final record | Remove only this session file if the session is intentionally discarded |

# Contract / ABI changes

- Contract: None.
- Version bump: None.
- Compatibility: No wire format or ABI changes.
- Reverse: Not applicable.

# State / schema changes

- Domain: Coordination issue/context records.
- Old schema: Existing issue records and user context.
- New schema: No schema change; one exact user context link, issue fields, and task ownership were added through the coordination CLI.
- Migration: None.
- Reverse: Repair context links with a reason and preserve the quote; do not delete or rewrite user context.

# Deleted paths

- Path: None.
- Before: Not applicable.
- Why: No paths were deleted.
- Reverse: Not applicable.

# Diagnostics

- Owner/category: Existing connection status text in src/network/multiplayer-packets.cpp, function mpConnectionHealthText.
- Input: Multiplayer context active flag, connection state, readiness and map-loading state.
- Decision: Inactive context returns Disconnected; active, nonterminal, non-ready states return Loading; reconnect terminal states have explicit text.
- Output: This narrows the next trace to the actual HUD owner and the transition that makes the context inactive. It does not establish the user session cause.
- Failure or rejection reason: The reported top-HUD label has not been reproduced in a synchronized visual run.
- Rate limiting: No diagnostic code changed.

# Validation

- Focused skill paths and results: docs/skills/issue-triage-v1.md and docs/skills/spec-behavior-review-v1.md reviewed; issue readiness passed 8/8 for issue-cf9cd77eb056 and issue-55bd775c8026.
- Tests and exact commands: No test was started by this session. Inspected the parallel-agent run 20261120_150433 and its paired journals; it is one positive TDM start/connect baseline, not the user-reported failure or a reliability proof.
- Build status: No build requested by this session. At the latest status check, the published EXE was stale and the shared dev-loop was building game DLL generation 404 for concurrent work.
- Runtime or hot-reload evidence: Run 20261120_143117 initially recorded a successful launch and join, then client PID 10676 exited with code 3221225477 (0xC0000005) and crash artifacts while the server remained. A later request for dust2cyberiav4 reached UDP-ready and hello-sent but no matching join acceptance was established in the scanned journal. Parallel run 20261120_150433 recorded a TDM client connect request at 15:04:47Z, join.accepted at 15:04:48Z, spawn.applied by 15:04:51Z, and continuing snapshots at 15:09:23Z. Its run.json has pinned_exe=false and agent empty; it does not test mode-switch, close/reopen, HUD status, or Payload visuals.
- Output files: logs/2026-11-20/20261120_143117/{dev-loop/events.jsonl,client-01/events.jsonl,run.json}; logs/2026-11-20/20261120_150433/{server-01/events.jsonl,client-01/events.jsonl,run.json}; .dev/launch-reports/2026-11-20T14-31-17.836Z/launch.json.

# Measured evidence

- Before values: No repeat-cycle baseline is established. The user report has not been reproduced visually.
- After values: One parallel TDM connect succeeded: connect.requested at 15:04:47Z; client join.accepted at 15:04:48Z; spawn.applied at 15:04:51Z. It remained one cycle and does not establish reliability.
- Timestamps: 2026-11-20 UTC; exact run IDs and event times are listed under Validation.
- Tick/frame/network measurements: Parallel TDM journal showed continued authoritative snapshots through 15:09:23Z. No 20-cycle, restart, reconnection, or Payload visual measurement exists.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: The reported floating NPC scene and top-HUD Disconnected state were not directly observed in a synchronized run. The 20261120_143117 client exit is confirmed in its dev-loop journal, but its causal relationship to the reported mode-switch failure remains unproven. Keep the issue open and do not create a confirmed regression record yet.
- Related regression paths: Existing docs/regressions/networking, docs/regressions/npc-movement, and docs/regressions/performance indexes were read; none was changed.

# Human acceptance

- Visual review: None performed in this session.
- Gameplay review: None performed in this session.
- Multiplayer review: One parallel TDM test run connected; it did not cover the requested lifecycle or user-visible status.
- Still unverified: Payload NPC/cart grounding, truthful HUD state during a delayed or failed join, mode switch, leave/rejoin, client close/reopen, reconnect to the retained server, and repeat-cycle reliability.

# Agent handoff context

- Owner: root-connection-reliability owns umbrella issue issue-cf9cd77eb056.
- Focus: Trace the first failed connection transition, directly inspect Payload NPC/cart positioning, then run the lifecycle matrix.
- Files to check: docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md and related issue-55bd775c8026; paired per-process journals for runs 20261120_143117 and 20261120_150433.
- What another agent should not redo: Do not create another umbrella issue or duplicate the already assigned evidence tasks. The parallel TDM run is a single positive baseline only.
- Next exact step: consistency-evidence-reproduction accepted lifecycle-first-failure-trace and reproduce-lifecycle-payload-evidence; arcade-collision-elite-proof owns payload-cart-visual-proof. Correlate run 150433 with the current build/test ownership, then reproduce the user report before any fix. reliability-cycle-matrix remains blocked on those evidence tasks.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 6d92afa35b434790e1b42ebb9667be69a216cde2 / 6d92afa35b434790e1b42ebb9667be69a216cde2
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T07:47:09.981634Z", "latest_successful_build_id": "2026-11-20T07:47:09.981634Z:9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "81f5611302ef5598408f6d9e5454fe53216bafd13d02b24f911e7a1ff4c81f05", "dll": "build\\game\\generation-401\\game-g401.dll", "generation": 401, "revision": "7601e31cb77479a754e49ddc83f74c2bf4bbbb89b034122953236751d776ef46", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-401\\game-g401.dll", "generation": 401, "published_utc": "2026-11-20T14:44:44.651Z", "revision": "7601e31cb77479a754e49ddc83f74c2bf4bbbb89b034122953236751d776ef46", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 11.985, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T07:47:36.592Z"}}
- Generation: 401
- Artifact: build\game\generation-401\game-g401.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_150433\client-01\events.jsonl
- Session ID: session-eb7bcc326184

# How to reverse

- This session changed no source/config/build file and deleted no path.
- Coordination updates are append-only. If the primary issue association is wrong, use context move with a reason; retain the original quote. Correct issue facts by appending updated evidence and preserving prior findings.
- Only remove this generated changelog if the session itself is intentionally discarded and no longer needs a session record.
- Preserve all concurrent shared-checkout changes listed in Automatic evidence.

# Related feature record

- Feature path: None created; the umbrella issue owns the current intake and proof plan.

## Captured git state

```text
M .opencode/plugins/mimita-user-context.ts
 M config/analytics.json
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/build_friction.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/gui.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
 M docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
 M docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md
 M docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-wake.md
 M docs/workflows/issue-intake-and-triage.md
 M src/actor/player-ai.cpp
 M src/physics/movement/actor-final-pose.cpp
 M src/physics/movement/physics-collision-core.cpp
 M src/physics/movement/physics-collision-glb-sweep-slide.cpp
?? .opencode/skills/
?? docs/changelog/2026-11-20/20261120_145200-end-to-end-consistency-investigation.md
?? docs/issues/bug/20261120-repair-replay-export-and-build-the-local-video-e-issue-612ce59ed73d.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6bc05eadfb7f.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-87f1017235e5.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-e84cfa28c7f7.md
?? docs/issues/capability/20261120-any-device-normalized-input-for-gameplay-and-cre-issue-22e043046cd0.md
?? docs/issues/capability/20261120-decentralized-durable-world-sharing-and-creator--issue-2ecdfa48ec44.md
?? docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
?? docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-c23580c9fff1.md
?? docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
```

## Captured diff stat

```text
.opencode/plugins/mimita-user-context.ts           | 117 +++---
 config/analytics.json                              |   2 +-
 devscripts/agent_coord.py                          |   9 +-
 devscripts/blender-ai                              |   0
 devscripts/build_friction.py                       |   4 +-
 devscripts/dev-loop.py                             |   6 +-
 devscripts/dev_loop/gui.py                         | 443 ++++++++++++++++-----
 ...e-issues-preview-overlay-first-frame-context.md |  34 ++
 docs/issues/README.md                              |  72 +++-
 ...tency-limbs-only-capsule--issue-da337d1fe2f3.md |  23 ++
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  45 +++
 ...lision-and-mace-active-po-issue-503f3e410c01.md |  10 +
 ...-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |  26 ++
 ...ocal-ai-player-controller-issue-f8873319f12b.md |  35 ++
 ...end-to-end-implementation-issue-565902eed8a7.md |  36 ++
 ...nd-observe-context-bundle-issue-6907072bb155.md |   6 +
 ...le-restart-and-self-recov-issue-6fc8f853a674.md |   7 +
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  12 +
 ...ture-slices-through-proof-issue-848a5ba01470.md |  26 ++
 ...the-user-adds-new-context-issue-a271b0296faa.md |  12 +
 docs/specs/dev-loop/agent-coordination.md          |   1 -
 docs/specs/dev-loop/agent-wake.md                  |   8 +
 docs/workflows/issue-intake-and-triage.md          |   8 +-
 src/actor/player-ai.cpp                            | 272 +++++++++++--
 src/physics/movement/actor-final-pose.cpp          |  36 ++
 src/physics/movement/physics-collision-core.cpp    |   9 +
 .../movement/physics-collision-glb-sweep-slide.cpp |  80 ++++
 27 files changed, 1162 insertions(+), 177 deletions(-)
```

## Captured changed paths

```text
M	.opencode/plugins/mimita-user-context.ts
M	config/analytics.json
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/build_friction.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/gui.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
M	docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
M	docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md
M	docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-wake.md
M	docs/workflows/issue-intake-and-triage.md
M	src/actor/player-ai.cpp
M	src/physics/movement/actor-final-pose.cpp
M	src/physics/movement/physics-collision-core.cpp
M	src/physics/movement/physics-collision-glb-sweep-slide.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-20T07:47:09.981634Z",
    "latest_successful_build_id": "2026-11-20T07:47:09.981634Z:9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "81f5611302ef5598408f6d9e5454fe53216bafd13d02b24f911e7a1ff4c81f05",
    "dll": "build\\game\\generation-401\\game-g401.dll",
    "generation": 401,
    "revision": "7601e31cb77479a754e49ddc83f74c2bf4bbbb89b034122953236751d776ef46",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-401\\game-g401.dll",
    "generation": 401,
    "published_utc": "2026-11-20T14:44:44.651Z",
    "revision": "7601e31cb77479a754e49ddc83f74c2bf4bbbb89b034122953236751d776ef46",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 11.985,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-20T07:47:36.592Z"
  }
}
```
