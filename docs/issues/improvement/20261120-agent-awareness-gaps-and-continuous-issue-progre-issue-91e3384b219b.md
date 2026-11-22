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

## User quote

### 2026-11-20T07:12:55.385Z — launch-latest-stable-fix

Via launch-latest-stable-fix; supports Explicit authorization to restart the existing dashboard/dev-loop daemon when the documented change path requires it, and a priority on collecting strong live proof of each change.:

> ensure that if tihs needs daemon restart or gui restart, ai agents are allowed and encouraged to restart either of those if needed/approrpaite, so that u can test ur changes , a big priroity for agents is to prove somehow whatever u did , like gt as much evidence live super good evidnece that whatever u did is working according to the goal

## Agent discussion and suggestions

### 2026-11-21T00:08:39.859Z — root

Message: `msg-4447a0f30794` (evidence)

Subject: AI eyes gap: desktop inspection timed out during public join retry

User's verbatim request is context ctx-9cff1e071bca. Repeat attempt at public room 9K5YPND: the CUA desktop inspection call again timed out at service_info before exposing any windows. The dev-loop status showed an already-running visible sandbox run (20261120_232200); I did not replace or interact with it. Local isolated join evidence does not establish public-server join. Suggested tooling work: provide a reliable, health-checked desktop/app inspection path with an alternate capture/control route owned by the dev loop; bind screenshots, click targets, build/run identity and client/server connection journals into one inspectable session; when inspection fails, surface the exact timeout as a machine-readable blocker and route to an automatic retry/fallback. Acceptance should include observing a listed public server JOIN attempt and retrieving its client connection events, with explicit failure status if UI inspection is unavailable.

## Agent suggestion

### 2026-11-21T00:10:58.095Z — fps-dip-auto-trace-intake

Related capability proposal: issue-13d080318e1c applies the AI-awareness goal to client performance: correlate each sub-240-FPS frame with exact run/build/workload and bounded timing/resource evidence, mark missing telemetry, and create or extend actionable diagnosis issues. No implementation or runtime proof is claimed.

## Agent suggestion

### 2026-11-21T00:21:03.334Z — root

Direction from user (context ctx-e785f09c43ef): make every MiMITA task inspectable and actionable by AI over time, with agents continuously expanding the capability surface. Build a capability inventory for repo, dev-loop, runtime, UI and external-tool actions; identify blind spots and safe fallbacks; turn recurring multi-step human workflows into one human request plus bounded automation. Where an external UI bridge fails, classify the fault, capture diagnostics, and use a supported repo-owned Python/CLI workflow when that workflow can safely achieve the same task; do not dynamically generate arbitrary scripts or claim external state was observed without evidence. Prioritize measurable reductions in human actions and increases in successful task completion, with proof and human review for risky or externally visible actions.

## User quote

### 2026-11-21T00:21:59.565Z — rocket-impact-investigation

Via rocket-impact-investigation; supports unspecified:

> also can u confirm that ai agents periodically or whenerv  like every other turn or every 5th turn idk, agetns pick a open issue or a importan issue and review it and add suggestions or debaate with other agetns to firgure out  good solution to wahtevr the issue is? so over time we keep ahving more and more progress made on issues, more evidence for each issue,  more clearler reasoni why the issue is happenig etc so its like we arent guessing we just have a bunhc bunch bunch of eviedence for any issue over time

## Agent suggestion

### 2026-11-21T00:22:03.349Z — rocket-impact-investigation

Current state: the coordination system supports issue review/suggestions and waiting agents can run next --mode waiting; this existing issue already has review-while-waiting and stale-issue-heartbeat tasks. I found no enforced every-N-activation/turn scheduler, so periodic review is not guaranteed for busy agents. Recommend a bounded heartbeat (for example every 5th activation, plus each time an agent becomes idle): select a high-priority open issue that is not already covered by active work, inspect linked evidence and proof freshness, append a concrete suggestion or missing-evidence note, and notify/collaborate with the current owner without duplicating work. Track the activation count and prove suggestions are added at the cadence. The cadence is a proposed default for review, not an implemented feature yet.

## Agent suggestion

### 2026-11-21T00:30:45.858Z — root

Follow-up context ctx-afcad8cdbcdb makes fallback mandatory: a computer-use error must trigger the next supported method, not task abandonment. For each capability, maintain an AI-readable map of supported inputs, preconditions, safe actions, evidence outputs and known limits. On tool failure, classify it and invoke an allowlisted repo-owned Python/CLI adapter where it can achieve the same objective; otherwise use the next available read-only evidence path and preserve the external blocker. Each iteration should convert one previously manual multi-step workflow into a single user request with bounded automatic execution, recovery, verification and a clear report. Agents should collaborate by adding missing capabilities and solutions to the shared issue/decision surface, measure human actions removed and task success, and keep audit trails. Do not use arbitrary generated scripts with broad powers; create small reusable adapters with explicit scope and proof.

## Agent discussion and suggestions

### 2026-11-21T00:30:47.012Z — root

Message: `msg-c16c2b03dd3a` (evidence)

Subject: User direction: continue through tool failures with safe scripted fallbacks

User follow-up is captured as high-priority context ctx-afcad8cdbcdb. They explicitly authorize Python scripts as a fallback for inspection/testing and ask that AI continue after computer-use failures, expand AI-doable workflows over time, and collaborate on novel solutions. Please extend the existing AI-awareness issue with a measured capability inventory and allowlisted Python/CLI fallback adapters, using one user action to automate bounded multi-step work. Evidence this turn: CUA still times out before inventory; a repo-local Python probe inspected the current dev-loop journal and found room code 9K5YPND in the active sandbox run, but that run is loopback and does not prove a public-server join. A Python POST probe to mimita.fun was blocked with HTTP 403 / Cloudflare 1010 in this environment. No repo-supported command joins an ICE room by code: agent.join accepts an IP/address; GUI uses mpIceConnectStart(roomCode).

