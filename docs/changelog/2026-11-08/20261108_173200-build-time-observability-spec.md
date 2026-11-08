// 2026-11-08 17:32 EST

# Build time observability specification

## Session record

- Branch: `checkpoint/pre-live-kernel-20261108`.
- HEAD at start: `760f2b82`.
- Pre-existing worktree edits: numerous unrelated edits were present across
  `AGENTS.md`, build/dev-loop scripts, configuration, networking/NPC/gameplay
  sources, and their documentation. They were preserved. In particular,
  `docs/ROUTER.md`, `docs/specs/dev-loop/dev-loop.md`, and
  `docs/operations/build-and-exe/build-and-exe.md` already contained edits;
  only the additions described below were made in those files.
- No source, compiler setting, lane, scheduler, or running process was changed.

## Live build inspection

- Ran `python devscripts/agent-lane.py status
  world-streaming-nav-reconnect-20261108`. It failed before returning status:
  `git status --porcelain --untracked-files=all` exited 128 because Git could
  not access `C:/mimita-v9/.git/modules/external/libjuice`.
- Read the lane's `.dev/state.json` directly. At 2026-11-08 17:31:41 EST it
  reported `build_status=building`, source generation 8, and 736.5 seconds of
  active elapsed time. Its output tail recorded waits for 891 MB free memory
  (1024 MB required) and the machine-wide EXE slot, followed by slot
  acquisition. `active_build_detail` remained `[HOT RELOAD] DLL up to date,
  skipping`; the last completed build result remained `failed` at 1591.89
  seconds. The state file's UTC update was current at inspection time.
- Process verification was inconclusive: `Get-Process` returned no matching
  PID output, and `tasklist` failed with `The user name or password is
  incorrect.` Therefore the state does not prove that the compiler process is
  still active or that it is stuck. Current output is insufficient to identify
  the active compile/link phase.

## Changes made

- Added `docs/specs/dev-loop/build-time-observability.md`. It specifies build
  attempt identity; UTC timestamps plus monotonic durations; separate wait,
  setup, PCH, per-translation-unit compile, link, validation, publication, and
  follow-up measurements; live phase/file status; flushed progress output;
  managed persisted timing records; and evidence-based diagnosis and
  acceptance scenarios.
- Added a router entry in `docs/ROUTER.md` for build timing and phase diagnosis.
- Linked the detailed contract from the existing timestamp requirements in
  `docs/specs/dev-loop/dev-loop.md`.

## Reasoning

The lane currently reports scheduler and memory waits but leaves the active
detail on an earlier hot-reload message for more than twelve minutes. The
status command itself is blocked by the submodule access error, and process
inspection was unavailable. The new specification requires enough streamed
and persisted evidence to separate queueing from compilation, linking, and
publishing, and to distinguish stale status from a live long phase. It does not
change authority or scheduling.

## Documentation and validation

- Read `docs/ROUTER.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/skills/documentation-checker-v1.md`.
- Documentation check: required purpose and DOES NOT statements are present;
  relative links resolve; `git diff --check` passed for the tracked document
  edits. No tests or builds were run because this change is specification-only.
- The routed dev-loop specification still has an older contradictory lane
  paragraph: `docs/specs/dev-loop/dev-loop.md:2727-2730` says Mode 2 lanes are
  experimental and must not be used, while the current AGENT instructions and
  `dev-loop.md:330-341` make Mode 2 mandatory. Smallest correction: replace the
  old prohibition with the current mandatory-lane rule and point to
  `agent-lanes-build-authority.md`. It was left untouched because lane-policy
  cleanup is separate from the requested timing contract.
- Unresolved unrelated documentation TODOs were left unchanged. The repository
  scan includes `docs/architecture/player-npc-systems/npc-movement.md:22`
  (`todo - explain`), which should explain the current NPC movement owner and
  inputs, and `docs/specs/weapons/tool-weapon-ideas.md:1` (`todo explain like`),
  which should identify the file as proposals and state the intended tool
  behavior. The broader TODO list remains outside this build-observability
  change.
- Focused review: `PASS_WITH_HUMAN_REVIEW`. The new contract is linked and
  internally consistent. Implementation and live timing acceptance remain
  future work; no build-performance cause has been confirmed from this run.

## Human review still needed

Review whether the proposed 30-attempt timing-history minimum and per-file
timing detail fit the desired disk and status-history behavior. Implementation
must later exercise no-op, small-source, broad-header, and real scheduler-wait
builds through the canonical dev-loop.
