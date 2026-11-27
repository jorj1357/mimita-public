# issue-593e7d506d74: Require approved single-flight build windows with timeout fallback

## Human report (verbatim, immutable)

> can u ensure that in agent isntructions  all agents must coordinate on 1 build and no one can run a buidl without other ppl approving unless like atimeout and todo suggest better ways to do that  bc the gaols are  avoid resourc pressure and be efficient with  builds


## Review (implemented-docs-and-guard-check)

### 2026-11-27T06:42:25.352Z — build-approval-policy

Reviewer: build-approval-policy

Implemented the user's build-coordination directive in AGENTS gate 0 and the dev-loop specs. AGENTS.md now requires: (1) a durable `agent-coord.py build request`, (2) build-owner approval or two non-requester peer confirmations when no owner is set, (3) build through the already-guarded `--request agent-build --build-request <approved>` path, (4) single-flight (no second approved/active request), (5) a 5/10-minute timeout fallback that lets the requester self-approve and record the wait only when nothing is active and no objection/halt/freeze exists, and (6) a postmortem read/action before the next request. `batch-build`, `build`, and `game-dll` are now owner/human seal actions that require a recorded approved request first.

Specs aligned: docs/specs/dev-loop/agent-lanes-build-authority.md gained the 'Build approval protocol (2026-11-27)' and 'Better build coordination - TODO proposals'; agent-build-queue.md gained single-flight/timeout rules; agent-coordination.md no longer says builds need no approval beyond the human.

Proposals recorded: merge-queue build token, fixed build windows, change-value gate, coalesce by path set, resource admission, quiet-window acks, preview only when needed, and postmortem-driven tuning.

Guard check observed: `dev-loop.py --request agent-build --build-request build-request-does-not-exist` was rejected by the daemon (lifecycle=request_rejected, game-build=idle, no build attempt). The CLI transport printed 'accepted=True queued=True' before the daemon rejection, so the operator-facing message can be improved as a follow-up. The unguarded `batch-build`/`build`/`game-dll` commands remain a policy-only gate until a minted token or `--build-request` requirement is implemented; that is the largest remaining hard-enforcement gap.

## Proof exemption

### 2026-11-27T06:42:25.898Z — build-approval-policy

Reason: Documentation/instruction change with no runtime behavior; acceptance is the text diff plus the existing agent-build approval guard check, so compile/runtime proof does not apply.

Paths: AGENTS.md, docs/specs/dev-loop/agent-lanes-build-authority.md, docs/specs/dev-loop/agent-build-queue.md, docs/specs/dev-loop/agent-coordination.md

## Resolution

### 2026-11-27T06:42:26.578Z — build-approval-policy

AGENTS and dev-loop specs now require approved single-flight build requests with a 5/10-minute timeout fallback; agent builds use the guarded agent-build path; proposals recorded.

Proof exemption: Documentation/instruction change with no runtime behavior; acceptance is the text diff plus the existing agent-build approval guard check, so compile/runtime proof does not apply.
