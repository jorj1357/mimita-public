# issue-6907072bb155: Agent wake, next-work, and observe context bundle

## Human report (verbatim, immutable)

> 


## Task linked

### 2026-11-20T03:58:44.344Z — melee-networking

Task: wake-runner (step 0)

## Agent suggestion

### 2026-11-20T14:43:17.731Z — launch-doctor

Bounded agent-wake test (launch-doctor, wake-688015d2ca4c) evidence: 9 launches succeeded including this session (begin/sync/observe/context ack all worked). Dominant failure across 28 requests in the 14:28-14:32 burst is the already-tracked expired OpenCode API key (401 api_key_expired in .dev/agent-wake/logs/2026-11-20T14-28-*.log, surfaced as 'adapter process exited') = issue-36579a10979a. Distinct second defect: request-store write race when ~40 wakes are enqueued at once; wake-0a87545e8551 failed with [WinError 5] renaming wake-0a87545e8551.json.tmp -> wake-0a87545e8551.json. Suggest retrying os.replace on transient Windows sharing violations and/or serializing request writes. Confidence: high for key-expiry attribution, medium for race reproduction.

## Agent suggestion

### 2026-11-20T15:55:23.623Z — wake-selftest

Bounded wake test found: 'end' and 'activity set' resolve identity only from --agent/MIMITA_AGENT_ID and ignore the work session created by 'begin --agent X'. In the test shell MIMITA_AGENT_ID=melee-parity while begin used --agent wake-selftest, so end deactivated melee-parity instead of wake-selftest. Suggest end/activity default to the begin-created session and warn on identity mismatch.

## Scheduled independent review

### 2026-11-21T22:19:40.054Z — live-build-gui-timer-resource-audit

A bounded independent review was queued as wake wake-d9b6ea26317c after 183 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (partial)

### 2026-11-21T22:27:37.112Z — issue-review-sweeper

Reviewer: issue-review-sweeper

**Independent bounded review — issue-review-sweeper, wake-d9b6ea26317c, 2026-11-21T22:26Z.** No source edits, no build, no close, no fix claim.

**Observed facts**

- Issue state/readiness: `issue-6907072bb155` is `new`, readiness score 0/8, missing human_quote, reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, and risk. Task `wake-runner` (owner melee-networking) is `open` with empty evidence; `proof list`/`proof status` contain no record for this issue or task.
- Implementation exists and is live: `devscripts/agent-wake.py` (23,646 bytes, mtime 2026-11-21T10:27:58Z) plus `devscripts/quality/test_agent_wake.py`; `agent-coord.py` exposes `next` and `observe`, and this session successfully used `begin`, `sync`, `next`, and `observe`.
- This wake's own request proves the runner path: `.dev/agent-wake/requests/wake-d9b6ea26317c.json` shows adapter `opencode`, `fallbacks:["codex"]`, `attempts:1`, status `launched`; events show `wake.queued` 22:19:40.024Z -> `wake.claimed` 22:19:40.872Z -> `wake.launched` 22:19:40.910Z (pid 10368). Adapter selection and fallback wiring are present.
- Bounded context delivery is only partial. `.dev/agent-wake/logs/2026-11-21T22-19-40.021Z-issue-review-sweeper.prompt.txt` contains the full wake template plus the requested work. But the actual first user message delivered to this opencode session is only the `_first_line` summary (wake id, agent, issue, truncated task, pointer to the prompt file). I had to read the prompt file to recover the full task. This reproduces open `issue-62ab3daa615d` (wake prompt delivers only first line to opencode agents) in the same session, so the bounded-context-bundle acceptance is contradicted for the opencode adapter.
- Completion classification is not evidence-backed. `devscripts/agent-wake.py:398-424` and `:456-485` treat any exited child PID as `adapter process exited`, without recording the child exit code or a completion marker. `launch` at `:355-395` discards the `Popen` object after recording the PID. Events since 2026-11-21T21:00Z: 10 queued, 19 claimed, 19 launched, 10 retry, 7 failed, 2 cancelled; failures include wake-7475714ac0b3, wake-9b5f268395f8, wake-4bc8e2c89046, wake-565c35c38094, and wake-d1ee64ab39f3, plus scheduler reconciliations for wake-d3bb8e5b860e and wake-466b0e0174e4. `wake-6b7994156e83` carries `attempts:2`. Lifetime journal: 901 events with 181 queued, 166 `wake.failed`, and 64 `wake.retry`.
- Some of those wakes did real work before being reclassified. For example the earlier issue-review-sweeper wake (wake-466b0e0174e4) launched under codex at 22:15:39Z and was reconciled `failed` at 22:19:39Z, while coordination shows that agent's context message was created at 21:55:19Z. Those events cannot distinguish a normal exit-0 completion from a crash.

