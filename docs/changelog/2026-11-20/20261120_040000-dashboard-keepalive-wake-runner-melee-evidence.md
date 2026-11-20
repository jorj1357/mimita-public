// 2026-11-20
/* purpose
* record dashboard keepalive, model-agnostic wake runner, and the first live melee parity trace
* preserve the distinction between source/build and live evidence
* this file DOES NOT claim melee 99.99% parity or a completed wake test
*/

# Dashboard keepalive, wake runner, and melee first divergence

# Task

- Task IDs: melee-networking, wake-runner, dashboard visibility
- Issues: issue-04079f42c6f9, issue-abac88bacd0d, issue-36579a10979a,
  issue-6907072bb155
- Owner: melee-networking
- Date: 2026-11-20T04:00:00+00:00
- Status: source implemented; unit/syntax checks; live melee evidence found
  a transport/dispatch first divergence; no 99.99% claim

# Dashboard

- Added `devscripts/dashboard-keepalive.py`.
- It finds the dashboard by process/command line, starts it if missing,
  restores minimized/off-screen windows, and can focus on request. It does not
  use topmost and does not stop game/build processes.
- Live restoration: the dashboard PID `2132` had rect `-1514,96,-495,943`; the
  keepalive moved it to `20,20,900,780` and emitted `dashboard.restored`.
- Started the keepalive daemon (`pythonw.exe ... --interval 5`).
- Logged `issue-abac88bacd0d` for the user report that the dashboard was not
  visible/open.

# Wake runner

- Added `devscripts/agent-wake.py` with durable JSON wake requests, build/
  generation/event/timeout conditions, bounded context prompts, and adapters:
  - opencode (`opencode run ...`)
  - codex (`codex exec ...`)
  - generic command template
- Added fallback adapter support and retry when a launched adapter process
  exits.
- The wake prompt requires `agent-coord begin/sync/observe`, test-runs through
  the root dev-loop, `events.jsonl` inspection, no compiler/build, and no
  edits/commits without approval.
- Started the wake daemon.
- First wake `wake-5c7592be30d2` triggered on game generation 393 but opencode
  returned `api_key_expired` (401). Codex is logged in and its adapter was
  observed spawning. Logged `issue-36579a10979a`.
- A codex smoke process was started by a manual test, then stopped; no code
  edits were claimed from it.

# Melee evidence

Pinned isolated run `20261120_034533`:
- artifact `2026-11-20T02:29:08.406738Z`
- game generation `393`
- TDM `dust2cyberiav4`, weapon set 31, `healthme 999999`, AI Mace

Result:
- client: 171 `melee.client.contact_claim`, 123 batches, all sent;
- server: zero `melee.contact.batch.received` and zero accepted/rejected claim
  decisions;
- therefore the first divergence is **packet dispatch/receipt**, not final
  damage or prediction.
- The report was rerun against the current journal; an earlier output was not
  stable and is not used as evidence.
- Server `server.damage.applied` events exist from NPC combat, but no melee
  claim path events.

Source changes queued:
- client blade-capsule fallback when configured damage boxes miss;
- server claimed-shape validation with current-pose/current-reach fallback when
  contact-tick pose/reach has diverged;
- server dispatch diagnostic `melee.contact.batch.dispatch` with bytes and
  expected batch size;
- route oversized NPC-damage batches to the authoritative melee handler.
- Build note `c995be063d53` and `cd0af6b94a11` queued; game-DLL request accepted.
- The existing generation 393 test did not include the new dispatch diagnostic;
  no server melee claim events were present.

# Validation

- `python -m py_compile` passed for `agent-wake.py` and
  `dashboard-keepalive.py`.
- Wake adapter dry-run and manual request paths exercised; failed opencode auth
  captured.
- Dashboard keepalive live-restored the existing window and logged the event.
- No successful woken codex/generic test run with issue evidence yet.
- No live accepted melee claim with the new build yet.

# Next exact step

1. Publish the queued game DLL with dispatch/current-pose changes.
2. Rerun a pinned TDM Mace run.
3. Require `melee.contact.batch.dispatch` in the server journal and correlate
   contact IDs through accepted/rejected and damage.
4. Fix the first divergence; repeat NPC then PvP.
5. Retest wake with codex fallback once the adapter/auth path is stable.

# Reverse

- Remove `devscripts/agent-wake.py`, `devscripts/dashboard-keepalive.py`.
- Revert the touched C++/docs paths; preserve unrelated dirty changes.