## Agent discussion and suggestions

### 2026-11-21T00:31:05.002Z — root

Message: `msg-6c9575bab813` (evidence)

Subject: User direction: continue through tool failures with safe scripted fallbacks

User follow-up is captured as high-priority context ctx-afcad8cdbcdb. They explicitly authorize Python scripts as a fallback for inspection/testing and ask that AI continue after computer-use failures, expand AI-doable workflows over time, and collaborate on novel solutions. They want a measured capability inventory and allowlisted Python/CLI fallback adapters, with one user action automating bounded multi-step work. Evidence this turn: CUA still times out before inventory; a repo-local Python probe inspected the current dev-loop journal and found room code 9K5YPND in the active sandbox run, but that run is loopback and does not prove a public-server join. A Python POST probe to mimita.fun was blocked with HTTP 403 / Cloudflare 1010 in this environment. No repo-supported command joins an ICE room by code: agent.join accepts an IP/address; the GUI path calls mpIceConnectStart(roomCode).

## Agent suggestion

### 2026-11-21T03:52:41.423Z — build-wait-reviewer

AI tool identity mismatch with live proof, 2026-11-21T03:3xZ, no source edits. This activation was launched as wake-03de8d93cd0e for agent build-wait-reviewer (opencode pid 37004, parent cmd 28280, title 'wake wake-03de8d93cd0e'). Every tool shell command instead reports MIMITA_AGENT_ID=root, MIMITA_WAKE_ID=wake-f0838de769a8, OPENCODE_PID=28600. Process 28600 is the still-running root wake launched 2026-11-21T00:13:33Z (wrapper cmd 25932), age about 3.6h at observation. Consequence: agent-coord commands run without an explicit --agent act as root. Confirmed: 'issue review issue-f9ea4f30ad18' was recorded with agent=root (review id 30 at 03:36:54Z) even though this wake's reviewer identity is build-wait-reviewer. Suggested direction: bind tool-shell env to the launching session instead of reusing a stale process env; at begin, verify MIMITA_AGENT_ID, MIMITA_WAKE_ID, and OPENCODE_PID match the active wake and fail loudly on mismatch; show the active identity in sync and brain_check. Cross-ref issue-9a4503f6ed68 (wake leak and duplicate launches) and issue-36579a10979a (wake adapter).

## Review (reviewed - bounded activation)

### 2026-11-21T04:53:36.832Z — fcs-npc-live-fix

Reviewer: fcs-npc-live-fix

Bounded review for ctx-afcad8cdbcdb (wake wake-1f9e04a64eae). Routing is correct: primary issue issue-91e3384b219b; related concrete blockers issue-0e981ef03d32 (CUA service_info timeout) and issue-80dfb9ab9c5c (daemon alive, control endpoint unreachable). No ownership repair needed; fcs-npc-live-fix made no code or build changes. The user's Python-fallback direction is captured by issue-0e981ef03d32's acceptance (supported fallback with real MiMITA state, not a generic unsafe script) and by the daemon recovery acceptance in issue-80dfb9ab9c5c. Separately, this session again received only the one-line wake banner even though requests/wake-1f9e04a64eae.json carries the full context-review prompt; that is recorded on issue-62ab3daa615d.

## Review (reviewed - bounded activation)

### 2026-11-21T05:51:50.116Z — rocket-impact-investigation

Reviewer: rocket-impact-investigation

Bounded review for ctx-afcad8cdbcdb (wake-612d9b042cf9, agent rocket-impact-investigation). Routing is correct: primary issue-91e3384b219b; related concrete blockers issue-0e981ef03d32 (CUA service_info timeout) and issue-80dfb9ab9c5c (daemon alive, control endpoint unreachable). No ownership repair needed. New evidence for the user's Python-fallback direction: devscripts/process-snapshot.py is an existing supported read-only inspection path; it produced 13 MiMITA-related process rows via Toolhelp32Snapshot at 2026-11-21T05:38:48Z when the PowerShell CIM query timed out, so agents already have a usable fallback for process-level awareness (suggestion filed on issue-0e981ef03d32). No code or build changes. Wake note: this session again received only the one-line banner while .dev/agent-wake/requests/wake-612d9b042cf9.json contains the full context-review prompt, another issue-62ab3daa615d reproduction.

## Agent suggestion

### 2026-11-21T14:29:51.346Z — root

New high-priority user context ctx-f9ce8d89a650: expand AI controls to include starting/inspecting servers and joining rooms. A room-code attempt must be part of a recoverable workflow: inspect the current server/listing, attempt the supplied code, classify the failure, and when the code is stale/unavailable or cannot be tested, start a new checkout-owned server through dev-loop, capture its new room code, then reproduce with a separate correlated client and journals. The user explicitly says not to get stuck on one exact code. Acceptance: one user request can start/identify a server, join by room code, automatically retry with a fresh owned room when appropriate, and report server/client/run IDs plus Connected or bounded failure evidence. Keep every step within dev-loop authority, preserve unrelated healthy pairs, avoid arbitrary process launches, and distinguish reproducing our fresh room from joining the original public server.

## Context

### 2026-11-21T14:36:13.852Z — visual-access-and-issue-review

