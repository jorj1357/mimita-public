# TDM NPC aim error override

- Result: PASS (configuration edit and JSONC validation); runtime/human review remains open.
- Time: 2026-10-26T19:30:39Z (2026-10-26 15:30:39 EDT, America/New_York).
- Branch: `20261011zt`; HEAD before this change: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`.
- Pre-existing worktree edits: `config/behavior-profiles.json` already had extensive unrelated local changes, including a `balanced` aim error of `50.0` at line 72. Those edits were preserved. Other existing modified files were left untouched.

## Change

In the `elite_squad` object of `config/behavior-profiles.json`, added:

```diff
       "id": "elite_squad",
       "skill_tier": "elite",
+      "aim_error_deg": 0.6,
       "focus_enabled": true,
```

The elite skill baseline is `0.06` degrees. The explicit profile value increases its maximum aim error tenfold. TDM assigns NPCs to `elite_npc`, which references `elite_squad`; other actors using `elite_squad`, including `elite_fighter`, receive the same tuning.

## Evidence

- Routing/spec/architecture were read: `docs/ROUTER.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, and `docs/architecture/json-configuration/json-configuration.md`.
- Prior trace established the configuration path: TDM `npc_role` → `elite_npc` role → `elite_squad` profile; `NpcCombat::effectiveAimErrorDegrees()` uses the profile value when active.
- Validation: `devscripts/jsonc.py` successfully parsed `config/behavior-profiles.json`; the loaded `elite_squad.aim_error_deg` was `0.6`. `git diff --check -- config/behavior-profiles.json` passed.
- No build or runtime scenario was run. The JSON edit alone does not prove the running server reloaded it or that in-game aim now feels right; those remain for live runtime/human review.
- No regression was recorded; this change addresses an explicitly requested tuning adjustment.
