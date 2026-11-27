# Build approval policy: single-flight builds with timeout fallback

- Task/agent: `build-approval-policy`
- Date, time, timezone: 2026-11-27T06:45Z, ISO 8601
- Status: instructions and specs updated; policy active; hard guard on the
  remaining seal commands is explicitly follow-up work
- Linked issue: `issue-593e7d506d74` (done)

## Requested behavior

All agents must coordinate on one build, and no agent may start a build without
other agents' approval. A bounded timeout may substitute for approval so one
silent or busy agent cannot block progress. Record suggestions for fewer and
cheaper builds, with the goals of avoiding resource pressure and being efficient
with builds.

## Specification and instruction changes

1. `AGENTS.md` gate 0 now makes build approval mandatory:
   - queue the DLL change note with `--why` before the first compiler-input save;
   - file `agent-coord.py build request --kind integration --paths ... --issue ...`;
   - build owner approves, or two non-requester peers confirm when no owner is
     set (at least one records the approval); a requester never self-approves,
     and if the requester is the owner a different agent approves;
   - agents build through `--request agent-build --build-request <approved>`;
   - `--request batch-build`, `--request build`, and `--request game-dll` are
     owner/human seal actions that require a recorded approved request first;
   - single-flight: no second approved/queued/building request;
   - 5-minute notice / 10-minute silence timeout permits self-approval only when
     no build is active/approved and no halt/freeze applies;
   - after every build, read `game-generation.ready.postmortem` and act on the
     largest measured cause before the next request.
2. `docs/specs/dev-loop/agent-lanes-build-authority.md` gained the
   "Build approval protocol (2026-11-27)" section and the
   "Better build coordination — TODO proposals" section; the Mode 3 flow now
   goes edit -> request/approve -> build -> drain -> postmortem -> test -> repeat.
3. `docs/specs/dev-loop/agent-build-queue.md` gained the single-flight and
   timeout rules and the postmortem step under auto-approval.
4. `docs/specs/dev-loop/agent-coordination.md` no longer says a build needs no
   approval beyond the human; it points at the approval path.

## Recorded proposals (TODO, not implemented)

- Merge-queue build token minted by approval; token required by `batch-build`.
- Fixed build windows (for example :00 and :30) with skipped empty windows.
- Change-value gate: skip builds for docs/config-only or fully reverted paths.
- Coalesce requests that touch the same path set into one integration build.
- Resource admission: reserve memory for pinned runtimes and deny approval above
  the reserve.
- Quiet-window acks from compiler-input owners to seal without polling.
- Preview builds only while integration is broken.
- Postmortem-driven tuning of the slowest TU/header fanout before adding builds.

## Guard check

- `python devscripts/dev-loop.py --request agent-build --build-request
  build-request-does-not-exist` reached the control transport, then the daemon
  rejected it: `lifecycle=request_rejected`, `game-build=idle`, no build attempt.
- The CLI printed `accepted=True queued=True` before the daemon rejection; that
  operator-facing message should be improved as follow-up.

## Not claimed

- No compiler, build, or runtime test was run for this change.
- `batch-build`, `build`, and `game-dll` remain policy-only gates. They do not
  yet require `--build-request` or a minted owner token. That is the largest
  remaining enforcement gap and is recorded in the issue and proposals.

## Coordination

- `agent-coord begin/sync`, issue intake with the verbatim user quote, review,
  proof exemption (documentation-only), and close were recorded under
  `build-approval-policy`.
- Existing agent work and processes were not stopped or changed.