risk: A fixed cadence can create repetitive low-value reviews, wake/build load, duplicated or conflicting agent edits, and false confidence if suggestions are mistaken for proof. Use a global event counter based on completed work activations (not private reasoning thoughts), one bounded review per cycle, deduplication/cooldowns, explicit owner/reviewer separation, and never auto-close without proof.
proof_expectation: Integration proof that a controlled sequence of completed agent work activations triggers a bounded review selection for an open unproven or stale issue; a different agent appends a labeled review with evidence and a next check; a second trigger inside the cooldown is deduplicated; board/coordination events show selection, reviewer, timing, and any failure. Also report review coverage and duplicate/no-op rate over a representative soak.

## Agent suggestion

### 2026-11-21T14:36:16.100Z — visual-access-and-issue-review

Current state: the specs already ask agents to review at phase boundaries/useful waits, use `next --mode waiting` to surface missing fields/unproven/stale proof, and say issues should receive periodic progress. I found no defined every-N-activation trigger or scheduler that guarantees this happens when agents are otherwise idle; the cadence is advisory/opportunistic. Recommendation: use completed work activations/events, never hidden model 'thoughts'. Trigger one rotating review after every 5 completed global activations (and one bounded scheduled sweep every 30 active minutes if no fifth activation occurs); immediately queue review after failed build/proof or a user-visible incomplete claim. Select failed/unproven proof, then high priority and oldest review timestamp; rotate to a non-owner reviewer when available; append one evidence-grounded review or explicitly record 'no new evidence' and next check; dedupe same issue for 24h unless new evidence arrives. Limit each sweep to 1-3 issues and measure open issue review coverage, no-op/duplicate rate, wake/build load, and time from failure to next evidence. Existing issue-91 is currently not ready: readiness reports missing risk and proof_expectation; these fields are now proposed in the issue context update.

## Agent suggestion

### 2026-11-21T14:36:50.167Z — visual-access-and-issue-review

Cadence audit (specified versus enforced): agent-coordination.md directs review at task phase boundaries and while waiting, and `next --mode waiting` surfaces missing fields and absent/stale proofs. agent-self-improvement.md says every issue should receive periodic progress and describes a rotating systems agent or scheduled audit. I found no cadence parameter, persistent activation counter, or scheduler implementation in devscripts/agent_coord.py or agent-wake.py, so actual cross-agent review is currently opportunistic and not guaranteed every N turns. Internal thoughts are not a stable trigger; use completed agent work activations. Proposal for review: a shared rotating queue triggers after every 5 completed work sessions (or 30 minutes idle, whichever comes first), reviews at most one stale issue per trigger, and uses round-robin by oldest last-review timestamp with high-priority, blocked, and unproven items elevated. Set a per-issue cooldown to avoid repeated reviews; target high-priority/unproven issues within 24h and every other open issue within 7 days, while tracking overdue audit debt. The reviewer records observed/inferred/unknown, evidence path/build/run, strongest counterevidence, next bounded experiment, and handoff-ready next action. If no new evidence exists, record only the audit checkpoint/deferral without duplicative prose. Acceptance metrics: percentage of open issues reviewed in SLA, median days since independent review, evidence-bearing review rate, duplicated-review rate, and issue progress/proof rate; roll out in shadow mode before waking agents automatically.

## Agent suggestion

### 2026-11-21T14:37:17.230Z — privacy-visual-design-review

Cadence review: the current coordination spec already allows issue review at phase boundaries/useful waits and exposes repair work through `agent-coord next --mode waiting`; self-improvement says every issue should receive periodic progress and describes a rotating systems agent or scheduled audit. However, I found no enforced timer or activation-count scheduler. Do not count hidden model thoughts. Recommended bounded experiment: trigger one rotating audit every 5 completed agent activations globally OR every 30 minutes, whichever comes first, reviewing at most 3 issues. Rank by high priority + no new evidence/review for 7 days, failed/stale proof, then unowned issues. Append findings only when evidence, counterevidence, a concrete next check, or a useful synthesis exists; record `last_reviewed` and a short no-change reason otherwise. Measure stale issue share, median time to new evidence, duplicate/no-op review rate, and whether cold reviewers can act from issue+artifacts. Never auto-close or self-approve behavior proof.

## Agent suggestion

### 2026-11-21T14:44:53.255Z — root

Live evidence 2026-11-21: fallback through authenticated dev-loop control accepted start-client for fresh room 6BXH7EE/client-02. Client journal confirms ICE transport open/ready, sends join requests, then remains WaitJoinAccept; emits network.join.map-required-missing. Server journal continues coordinator ICE polls with players=0, so no accepted roster/player. This reproduces a fresh-room failure, not the original public room. Dev-loop later stopped server-01 at 14:43:03Z with reason 'server replacement or explicit server-stop completed'; client-02 stopped at 14:43:08Z. Improve AI-facing lifecycle controls with bounded create/start/join/status/stop commands, per-attempt correlation and deadlines, observable terminal failure classification, and automatic retry on a newly created room when the first room cannot be joined. Preserve original-room result separately from fresh-room reproduction; prevent unrelated process/server replacement. Python fallback succeeded only because it used the supported authenticated dev-loop control protocol after the CLI hung.

## Agent suggestion

### 2026-11-21T15:02:23.415Z — review-scheduler-code-review

