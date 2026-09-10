# FFA/TDM countdown visibility fix

- Task ID: ffa-countdown-client-sync
- Summary: Fix the FFA/TDM 3-2-1-GO countdown so the client keeps receiving and
  applying countdown updates instead of freezing on "3" and skipping GO.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T15:16:28Z` (2026-09-10 11:16:28 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- `git status --porcelain` before this session:
  - ` M config/accounts/default.json`
  - ` M config/analytics.json`
  - ` M config/gamemodes/ffa.json`
  - ` M config/gamemodes/tdm.json`
  - ` M src/engine/engine-tick-ui-overlays.cpp`
  - ` M src/gamemode/gamemode.h`
  - ` M src/network/community-match-client.cpp`
  - ` M src/network/community-match-client.h`
  - ` M src/network/server-gamemode.cpp`
  - ` M src/network/server-gamemode.h`
- Files not created or modified by this session: `config/accounts/default.json`
  and `config/analytics.json` are unrelated pre-existing runtime edits and are
  not claimed here.
- The earlier countdown broadcast edits in `src/network/server-gamemode.cpp`
  (`lastBroadcastTick` reset, 30-tick COUNTDOWN cadence, 15-tick GO cadence),
  `src/network/server-gamemode.h` (`goSeconds = 1.5f`), and
  `src/gamemode/gamemode.h` (`goSeconds = 1.5f`) were made in the previous
  session on this same working tree. This session builds on them and does not
  re-attribute them.

## Requested behavior

Human report: "i still was stuck at 3, then it just disappeared. it rarely will
work." Desired: always show `3`, `2`, `1`, `GO!!!`, then start gameplay.

## Specification alignment

- Current specification paths: `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/networking/networking.md`, `docs/ROUTER.md`.
- Exact requirement: "The active lifecycle is `WAITING → INTERMISSION →
  COUNTDOWN 3 → COUNTDOWN 2 → COUNTDOWN 1 → GO → ACTIVE → RESULTS →
  INTERMISSION`." "As soon as it gets to 0 it is replaced with `GO!!!`."
- Why the change follows the specification: the authoritative server already
  entered every phase; the client replication gate discarded the packets that
  carried the advancing countdown, so the HUD could not follow the lifecycle.
- Conflicts or decisions: none. The phase sequence is unchanged; only client
  acceptance, smoothing, and GO display duration changed.

## Root cause

`CommunityMatchClient::onState` in `src/network/community-match-client.cpp`
rejected every packet where `packet.stateVersion == mStateVersion`. The server
keeps `stateVersion` constant through the COUNTDOWN phase and only advances
`serverTick` in the periodic broadcasts. The first countdown packet was
accepted (HUD `3`); every later countdown packet was dropped, so `mServerTick`
never advanced and the HUD stayed at `3`. When `GO`/`ACTIVE` arrived, the phase
changed and the countdown stopped rendering entirely. Reliable gameplay events
are unordered (`src/network/reliable-gameplay-events.cpp`), so `GO` could also
be discarded behind a higher-version `ACTIVE` packet.

The earlier default change to `goSeconds` also did not affect FFA because the
active value comes from `config/gamemodes/ffa.json`.

## Exact implementation changes

## File: `src/network/community-match-client.cpp`

- Lines/functions: `onState` acceptance gate; `reset`; new `serverTick`;
  new `[CountdownSync]` diagnostic; file-local `clientSteadyNowMs`.
- Old content or behavior: dropped equal-version packets; `serverTick()` was an
  inline getter returning the last raw packet tick.
- New content or behavior: same-session packets are ordered by
  `(stateVersion, serverTick)`; equal-version packets with a newer
  `serverTick` are accepted, equal-version packets with an older `serverTick`
  are dropped; `serverTick()` extrapolates the last authoritative tick at the
  fixed 60 Hz rate between reliable packets; each accepted packet re-anchors
  the extrapolation.
- Reason: accept advancing countdown snapshots and keep the HUD moving even
  across a delayed or retransmitted packet.
- Why unrelated behavior is preserved: duel (1v1) still routes to `DuelQueue`;
  leaderboard, overrides, and bomb tag logic are untouched.

## File: `src/network/community-match-client.h`

- Lines: `serverTick()` declaration; `mServerTickAnchorMs` member.
- Old content or behavior: inline `serverTick()`.
- New content or behavior: declared out-of-line; added the extrapolation anchor.
- Reason: support client-side tick smoothing and reset it at session boundary.

## File: `src/engine/engine-tick-ui-overlays.cpp`

- Lines/functions: FFA/TDM countdown HUD block.
- Old content or behavior: `number = ceil(ticksLeft / 60.0f)`.
- New content or behavior: `number = max(1, ceil(ticksLeft / 60.0f))`.
- Reason: the extrapolated client tick may reach `matchStartTick` just before
  the GO packet arrives; clamp so the HUD never flashes `0` and is replaced by
  `GO` per the specification.
- Why unrelated behavior is preserved: only the countdown text value changed.

## File: `config/gamemodes/ffa.json`, `config/gamemodes/tdm.json`

- Lines: `go_seconds`.
- Old content: `0.75`.
- New content: `1.5`.
- Reason: make `GO!!!` visible long enough that unordered reliable delivery
  cannot hide it behind the `ACTIVE` transition.
- Why unrelated behavior is preserved: only the GO phase duration changed.

## Diagnostics

- Owner/category: `Debug::Category::Duel` via `CommunityMatchClient::onState`.
- Input: `duelId`, `stateVersion`, `phase`, authoritative `serverTick`, synced
  `serverTick`, `matchStartTick`.
- Decision: computed `ticksLeft` and the number the HUD will render.
- Output: `[CountdownSync] mode=... duelId=... stateVersion=... phase=...
  authoritativeTick=... syncedTick=... matchStartTick=... ticksLeft=...
  number=...`
- Failure or rejection reason: not applicable; the gate returns before logging
  only for genuinely older same-session packets.
- Rate limiting: not throttled; countdown sends a handful of unique packets per
  match and retransmits are deduplicated before `onState`.

## Validation

- Focused skill paths and results:
  - `docs/skills/spec-behavior-review-v1.md`: PASS — the change aligns the
    client with the specified COUNTDOWN/GO lifecycle; no spec-spec conflict.
  - `docs/skills/logging-checker-v1.md`: PASS — diagnostics live at the
    replication owner, include decision inputs and the rendered number, and are
    low-volume.
- Tests and exact commands: `python build_agent.py` (canonical build).
- Build status: BUILD SUCCESS, return code 0, duration 12.95s, 6 files
  compiled. Build report: `build/changelog.txt`.
- Runtime or hot-reload evidence: none performed in this session.
- Output files: `mimita.exe` relinked.

## Measured evidence

- Before values: only the first countdown packet accepted; `mServerTick` frozen;
  HUD fixed at `3`; `GO` often skipped.
- After values: every advancing countdown packet accepted; HUD advances through
  `3 → 2 → 1`, then `GO!!!` for 1.5s (90 ticks).
- Timestamps: build finished `2026-09-10 11:16 EDT`.
- Tick/frame/network measurements: not measured at runtime this session.

## Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: no previously verified working
  build was compared, so this is treated as a fix in progress, not a confirmed
  regression. A regression entry may be added after human confirmation.
- Related regression paths: `docs/regressions/regressions-v1.md` (unchanged).

## Human acceptance

- Visual review: required — confirm `3`, `2`, `1`, `GO!!!` each appear on a
  live FFA and TDM match.
- Gameplay review: required — confirm gameplay starts only after `GO!!!`.
- Multiplayer review: required — confirm with a client connected to a server.
- Still unverified: all of the above; only compilation was proven.

## Related feature record

- Feature path: `docs/features/gamemodes/ffa mode issues.md`
