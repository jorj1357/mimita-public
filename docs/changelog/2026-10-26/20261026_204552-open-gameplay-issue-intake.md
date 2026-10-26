# Open gameplay issues intake: blood and dev-loop feedback

- Result: PASS
- Timestamp: 2026-10-26T20:45:52Z (2026-10-26 16:45:52 EDT)
- Branch: 20261011zt
- Base commit: 926c2d12; no commit created.
- Pre-existing edits: numerous config files were already modified. `docs/specs/open-gameplay-issues.md` also had a pre-existing one-line edit in the priority table describing bullet-hole, blood, and world-impact marks; it was preserved. The earlier session's Juggernaut avatar changelog and the new image-investigation changelog were present and untouched.

## Changed

- `docs/specs/open-gameplay-issues.md`, New intake section (now lines 55-80):
  - Replaced the old stale-mode entry with a broader live-journal/live-mode issue. Preserved the 09:39 EDT TDM report; added observed evidence that run `20261026_145155` reported Sandbox, and requested that status distinguish the current journal/live server mode from launch profile.
  - Added the user-reported Juggernaut blood-spray symptom: visible in TDM, absent in Juggernaut, then worked after restart. Cause and whether restart is causal remain explicitly unknown; request matched live-hit traces before/after restart.
  - Added the dev-loop launch-latency ideal of <=1 ms from pressing `1` to `mimita.exe` process start, explicitly recording it as an unmeasured target, not a guarantee.
- `docs/changelog/2026-10-26/20261026_204552-open-gameplay-issue-intake.md`: this session record.

## Exact content changes

- Old: `Dev-loop status stays on an old game mode` followed by the TDM/`duel_presentation` stale status report and a request to distinguish launch profile from live mode.
- New: `Dev-loop status shows stale journal or mode instead of the live run`, retaining the original report and adding run `20261026_145155`'s Sandbox heartbeat; asks for current journal path and authoritative server mode alongside launch profile.
- Added: `Juggernaut blood spray appears only after restarting the game`; records the TDM-versus-Juggernaut symptom, restart observation, unknown cause, and trace request.
- Added: `Dev-loop launch latency target`; records <=1 ms as the user's ideal and requests measurement and optimization.

## Review and validation

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Router-listed `docs/doc-review-09-03-2026.md` is absent from this checkout (`Test-Path` false); recorded as an unavailable review reference, not recreated.
- Documentation Checker v1: PASS for this edit. New items are under the existing dated intake, reports/observations/targets are labeled, causes remain unknown where unproven, and no links were added. The pre-existing open-issues doc and other unrelated edits were preserved.
- `git diff --check`: PASS for whitespace. Git printed line-ending notices for pre-existing changes in `config/gamemodes/juggernaut.json` and `config/gamemodes/tdm.json`; neither was changed in this session.
- No tests, build, runtime changes, or visual checks were needed for documentation-only intake. The blood report, live status ownership, and launch latency remain open for investigation and measurement.