Read-only scheduler design review (existing code, before implementation): work_sessions is the durable shared counter source, and agent-wake.py already supports explicit one-shot manual wake requests plus un --once; build_friction.py demonstrates the add-then-run pattern. Recommended trigger at five newly completed work sessions or 30 minutes after last sweep, evaluated on sync and work_end (document clearly that no AI runs during total inactivity); never count private model thoughts. Concurrency/counterpoint: sync and work_end may race, so persist the last scheduled completed-session count, last-sweep UTC, issue, and wake/dedupe key in the coordination DB under a short transaction, then release the DB before subprocess launch. Add a wake dedupe_key so a timeout/crash after writing the wake file but before updating scheduler state cannot queue duplicates. If an eligible review wake is already queued/active, do not schedule another. Select one issue per sweep, prioritizing unproven/stale proof, high priority, and oldest independent review; skip done/cancelled/deferred/needs_clarification and recent reviews. Advance cadence and append a no-candidate event when the queue is empty to avoid per-sync noise. Wake prompt should cold-read issue brief/artifacts, append a useful evidence/counterevidence review or explicit no-new-evidence checkpoint, then suggest at most one evidence-backed development-process improvement to this issue (or issue-70cbf4d13c02), or state no measurable opportunity; no generic brainstorm, auto-close, self-approval, or source edits. Measure review coverage, evidence-bearing share/no-op rate, duplicate wake count, and time from stale proof/failure to review.

## Agent suggestion

### 2026-11-21T15:10:21.489Z — general-issue-review-scheduler

Implementation started in this activation under the user's explicit request: a durable `issue_review_scheduler` checkpoint counts completed `work_sessions`, and an event-driven tick runs from `sync` and first `work_end`. The proposed scheduler fires every 5 global completed sessions or after 30 minutes when another session has completed, selects one open issue with stale/unproven proof first and excludes issues reviewed within 24h, then queues a deduplicated independent `agent-wake` review. The wake prompt explicitly asks for one evidence/counterevidence review and one development-process improvement suggestion only when the linked artifacts support it. It never edits source, closes the issue, or self-approves proof. No runtime/CLI proof yet; source is being reviewed and will receive a bounded syntax check only (no test suite requested).

## Agent suggestion

### 2026-11-21T15:15:12.052Z — review-scheduler-code-review

Scheduler code review found a concrete lost-review path in devscripts/agent_coord.py::_issue_review_scheduler_tick and devscripts/agent-wake.py::add_request. The coordinator cancels a queued/claimed review wake after 900s, then retries with the same sweep_key; dd_request only resets an existing dedupe-key request when status is ailed, while a cancelled record is returned unchanged. The coordinator treats the successful CLI return as a queued wake and advances last_completed_count/last_scheduled_utc even though the reused request is still cancelled. Counterexample: wake daemon is unavailable for >15 minutes, sweep becomes due, stale queued wake is cancelled, same key is re-added, returned status remains cancelled, scheduler advances cadence, no review runs. Fix by requeueing cancelled dedupe records (clear cancellation/error/claim fields, update status) or generate a new retry key, and only advance scheduler state after confirming returned wake status queued/claimed/launched. Second reliability risk: scheduler considers every launched wake pending without checking child PID/age; if no wake daemon processes the exited child, later sync returns pending forever and does not call un --once to reconcile it. Reconcile launched wake process state or run the wake queue on every due scheduler check, with bounded failures recorded and retryable. Third, agent-wake un_once and scheduler's explicit un --once can race a separately running agent-wake daemon; run_once reads/rewrites request JSON without an atomic claim lock, so both may launch duplicate reviewer processes. Use one process-safe claim/lock for request IDs. Policy/spec additions are otherwise coherent: they say the due check occurs on an activation, not while the whole system is idle, and the wake remains review-only/no source edits or self-approval.

## Agent suggestion

### 2026-11-21T15:30:41.233Z — general-issue-review-scheduler

Implemented bounded general issue-review scheduler in devscripts/agent_coord.py: checks at sync/work-end, one open issue per due sweep after 5 completed work sessions or 30 minutes after at least one new session, durable DB cadence checkpoint, stable wake dedupe key, unproven/stale evidence and priority ordering, one-review-per-issue 24h cooldown, reviewer cold-read prompt for evidence/counterevidence/next check plus a measured development-process improvement or explicit none-found. Recovery correction from independent review: devscripts/agent-wake.py now requeues failed/cancelled dedupe requests; scheduler reconciles old launched child PIDs to avoid indefinite stale-pending state. Docs updated in agent-coordination.md and agent-self-improvement.md. Static AST syntax parse passed and git diff --check had no whitespace errors (only line-ending warnings). Runtime wake/reviewer execution and proof of scheduler cadence remain unverified; next check is a real coordinator activation with configured wake adapter, inspect coord.issue_review.* and wake.* events, then inspect reviewer issue append. This is a bounded scheduler, not an always-running background model; review cadence requires agent sync/work-end.

## Agent suggestion

### 2026-11-21T15:34:44.287Z — general-issue-review-scheduler

Follow-up runtime evidence: initial coordinator sync exposed an existing database migration gap: legacy issue_review_scheduler tables lacked last_attempt_utc, so first use raised IndexError despite valid Python syntax. Fixed by adding _ensure_issue_review_scheduler_columns() in agent_coord.py using PRAGMA table_info plus ALTER TABLE for missing scheduler columns. After migration, a real gent-coord.py sync completed successfully and returned scheduled_issue_review={due:false,scheduled:false,completed_sessions:134,sessions_since_review:0}. This proves scheduler initialization/migration and non-due sync path; due selection, wake launch, reviewer append, cadence dedupe, and stale-PID recovery are still unproven. Preserve that distinction; next proof requires natural cadence or an explicitly isolated integration run plus coord.issue_review.* and wake.* event inspection.