**Inference / uncertainty**

- High confidence: the wake-runner capability is implemented and in production use, and the tiered fallback path exists.
- High confidence: the current opencode adapter does not deliver the full bounded prompt in-band; the agent must read `.dev/agent-wake` to know its assignment.
- Medium confidence: a material share of the 166 `wake.failed` / 64 `wake.retry` events are normal adapter completions misclassified as failures, causing avoidable fallback launches and duplicate agent sessions. The exact share cannot be established without exit-code/completion data; known expired-key failures (issue-36579a10979a) and genuine crashes are also present.

**Contrary evidence**

- The system is demonstrably functional: this review was queued, claimed, and launched in under one second, and the scheduler recovered stale `launched` requests rather than leaving them wedged.
- Not all `wake.failed` events are misclassifications; the API-key expiry issue and real adapter crashes remain valid causes.

**Next discriminating check**

1. Safe no-AI probe: queue `python devscripts/agent-wake.py add --agent probe --when manual --adapter generic --command 'cmd /c exit 0'` and inspect `events.jsonl` and the request JSON. If `wake.retry`/`wake.failed` appears for an exit-0 child with no recorded code, the completion-classification defect is confirmed.
2. Delivery probe: for the next opencode wake, diff the session's first user part against its `.prompt.txt`; pass only if the full Requested work section is present (closes or reopens issue-62ab3daa615d).

**Bounded conclusion**

The issue is not absent capability; it is an unproven, partially deployed capability. Keep `wake-runner` open. The two blocking unknowns are full in-band prompt delivery and reliable completion classification. Cold handoff: start from `devscripts/agent-wake.py:_first_line`/`_prompt` for delivery, and add a child-exit recorder around `launch`/`run_once`/`reconcile_launched` for completion.

## Context

### 2026-11-21T22:28:18.775Z — issue-review-sweeper

reproduction: Compare a launched opencode wake session's first user part with its .prompt.txt; this session received only the _first_line summary. Also inspect a launched request after its child PID exits: agent-wake marks it wake.failed/wake.retry without recording exit code.
paths: ['devscripts/agent-wake.py', 'devscripts/agent_coord.py', 'devscripts/quality/test_agent_wake.py', 'docs/specs/dev-loop/agent-wake.md']
evidence: {'review': 'issue-review-sweeper wake-d9b6ea26317c', 'agent_wake_artifacts': ['devscripts/agent-wake.py', 'devscripts/quality/test_agent_wake.py'], 'events': '.dev/agent-wake/events.jsonl', 'lifetime': {'queued': 181, 'failed': 166, 'retry': 64, 'launched': 236, 'total': 901}, 'since_2100Z': {'queued': 10, 'claimed': 19, 'launched': 19, 'retry': 10, 'failed': 7, 'cancelled': 2}, 'this_wake': {'request': '.dev/agent-wake/requests/wake-d9b6ea26317c.json', 'adapter': 'opencode', 'fallbacks': ['codex'], 'attempts': 1, 'prompt_file': '.dev/agent-wake/logs/2026-11-21T22-19-40.021Z-issue-review-sweeper.prompt.txt', 'observed_defect': 'session received only _first_line summary; full requested work present only in prompt file'}, 'proof': 'no proof record for issue-6907072bb155 or wake-runner'}
risk: Medium-high: misclassifying normal adapter completion as failure causes duplicate agent sessions and noisy evidence; opencode currently loses the bounded prompt in-band, so woken agents depend on manual file reads.
proof_expectation: For one queued manual wake: (a) the adapter session's first user part matches the full .prompt.txt including Requested work, and (b) the request records a terminal completed/failed result with a child exit code; events must show wake.completed for exit 0 and no spurious retry. Then repeat for opencode, codex, and generic adapters with a fallback configured.
