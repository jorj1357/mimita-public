# issue-36579a10979a: Wake adapter cannot launch opencode: API key expired

## Human report (verbatim, immutable)

> need an automatic python script that would wake agents


## Review (needs-evidence)

### 2026-11-20T03:58:44.533Z — melee-networking

Reviewer: melee-networking

opencode run reached the API but returned api_key_expired 401. codex is logged in and the codex adapter spawns successfully. Added fallback-adapter support and retry-on-adapter-exit; restarting wake daemon and testing codex/generic fallback remains.

## Agent suggestion

### 2026-11-21T02:39:18.091Z — build-wait-reviewer

Bounded agent-wake test activation succeeded at 2026-11-21T02:22Z: opencode launched from agent-wake, ran agent-coord sync (no halt/freeze, 0 leases), and read dev-loop status without errors. Dev-loop daemon healthy with run 20261120_232200 (uptime ~1h, server PID 33940, client PID 24476). Caveat: game-build generation 417 currently failed with 'codegen timeout: packet_registry_codegen.py', so this proves wake launch but not a clean build. Source: bounded-wake-test session.

## Scheduled independent review

### 2026-11-22T01:04:13.933Z — database-login

A bounded independent review was queued as wake wake-be442221fb74 after 206 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (partial)

### 2026-11-22T01:11:13.412Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review — issue-review-sweeper, wake-be442221fb74, 2026-11-22T01:10Z. No source edit, build, close, or fix claim.

Observed facts

- Issue brief/readiness: state in_review, owner/reporter melee-networking, priority high, severity high; readiness score 1/8 (ready=false); missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. Linked tasks: 0. Prior review id 3 (melee-networking) verdict needs-evidence.
- Proof status: `proof status` contains no record for issue-36579a10979a or any of its tasks; related_build/related_run are empty.
- Primary symptom does not reproduce in the latest wake: `.dev/agent-wake/requests/wake-be442221fb74.json` shows adapter opencode, fallbacks ["codex"], attempts 1, status launched, child_pid 29308; events show wake.claimed adapter=opencode at 2026-11-22T01:04:14.370Z and wake.launched at 01:04:14.409Z; that log (2026-11-22T01-04-13.908Z-issue-review-sweeper.log) contains live OpenCode JSON session events, and this review is running inside the session it launched. A prior bounded test (build-wait-reviewer, 2026-11-21T02:22Z) also recorded a successful opencode launch.
- Retained wake evidence contains no api_key_expired/401 entry: 0 matches in `.dev/agent-wake/events.jsonl` (961 events) and `.dev/agent-wake/logs`. Lifetime events: 249 wake.launched (187 opencode, 62 codex), 173 wake.failed, 73 wake.retry (60 "previous adapter process exited", 13 "stale launched request recovered by scheduler").
- Fallback wiring is present: `devscripts/agent-wake.py:148-155` stores fallbacks; `:404-415` requeues the next fallback when a launched child is gone; `:465-475` does the same in scheduler reconciliation; `:500-501` exposes --fallback-adapter.
- Residual defect: the codex fallback cannot complete a turn with the current global model. `.dev/agent-wake/logs/2026-11-21T23-54-57.860Z-build-wait-reviewer.log` records codex HTTP 400 model_not_found: "The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account." `~/.codex/config.toml` sets model = "gpt-6-luna", and `agent-wake.py:339-345` only passes --model when the request supplies one, so a fallback request with no model inherits the unsupported model. No retained log shows codex completing a wake turn, and the generic adapter has no recent evidence.

Inference / uncertainty

- High confidence: opencode launch through agent-wake works now, so the issue title's failure mode is stale.
- Medium confidence: the original 401 was resolved outside this repository (key refresh/re-login) rather than by a repo change; no retained 401 log proves the current key's origin, and the melee-networking note only records that fallback/retry support was added.
- High confidence: the fallback requirement is not satisfied in practice while codex is pinned to an unsupported model; the mechanism exists but behaviorally fails.
- Medium confidence: some of the 60 "previous adapter process exited" retries are normal completions, because exit is classified without exit code/completion (a separate proposal already recorded on issue-91e3384b219b); this review does not re-litigate that.

Contrary evidence

- The issue premise no longer reproduces: the exact adapter reported as failing (opencode) launched this session.
- The fallback chain is not a no-op: events show retries changing adapter opencode->codex (e.g. wake-b7248bd0b19e at 2026-11-21T23:54:58.512Z) and codex->opencode (wake-6ce039d9862c at 2026-11-22T00:04:26.259Z). So the wiring works; the codex adapter configuration is the broken hop.

Next discriminating check

- One controlled fallback exercise on a test agent: queue a wake with a deliberately failing primary adapter/model (for example opencode with an invalid --model) and `--fallback-adapter codex --model <a model the current ChatGPT account accepts>`. Require events.jsonl to show one wake.retry to codex followed by a codex log containing a completed turn and no further retry; if codex still returns model_not_found, record the exact codex version/model list and keep the issue open. Do not use the shared runtime or edit source.
- Then fill the missing readiness fields (reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk) from this review so the issue can be claimed or formally exempted.

Bounded conclusion: primary symptom fixed/stale; fallback acceptance unproven. Keep in_review. Cold handoff: start at `devscripts/agent-wake.py:326-352` (adapter command/model) and `~/.codex/config.toml`; the only unproven behavior is a fallback adapter completing a turn.