## Agent suggestion

### 2026-11-21T15:37:04.045Z — opencode

Capability gap from ctx-f9ce8d89a650 (user): AI needs better controls to start a server and get into a server, computer-use-style, without being stuck on one room code. Concretely: an agent-drivable path that (1) starts/owns a server, (2) reads the published room code, (3) joins with a client, and (4) if that room code/join fails or times out, automatically retries and/or spawns a fresh server with a new room code instead of blocking. This is the autonomy half; the concrete connection-reliability reproducer lives in issue-cf9cd77eb056 and AI-run server validation under load in issue-be39a4f5ee17. Suggested first slice: a single 'agent join server' command that encapsulates start -> publish code -> connect -> verify connected, with retry-on-new-code, emitting the exact run/room-code/connected journal evidence.

## Review (reviewed)

### 2026-11-21T15:39:03.822Z — root-dll-status-review

Reviewer: root-dll-status-review

Bounded independent review for ctx-d1c3f01ad6f7 (wake-77ba6abcc734, agent root-dll-status-review). Routing is correct: primary issue-91e3384b219b; related shared-evidence issue-70cbf4d13c02. No ownership repair needed; no source edits, build, or test run started.

Observed (uncommitted working tree; not my change):
- devscripts/agent_coord.py `_issue_review_scheduler_tick` (line 829) implements the general scheduler: due after >=5 newly completed work sessions or >=30 min with count_delta>0, 60s retry cooldown, single reviewer `issue-review-sweeper`, `issue-review-sweep-<completed>` dedupe, stale-wake cancel, one-issue selection preferring unproven/stale proof then priority then age, and `coord.issue_review.scheduled|skipped|failed` events; sync exposes `scheduled_issue_review`.
- docs/specs/dev-loop/agent-self-improvement.md adds the end-of-activation process check and the rotating scheduler policy, including the duty to suggest development-process improvements more often.
- docs/specs/dev-loop/agent-context-contract.md "Cold-handoff minimum" plus docs/workflows/issue-intake-and-triage.md require claim labels (observed/inferred/hypothesized/disproved/unknown), evidence links, failed attempts, counterevidence, build/run/journal identity, and the next discriminating check, so a cold agent can continue.

Gap / counterevidence: runtime proof is missing.
- `coord.issue_review.*` and `issue-review-sweeper` do not appear in .dev/agent-coord/events.jsonl, and no wake request exists for that reviewer.
- issue_review_scheduler is last_completed_count=129, last_scheduled_utc=2026-11-21T15:04:32Z, but last_issue_id/last_wake_id/last_result are empty. Schema init (agent_coord.py:547-554) seeds exactly those values, so this is initialization, not a recorded review.
- `python devscripts/quality/test_agent_coord.py` passes (41 tests) but has no test for scheduler cadence, dedupe, cooldown, or the no-adapter failure path.

Inference/uncertainty: the code path reads coherently, but it has not been exercised against the live brain; whether it selects and launches a bounded review correctly is unproven.

Next discriminating check (owner general-issue-review-scheduler or any reviewer): complete 5 ended work sessions (or 30 min plus a new activation), then confirm one `coord.issue_review.scheduled` event with issue/wake_id, exactly one queued/running `issue-review-sweeper` wake, and that a second sync inside the cooldown returns retry/dedupe instead of a second wake; add unit tests for cadence, dedupe, and no-adapter failure. No fix or closure claimed.

## Review (reviewed - bounded activation, implementation verified present (runtime cadence unproven))

### 2026-11-21T15:39:11.354Z — payload-arcade-collision-fix

Reviewer: payload-arcade-collision-fix

Bounded review for ctx-d1c3f01ad6f7 (wake-8fd77a7ad717, agent payload-arcade-collision-fix). Routing is correct: primary issue-91e3384b219b covers the general scheduler and periodic development-process suggestions; the shared-evidence/cold-handoff ask is owned by issue-70cbf4d13c02, now linked as related. No ownership repair needed and I made no code, spec, build, or run changes; the active owner is general-issue-review-scheduler (task stale-issue-heartbeat). Verification evidence from the working tree at 2026-11-21T15:3xZ: devscripts/agent_coord.py defines issue_review_scheduler, _issue_review_scheduler_tick at :829, and calls it from sync (:1102-ish) and work_end; cadence is 5 completed work_sessions or 30 min with >=1 new session, one issue per sweep, 24h per-issue review cooldown, 60s retry cooldown, stale/unproven-proof then priority then oldest-review ordering, stable sweep_key dedupe, and a wake prompt that requests one evidence/counterevidence review plus one measured process-improvement proposal or explicit none-found. devscripts/agent-wake.py add_request requeues failed/cancelled dedupe records and the coordinator reconciles old launched child PIDs before treating a launch as in flight, addressing the lost-review path in the prior independent review. Docs' agent-coordination.md, agent-self-improvement.md, issue-intake-and-triage.md, and agent-context-contract.md describe the scheduler, cold handoff, and periodic process review. AST parse of both changed Python files passed. Remaining gap: no observed coord.issue_review.scheduled event with a real reviewer append yet, so scheduler cadence/proof is documented and code-static only, not runtime-proven; the owner already records this as the next check. The user's 'AI should suggest more ideas more often' is captured by the scheduler prompt and self-improvement cadence text (periodic, evidence-backed, rate-limited), not by an unbounded brainstorm loop.

## Agent suggestion

### 2026-11-21T15:39:34.340Z — general-issue-review-scheduler

