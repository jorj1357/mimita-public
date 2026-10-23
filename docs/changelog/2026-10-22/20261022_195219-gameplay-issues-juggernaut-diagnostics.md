# Gameplay issue intake: F3, Juggernaut, and dev-loop timing

- Task: Record new gameplay and tooling issues and inspect the supplied build 2843 journal.
- Result: PASS_WITH_FOLLOW_UP
- Time: 2026-10-22T19:52:19Z (America/New_York)

## Change

### `docs/specs/open-gameplay-issues.md`

- Lines 114-117 added four intake reports: F3 network diagnostics opening stutter; build 2843 Juggernaut startup/frame/tick pacing, memory growth, reported snapshot-age increase and freeze/disconnect; NPC stuck episodes, avatar texture appearance, and avatar-scale collision fit on Dust2CyberiaV4; and lower-priority rainbow clock/status refresh timing.
- Lines 129-132 added matching issue-index entries.
- Before: none of these four reports or their build 2843 journal evidence appeared in the 2026-10-22 intake or issue index.
- After: the reports include verified journal measurements where available and explicitly preserve unresolved causes and symptoms not captured by the journal.

## Evidence and limits

- Inspected `logs/2026-10-22/20261022_194716/events.jsonl`: build 2843, Juggernaut, map `dust2cyberiav4`, connected client/server, 38 frame-budget spikes, first sampled 60-frame window average 26.48 ms / maximum 1357.04 ms, and early 67-NPC server windows averaging as high as 170.72 ms / maximum 1183.58 ms.
- The logged working set grew from about 223 MB to 4.57 GB in roughly 57 seconds. This is measured in the supplied run; the cause and relationship to the separate reconnect report are not established.
- Journal contains 1,014 NPC stuck episodes and 306 recovery events. Coordinates were recorded, but Dust2CyberiaV4 geometry was not checked to prove they are corners.
- Journal ends connected and contains no disconnect event. Its snapshot window reports interarrival timing, not the reported 230–350 ms snapshot-age value. A 240 FPS target is present but does not prove achieved FPS. The later server-window averages were mostly 4–8 ms, so this journal does not establish a continually worsening server tick cost.
- F3 stutter, chopped avatar textures, avatar-scale collision mismatch, disconnect/freeze, snapshot-age values, and actual steady 240 FPS remain user-reported and unverified in this journal.

## Review and validation

- Read `docs/ROUTER.md` and the router-selected issue, performance, Juggernaut, NPC, collision, GUI, networking, logging, dev-loop, runtime-validation, and documentation-review guidance.
- Confirmed linked owner documents exist. The dev-loop spec documents one-second status animation cadence and immediate redraw for key-triggered status changes; the new report asks to compare actual clock/status refresh with that contract.
- `git diff --check -- docs/specs/open-gameplay-issues.md` passed (Git noted the file's LF-to-CRLF conversion setting).
- TODO scan of the edited issue inbox found one pre-existing embedded TODO at line 23 concerning JSON-authored spawn points for every map; it was not changed.
- Documentation-only intake; no build, runtime replay, tests, or human visual acceptance performed.

## Repository state

- `docs/specs/open-gameplay-issues.md` already had unrelated prior-session edits before this turn; those were preserved.
- No other repository files were edited by this turn except this required changelog.