Live due-path evidence after schema migration: real coordinator sync emitted coord.issue_review.scheduled event COORD_3575 at 2026-11-21T15:34:07.723Z, selected issue-0cdac76fb006 (Make small desktop behavior fixes discoverable and proof-gated), and queued wake-d3c001075d14 with dedupe key issue-review-sweep-134. Request transitioned to launched at 15:34:08.210Z with child PID 11884; wake log .dev/agent-wake/logs/2026-11-21T15-34-07.705Z-issue-review-sweeper.log is accumulating output. At last inspection, request remained launched and the target issue had only the scheduling note, so independent review append, cadence re-dedupe, and reviewer completion are pending. This validates due selection and wake launch, not final review quality or fix proof.

## Agent suggestion

### 2026-11-21T15:47:49.982Z — root-dll-status-review

Update to review 43 (append-only correction, not a rewrite): the scheduler did fire after that review. Evidence: coord.issue_review.scheduled at 2026-11-21T15:34:07.723Z (event COORD_3575), completed_sessions=134, issue=issue-0cdac76fb006, wake_id=wake-d3c001075d14, sweep_key=issue-review-sweep-134; .dev/agent-wake/requests/wake-d3c001075d14.json status=launched, and issue-review-sweeper is active. This satisfies the scheduled-event and single-wake portion of the proof expectation. Still unproven: the reviewer's appended review on issue-0cdac76fb006, a second trigger inside the cooldown returning dedupe/retry instead of a second wake, and any scheduler-cadence test in devscripts/quality/test_agent_coord.py. Owner next step unchanged.

## Agent suggestion

### 2026-11-21T15:53:07.822Z — opencode

Bounded independent verification by opencode (no source edits, no ownership change). Routing is correct: ctx-d1c3f01ad6f7 is linked as primary to issue-91e3384b219b, and general-issue-review-scheduler owns the implementation (its file leases expired at 15:18Z while it remained active).

Observed (source/unit evidence only, not behavior proof):
- devscripts/agent-wake.py add_request now requeues a dedupe_key whose existing status is failed OR cancelled, resetting attempts/error/child_pid/claimed_utc/launched_utc before returning it. This addresses the lost-review path raised in the 15:15:12 review.
- `python -m py_compile devscripts/agent_coord.py devscripts/agent-wake.py` OK.
- `python -m unittest devscripts.quality.test_agent_coord` = 41 tests OK (20.6s) on the current working tree.
- The cold-handoff requirement is now written in docs/workflows/issue-intake-and-triage.md and docs/specs/dev-loop/agent-self-improvement.md (observed/inferred labels, exact build/run/journal identity, counterevidence, next check), matching the third user ask.

Inference/uncertainty: the scheduler has NOT been shown to fire end-to-end (five completed work_sessions -> exactly one deduplicated independent review wake -> review appended). No test in test_agent_coord.py exercises _issue_review_scheduler_tick cadence, dedupe, or the cancelled/failed wake path, and I produced no runtime journal evidence.

Next discriminating check (cold-pickup candidate): a temp-coordination-root harness that inserts N completed work_sessions, calls sync twice, and asserts (a) one review wake at the 5th completion, (b) the second sync reuses it, (c) a cancelled/failed wake does not advance last_completed_count, and (d) a due-but-unlaunchable wake records a visible pending/failed event. This is source/unit evidence only; a passing build or unit test is not behavior proof.

## Agent suggestion

### 2026-11-21T15:56:34.106Z — root-dll-status-review

Second append-only update: the reviewer half is now proven too. issue-review-sweeper appended review id 1 on issue-0cdac76fb006 at 2026-11-21T15:50:23Z (verdict inconclusive, evidence-grounded, no fix claimed), and issue_review_scheduler now records last_completed_count=134, last_issue_id=issue-0cdac76fb006, last_wake_id=wake-d3c001075d14, last_result=queued. Together with COORD_3575 this demonstrates selection -> single wake -> independent append. Remaining proof items are narrower: a second trigger inside the cooldown must dedupe/retry without a duplicate wake, and devscripts/quality/test_agent_coord.py still has no scheduler cadence/dedupe/no-adapter test. Reviewer root-dll-status-review made no edits or builds.

## Agent suggestion

### 2026-11-21T16:01:48.775Z — visual-access-and-issue-review

AI server start/join control gap (user ctx-f9ce8d89a650, 2026-11-21): "we need better AI controls for starting server and getting into a server and stuff, like computer use service issue ... if the room code doesnt work, try reproduce with a new server room code, dont have to be stuck on that code exxactly."

What already exists (source/spec only, not live-proven): `python devscripts/dev-loop.py --command "ss"` is the intended one-command AI path (docs/specs/dev-loop/dev-loop.md 19.7, docs/specs/networking/networking.md 67.1/4385-4466), with isolated `--start-test-run`/`--start-test-client` for pinned runs and `--room <ROOM-CODE>` for joins. The dev-loop already persists/reuses a room code and refuses to reuse a dead server's code (devscripts/dev-loop.py ~4400-4461).

Gap: there is no single documented, proven AI flow for start -> read published room code (`network.room.registered`) -> join -> and, on stale/failed room code, abandon it and start a fresh server/room instead of retrying the same code. Keep the computer-use/desktop bridge out of the primary path; it is already failing (issue-0e981ef03d32).

Smallest proposal: one owner (dev-loop command path) with a documented "start-and-join, fresh-room fallback" procedure: on room lookup/join failure, classify stale room (no registration, dead PID, wrong run/build identity), start a new server identity, record both attempts' room codes and first failing event, and require a live journal proof. Related: issue-cf9cd77eb056 (lifecycle), issue-c8ac61cf07fc (room timeout), issue-be39a4f5ee17 (AI server validation), issue-9215d710021d (build wait).

## Agent suggestion

### 2026-11-21T16:53:44.454Z — issue-review-sweeper

Process/evidence improvement found during independent review of issue-55bd775c8026.

Evidence: two reconnect failures on different builds ended in token_not_found - run 20261120_152743 (Nov 20) and run 20261121_140624 (Nov 21, build 2026-11-21T02:26:50.306117Z:8b5838c...). Both server removal events reported reconnect_token_retained=true, yet src/network/reliable-gameplay-events.cpp:144 erases the player slot and src/network/server-packets.cpp:2352-2367 matches tokens only in live players, so lookup returned token_not_found. The Nov 20 note left this unexplained as reason the server had no player record remains unresolved; the same ambiguity repeated a day later. This is a repeated missing-evidence/attribution delay of more than 24h, not a gameplay request.

Proposal: make runtime evidence state actual disposition and require attribution on rejection. (a) reconnect_token_retained must be true only when a later reconnect lookup can still match; otherwise false or replace it with retained_until_ms. (b) Every network.server.reconnect.rejected must carry last_removal_reason and last_removal_wall_time for that connection/player.

Measurable target: on the next build cycle, 100 percent of reconnect.rejected(token_not_found) events include last_removal_reason plus last_removal_wall_time, and zero removal events report reconnect_token_retained=true when the slot was erased; a reviewer identifies the erase path from one paired journal in under 5 minutes without reading source.

Smallest experiment: add only those evidence fields in serverLogPlayerSessionRemoval and the reconnect-rejected emit; run one pinned TDM reliable-failure -> reconnect cycle through the root dev-loop and verify with paired journals. Do not change reconnect or removal behavior in this experiment; gameplay fix stays on issue-55bd775c8026. Reviewed read-only by issue-review-sweeper; no code or build performed.

## Agent suggestion

### 2026-11-21T19:39:09.921Z — issue-review-sweeper

Process/evidence improvement found during independent review of issue-cf9cd77eb056 (2026-11-21T19:36Z).

Evidence: isolated run 20261121_192618 reached dev-loop.parallel-run.ready at 19:26:37Z on generation 427, then `--stop-test-run` at 19:28:32Z reported stop_partial with `stopped_pids: []` and "process identity does not match run manifest" for both PIDs, while run.json stayed `state=online`; both server/client event journals (9.3/12.4 MB) had disappeared by the next inspection and only run.json remained. Source shows the manifest stores `process_started_utc` at millisecond precision (`devscripts/dev_loop/parallel_test_runs.py:411-416`) while identity returns microseconds (`devscripts/process_identity.py:191-196`), and stop compares them as exact strings (`parallel_test_runs.py:850-853`). The result is an avoidable manual-recovery step and loss of the exact evidence the issue needs. The same sweep also found a proof record whose `status=runtime_proven` coexists with `result=unproven`, `verified_utc=""`, and an unimplemented field predicate.

Proposal: make the isolated-run stop path preserve evidence and compare identity at normalized precision. First, normalize both timestamps to milliseconds (or compare epochs within a tolerance) before the exact identity decision. Second, write a structured mismatch event containing both raw timestamps and refuse to transition a run to a terminal state until journals are confirmed present; never delete or fail to preserve a run's journals during stop.

Measurable target: on the next 5 isolated TDM starts/stops, 100 percent either stop the owned PIDs or emit a structured mismatch with both timestamps at full precision, with zero false mismatches where only sub-millisecond precision differs; 100 percent of stopped/failed runs retain run.json plus both journals and leave no manifest `state=online` entry whose PID is absent; a reviewer links each task proof to a run/journal in under 5 minutes. Also require proof listing to label or reject `status=runtime_proven` when `result!=proven` or `verified_utc` is empty.

Smallest experiment: one pinned isolated TDM run. Capture the raw manifest and fresh identity timestamps before stop, run `--stop-test-run` once, and list the run directory before/after. Add only the precision normalization assertion and a mismatch event with both timestamps; do not change retention, networking, or gameplay behavior in this experiment. Cross-links: issue-cf9cd77eb056, issue-4e3ef676ba20, issue-3f776ce60511, issue-58ecf44ac956. Reviewed read-only by issue-review-sweeper; no code, build, or runtime change performed.

## Agent suggestion

### 2026-11-21T22:28:15.917Z — issue-review-sweeper

Process proposal from the bounded wake review of issue-6907072bb155 (issue-review-sweeper, wake-d9b6ea26317c, 2026-11-21T22:27Z).

Evidence: `.dev/agent-wake/events.jsonl` has 901 events with 181 queued, 166 `wake.failed`, and 64 `wake.retry`; since 2026-11-21T21:00Z there were 10 queued, 19 claimed, 19 launched, 10 retry, 7 failed, and 2 cancelled. `wake-6b7994156e83` carries attempts=2. `wake-466b0e0174e4` launched at 22:15:39Z and was reconciled failed at 22:19:39Z even though that agent had already produced a coordination context message at 21:55:19Z. Cause in `devscripts/agent-wake.py`: `launch` (`:355-395`) discards the `Popen` handle after recording the PID, and `run_once`/`reconcile_launched` (`:398-424`, `:456-485`) treat any non-alive child PID as `adapter process exited` with no exit code or completion marker. This creates avoidable fallback launches, duplicate agent sessions, and noisy proof evidence; it also makes it impossible to tell normal completion from a crash.

Measurable target: over 20 consecutive generic exit-0 probe wakes, record 20/20 terminal `wake.completed` with exit_code=0 and 0 spurious `wake.retry`/`wake.failed`; genuine non-zero exits still retry/fail. Acceptance: a reader can tell success from failure for every wake from events alone, and duplicate launches per successful wake drop to 0.

Smallest experiment: (1) add a waiter or periodic `child.poll()` that records `returncode` and emits `wake.completed`/`wake.failed`; (2) run 10 wakes with adapter `generic` and `--command 'cmd /c exit 0'` plus 10 with `--command 'cmd /c exit 3'`; require 10/10 success classification and 0/10 success classification respectively; (3) observe the next 20 real wakes for duplicate launches. Keep `issue-62ab3daa615d` separate: full in-band prompt delivery is an independent defect.

## Agent suggestion

### 2026-11-21T22:59:14.439Z — live-build-gap-closure

Cross-agent closure rule requested (2026-11-21): when an investigation discovers a material evidence, instrumentation, capability, or implementation gap, the agent must convert it into an owned issue/task with evidence, a concrete next action, and an acceptance/proof condition, then continue safe work until it is fixed or reduced to a clearly evidenced external/user decision blocker. A report must distinguish gaps that were closed from open blockers; do not leave a known actionable gap as an untracked final recommendation. For shared build/runtime authority or risky architecture decisions, prepare a reviewable proposal and evidence rather than changing the authority unilaterally. Prefer reliable Python/API/CLI probes over UI automation when they provide the needed evidence.

## Agent suggestion

### 2026-11-21T23:08:55.356Z — issue-review-sweeper

Process proposal from bounded independent review of issue-394f2607b148 (issue-review-sweeper, wake-af48c2657e0b, 2026-11-21T23:08Z). No source edit, build, or fix claimed.

Observed: agent-self-improvement.md lines 164-184 label `agent-coord retro <issue>` mandatory after a close, unexpected build wait, or proof failure, but `agent-coord retro` is rejected as an invalid choice and no retro command exists in devscripts/agent_coord.py. agent-runtime-registry.md lines 140-150 document `agent-coord runtime list|show|start|stop|health`, while the CLI exposes register|list|update and a separate devscripts/agent-runtime.py exposes start|stop|status|list. agent-build-queue.md documents build/game/cohorts/<cohort>/current.json, but build/game/cohorts/ does not exist. A cold agent following the specs hits an invalid-choice error or a missing path, and issue-394f2607b148 has sat new with 0/8 readiness fields for about 44h. This is documentation/CLI drift and lost time, not a gameplay defect.

Proposal: add a read-only documentation/CLI conformance check. `devscripts/doc-command-check.py` scans routed dev-loop specs for `agent-coord <group> <verb>` and `devscripts/<tool>.py <verb>` tokens inside code fences, resolves each against that tool's --help, and reports `path:line token -> unknown` with a nonzero exit. Optional --json output for issue intake.

Measurable target: within the four reviewed specs, 0 documented commands that fail to resolve, with each either implemented or explicitly marked proposed/not implemented; a cold agent detects a stale mandatory command in under 2 minutes; the check runs in under 5 seconds with no build.

Smallest experiment: implement the scanner for only the four reviewed specs, run it on the current working tree, and confirm it reports the known `retro` and `runtime show|start|stop|health` mismatches; then implement or mark those sections. Keep it read-only and limit matching to explicit agent-coord/devscripts tokens inside code fences to avoid prose false positives. Class R local evidence tooling; no doctrine or authority change. Cross-link: issue-394f2607b148.

## Agent suggestion

### 2026-11-22T00:12:50.316Z — issue-review-sweeper

Process proposal from issue-review-sweeper bounded sweep (wake-fefc3ae78b7c, 2026-11-22T00:1xZ), based on issue-7d3dcfd7433b evidence.

Observed pattern: the same latency-measurement defect survived many sessions and ~40 launch reports (2026-11-20 09:42 through 2026-11-21 22:40) because reviewers read the issue narrative and reported a "dominant stage" using `stage_elapsed_ms` without checking whether that field is a per-stage delta. In the newest 20 reports, 240 of 288 stage rows across 10 stage types (artifact_selection/validation, launch_configuration, client/server reconciliation, server_reuse, server_gameplay_readiness, build_identity_match, complete, stable_launch_activation) record cumulative `elapsed_ms` because the dev-loop `stage()` call sites omit `stage_started`. Two tracker tasks also have zero `task_proofs` rows and readiness is 1/8.

This is an agent-awareness gap: raw evidence fields were trusted without an invariant check, so a broken measurement passed as "measured".

Proposal (smallest, read-only first): before an issue review writes latency conclusions, run an automated "latency-report lint" over the newest launch/build `stages.jsonl`/timing rows that (a) flags any non-first row where `stage_elapsed_ms == elapsed_ms` (cumulative-not-delta), (b) lists expected-but-missing stages, and (c) reports whether each latency task has a live proof or exemption. Attach the lint output to the review.

Measurable target: over the next 10 launch reports after the lint is adopted, zero unflagged rows with `stage_elapsed_ms == elapsed_ms`, and every latency-tracker review includes the lint result plus proof status; tracker proof rows non-zero or explicitly exempted.

Smallest experiment: implement the checker as a standalone read-only script over existing reports (no source change to the dev loop) and run it on the current 40 reports to confirm the baseline counts above; only then wire it into the scheduled review path. Do not edit `dev-loop.py` under this proposal.
